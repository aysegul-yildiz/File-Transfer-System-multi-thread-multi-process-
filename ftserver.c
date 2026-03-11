
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

#define BUFFER_SIZE 1024

void serve_client(const char *directory, int send_size, const char *mq_name, const char *cs_pipe, const char *sc_pipe);
void handle_sigterm(int signal_num);
mqd_t mq;
pid_t child_pids[10];
int child_count = 0;
const char *mq_name;

int main(int argc, char *argv[]){
    if (argc != 4){
        printf("server call should be in the format: %s DIRECTORY MQNAME SENDSIZE\n", argv[0]);
        exit(1);
    }

    printf("server process started with pid: %d\n", getpid());
    const char *directory_name = argv[1];
    mq_name = argv[2];
    int send_size = atoi(argv[3]);
    struct mq_attr attributes;
    attributes.mq_maxmsg = 10;
    attributes.mq_msgsize = BUFFER_SIZE;
    mq = mq_open(mq_name, O_CREAT | O_RDWR, 0644, &attributes);
    if (mq == -1){
        perror("message queue could not be opened");
        exit(1);
    }
    printf("message queue '%s' opened\n", mq_name);
    signal(SIGTERM, handle_sigterm);

    char buffer[BUFFER_SIZE];
    while (1){
        ssize_t n = mq_receive(mq, buffer, BUFFER_SIZE, NULL);
        printf("server is waiting for a message from client\n");
        if (n == -1){
            printf("message could not be received\n");
            break;
        }
        printf("the message received from the client: %s\n", buffer);

        pid_t client_pid;
        char cs_pipe[BUFFER_SIZE];
        char sc_pipe[BUFFER_SIZE];
        sscanf(buffer, "%d,%s,%s", &client_pid, cs_pipe, sc_pipe);
        printf("the client's id: %d\n", client_pid);
        snprintf(cs_pipe, sizeof(cs_pipe), "/tmp/cs_pipe_%d", client_pid);
        snprintf(sc_pipe, sizeof(sc_pipe), "/tmp/sc_pipe_%d", client_pid);

        pid_t pid = fork();
        if (pid < 0){
            printf("fork was not successful\n");
            continue;
        }
        else if (pid == 0){
            printf("fork successful, a server child will serve the client now\n");
            serve_client(directory_name, send_size, mq_name, cs_pipe, sc_pipe);
            exit(0);
        }
        else{
            child_pids[child_count++] = pid;
            printf("parent process with id: %d\n", pid);
        }
    }
    mq_close(mq);
    mq_unlink(mq_name);
    return 0;
}

void serve_client(const char *directory_name, int send_size, const char *mq_name, const char *cs_pipe, const char *sc_pipe){

    printf("Now the server child will serve the client with pipes %s and %s\n", cs_pipe, sc_pipe);

    int cs_open;
    cs_open = open(cs_pipe, O_RDONLY);

    if (cs_open != -1){
        printf("server child opened cs pipe %s\n", cs_pipe);
    }
    else{
        printf("server child could not open cs pipe ");
        return;
    }

    int sc_open;
    sc_open = open(sc_pipe, O_WRONLY);

    if (sc_open != -1){
        printf("server child opened sc pipe %s\n", sc_pipe);
    }
    else{
        printf("server child could not open sc_pipe");
        return;
    }

    const char *connection_msg = "server child is connected\n";
    write(sc_open, connection_msg, strlen(connection_msg));

    char request[BUFFER_SIZE];
    while (1){
        ssize_t bytes_read = read(cs_open, request, BUFFER_SIZE);
        if (bytes_read <= 0){
            printf("could not read request");
            break;
        }
        printf("%s request received\n", request);

        if (strncmp(request, "quit", 4) == 0){
            printf("Client quit\n");
            break;
        }
        else if (strncmp(request, "list", 4) == 0){
            printf("list command received\n");
            DIR *dir = opendir(directory_name);
            if (dir == NULL){
                printf("couldd not open directory");
                break;
            }
            struct dirent *entry;
            char file_list[BUFFER_SIZE] = "";

            while ((entry = readdir(dir)) != NULL){
                if (entry->d_type == DT_REG){
                    if (strlen(file_list) + strlen(entry->d_name) + 2 >= BUFFER_SIZE){
                       if (strlen(file_list) > 0) {
                             write(sc_open, file_list, strlen(file_list) + 1);
                        }

                        file_list[0] = '\0';
                    }
                    strcat(file_list, entry->d_name);
                    strcat(file_list, "\n");
                }
            }
            write(sc_open, file_list, strlen(file_list));
            closedir(dir);
        }
        else if (strncmp(request, "get ", 4) == 0){
            printf("get command received\n");
            char filename[BUFFER_SIZE];
            sscanf(request, "get %s", filename);
            char filepath[BUFFER_SIZE];
            snprintf(filepath, sizeof(filepath), "%s/%s", directory_name, filename);

            FILE *file = fopen(filepath, "rb");
            if (file){
                char buffer[send_size];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, send_size, file)) > 0){
                    ssize_t bytes_written = write(sc_open, buffer, bytes_read);
                    if (bytes_written < 0){
                        printf("write failed");
                        break;
                    }
                }
                fclose(file);
            }
            else{
                const char* error_msg = "file not found\n";
                write(sc_open, error_msg, strlen(error_msg) + 1);
            }
        }
    }
    mq_close(mq);
    close(cs_open);
    close(sc_open);
    unlink(cs_pipe);
    unlink(sc_pipe);
}

void handle_sigterm(int signal_num){
    printf("terminating server\n");

    for (int i = 0; i < child_count; i++){
        if (child_pids[i] > 0){
            kill(child_pids[i], SIGTERM);
        }
    }

    mq_close(mq);
    mq_unlink(mq_name);

    exit(0);
}

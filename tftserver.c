#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

void *serve_client(void *args);
void handle_sigterm(int signal_num);
mqd_t mq;
pthread_t child_threads[10];
int child_count = 0;
const char *mq_name;

typedef struct {
    char directory[BUFFER_SIZE];
    int send_size;
    char cs_pipe[BUFFER_SIZE];
    char sc_pipe[BUFFER_SIZE];
} client_args_t;

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

        client_args_t *client_args = malloc(sizeof(client_args_t));
        strncpy(client_args->directory, directory_name, BUFFER_SIZE);
        client_args->send_size = send_size;
        strncpy(client_args->cs_pipe, cs_pipe, BUFFER_SIZE);
        strncpy(client_args->sc_pipe, sc_pipe, BUFFER_SIZE);

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, serve_client, (void *)client_args) != 0){
            printf("thread could not be created\n");
            free(client_args);
            continue;
        }

        child_threads[child_count++] = thread_id;
        printf("parent process created thread %lu for client\n", thread_id);
    }

    mq_close(mq);
    mq_unlink(mq_name);
    return 0;
}

void *serve_client(void *args){
    client_args_t *client = (client_args_t *)args;

    printf("server child thread will serve the client with pipes %s and %s\n", client->cs_pipe, client->sc_pipe);

    int cs_open = open(client->cs_pipe, O_RDONLY);
    if (cs_open != -1){
        printf("server thread opened cs pipe %s\n", client->cs_pipe);
    }
    else{
        printf("server thread could not open cs pipe\n");
        free(client);
        pthread_exit(NULL);
    }

    int sc_open = open(client->sc_pipe, O_WRONLY);
    if (sc_open != -1){
        printf("server thread opened sc pipe %s\n", client->sc_pipe);
    }
    else{
        printf("server thread could not open sc pipe\n");
        close(cs_open);
        free(client);
        pthread_exit(NULL);
    }

    const char *connection_msg = "server thread is connected\n";
    write(sc_open, connection_msg, strlen(connection_msg));

    char request[BUFFER_SIZE];
    while (1){
        ssize_t bytes_read = read(cs_open, request, BUFFER_SIZE);
        if (bytes_read <= 0){
            printf("could not read request\n");
            break;
        }
        printf("%s request received\n", request);

        if (strncmp(request, "quit", 4) == 0){
            printf("Client quit\n");
            break;
        }
        else if (strncmp(request, "list", 4) == 0){
            printf("list command received\n");
            DIR *dir = opendir(client->directory);
            if (dir == NULL){
                printf("could not open directory\n");
                break;
            }
            struct dirent *entry;
            char file_list[BUFFER_SIZE] = "";

            while ((entry = readdir(dir)) != NULL){
                if (entry->d_type == DT_REG){
                    if (strlen(file_list) + strlen(entry->d_name) + 2 >= BUFFER_SIZE){
                        if (strlen(file_list) > 0){
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
            snprintf(filepath, sizeof(filepath), "%s/%s", client->directory, filename);

            FILE *file = fopen(filepath, "rb");
            if (file){
                char buffer[client->send_size];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, client->send_size, file)) > 0){
                    ssize_t bytes_written = write(sc_open, buffer, bytes_read);
                    if (bytes_written < 0){
                        printf("write failed\n");
                        break;
                    }
                }
                fclose(file);
            }
            else{
                const char *error_msg = "file not found\n";
                write(sc_open, error_msg, strlen(error_msg) + 1);
            }
        }
    }

    close(cs_open);
    close(sc_open);
    unlink(client->cs_pipe);
    unlink(client->sc_pipe);
    free(client);
    pthread_exit(NULL);
}

void handle_sigterm(int signal_num){
    printf("terminating server\n");

    for (int i = 0; i < child_count; i++){
        pthread_cancel(child_threads[i]);
    }

    mq_close(mq);
    mq_unlink(mq_name);

    exit(0);
}

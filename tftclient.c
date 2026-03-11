#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h> 

#define BUFFER_SIZE 1024

mqd_t mq;
int main(int argc, char const *argv[]){
    if (argc != 2){
        printf("call for client should be in this format: %s MQNAME\n", argv[0]);
        exit(1);
    }

    const char *mq_name = argv[1];
    mq = mq_open(mq_name, O_WRONLY);

    if (mq == -1){
        printf("client couldn't open message queue\n");
        exit(1);
    }

    char cs_pipe[BUFFER_SIZE];
    snprintf(cs_pipe, sizeof(cs_pipe), "/tmp/cs_pipe_%d", getpid());

    unlink(cs_pipe);
    if (mkfifo(cs_pipe, 0666) == -1){
        printf("cs pipe was not created \n");
        exit(1);
    }

    char sc_pipe[BUFFER_SIZE];
    snprintf(sc_pipe, sizeof(sc_pipe), "/tmp/sc_pipe_%d", getpid());

    unlink(sc_pipe);
    if (mkfifo(sc_pipe, 0666) == -1){
        printf("sc_pipe was not created\n");
        exit(1);
    }

    printf("created pipes: %s %s", cs_pipe, sc_pipe);
    sleep(1);

    char con_msg[BUFFER_SIZE];
    snprintf(con_msg, sizeof(con_msg), "%d,%s,%s", getpid(), cs_pipe, sc_pipe);

    if (mq_send(mq, con_msg, strlen(con_msg) + 1, 0) == -1){
        printf("message was not sent.\n");
        exit(1);
    }
    printf("message: '%s' sent\n", con_msg);

    int cs_open;
    cs_open = open(cs_pipe, O_WRONLY);
    if (cs_open == -1){
        printf("client could not open cs pipe.\n");
        exit(1);
    }
    else{
        printf("client successfully opened cs pipe\n");
    }

    int sc_open;
    sc_open = open(sc_pipe, O_RDONLY);
    if (sc_open == -1){
        printf("client could not open sc pipe.\n");
        exit(1);
    }
    else{
        printf("client successfully opened sc pipe\n");
    }

    FILE *local_file;
    char local_filename[256];

    while (1){
        char request[BUFFER_SIZE];
        printf("ennter your request (list, get <filename>, quit): ");
        fgets(request, sizeof(request), stdin);
        request[strcspn(request, "\n")] = 0;
        if (strncmp(request, "quit", 4) == 0){
            write(cs_open, request, strlen(request) + 1);
            printf("sent quit request\n");
            break;
        }
        else if (strncmp(request, "list", 4) == 0){
            write(cs_open, request, strlen(request) + 1);
            printf("sent list request\n");
            char response[BUFFER_SIZE];
           clock_t start_time = clock();
            sleep(1);  
            ssize_t bytes_read = read(sc_open, response, sizeof(response));
            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) * 1000.0  / CLOCKS_PER_SEC;
            response[bytes_read]= '\0';
            printf("files:\n%s", response);
            printf("Time taken to get the response: %.2f seconds\n", elapsed_time);
        
        }

        else if (strncmp(request, "get ", 4)==0){
            char filename[BUFFER_SIZE];
            sscanf(request, "get %s", filename);
            write(cs_open, request, strlen(request) + 1);
            printf("sent get request for file: %s\n", filename);

            snprintf(local_filename, sizeof(local_filename), "%s", filename);
            local_file = fopen(local_filename, "wb");
            if (local_file == NULL){
                perror("could not open file for writing");
                continue;
            }

            char buffer[BUFFER_SIZE];
            ssize_t bytes_read;
            clock_t start_time = clock();
            while ((bytes_read = read(sc_open, buffer, sizeof(buffer))) > 0){
                fwrite(buffer, 1, bytes_read, local_file);
                if (bytes_read < sizeof(buffer)){
                    break;
                }
            }

            clock_t end_time = clock();
            double elapsed_time = (double)(end_time - start_time) * 1000.0 / CLOCKS_PER_SEC;

            fclose(local_file);
            printf("file saved as '%s'\n", local_filename);
             printf("Time taken to transfer file: %.2f seconds\n", elapsed_time);
        }
     }
    
    close(cs_open);
    close(sc_open);
    unlink(cs_pipe);
    unlink(sc_pipe);
    mq_close(mq);
    return 0;
}

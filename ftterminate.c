#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char *argv[]){
    if (argc != 2){
        printf("the call for terminator : %s PID\n", argv[0]);
        exit(1);
    }

    pid_t server_pid = (pid_t)atoi(argv[1]);
    if (kill(server_pid, SIGTERM) == -1){
        printf("could not send SIGTERM");
        exit(1);
    }
    printf("sent SIGTERM to server with PID %d\n", server_pid);
    return 0;
}

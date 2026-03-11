# File-Transfer-System-multi-thread-multi-process-
This project was developed for CS 342, operating systems at Bilkent University in 2024. It implements a server client file transfer system using two different concurrency approaches in C.

The system allows a client to connect to a server to either list available files or download a specific file.
Multi process version: Uses fork() to create a new process for every client.
Multi threaded version: Uses pthread_create() to handle multiple clients within a single process.

###### POSIX message queues: Used for the connection requests between client and server.
###### Named pipes (FIFOs): Dedicated pipes are created for each client session to handle data transfer (list and get operations).
###### Performance analysis: A detailed study of how client density, message send sizes and file sizes impact system throughput and response times.

ftserver.c - ftclient.c: the multitprocess implementation  
tftserver.c - tftclient.c: the multitthreaded implementation  
ftterminate.c: a utility to safely shut down the server using SIGTERM  
Makefile: Automates the compilation of all components.  
CS 342-report.docx: Contains experimental results and performance comparisons.  

### How to run?
Build all executables using the provided Makefile by typing "make"  
Start the server by providing the directory to serve, a unique message queue name and the buffer send size. eg: ./ftserver ./files my_queue 512  
start the client: ./ftclient my_queue  
Commands: once connected, the client supports list, get <filename> and quit  
to shut down the server properly, use the termination tool with the server's PID: ./ftterminate <PID>



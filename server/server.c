/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include "token.h"

#define BACKLOG MAX_N    // how many pending connections queue will hold

#define PORT_IDX    1
#define OUT_IDX     2
#define N_IDX       3
#define NI_IDX      4
#define ADDR_IDX    5

unsigned* complete; 

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void client_send(token_t* token, char* hostname, char* port)
{
    int sockfd, numbytes;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    // printf("client_send %s : %s\n", hostname, port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    // printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    // Send the token
    if (send(sockfd, token, sizeof(token_t), 0) == -1)
        perror("send");

    close(sockfd);
}

void end_prog(int sig)
{
    if(sig == SIGALRM) {

        printf("Server closing\n");
        exit(0); 
    }
}

int main(int argc, char *argv[])
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    token_t token;
    int token_sent=0;
    int i, n, path_len, n_i;
    int all_complete;
    int complete_id;
    unsigned label;



    if(argc < 6) {
        fprintf(stderr, "usage: <port> <output_file.txt> <n> <n[i]> <hostname[0]> <port[0]> ... <path_len> <path[0]> ... \n");
        exit(1);
    }
    n = strtol(argv[N_IDX], NULL, 10);
    n_i = strtol(argv[NI_IDX], NULL, 10);
    path_len = strtol(argv[NI_IDX+n*2+1], NULL, 10);

    srand(n_i+time(NULL));
    label = rand() % 10 +1;

    printf("n = %d\n", n);
    printf("n_i = %d\n", n_i);
    printf("path_len = %d\n", path_len);
    printf("label= %d\n", label);

    if(n > MAX_N) {
        printf("n > MAX_LEN: %d\n", MAX_N);
    }

    // initialize the complete array
    complete_id = shmget(IPC_PRIVATE, MAX_N*sizeof(unsigned), IPC_CREAT | 0666);
    if (complete_id < 0) {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }

    complete = (int *) shmat(complete_id, NULL, 0);
    if (complete == NULL) {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }
    memset(complete, 0, sizeof(complete));

    // Register the signal callback
    signal(SIGALRM,(void (*)(int))end_prog);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Setup the token
    token.complete = 0;
    token.origin = n_i;
    token.path_i = 0;
    for(i = 0; i < path_len; i++) {
        token.path[i] = strtol(argv[NI_IDX+n*2+2+i], NULL, 10);
    }

    // Set the last node in the path to this node
    token.path[i++] = n_i;

    // Finish intializing the token
    for(;i < MAX_PATHLEN; i++) {
        token.path[i] = -1;
    }

    for(i = 0; i < 10; i++)
    {
        token.sum[i] = 0;
    }

    while(1) {  // main accept() loop

        // Send the token if this is the first node in the loop
        if(token_sent == 0) {
            if(!fork()) { // child process

                // Wait for 10 seconds for all of the servers to come online
                printf("Waiting for 10s before initiating token\n");
                sleep(10);

                // Send the initial token
                printf("Sending initial token\n");
                client_send(&token, argv[ADDR_IDX+token.path[0]*2], 
                    argv[ADDR_IDX+token.path[0]*2+1]);
                exit(0);
            }
            token_sent = 1;
        }

        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        // printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener

            // Receive the token from the client
            if (recv(new_fd, &token, sizeof(token_t), 0) == -1)
                perror("recv");

            // printf("Received token from %d\n", token.origin);
            close(new_fd);

            // Increment the path and sum
            token.path_i++;
            token.sum[label]++;

            // ***** Parse the token
            if(token.complete == 1) {
                complete[token.origin] = 1;
                // printf("Received completion token\n");
            } else if(token.path[token.path_i] == -1) {
                // Last token in the path
                complete[n_i] = 1;

                // Send a completion token to the rest of the nodes
                token.complete = 1;
                token.origin = n_i;
                for(i = 0; i < n; i++) {
                    if(i == n_i) {
                        continue;
                    }
                    client_send(&token, argv[ADDR_IDX+i*2], argv[ADDR_IDX+i*2+1]);
                }

                // Print the label values
                printf("labels: ");
                for(i = 0; i < 10; i++)
                {
                    printf("%d ", token.sum[i]);
                }
                printf("\n");

                // printf("Last token in path\n");
            } else {
                // Send it to the next node
                token.origin = n_i;
                client_send(&token, argv[ADDR_IDX+token.path[token.path_i]*2], 
                    argv[ADDR_IDX+token.path[token.path_i]*2+1]);

                // printf("Token passed along\n");
            }

            // Test if we have received completion tokens from the rest of the nodes
            all_complete = 1;
            for(i = 0; i < n; i++) {
                if(!complete[i]) {
                    all_complete = 0;
                } 
            }

            if(all_complete) {

                // Send signal to parent that we're finished
                kill(getppid(), SIGALRM);
            }

            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}

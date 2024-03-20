#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()
#include <arpa/inet.h> // inet_addr()
#include "sistos.pb-c.h" // import the generated file from the .proto

#define BUFFER_SIZE 250
#define SA struct sockaddr

#define MAX_CLIENTS 3
int clients_connected = 0;

#define CONNECTION_REQUEST_BEFORE_REFUSAL 5

struct sockaddr_in servaddr, cli;

//Client Structure
typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int client_id;
    clock_t last_connection;
    int status_changed_last_connection;
    char status[32];
    char name[32];
} client_str;

// Function designed for chat between client and server.
//void func(int connfd)
//{
//    char buff[MAX];
//    int n;
//    // infinite loop for chat
//    for (;;) {
//        bzero(buff, MAX);
//
//        // read the message from client and copy it in buffer
//        read(connfd, buff, sizeof(buff));
//        // print buffer which contains the client contents
//        printf("From client: %s\t To client : ", buff);
//        bzero(buff, MAX);
//        n = 0;
//        // copy server message in the buffer
//        while ((buff[n++] = getchar()) != '\n')
//            ;
//
//        // and send that buffer to client
//        write(connfd, buff, sizeof(buff));
//
//        // if msg contains "Exit" then server exit and chat ended.
//        if (strncmp("exit", buff, 4) == 0) {
//            printf("Server Exit...\n");
//            break;
//        }
//    }
//}

/*
 * Create a socket and listen for incoming connections
 * Reference: https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
 */
void create_socket(int port){

}


int main(int argc, char *argv[]){
    if(argc < 2){
        printf("Please provide a port number");
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }

    int port = atoi(argv[1]);  // Convert the port number from string to integer

    // Create the socket for the server
    int sockfd;
    struct sockaddr_in servaddr, cli;

    // Socket Creation and Verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // Assign IP, PORT
    servaddr.sin_family = AF_INET;  // IPv4
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  // Accept any incoming IP
    servaddr.sin_port = htons(port);  // Convert the port number (CLI arg) to network byte order

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("Socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, CONNECTION_REQUEST_BEFORE_REFUSAL)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");

    // From now, the socket has been set up and is listening for incoming connections

    while(1){
        int connfd;
        struct sockaddr_in cli;
        int len2 = sizeof(cli);
        // Listen for incoming connections
        connfd = accept(sockfd, (SA*)&cli, &len2);

        if (connfd < 0) {
            printf("Server accept failed...\n");
            exit(0);
        }
        else{  // Checks for available slots
            if((clients_connected + 1) >= MAX_CLIENTS){
                printf("Server refused the client... (Server's full)\n");
                close(connfd);
                continue;
            }
            printf("Server accepted the client...\n");
        }

        // Declare a buffer to store the received data
        void *buf = malloc(BUFFER_SIZE);
        if (buf == NULL) {
            fprintf(stderr, "Error allocating memory for the receive buffer.\n");
            exit(1);
        }

// Receive the data from the client
        ssize_t len3 = recv(connfd, buf, BUFFER_SIZE, 0);
        if (len3 == -1) {
            perror("Receive failed");
            exit(EXIT_FAILURE);
        }

// Unpack the received data
        Chat__ClientPetition *clientPetition;
        clientPetition = chat__client_petition__unpack(NULL, len3, buf);
        if (clientPetition == NULL) {
            fprintf(stderr, "Error unpacking incoming message.\n");
            exit(1);
        }

// Print the unpacked data
        printf("Received message from client: %s\n", clientPetition->registration->username);
        printf("Received message from client: %s\n", clientPetition->registration->ip);

// Free the unpacked message
        chat__client_petition__free_unpacked(clientPetition, NULL);

// Free the buffer
        free(buf);



        // After chatting close the socket
        close(sockfd);
    }
}

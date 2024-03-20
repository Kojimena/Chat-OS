#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()
#include <arpa/inet.h> // inet_addr()
#include <pthread.h>


#include "sistos.pb-c.h" // import the generated file from the .proto

#define BUFFER_SIZE 250
#define USERNAME_SIZE 32
#define STATUS_SIZE 32
#define SA struct sockaddr

#define MAX_CLIENTS 3
int clients_connected = 0;

#define CONNECTION_REQUEST_BEFORE_REFUSAL 5

struct sockaddr_in servaddr, cli;

//Client Structure
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    pthread_t thread_id;
    int client_id;
    clock_t last_connection;
    char status[STATUS_SIZE];
    char username[USERNAME_SIZE];
} client_struct;

/*
 * Creates a new client_struct and returns a pointer to it
 */
client_struct *create_client_struct(struct sockaddr_in address, int sockfd, int client_id, clock_t last_connection, char *status, char *username){
    client_struct *client = (client_struct *)malloc(sizeof(client_struct));
    client->address = address;
    client->sockfd = sockfd;
    client->client_id = client_id;
    client->last_connection = last_connection;
    strcpy(client->status, status);
    strcpy(client->username, username);

    // Show the new client's information
    printf("Client %d connected\n", clients_connected);
    printf("Username: %s\n", client->username);
    printf("ID: %d\n", client->client_id);
    printf("IP: %s\n", inet_ntoa(address.sin_addr));
    printf("Port: %d\n", ntohs(address.sin_port));
    printf("Status: %s\n", client->status);
    printf("Last Connection: %ld\n", client->last_connection);

    return client;
}

void *handle_client(void *arg){
    client_struct *client = (client_struct *)arg;
    printf("Handling client %d\n", client->client_id);
    return NULL;
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

    int continue_running = 1;
    while(continue_running){
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

        // The user can now interact with the server

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

        // The server has received the data and unpacked it

        // Create a client_struct to store the client's information
        client_struct *client = create_client_struct(
            cli,
            connfd,
            clients_connected,
            clock(),
            "Online",
            clientPetition->registration->username
        );

        // Free the unpacked message
        chat__client_petition__free_unpacked(clientPetition, NULL);

        // Create a thread ot handle the client
        pthread_t client_thread;

        // Add the thread id to the client_struct
        client->thread_id = client_thread;


//        pthread_create(&client_thread, NULL, handle_client, (void *)client);



        // Free the buffer
        free(buf);
    }
    // Handle exit
    close(sockfd);
    return 0;
}

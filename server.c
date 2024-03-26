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
#define MAX_CLIENTS 100
#define CONNECTION_REQUEST_BEFORE_REFUSAL 5

struct sockaddr_in servaddr, cli;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


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

client_struct* clients[MAX_CLIENTS] = {NULL};
int clients_connected = 0;


void add_client(client_struct *client){
    pthread_mutex_lock(&clients_mutex); // Bloquea el mutex antes de acceder a la lista de clientes
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i] == NULL){
            clients[i] = client;
            clients_connected++;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex); // Libera el mutex después de modificar la lista de clientes
}


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

void broadcast(char *message, int current_client){
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i]){
            if (clients[i]->client_id != current_client) {
                //Send message using protobuf
				Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
				void *buf; // Buffer to store serialized data
				unsigned len;
				srv_res.option = 4;
				Chat__MessageCommunication msg = CHAT__MESSAGE_COMMUNICATION__INIT;
				msg.message = message;
				msg.recipient = "everyone";
				msg.sender = clients[current_client]->username;
				srv_res.messagecommunication = &msg;
				len = chat__server_response__get_packed_size(&srv_res);
				buf = malloc(len);
				chat__server_response__pack(&srv_res, buf);
				if (send(clients[i]->sockfd, buf, len, 0) < 0)
				{
					perror("Failed to send message to client");
                    break;
				}
				free(buf);
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg){
    client_struct *client = (client_struct *)arg;
    char buffer[BUFFER_SIZE];

    while(1) {
        bzero(buffer, BUFFER_SIZE);
        ssize_t message_len = recv(client->sockfd, buffer, BUFFER_SIZE, 0);

        // Verificar si el cliente se desconectó o si hubo un error en recv
        if (message_len <= 0) {
            if (message_len == 0) {
                printf("Client %d disconnected\n", client->client_id);
            } else {
                perror("recv failed");
            }
            close(client->sockfd);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] && clients[i]->client_id == client->client_id) {
                    clients[i] = NULL;
                    clients_connected--;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            free(client);
            return NULL;
        }

        printf("Received message length: %zd\n", message_len);

        printf("Buffer contents: ");
        for (int i = 0; i < message_len; ++i) {
            printf("%02x ", (unsigned char)buffer[i]);
        }
        printf("\n");

        // Unpack the received message
        Chat__ClientPetition *clientPetition;
        clientPetition = chat__client_petition__unpack(NULL, message_len, buffer);

        if (clientPetition == NULL) {
            fprintf(stderr, "Error unpacking incoming message\n");
            exit(1);
        }

        // Show the received message
        printf("Received message from client %d\n", client->client_id);

        // Broadcast the message to all clients
        broadcast(buffer, client->client_id);

        // Free the unpacked message
        chat__client_petition__free_unpacked(clientPetition, NULL);

    }
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

        client_struct *client = create_client_struct(cli, connfd, clients_connected, clock(), "Online", clientPetition->registration->username);

        // Añadir el cliente a la lista de clientes
        add_client(client);

        // Crear un hilo para manejar al cliente
        if(pthread_create(&client->thread_id, NULL, handle_client, (void*) client) != 0){
            perror("Failed to create thread");
        }

        


        // Free the unpacked message
        chat__client_petition__free_unpacked(clientPetition, NULL);

        // Free the buffer
        free(buf);
    }
    // Handle exit
    close(sockfd);
    return 0;
}

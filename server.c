#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "proto.h"
#include "server.h"

#include "sistos.pb-c.h" // import the generated file from the .proto

// Global variables
int server_sockfd = 0, client_sockfd = 0;
ClientList *root, *now;

/**
 * Signal handler for SIGINT
 * Handles the exit of the server
 * @param sig Signal number
 */
void catch_exit(int sig) {
    ClientList *tmp;
    while (root != NULL) {
        close(root->data);  // Closes all the sockets including the server_sockfd
        printf("\nSocketfd closed: %d\n", root->data);
        tmp = root;
        root = root->link;
        free(tmp);  // Removes the node
    }
    printf("Bye\n");
    exit(EXIT_SUCCESS);
}

/**
 * Sends a message to all clients except the one that sent the message
 * @param np  The client that sent the message
 * @param buffer  The message to send (Protocol expected)
 * @param len  The length of the message
 */
void send_to_all_clients(ClientList *np, void *buffer, size_t len) {
    ClientList *tmp = root->link;
    while (tmp != NULL) {
        if (np->data != tmp->data) { // All Clients except itself
            printf("Sent to sockfd %d\n", tmp->data);
            send(tmp->data, buffer, len, 0);
        }
        tmp = tmp->link;
    }
}

/**
 * Sends a protocol message that represents the list of connected users
 * @param np The client that requested the list
 */
void send_users_list(ClientList *np) {
    printf("Sending user list to %s\n", np->name);

    Chat__UserInfo **users = malloc(sizeof(Chat__UserInfo *) * MAX_CLIENTS);  // Array of users
    ClientList *tmp = root->link;
    int j = 0;

    // Iterate over the linked list of clients
    while (tmp != NULL) {
        Chat__UserInfo *user = malloc(sizeof(Chat__UserInfo));
        chat__user_info__init(user);
        user->username = tmp->name;
        user->status = tmp->status;
        users[j] = user;
        j++;
        tmp = tmp->link;
    }

    // Create the protocol message
    Chat__ConnectedUsersResponse connected_users = CHAT__CONNECTED_USERS_RESPONSE__INIT;
    connected_users.n_connectedusers = j;
    connected_users.connectedusers = users;

    // Create the server response
    Chat__ServerResponse srv_res = CHAT__SERVER_RESPONSE__INIT;
    srv_res.option = 2;
    srv_res.connectedusers = &connected_users;

    void *buf;
    unsigned len;
    len = chat__server_response__get_packed_size(&srv_res);
    buf = malloc(len);
    chat__server_response__pack(&srv_res, buf);

    // Send the server response
    send(np->data, buf, len, 0);

    // Free the buffer
    free(buf);
    free(users);
}


/**
 * Handles the client on the server side, via threads
 * @param p_client
 */
void client_handler(void *p_client) {
    char username[USERNAME_SIZE] = {};
    char recv_buffer[LENGTH_MSG] = {};
    ClientList *np = (ClientList *) p_client;

    // Receive the username
    recv(np->data, username, USERNAME_SIZE, 0);
    // Set the username
    strncpy(np->name, username, USERNAME_SIZE);
    
    if (np->status != NULL) {
        strcpy(np->status, "activo");
    }

    // Petition loop
    while (1) {
        // Wait for a petition
        int receive = recv(np->data, recv_buffer, LENGTH_MSG, 0);
        if (receive < 0) {   // Error receiving message
            printf("ERROR RECEIVING PETITION (loop)\n");
            break;
        } else if (receive == 0) {  // Client disconnected
            break;
        }

        // Unpack the received petition
        Chat__ClientPetition *petition = chat__client_petition__unpack(NULL, receive, recv_buffer);
        if (petition == NULL) {
            printf("Error unpacking message.\n");
            break;
        }

        switch (petition->option) {
            case 1:
                break;
            case 2:
                //Getr user list, send the root of the list
                send_users_list(np);
                break;
            case 3: // Change user status
                                
                break;
            case 4: // Chatroom message
                int leave_flag = 0;
                while (!leave_flag) {  // Chat loop
                    // Check if the message is 'exit'
                    if (strcmp(petition->messagecommunication->message, "exit") == 0) {
                        leave_flag = 1;
                    }

                    // Create a server response
                    Chat__MessageCommunication msgCommSend = CHAT__MESSAGE_COMMUNICATION__INIT;
                    msgCommSend.sender = petition->messagecommunication->sender;
                    msgCommSend.recipient = petition->messagecommunication->recipient;
                    msgCommSend.message = petition->messagecommunication->message;

                    Chat__ServerResponse serverResponse = CHAT__SERVER_RESPONSE__INIT;
                    serverResponse.option = 4;
                    serverResponse.messagecommunication = &msgCommSend;

                    size_t len = chat__server_response__get_packed_size(&serverResponse);
                    void *buffer = malloc(len);
                    if (buffer == NULL) {
                        printf("Error assigning memory\n");
                        break;
                    }
                    chat__server_response__pack(&serverResponse, buffer);

                    printf("%s sent: %s\n", petition->messagecommunication->sender,
                           petition->messagecommunication->message);

                    // Send the server response
                    send_to_all_clients(np, buffer, len);

                    // Free the buffer
                    free(buffer);

                    // Receive the next petition
                    receive = recv(np->data, recv_buffer, LENGTH_MSG, 0);
                    if (receive < 0) {
                        printf("ERROR RECEIVING PETITION (next petition)\n");
                        break;
                    }

                    // Unpack the received petition
                    petition = chat__client_petition__unpack(NULL, receive, recv_buffer);
                    if (petition == NULL) {
                        printf("Error unpacking message.\n");
                        break;
                    }

                    // Loop with the new petition
                }
                break;  // End of case 4
                default:
                    break;
        }
    }
    // Remove Node
    close(np->data);  // Close the socket

    if (np == now) { // remove an edge node
        now = np->prev;
        now->link = NULL;
    } else { // remove a middle node
        np->prev->link = np->link;
        np->link->prev = np->prev;
    }
}

int main(int argc, char *argv[]) {
    // Create the server with console args
    if (argc < 2) {
        printf("Please provide a port number\n");
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }
    int port = atoi(argv[1]);  // Convert the port number from string to integer

    signal(SIGINT, catch_exit);

    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1) {
        printf("Fail to create a socket.\n");
        exit(EXIT_FAILURE);
    } else printf("Socket created!\n");

    // Socket information
    struct sockaddr_in server_info, client_info;

    int s_addrlen = sizeof(server_info);    // Server address length
    int c_addrlen = sizeof(client_info);    // Client address length

    memset(&server_info, 0, s_addrlen); // Clear the server info
    memset(&client_info, 0, c_addrlen); // Clear the client info

    server_info.sin_family = PF_INET;           // Set the server family
    server_info.sin_addr.s_addr = INADDR_ANY;   // Set the server address
    server_info.sin_port = htons(port); // Set the server port (from the console args)

    // Bind and Listen
    if (bind(server_sockfd, (struct sockaddr *) &server_info, s_addrlen) == -1) {
        printf("Fail to bind socket.\n");
        exit(EXIT_FAILURE);
    } else printf("Socket bound!\n");

    if (listen(server_sockfd, MAX_CLIENTS) == -1) {
        printf("Fail to listen on socket.");
        exit(EXIT_FAILURE);
    } else printf("Listening on socket!\n");

    // Set the server IP
    getsockname(server_sockfd, (struct sockaddr *) &server_info, (socklen_t * ) & s_addrlen);
    printf("Server started on: %s:%d\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));

    // Initial linked list for clients
    root = newNode(server_sockfd, inet_ntoa(server_info.sin_addr));
    now = root;

    while (1) {
        client_sockfd = accept(server_sockfd, (struct sockaddr *) &client_info, (socklen_t * ) & c_addrlen);

        // Print Client IP on the server
        getpeername(client_sockfd, (struct sockaddr *) &client_info, (socklen_t * ) & c_addrlen);
        printf("%s:%d | Client accepted.\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

        // Append linked list for clients
        ClientList *c = newNode(client_sockfd, inet_ntoa(client_info.sin_addr));
        c->prev = now;
        now->link = c;
        now = c;

        pthread_t id;
        if (pthread_create(&id, NULL, (void *) client_handler, (void *) c) != 0) {
            perror("Create pthread error!\n");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
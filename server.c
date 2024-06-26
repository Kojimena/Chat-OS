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

pthread_mutex_t mutex_chatroom = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_privateChat = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_status_change = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_user_info = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_listInfo = PTHREAD_MUTEX_INITIALIZER;

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
    srv_res.code = 200;

    void *buf;
    unsigned len;
    len = chat__server_response__get_packed_size(&srv_res);
    buf = malloc(len);
    chat__server_response__pack(&srv_res, buf);

    // Send the server response
    if (send(np->data, buf, len, 0) == -1) {
        printf("Error sending user list to %s\n", np->name);
    } else {
        printf("Sent user list to %s\n", np->name);
    }

    // Free the buffer
    free(buf);

    // Free each Chat__UserInfo object
    for (int i = 0; i < j; i++) {
        free(users[i]);
    }

    // Free the users array
    free(users);

}


/**
 * Handles the client on the server side, via threads
 * @param p_client
 */
void client_handler(void *p_client) {
    // Mutex init
    pthread_mutex_init(
            &mutex_chatroom, NULL);
    pthread_mutex_init(&mutex_privateChat, NULL);
    pthread_mutex_init(&mutex_status_change, NULL);
    pthread_mutex_init(&mutex_user_info, NULL);
    pthread_mutex_init(&mutex_listInfo, NULL);


    char username[USERNAME_SIZE] = {};
    char recv_buffer[LENGTH_MSG] = {};
    ClientList *np = (ClientList *) p_client;

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
            printf("Client disconnected\n");
            break;
        }

        // Unpack the received petition
        Chat__ClientPetition *petition = chat__client_petition__unpack(NULL, receive, recv_buffer);
        if (petition == NULL) {
            printf("Error unpacking message.\n");
            break;
        }

        // Update the last connection time
        np->last_connection = clock();
        if (strcmp(np->status, "inactivo") == 0) {
            strcpy(np->status, "activo");
        }
        printf("Petition option: %d\n", petition->option);

        switch (petition->option) {
            case 1: // New User ?
                printf("Attempting to create new user: %s\n", petition->registration->username);
                // Set the username
                strncpy(np->name, petition->registration->username, USERNAME_SIZE);
                strncpy(np->status, "activo", strlen("activo") + 1);


                // Send a server response
                Chat__ServerResponse response = CHAT__SERVER_RESPONSE__INIT;
                response.option = 1;
                response.code = 200;

                // Check if the username is already taken
                // The user_register should be 1 if not repeated
                int users_registered = 0;
                ClientList *new_tmp = root->link;
                while (new_tmp != NULL) {
                    if (strcmp(new_tmp->name, np->name) == 0) {
                        users_registered += 1;
                    }
                    new_tmp = new_tmp->link;
                }

                if (users_registered > 1) {
                    response.code = 500;
                }

                // Send the response
                size_t new_len = chat__server_response__get_packed_size(&response);
                void *new_buffer = malloc(new_len);
                if (new_buffer == NULL) {
                    printf("Error assigning memory\n");
                    break;
                }

                chat__server_response__pack(&response, new_buffer);
                send(np->data, new_buffer, new_len, 0);

                // Free the buffer
                free(new_buffer);

                if(response.code == 200) {
                    printf("User %s created\n", np->name);
                    printf("%s", np->status);
                } else {
                    printf("User %s already exists\n", np->name);
                }

                break;
            case 2: // Get users list
                pthread_mutex_lock(&mutex_listInfo);
                send_users_list(np);
                pthread_mutex_unlock(&mutex_listInfo);
                break;
            case 3: // Change user status
                pthread_mutex_lock(&mutex_status_change);
                strcpy(np->status, petition->change->status);

                // Create a server response
                Chat__ChangeStatus changeStatusResponse = CHAT__CHANGE_STATUS__INIT;
                changeStatusResponse.username = np->name;
                changeStatusResponse.status = np->status;

                Chat__ServerResponse responseServerResponse = CHAT__SERVER_RESPONSE__INIT;
                responseServerResponse.option = 3;
                responseServerResponse.change = &changeStatusResponse;
                responseServerResponse.code = 200;

                size_t len = chat__server_response__get_packed_size(&responseServerResponse);
                void *buffer = malloc(len);
                if (buffer == NULL) {
                    printf("Error assigning memory\n");
                    break;
                }
                chat__server_response__pack(&responseServerResponse, buffer);

                // Send the server response to the client
                send(np->data, buffer, len, 0);

                // Free the buffer
                free(buffer);

                printf("%s changed status to %s\n", np->name, np->status);
                pthread_mutex_unlock(&mutex_status_change);
                break;
            case 4: // Messaging (chatroom or private chat)
                int leave_flag = 0;
                while (!leave_flag) {  // Chat loop
                    //update the last connection time
                    if (strcmp(petition->messagecommunication->sender, np->name) == 0) {
                        np->last_connection = clock();
                        if (strcmp(np->status, "inactivo") == 0) {
                            strcpy(np->status, "activo");
                        }

                    }

                    // Check if the message is 'exit'
                    if (strcmp(petition->messagecommunication->message, "exit") == 0) {
                        leave_flag = 1;
                        break;
                    }

                    if (strcmp(petition->messagecommunication->recipient, "everyone") == 0) {
                        // Create a server response
                        Chat__MessageCommunication msgCommSend = CHAT__MESSAGE_COMMUNICATION__INIT;
                        msgCommSend.sender = petition->messagecommunication->sender;
                        msgCommSend.recipient = petition->messagecommunication->recipient;
                        msgCommSend.message = petition->messagecommunication->message;

                        Chat__ServerResponse serverResponse = CHAT__SERVER_RESPONSE__INIT;
                        serverResponse.option = 4;
                        serverResponse.messagecommunication = &msgCommSend;
                        serverResponse.code = 200;

                        size_t len = chat__server_response__get_packed_size(&serverResponse);
                        void *buffer = malloc(len);
                        if (buffer == NULL) {
                            printf("Error assigning memory\n");
                            break;
                        }
                        chat__server_response__pack(&serverResponse, buffer);

                        // Send the server response
                        send_to_all_clients(np, buffer, len);

                        // Free the buffer
                        free(buffer);

                        printf("%s sent in chatroom: %s\n", petition->messagecommunication->sender,
                               petition->messagecommunication->message);

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
                    } else { // Private chat
                        // Find the recipient in the linked list
                        ClientList *tmp = root->link;
                        while (tmp != NULL) {
                            if (strcmp(tmp->name, petition->messagecommunication->recipient) == 0) {
                                break;
                            }
                            tmp = tmp->link;
                        }

                        // Create a server response
                        Chat__MessageCommunication msgCommSend = CHAT__MESSAGE_COMMUNICATION__INIT;
                        msgCommSend.sender = petition->messagecommunication->sender;
                        msgCommSend.recipient = petition->messagecommunication->recipient;
                        msgCommSend.message = petition->messagecommunication->message;

                        Chat__ServerResponse serverResponse = CHAT__SERVER_RESPONSE__INIT;
                        serverResponse.option = 4;
                        serverResponse.messagecommunication = &msgCommSend;
                        serverResponse.code = 200;

                        size_t len = chat__server_response__get_packed_size(&serverResponse);
                        void *buffer = malloc(len);
                        if (buffer == NULL) {
                            printf("Error assigning memory\n");
                            break;
                        }
                        chat__server_response__pack(&serverResponse, buffer);

                        // Send the server response to the recipient
                        send(tmp->data, buffer, len, 0);

                        // Free the buffer
                        free(buffer);

                        printf("%s sent to %s: %s\n", petition->messagecommunication->sender,
                               petition->messagecommunication->recipient, petition->messagecommunication->message);

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
                    }
                    // Free buffer          
                    free(buffer);

                    // Loop with the new petition
                }

                break;  // End of case 4
            case 5: // Get user info
                pthread_mutex_lock(&mutex_user_info);
                printf("%s requested info of %s\n", np->name, petition->users->user);

                // Find in the linked list the user with the requested username
                ClientList *tmp = root->link;
                while (tmp != NULL) {
                    if (strcmp(tmp->name, petition->users->user) == 0) {   // Found the user
                        break;
                    }
                    tmp = tmp->link;
                }

                // Create a server response
                Chat__UserInfo userInfoResponse = CHAT__USER_INFO__INIT;
                Chat__ServerResponse serverInfoResponse = CHAT__SERVER_RESPONSE__INIT;

                userInfoResponse.username = tmp->name;
                userInfoResponse.status = tmp->status;
                userInfoResponse.ip = tmp->ip;

                // If not found, send an error message
                if (tmp == NULL) {
                    userInfoResponse.username = "Is not connected";
                    userInfoResponse.status = "Unreachable";

                    serverInfoResponse.code = 500;
                }

                serverInfoResponse.option = 5;
                serverInfoResponse.code = 200;
                serverInfoResponse.userinforesponse = &userInfoResponse;

                size_t info_len = chat__server_response__get_packed_size(&serverInfoResponse);
                void *info_buffer = malloc(info_len);
                if (info_buffer == NULL) {
                    printf("Error assigning memory\n");
                    break;
                }
                chat__server_response__pack(&serverInfoResponse, info_buffer);

                // Send the server response to the client
                send(np->data, info_buffer, info_len, 0);

                // Free the buffer
                free(info_buffer);
                pthread_mutex_unlock(&mutex_user_info);
                break;
            default:
                break;
        }
        // Free the petition
        chat__client_petition__free_unpacked(petition, NULL);
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


/**
 * Handles the inactivity of the client, after 60 seconds of inactivity, the client's status is set to 'inactivo'
 * @param p_client
 */
void client_inactivity(void *p_client) {
    ClientList *np = (ClientList *) p_client;
    while (1) {
        if ((strcmp(np->status, "inactivo") != 0) &&
            ((clock() - np->last_connection) / CLOCKS_PER_SEC > INACTIVITY_TIME)) {
            pthread_mutex_lock(&mutex_status_change);

            strcpy(np->status, "inactivo");

            printf("%s changed status to %s due to inactivity.\n", np->name, np->status);
            pthread_mutex_unlock(&mutex_status_change);
        }
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

        // Set the node ip
        strncpy(c->ip, inet_ntoa(client_info.sin_addr), 16);

        // Set the status of the client
        strncpy(c->status, "activo", strlen("activo") + 1);

        // Insert in the linked list
        c->prev = now;
        now->link = c;
        now = c;

        pthread_t id;
        if (pthread_create(&id, NULL, (void *) client_handler, (void *) c) != 0) {
            perror("Create pthread error!\n");
            exit(EXIT_FAILURE);
        }

        pthread_t inactivity_id;
        if (pthread_create(&inactivity_id, NULL, (void *) client_inactivity, (void *) c) != 0) {
            perror("Create pthread error!\n");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
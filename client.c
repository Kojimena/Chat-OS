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
#include "string.h"

#include "sistos.pb-c.h" // import the generated file from the .proto

// Global variables
int exit_flag = 0; // Flag to signal threads to exit
int sockfd = 0;  // Socket file descriptor
char username[USERNAME_SIZE] = {};  // Username

/**
 * Signal handler for SIGINT
 * @param sig Signal number
 */
void catch_exit(int sig) {
    exit_flag = 1;
}

/**
 * Handles the reception of messages from the server.
 * Expects a message in the protocol format. chat__server_response
 */
void recv_msg_handler() {
    char recv_buffer[LENGTH_SEND] = {}; // Buffer to receive the message (protocol expected)

    // Detach the current thread
    pthread_detach(pthread_self());
    while (!exit_flag) {
        int receive = recv(sockfd, recv_buffer, LENGTH_SEND, 0);  // Receive the message
        if (receive == -1) {
            printf("Error receiving the message\n");
            break;
        } else if (receive == 0) {
            break;
        }
        // Unpack the message
        Chat__ServerResponse *serverResponse = chat__server_response__unpack(NULL, receive, recv_buffer);
        if (serverResponse == NULL) {
            printf("Error unpacking the message\n");
            continue;
        }

        // Print the message
        if (serverResponse->code == 200) {
            // If the message is public, print on default color
            if (strcmp(serverResponse->messagecommunication->recipient, "everyone") == 0) {
                printf("\r%s | %s\n", serverResponse->messagecommunication->sender,
                       serverResponse->messagecommunication->message);
            } else {
                // If the message is private, print on blue color
                printf("\033[0;34m\r%s | %s\n\033[0m", serverResponse->messagecommunication->sender,
                       serverResponse->messagecommunication->message);
            }
        } else {
            printf("Error code: %d\n", serverResponse->code);
        }

        // Free the memory
        chat__server_response__free_unpacked(serverResponse, NULL);


        // Print esthetics
        str_overwrite_stdout();
    }
    pthread_exit(0);
}


void change_status() {
    int option = 1;
    char *status;
    printf("1. Activo\n");
    printf("2. Ocupado\n");
    printf("3. Inactivo\n");
    scanf("%d", &option);

    switch (option) {
        case 1:
            status = "activo";
            break;
        case 2:
            status = "ocupado";
            break;
        case 3:
            status = "inactivo";
            break;
        default:
            printf("Invalid option\n");
            break;
    }

    Chat__ChangeStatus change_status_petition = CHAT__CHANGE_STATUS__INIT;
    change_status_petition.username = username;
    change_status_petition.status = status;


    Chat__ClientPetition client_petition = CHAT__CLIENT_PETITION__INIT;
    client_petition.option = 3;
    client_petition.change = &change_status_petition;

    size_t len = chat__client_petition__get_packed_size(&client_petition);
    void *buffer = malloc(len);
    if (buffer == NULL) {
        printf("Error assigning memory\n");
        return;
    }
    chat__client_petition__pack(&client_petition, buffer);

    if (send(sockfd, buffer, len, 0) < 0) {
        printf("Error sending status change petition to the server\n");
        return;
    }

    free(buffer);

    // Wait for the server response of the status change
    char recvBuff[BUFF_SIZE];
    int n = recv(sockfd, recvBuff, BUFF_SIZE, 0);
    if (n == -1) {
        perror("Recv failed");
        exit(EXIT_FAILURE);
    }

    Chat__ServerResponse *srv_res = chat__server_response__unpack(NULL, n, recvBuff);
    if (srv_res == NULL) {
        fprintf(stderr, "Error unpacking incoming message\n");
        exit(1);
    }

    if (srv_res->option == 3 && srv_res->code == 200) {
        printf("Status changed to: %s\n", srv_res->change->status);
    }

    chat__server_response__free_unpacked(srv_res, NULL);
    return;
}


void receive_users_list(int sockfd) {
    char recvBuff[BUFF_SIZE];
    int n = recv(sockfd, recvBuff, BUFF_SIZE, 0);
    if (n == -1) {
        perror("Recv failed");
        exit(EXIT_FAILURE);
    }

    Chat__ServerResponse *srv_res = chat__server_response__unpack(NULL, n, recvBuff);
    if (srv_res == NULL) {
        fprintf(stderr, "Error unpacking incoming message\n");
        exit(1);
    }

    if (srv_res->option == 2 && srv_res->code == 200) {
        printf("Connected users:\n");
        for (int i = 0; i < srv_res->connectedusers->n_connectedusers; i++) {
            printf("- %s (%s)\n", srv_res->connectedusers->connectedusers[i]->username,
                   srv_res->connectedusers->connectedusers[i]->status);
        }
    }
    chat__server_response__free_unpacked(srv_res, NULL);
}

void list_connected_users(int sockfd) {
    Chat__ClientPetition cli_ptn = CHAT__CLIENT_PETITION__INIT;
    Chat__UserRequest usr_rqst = CHAT__USER_REQUEST__INIT;

    void *buf;
    unsigned len;

    usr_rqst.user = "everyone";

    cli_ptn.option = 2;
    cli_ptn.users = &usr_rqst;

    len = chat__client_petition__get_packed_size(&cli_ptn);
    buf = malloc(len);

    chat__client_petition__pack(&cli_ptn, buf);

    if (send(sockfd, buf, len, 0) == -1) {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }

    free(buf);

    receive_users_list(sockfd);
}

void user_info(int sockfd) {
    char user_to_find[USERNAME_SIZE];
    printf("Enter the username to get the information: ");
    scanf("%s", user_to_find);

    Chat__UserRequest user_request = CHAT__USER_REQUEST__INIT;
    user_request.user = user_to_find;

    Chat__ClientPetition client_petition = CHAT__CLIENT_PETITION__INIT;
    client_petition.option = 5;
    client_petition.users = &user_request;

    size_t len = chat__client_petition__get_packed_size(&client_petition);
    void *buffer = malloc(len);
    if (buffer == NULL) {
        printf("Error assigning memory\n");
        return;
    }

    chat__client_petition__pack(&client_petition, buffer);

    if (send(sockfd, buffer, len, 0) < 0) {
        printf("Error sending user info petition to the server\n");
        return;
    }

    free(buffer);

    // Wait for the server response of the user info
    char recvBuff[BUFF_SIZE];
    int n = recv(sockfd, recvBuff, BUFF_SIZE, 0);
    if (n == -1) {
        perror("Recv failed");
        exit(EXIT_FAILURE);
    }

    Chat__ServerResponse *srv_res = chat__server_response__unpack(NULL, n, recvBuff);
    if (srv_res == NULL) {
        fprintf(stderr, "Error unpacking incoming message\n");
        exit(1);
    }

    if (srv_res->option == 5 && srv_res->code == 200) {
        printf("User info:\n");
        printf("- Username: %s\n", srv_res->userinforesponse->username);
        printf("- Status: %s\n", srv_res->userinforesponse->status);
        printf("- IP: %s\n", srv_res->userinforesponse->ip);
    }

    chat__server_response__free_unpacked(srv_res, NULL);
    return;
}

/**
 * Handles the sending of messages to the server. Sends a message in the protocol format. chat__client_petition
 */
void send_msg_handler(char *user_to_write) {
    char message[LENGTH_MSG] = {};  // Buffer to store the message

    // Detach the current thread
    pthread_detach(pthread_self());

    while (!exit_flag) {
        str_overwrite_stdout();
        while (fgets(message, LENGTH_MSG, stdin) != NULL) { // Read the message
            str_trim_lf(message, LENGTH_MSG);  // Remove \n
            if (strlen(message) == 0) { // If the message is empty
                str_overwrite_stdout();
            } else {    // Proceed to send the message
                break;
            }
        }

        // Pack the message
        Chat__MessageCommunication msgComm = CHAT__MESSAGE_COMMUNICATION__INIT;
        msgComm.message = message;
        msgComm.recipient = user_to_write;
        msgComm.sender = username;

        Chat__ClientPetition petition = CHAT__CLIENT_PETITION__INIT;
        petition.option = 4;
        petition.messagecommunication = &msgComm;

        size_t len = chat__client_petition__get_packed_size(&petition);
        void *buffer = malloc(len);
        if (buffer == NULL) {
            printf("Error assigning memory\n");
            break;
        }
        chat__client_petition__pack(&petition, buffer);

        // Send the message. If it wants to leave the chatroom, notify the server by sending "exit"
        if (send(sockfd, buffer, len, 0) < 0) {
            printf("Error sending the message to the server\n");
            break;
        }

        // If the message is "exit", leave the chatroom client side
        if (strcmp(message, "exit") == 0) {
            printf("Leaving chatroom\n");
            exit_flag = 1;  // Set the flag to signal threads to exit
            pthread_exit(0);  // Terminate the current thread
        }

        // Free the buffer
        free(buffer);
        if (exit_flag) {
            break;
        }
    }
    pthread_exit(0);
}


int main(int argc, char *argv[]) {
    char *username_arg = argv[1];
    char *server_ip = argv[2];
    int port = atoi(argv[3]);

    // Trim the username
    str_trim_lf(username_arg, strlen(username_arg));
    if (strlen(username_arg) > USERNAME_SIZE || strlen(username_arg) < 1) {
        printf("Invalid username\n");
        return EXIT_FAILURE;
    }
    // Set username
    strcpy(username, username_arg);

    signal(SIGINT, catch_exit);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Fail to create a socket.");
        exit(EXIT_FAILURE);
    } else printf("Socket created!\n");

    // Socket information
    struct sockaddr_in server_info, client_info;

    int s_addrlen = sizeof(server_info);    // Server address length
    int c_addrlen = sizeof(client_info);    // Client address length

    memset(&server_info, 0, s_addrlen); // Clear the server_info
    memset(&client_info, 0, c_addrlen); // Clear the client_info

    server_info.sin_family = AF_INET;                           // Set the server family to IPv4
    server_info.sin_addr.s_addr = inet_addr(server_ip);     // Set the server IP (console arg)
    server_info.sin_port = htons(port);                 // Set the server port (console arg)

    // Connect to Server
    int err = connect(sockfd, (struct sockaddr *) &server_info, s_addrlen);
    if (err == -1) {
        printf("Error. Can't connect to the server =(\n");
        exit(EXIT_FAILURE);
    }

    // Show the connection info
    getsockname(sockfd, (struct sockaddr *) &client_info, (socklen_t * ) & c_addrlen);
    getpeername(sockfd, (struct sockaddr *) &server_info, (socklen_t * ) & s_addrlen);

    printf("SUCCESS: Connected to %s:%d !\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));
    printf("You are: %s:%d\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

    // Create a User Register petition
    Chat__UserRegistration user_register = CHAT__USER_REGISTRATION__INIT;
    user_register.username = username;
    user_register.ip = inet_ntoa(client_info.sin_addr);

    Chat__ClientPetition new_client_petition = CHAT__CLIENT_PETITION__INIT;
    new_client_petition.option = 1;
    new_client_petition.registration = &user_register;

    size_t new_len = chat__client_petition__get_packed_size(&new_client_petition);

    void *new_buffer = malloc(new_len);
    if (new_buffer == NULL) {
        printf("Error assigning memory\n");
        return EXIT_FAILURE;
    }
    chat__client_petition__pack(&new_client_petition, new_buffer);

    // Send the petition
    if (send(sockfd, new_buffer, new_len, 0) < 0) {
        printf("Error sending the petition to the server\n");
        return EXIT_FAILURE;
    }

    // Receive the server response
    char new_recvBuff[BUFF_SIZE];
    int new_n = recv(sockfd, new_recvBuff, BUFF_SIZE, 0);
    if (new_n == -1) {
        perror("Recv failed");
        exit(EXIT_FAILURE);
    }


    Chat__ServerResponse *new_srv_res = chat__server_response__unpack(NULL, new_n, new_recvBuff);
    if (new_srv_res == NULL) {
        fprintf(stderr, "Error unpacking incoming message\n");
        exit(1);
    }

    if (new_srv_res->code != 200) {
        printf("Error connecting to the server. Username %s already exists.\n", username);
        exit(EXIT_FAILURE);
    }

    // Show menu
    while (1) {
        char input[100];
        int option = 0;

        printf("1. Join Chatroom\n");
        printf("2. Private Chat\n");
        printf("3. Change Status\n");
        printf("4. List Connected Users\n");
        printf("5. Show User Info\n");
        printf("6. Help\n");
        printf("7. Exit\n");

        // Get user input
        fgets(input, sizeof(input), stdin);

        // Try to parse the input as an integer
        if (sscanf(input, "%d", &option) != 1) {
            // If the parsing fails, treat it as a default case
            option = -1;
        }

        switch (option) {
            case 1: // Join Chatroom
                // Reset the exit_flag
                exit_flag = 0;

                // Pack the message
                Chat__MessageCommunication msgComm = CHAT__MESSAGE_COMMUNICATION__INIT;
                char message[LENGTH_MSG];
                snprintf(message, LENGTH_MSG, "%s has joined the chatroom", username);
                msgComm.message = message;
                msgComm.recipient = "everyone";
                msgComm.sender = "Server";

                Chat__ClientPetition petition = CHAT__CLIENT_PETITION__INIT;
                petition.option = 4;
                petition.messagecommunication = &msgComm;

                size_t len = chat__client_petition__get_packed_size(&petition);
                void *buffer = malloc(len);
                if (buffer == NULL) {
                    printf("Error assigning memory\n");
                    break;
                }
                chat__client_petition__pack(&petition, buffer);

                // Send the message. If it wants to leave the chatroom, notify the server by sending "exit"
                if (send(sockfd, buffer, len, 0) < 0) {
                    printf("Error sending the message to the server\n");
                    break;
                }

                // Create threads for sending and receiving messages
                pthread_t send_msg_thread, recv_msg_thread;
                if (pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, "everyone") != 0 ||
                    pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0) {
                    printf("Error creating threads.\n");
                    exit(EXIT_FAILURE);
                }

                while (!exit_flag) {
                    sleep(1);
                }

                pthread_cancel(send_msg_thread);
                pthread_cancel(recv_msg_thread);

                // Free the buffer
                free(buffer);

                break;
            case 2: // Private Chat
                // Get the username to chat with
                char user_to_write[USERNAME_SIZE];
                printf("Enter the username to chat with: ");
                scanf("%s", user_to_write);

                // Reset the exit_flag
                exit_flag = 0;

                // Pack the message
                Chat__MessageCommunication msgComm_dm = CHAT__MESSAGE_COMMUNICATION__INIT;
                char message_dm[LENGTH_MSG];
                snprintf(message_dm, LENGTH_MSG, "%s has joined the private chat", username);
                msgComm_dm.message = message_dm;
                msgComm_dm.recipient = user_to_write;
                msgComm_dm.sender = "Server";

                Chat__ClientPetition petition_dm = CHAT__CLIENT_PETITION__INIT;
                petition_dm.option = 4;
                petition_dm.messagecommunication = &msgComm_dm;

                size_t len_dm = chat__client_petition__get_packed_size(&petition_dm);
                void *buffer_dm = malloc(len_dm);
                if (buffer_dm == NULL) {
                    printf("Error assigning memory\n");
                    break;
                }
                chat__client_petition__pack(&petition_dm, buffer_dm);

                // Send the message. If it wants to leave the chatroom, notify the server by sending "exit"
                if (send(sockfd, buffer_dm, len_dm, 0) < 0) {
                    printf("Error sending the message to the server\n");
                    break;
                }

                // Create threads for sending and receiving messages
                pthread_t send_msg_thread_dm, recv_msg_thread_dm;
                if (pthread_create(&send_msg_thread_dm, NULL, (void *) send_msg_handler, user_to_write) != 0 ||
                    pthread_create(&recv_msg_thread_dm, NULL, (void *) recv_msg_handler, NULL) != 0) {
                    printf("Error creating threads.\n");
                    exit(EXIT_FAILURE);
                }

                while (!exit_flag) {
                    sleep(1);
                }

                pthread_cancel(send_msg_thread_dm);
                pthread_cancel(recv_msg_thread_dm);

                // Free the buffer
                free(buffer_dm);

                break;
            case 3: // Change Status
                change_status();
                // Clean the las '\n'
                getchar();
                break;
            case 4: // List Connected Users
                list_connected_users(sockfd);
                break;
            case 5:
                // Show User Info
                user_info(sockfd);
                // Clean the las '\n'
                getchar();
                break;
            case 6:
                // Help
                printf("\n=============================================================================\n");
                printf("Este chat permite a los usuarios conectarse a un servidor de chat y enviar mensajes a otros usuarios.\n");
                printf("Las opciones disponibles son:\n");
                printf("1. Chat with all users: Permite enviar mensajes a todos los usuarios conectados al servidor.\n");
                printf("-\t Para salir del chat, escriba 'exit'.\n");
                printf("2. Private Chat: Permite enviar mensajes privados a un usuario específico.\n");
                printf("-\t Para salir del chat, escriba 'exit'.\n");
                printf("3. Change Status: Permite cambiar el estado del usuario (activo, inactivo, ocupado).\n");
                printf("4. List Connected Users: Muestra la lista de usuarios conectados al servidor.\n");
                printf("5. Show User Info: Muestra la información de un usuario específico.\n");
                printf("6. Help: Muestra la lista de opciones disponibles.\n");
                printf("7. Exit: Sale del chat.\n");
                printf("\n=============================================================================\n");
                break;
            case 7: // Exit
                printf("Exiting...\n");
                close(sockfd);
                exit(EXIT_SUCCESS);
            default:
                printf("Invalid option\n");
                break;
        }
    }

    while (1) {
        if (exit_flag) {
            printf("\nBye\n");
            break;
        }
    }

    close(sockfd);
    return 0;
}
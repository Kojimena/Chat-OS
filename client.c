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
        printf("\r%s | %s\n", serverResponse->messagecommunication->sender,
               serverResponse->messagecommunication->message);

        // Free the memory
        chat__server_response__free_unpacked(serverResponse, NULL);

        // Print esthetics
        str_overwrite_stdout();
    }
    pthread_exit(0);
}

/**
 * Handles the sending of messages to the server. Sends a message in the protocol format. chat__client_petition
 */
void send_msg_handler() {
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
        msgComm.recipient = "everyone";
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

    // Send username to server
    send(sockfd, username, USERNAME_SIZE, 0);

    // Show menu
    while (1) {
        int option = 0;
        printf("1. Join Chatroom\n");
        printf("2. Private Chat\n");
        printf("3. Change Status\n");
        printf("4. List Connected Users\n");
        printf("5. Show User Info\n");
        printf("6. Help\n");
        printf("7. Exit\n");

        // Get user option
        scanf("%d", &option);

        switch (option) {
            case 1: // Join Chatroom
                // Reset the exit_flag
                exit_flag = 0;

                // Create threads for sending and receiving messages
                pthread_t send_msg_thread, recv_msg_thread;
                if (pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0 ||
                    pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0) {
                    printf("Error creating threads.\n");
                    exit(EXIT_FAILURE);
                }

                while (!exit_flag) {
                    sleep(1);
                }

                pthread_cancel(send_msg_thread);
                pthread_cancel(recv_msg_thread);

                break;
            case 2:
                // Private Chat
                break;
            case 3:
                // Change Status
                break;
            case 4:
                // List Connected Users
                break;
            case 5:
                // Show User Info
                break;
            case 6:
                // Help
                break;
            case 7: // Exit
                printf("Exiting...\n");
                close(sockfd);
                exit(EXIT_SUCCESS);
            default:
                printf("Invalid option\n");
                break;
        }
        if (exit_flag) {
            exit_flag = 0;
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
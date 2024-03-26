#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include "sistos.pb-c.h" // import the generated file from the .proto

#define MAX 80
#define PORT 8080
#define SA struct sockaddr
#define BUFF_SIZE 100
#define USERNAME_SIZE 100
#define LENGTH 2048

/*
 * Deletes the '\n' (new line char) from the given string
 *
*/
void delete_lb(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++)
    {
        if (arr[i] == '\n')
        {
            arr[i] = '\0';  // Replaces the \n with a null char
            break;
        }
    }
}


//Private message to specific user sender function
void private_message(char *username, int sockfd ) {
    printf("Chatting with a specific user\n");
    char recipient[USERNAME_SIZE];
    char messageContent[BUFF_SIZE];
    int exitChat = 0;

    printf("Enter the recipient's username: ");
    scanf("%s", recipient);
    printf("Recipient: %s\n", recipient);

    do {
        printf("> Enter your message (or type 'exit' to quit): ");
        fgets(messageContent, BUFF_SIZE, stdin);
        messageContent[strcspn(messageContent, "\n")] = 0;

        if (strcmp(messageContent, "exit") == 0) {
            exitChat = 1;
            break;
        }

        Chat__MessageCommunication msgComm = CHAT__MESSAGE_COMMUNICATION__INIT;
        msgComm.message = messageContent;
        msgComm.recipient = recipient;
        msgComm.sender = username;

        Chat__ClientPetition clientPetition = CHAT__CLIENT_PETITION__INIT;
        clientPetition.option = 4;
        clientPetition.messagecommunication = &msgComm;

        //Pack protobuf petition
        unsigned len = chat__client_petition__get_packed_size(&clientPetition);
        void *buf = malloc(len);
        if (buf == NULL) {
            // Manejo de error de memoria
            fprintf(stderr, "Error al asignar memoria para el buffer de empaquetado.\n");
            exit(1);
        }

        chat__client_petition__pack(&clientPetition, buf);

        if (send(sockfd, buf, len, 0) == -1) {
            perror("Send failed");
            exit(EXIT_FAILURE);
        }

        printf("Message sent\n");

        // Receive and display any incoming messages
        char recvBuff[BUFF_SIZE];
        int n = recv(sockfd, recvBuff, BUFF_SIZE, 0);
        if (n == -1) {
            perror("Recv failed");
            exit(EXIT_FAILURE);
        }

        Chat__ClientPetition *clientPetitionRecv = chat__client_petition__unpack(NULL, n, recvBuff);
        if (clientPetitionRecv == NULL) {
            fprintf(stderr, "Error unpacking incoming message\n");
            exit(1);
        }

        if (clientPetitionRecv->messagecommunication != NULL) {
            printf("- %s: %s\n", clientPetitionRecv->messagecommunication->sender, clientPetitionRecv->messagecommunication->message);
        }

    } while (!exitChat);
    
}

//Chat with all users function
void chat_all_users(char *username, int sockfd) {
    printf("Chatting with all users\n");
    int exitChat = 0;
    do {
        printf("> Enter your message (or type 'exit' to quit): ");
        char messageContent[BUFF_SIZE];
        fgets(messageContent, BUFF_SIZE, stdin);
        messageContent[strcspn(messageContent, "\n")] = 0;

        if (strcmp(messageContent, "exit") == 0) {
            exitChat = 1;
            break;
        }

        Chat__MessageCommunication msgComm = CHAT__MESSAGE_COMMUNICATION__INIT;
        msgComm.message = messageContent;
        msgComm.recipient = "everyone";
        msgComm.sender = username;  

        Chat__ClientPetition clientPetition = CHAT__CLIENT_PETITION__INIT;
        clientPetition.option = 4;
        clientPetition.messagecommunication = &msgComm;

        //Pack protobuf petition
        unsigned len = chat__client_petition__get_packed_size(&clientPetition);
        void *buf = malloc(len);
        if (buf == NULL) {
            // Manejo de error de memoria
            fprintf(stderr, "Error al asignar memoria para el buffer de empaquetado.\n");
            exit(1);
        }

        printf("Packed size: %d\n", len);
        chat__client_petition__pack(&clientPetition, buf);

        if (send(sockfd, buf, len, 0) == -1) {
            perror("Send failed");
            exit(EXIT_FAILURE);
        }

        printf("Message sent\n");

        // Receive and display any incoming messages
        char recvBuff[BUFF_SIZE];
        int n = recv(sockfd, recvBuff, BUFF_SIZE, 0);
        if (n == -1) {
            perror("Recv failed");
            exit(EXIT_FAILURE);
        }

        Chat__ClientPetition *clientPetitionRecv = chat__client_petition__unpack(NULL, n, recvBuff);
        if (clientPetitionRecv == NULL) {
            fprintf(stderr, "Error unpacking incoming message\n");
            exit(1);
        }

        if (clientPetitionRecv->messagecommunication != NULL) {
            printf("- %s: %s\n", clientPetitionRecv->messagecommunication->sender, clientPetitionRecv->messagecommunication->message);
        }



    } while (!exitChat);
}


int main(int argc, char *argv[]){

    char *username = argv[1];
    char *server_ip = argv[2];
    int port = atoi(argv[3]);


    // new user
    delete_lb(username, strlen(username));

    if (strlen(username) > USERNAME_SIZE || strlen(username) < 1){
        printf("Invalid username\n");
        return EXIT_FAILURE;
    }

    //init socket
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));


    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    servaddr.sin_port = htons(port);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");


    //Create Protobuf User Registration
    Chat__ClientPetition clientPetition = CHAT__CLIENT_PETITION__INIT;
    Chat__UserRegistration userRegistration = CHAT__USER_REGISTRATION__INIT;

    unsigned len;

    printf("Registering user: %s\n", username);
    printf("Server IP: %s\n", server_ip);

    userRegistration.username = username;
    userRegistration.ip = server_ip;

    clientPetition.option = 1;
    clientPetition.registration = &userRegistration;


    //Pack protobuf petition
    len = chat__client_petition__get_packed_size(&clientPetition);
    void *buf = malloc(len);
    if (buf == NULL) {
        // Manejo de error de memoria
        fprintf(stderr, "Error al asignar memoria para el buffer de empaquetado.\n");
        exit(1);
    }

    chat__client_petition__pack(&clientPetition, buf);

    if (send(sockfd, buf, len, 0) == -1) {
        perror("Send failed");
        exit(EXIT_FAILURE);
    }

    printf("User created succesfully.\n");


    char buffer[BUFF_SIZE];
    char recipient[USERNAME_SIZE];

    int option = 0;
    int option2 = 0;
    do{
        printf("1. Chat with all users\n");
        printf("2. Private Chat\n");
        printf("3. Change Status\n");
        printf("4. List Connected Users\n");
        printf("5. Show User Info\n");
        printf("6. Help\n");
        printf("7. Exit\n");
        //switch case
        int exitChat = 0;
        scanf("%d", &option);
        switch(option){
            case 1: // Chat with all users
                chat_all_users(username, sockfd);

                break;
            case 2: // Send or receive private messages
                private_message(username, sockfd);
                break;
            case 3: // Change Status
                do{
                    printf("Select the status:\n");
                    printf("1. Online\n");
                    printf("2. Busy\n");
                    printf("3. Offline\n");
                    printf("4. Exit\n");
                    scanf("%d", &option2);

                    switch(option2){
                        case 1: // Online
                            printf("Online\n");
                            break;
                        case 2: // Busy
                            printf("Busy\n");
                            break;
                        case 3: // Offline
                            printf("Offline\n");
                            break;
                        case 4: // Exit
                            printf("Exiting\n");
                            break;
                        default:
                            printf("Invalid option\n");
                            break;
                    }
                } while(option2 != 4);
                break;
            case 4: // List Connected Users

                break;
            case 5: // User Info
                printf("Insert the username to get the information: ");
                scanf("%s", recipient);
                printf("%s's Information: ");

                break;
            case 6: // Help
                break;
            case 7: // Exit
                printf("Good Bye!\n");
                break;
            default:
                printf("Invalid option\n");
                break;
        }
    } while (option != 7);

    return 0;
}
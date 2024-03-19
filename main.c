#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()


#include "create_user.c"

// Imports from the .proto file
#include "sistos.pb-c.h"


#define BUFF_SIZE 100
#define USERNAME_SIZE 100

int main(int argc, char *argv[]){

        char *client_name = argv[1];
        char *username = argv[2];
        char *server_ip = argv[3];
        int port = atoi(argv[4]);

//        create_user(client_name, username, server_ip, port);

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
            scanf("%d", &option);
            switch(option){
                case 1: // Chat with all users
                    printf("Chatting with all users\n");

                    break;
                case 2: // Send or receive private messages
                    printf("Private Chat\n");
                    printf("Insert the username of the user you want to chat with: ");
                    scanf("%s", recipient);
                    printf("Recipient: %s\n", recipient);


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
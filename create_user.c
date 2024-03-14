#include <stdio.h>
int create_user(const char *client_name, const char *username, const char *server_ip, int port){
    printf("Creating user:\n");
    printf("Client name: %s\n", client_name);
    printf("Username: %s\n", username);
    printf("Server IP: %s\n", server_ip);
    printf("Port: %d\n", port);
    return 1;
}

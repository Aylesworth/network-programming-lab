#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define BUFF_SIZE 1024

int client_socket;
char buff[BUFF_SIZE];
int bytes_sent, bytes_received;

int login = 0; // whether user logged in or not

void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void sig_int(int signo) {
    close(client_socket);
    exit(EXIT_SUCCESS);
}

void menu() {
    printf("\nMENU\n");
    printf("------------------------------------\n");
    printf("1. Login\n");
    printf("2. Logout\n");
    printf("Your choice: ");
    int choice;
    scanf("%d", &choice);
    clear_stdin();
    switch (choice) {
        case 1: // Handle login
            if (login) {
                printf("You are already logged in\n");
                break;
            }

            char username[BUFF_SIZE], password[BUFF_SIZE];

            // Get username
            printf("Enter username: ");
            fgets(username, BUFF_SIZE, stdin);
            username[strcspn(username, "\n")] = '\0';

            // Get password
            printf("Insert password: ");
            fgets(password, BUFF_SIZE, stdin);
            password[strcspn(password, "\n")] = '\0';

            // Send username and password
            bytes_sent = send(client_socket, username, strlen(username), 0);
            if (bytes_sent < 0) {
                perror("Error");
                return;
            }
            usleep(1000);
            bytes_sent = send(client_socket, password, strlen(password), 0);
            if (bytes_sent < 0) {
                perror("Error");
                return;
            }

            // Receive response
            bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
            if (bytes_received <= 0) {
                perror("Error");
                return;
            }
            buff[bytes_received] = '\0';
            printf("%s\n", buff);

            if (strcmp(buff, "Login successfully") == 0) {
                login = 1;
            }

            break;

        case 2: // Handle logout
            if (!login) {
                printf("You are not logged in\n");
                break;
            }

            // Send logout request
            bytes_sent = send(client_socket, "logout", 6, 0);
            if (bytes_sent < 0) {
                perror("Error");
                return;
            }
            // Receive response
            bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
            if (bytes_received <= 0) {
                perror("Error");
                return;
            }
            buff[bytes_received] = '\0';
            printf("%s\n", buff);

            return;
    }
    menu();
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Wrong arguments!\nCorrect usage: %s <IP_ADDRESS> <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    const int SERV_PORT = atoi(argv[2]);

    struct sockaddr_in server_addr;

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  /* calls socket() */
        perror("\nError: ");
        exit(0);
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERV_PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sig_int);

    menu();

    close(client_socket);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define BUFF_SIZE 1024
#define MAX_LEN 256

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

void *receive_msg_handler(void *arg) {
    while (1) {
        bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
        if (bytes_received <= 0) {
            perror("Error");
            return NULL;
        }
        buff[bytes_received] = '\0';

        printf("%s\n\n", buff);
    }

    return NULL;
}

char *getfilename(char *filepath) {
    int i = strlen(filepath);
    while (i > 0 && filepath[i] != '/')
        i--;

    if (i == 0) return filepath;
    return filepath + i + 1;
}

void menu() {
    printf("\nMENU\n");
    printf("------------------------------------\n");
    printf("1. Login\n");
    printf("2. Chat\n");
    printf("3. Send file\n");
    printf("4. Logout\n");
    printf("5. Quit\n");
    printf("Your choice: ");
    int choice;
    scanf("%d", &choice);
    clear_stdin();
    switch (choice) {
        case 1: // Handle login
        {
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

            memset(buff, 0, BUFF_SIZE);
            sprintf(buff, "%d\n%s\n%s\n", 1, username, password);

            // Send username and password
            bytes_sent = send(client_socket, buff, strlen(buff), 0);
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
        }

        case 2: // Handle chat
        {
            if (!login) {
                printf("You are not logged in\n");
                break;
            }

            // Send joining signal
            bytes_sent = send(client_socket, "2\ninit\n\n", 8, 0);
            if (bytes_sent < 0) {
                perror("Error");
                return;
            }

            printf("You joined the chat. Enter \"exit\" to leave\n\n");
            char msg[BUFF_SIZE];
            pthread_t tid;
            pthread_create(&tid, NULL, receive_msg_handler, NULL); // run receive thread

            // Input message
            while (1) {
                fgets(msg, BUFF_SIZE - 1, stdin);
                msg[strcspn(msg, "\n")] = '\0';

                memset(buff, 0, BUFF_SIZE);
                sprintf(buff, "2\n%s\n", msg);
                bytes_sent = send(client_socket, buff, strlen(buff), 0);
                if (bytes_sent < 0) {
                    perror("Error");
                    return;
                }

                if (strcmp(msg, "exit") == 0) break;

                printf("[You]: %s\n\n", msg);
            }

            // Cancel receive thread
            pthread_cancel(tid);
            break;
        }

        case 3: // Handle send file
        {
            if (!login) {
                printf("You are not logged in\n");
                break;
            }

            // Get file path
            char filepath[MAX_LEN];
            printf("Relative path to the file: ");
            fgets(filepath, MAX_LEN - 1, stdin);
            filepath[strcspn(filepath, "\n")] = '\0';
            // Open file
            FILE *f = fopen(filepath, "rb");
            if (f == NULL) {
                printf("File not found or not enough permission\n");
                break;
            }

            // Get file name and size
            char *filename = getfilename(filepath);
            fseek(f, 0, SEEK_END);
            long filesize = ftell(f);
            long sent_size = 0;
            fclose(f);
            f = fopen(filepath, "rb");

            // Send request
            memset(buff, 0, BUFF_SIZE);
            sprintf(buff, "3\n%s\n%ld\n", filename, filesize);
            bytes_sent = send(client_socket, buff, strlen(buff), 0);
            if (bytes_sent < 0) {
                perror("Error");
                return;
            }

            // Start sending data
            usleep(1000);
            int bytes_read;
            while (1) {
                memset(buff, 0, BUFF_SIZE);
                // Read data from file
                bytes_read = fread(buff, sizeof(char), BUFF_SIZE - 1, f);
                if (bytes_read <= 0) {
                    break;
                }

                // Send data
                bytes_sent = send(client_socket, buff, bytes_read, 0);
                if (bytes_sent <= 0) {
                    perror("Error");
                    exit(EXIT_FAILURE);
                }

                sent_size += bytes_sent;
                printf("Sent %ld/%ld bytes\n", sent_size, filesize);
            }

            // Send finish signal
            fclose(f);
            strcpy(buff, "<<EOF>>");
            bytes_sent = send(client_socket, buff, strlen(buff), 0);
            if (bytes_sent < 0) {
                perror("Error");
                return;
            }
            printf("Finished sending file\n");
            break;
        }

        case 4: // Handle logout
        {
            if (!login) {
                printf("You are not logged in\n");
                break;
            }

            // Send logout request
            bytes_sent = send(client_socket, "4", 1, 0);
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

        default: // Quit
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
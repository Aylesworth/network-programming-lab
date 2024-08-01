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

int client_sock;

// Flush stdin
void clear_stdin() {
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

// Handle Ctrl + C interrupt
void handle_signal(int signal) {
    printf("Client closed\n");
    close(client_sock);
    exit(0);
}

// Extract file name from file path
char *getfilename(char *filepath) {
    int i = strlen(filepath);
    while (i > 0 && filepath[i] != '/')
        i--;
    
    return filepath + i + 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Wrong arguments!\n");
        return 1;
    }
    
    signal(SIGINT, handle_signal);

    const int SERV_PORT = atoi(argv[2]);

    char buff[BUFF_SIZE];
    struct sockaddr_in server_addr;
    int sin_size;

    // Create socket
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {  /* calls socket() */
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Provide server info
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERV_PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    
    // Connect to server
    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    int exited = 0;
    int n_bytes;

    // Show menu
    do {
        printf("\nMENU\n");
        printf("-----------------------------------\n");
        printf("1. Send a string\n");
        printf("2. Send file content\n");
        printf("3. Quit\n");
        printf("Your choice: ");

        int choice;
        scanf("%d", &choice);

        switch (choice) {
            // Send a string
            case 1:
                clear_stdin();
                while (1) {
                    // Input string
                    printf("\nEnter string: ");
                    char input[BUFF_SIZE - 2];
                    fgets(input, BUFF_SIZE - 3, stdin);
                    input[strcspn(input, "\n")] = '\0';

                    // Check stop loop signal
                    if (strlen(input) == 0)
                        break;

                    // Send request to server
                    memset(buff, '\0', BUFF_SIZE);
                    sprintf(buff, "%d:%s", choice, input);
                    n_bytes = send(client_sock, buff, strlen(buff), 0);
                    if (n_bytes <= 0) {
                        perror("Error");
                        exit(EXIT_FAILURE);
                    }

                    // Receive response from server
                    n_bytes = recv(client_sock, buff, BUFF_SIZE - 1, 0);
                    if (n_bytes <= 0) {
                        if (n_bytes == 0)
                            printf("Error: Server closed\n");
                        else
                            perror("Error");
                        exit(EXIT_FAILURE);
                    }
                    buff[n_bytes] = '\0';
                    printf("%s\n", buff);
                }
                break;
                
            // Send file content
            case 2:
                // Get file path
                printf("\nEnter file path: ");
                char filepath[BUFF_SIZE - 2];
                clear_stdin();
                fgets(filepath, BUFF_SIZE - 3, stdin);
                filepath[strcspn(filepath, "\n")] = '\0';

                // Open file
                FILE *f = fopen(filepath, "rb");
                if (f == NULL) {
                    printf("File not found\n");
                    break;
                }

                // Send request
                memset(buff, '\0', BUFF_SIZE);
                sprintf(buff, "%d:%s", choice, getfilename(filepath));
                n_bytes = send(client_sock, buff, strlen(buff), 0);
                if (n_bytes <= 0) {
                    perror("Error");
                    exit(EXIT_FAILURE);
                }

                // Process data sending
                memset(buff, 0, BUFF_SIZE);
                int n_read;
                while (1) {
                    // Read data from file
                    n_read = fread(buff, sizeof(char), BUFF_SIZE - 1, f);
                    if (n_read <= 0) {
                        break;
                    }
                    
                    // Send data
                    n_bytes = send(client_sock, buff, n_read, 0);
                    if (n_bytes <= 0) {
                        perror("Error");
                        exit(EXIT_FAILURE);
                    } 
                }
                
                // Send END signal
                sleep(1);
                memset(buff, 0, BUFF_SIZE);
                sprintf(buff, "END");
                n_bytes = send(client_sock, buff, strlen(buff), 0);
                if (n_bytes <= 0) {
                    perror("Error");
                    exit(EXIT_FAILURE);
                } 
                fclose(f);

                // Receive response from server
                n_bytes = recv(client_sock, buff, BUFF_SIZE - 1, 0);
                if (n_bytes <= 0) {
                    if (n_bytes == 0)
                        printf("Error: Server closed\n");
                    else
                        perror("Error");
                    exit(EXIT_FAILURE);
                }
                buff[n_bytes] = '\0';
                printf("%s\n", buff);
                break;

            // Exit
            default:
                // Send exit signal to server
                memset(buff, '\0', BUFF_SIZE);
                sprintf(buff, "%d", choice);
                n_bytes = send(client_sock, buff, strlen(buff), 0);
                if (n_bytes <= 0) {
                    perror("Error");
                    exit(EXIT_FAILURE);
                } 
                exited = 1;
                break;
        }
    } while (!exited);

    close(client_sock);
    return 0;
}
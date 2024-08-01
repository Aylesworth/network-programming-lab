#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <openssl/md5.h>
#include <signal.h>

#define BUFF_SIZE 1024

// Check if the string is valid (only contains letters and digits)
int validate(char *string) {
    int n = strlen(string);
    for (int i = 0; i < n; i++) {
        if (!isalnum(string[i]))
            return 0;
    }
    return 1;
}

// Encode using MD5
void md5_encode(const char *input, char *output) {
    unsigned char digest[MD5_DIGEST_LENGTH];

    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, input, strlen(input));
    MD5_Final(digest, &context);

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&output[i * 2], "%02x", digest[i]);
    }
    output[2 * MD5_DIGEST_LENGTH] = '\0';
}

// Split the string into separate strings of letters and digits
int split(const char *str, char *alpha, char *digit) {
    int n = strlen(str);
    if (n == 0) {
        return -1;
    }

    int i, j, k;
    j = k = 0;

    for (i = 0; i < n; i++) {
        if (isalpha(str[i])) {
            alpha[j] = str[i];
            j++;
        } else if (isdigit(str[i])) {
            digit[k] = str[i];
            k++;
        } else {
            return -1;
        }
    }

    alpha[j] = '\0';
    digit[k] = '\0';
    return 1;
}


// Construct server

int server_sock;

void handle_client(int client_sock) {
    char buff[BUFF_SIZE];
    int n_bytes;

    while (1) {
        // Receive request
        n_bytes = recv(client_sock, buff, BUFF_SIZE - 1, 0);
        if (n_bytes <= 0) {
            if (n_bytes < 0)
                perror("Error");
            return;
        }
        buff[n_bytes] = '\0';

        // Unpack request into choice and content
        int choice;
        sscanf(buff, "%d", &choice);
        char *content = buff + 2;

        switch (choice) {
            // Client sends string
            case 1:
                // Check if string is valid
                if (!validate(content)) {
                    memset(buff, '\0', BUFF_SIZE);
                    sprintf(buff, "Error");
                    n_bytes = send(client_sock, buff, strlen(buff), 0);
                    if (n_bytes <= 0) {
                        perror("Error");
                        return;
                    }
                    break;
                }

                // Encode and split letters and digits
                char hash[BUFF_SIZE];
                char alpha[BUFF_SIZE], digit[BUFF_SIZE];
                md5_encode(content, hash);
                split(hash, alpha, digit);

                // Print results
                printf("\nClient sent string: %s\n", content);
                printf("Hash: %s\n", hash);
                printf("Letters: %s\n", alpha);
                printf("Digits: %s\n", digit);

                // Send response to client
                memset(buff, 0, BUFF_SIZE);
                sprintf(buff, "String received by server");
                n_bytes = send(client_sock, buff, strlen(buff), 0);
                if (n_bytes <= 0) {
                    perror("Error");
                    return;
                }
                break;

            // Client sends file content
            case 2:
                // Create path to save file
                char filepath[BUFF_SIZE] = "server-files/";
                strcat(filepath, content);
                FILE *f = fopen(filepath, "wb");
                if (f == NULL) {
                    printf("Error while creating file\n");
                    return;
                }

                // Process incoming data
                int n_write;
                while (1) {
                    // Receive data
                    n_bytes = recv(client_sock, buff, BUFF_SIZE - 1, 0);
                    if (n_bytes <= 0) {
                        if (n_bytes < 0)
                            perror("Error");
                        return;
                    }
                    buff[n_bytes] = '\0';

                    // Check END signal
                    if (strcmp(buff, "END") == 0)
                        break;

                    // Write data to file
                    n_write = fwrite(buff, sizeof(char), n_bytes, f);
                    if (n_write <= 0) {
                        break;
                    }
                }
                fclose(f);
                printf("\nClient sent file: %s\n", filepath);

                // Send response to client
                memset(buff, 0, BUFF_SIZE);
                sprintf(buff, "File received by server");
                n_bytes = send(client_sock, buff, strlen(buff), 0);
                if (n_bytes <= 0) {
                    perror("Error");
                    return;
                }
                break;
            
            // Client exits
            default:
                return;
        }
    };
}

void run_server(int port) {
    // Create socket
    struct sockaddr_in server;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero), 8);

    if (bind(server_sock, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) == -1) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d.\n", port);

    // Communicate with client
    int client_sock;
    struct sockaddr_in client;
    int sin_size = sizeof(struct sockaddr_in);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client, &sin_size);

        char ip_addr[20];
        inet_ntop(AF_INET, &server.sin_addr, ip_addr, INET_ADDRSTRLEN);
        printf("\nClient %s:%u connected\n", ip_addr, ntohs(client.sin_port));

        handle_client(client_sock);
        
        printf("\nClient disconnected\n");
        close(client_sock);
    }

    close(server_sock);
}

// Handle Ctrl + C interrupt
void handle_signal(int signal) {
    printf("Server closed\n");
    close(server_sock);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Wrong arguments!\nCorrect usage: %s <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);

    const int PORT = atoi(argv[1]);
    run_server(PORT);

    return 0;
}
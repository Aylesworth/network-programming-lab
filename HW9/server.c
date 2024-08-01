#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include "aes.h"

#define BUFF_SIZE 1024
#define MAX_CLIENTS 20

const unsigned char KEY[20] = "CHAT_APP";

// Server operations

int server_socket;
int client_sockets[MAX_CLIENTS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_handler(void *arg) {
    int client_socket = *(int *) arg;
    unsigned char plain[BUFF_SIZE], cipher[BUFF_SIZE];
    int n_received, n_sent, cipher_length;

    memset(plain, 0, BUFF_SIZE);
    sprintf(plain, "Client#%d joined", client_socket);
    cipher_length = encrypt_aes(plain, cipher, KEY);

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0 && client_sockets[i] != client_socket) {
            n_sent = send(client_sockets[i], cipher, cipher_length, 0);
        }
    }
    pthread_mutex_unlock(&mutex);

    while (1) {
        n_received = recv(client_socket, cipher, BUFF_SIZE - 1 - 1, 0);
        if (n_received <= 0) {
            printf("Client#%d disconnected\n", client_socket);
            close(client_socket);

            memset(plain, 0, BUFF_SIZE);
            sprintf(plain, "Client#%d left", client_socket);
            cipher_length = encrypt_aes(plain, cipher, KEY);

            pthread_mutex_lock(&mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == client_socket) client_sockets[i] = -1;
                else if (client_sockets[i] > 0) {
                    n_sent = send(client_sockets[i], cipher, cipher_length, 0);
                }
            }
            pthread_mutex_unlock(&mutex);

            return NULL;
        }
        cipher[n_received] = '\0';

        printf("\nReceived ciphertext: ");
        for (int i = 0; cipher[i] != '\0'; ++i) {
            printf("%02x", cipher[i]);
        }
        printf("\n");

        memset(plain, 0, BUFF_SIZE);
        decrypt_aes(cipher, plain, KEY);
        printf("Received plaintext: %s\n", plain);

        unsigned char tmp[BUFF_SIZE];
        memset(tmp, 0, BUFF_SIZE);
        sprintf(tmp, "[Client#%d]: %s", client_socket, plain);
        printf("Sent plaintext: %s\n", tmp);

        memset(cipher, 0, BUFF_SIZE);
        cipher_length = encrypt_aes(tmp, cipher, KEY);
        printf("Sent ciphertext: ");
        for (int i = 0; i < cipher_length; ++i) {
            printf("%02x", cipher[i]);
        }
        printf("\n\n");

        pthread_mutex_lock(&mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0 && client_sockets[i] != client_socket) {
                n_sent = send(client_sockets[i], cipher, cipher_length, 0);
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    close(client_socket);
    return NULL;
}

void sig_int(int signo) {
    close(server_socket);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Wrong arguments!\nCorrect usage: %s <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, sig_int);

    const int PORT = atoi(argv[1]);

    struct sockaddr_in server;
    struct sockaddr_in client;
    int sin_size = sizeof(struct sockaddr_in);

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  /* calls socket() */
        perror("\nError: ");
        exit(0);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero), 8);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("\nError: ");
        exit(0);
    }

    // Listen
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d.\n", PORT);

    // Init
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }

    // Handle clients
    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *) &client, &sin_size);
        printf("Client#%d connected.\n", client_socket);

        // Add client to list
        pthread_mutex_lock(&mutex);
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == -1) {
                client_sockets[i] = client_socket;
                break;
            }
        }
        pthread_mutex_unlock(&mutex);

        if (i == MAX_CLIENTS) {
            printf("Max client number reached!\n");
            continue;
        }

        // Create new thread to handle each client
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, (void *) &client_socket);
    }

    close(server_socket);

    return 0;
}
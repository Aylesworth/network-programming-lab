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
#include "aes.h"

#define BUFF_SIZE 1024

const unsigned char KEY[16] = "CHAT_APP";

int client_socket;
unsigned char buff[BUFF_SIZE];
int bytes_sent, bytes_received;

void sig_int(int signo) {
    close(client_socket);
    exit(EXIT_SUCCESS);
}

void *receive_handler(void *arg) {
    while (1) {
        bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("Server closed\n");
            exit(EXIT_FAILURE);
        }
        buff[bytes_received] = '\0';

        unsigned char msg[BUFF_SIZE];
        memset(msg, 0, BUFF_SIZE);
        decrypt_aes(buff, msg, KEY);
        printf("%s\n\n", msg);
    }
    return NULL;
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

    pthread_t tid;
    pthread_create(&tid, NULL, receive_handler, NULL);

    printf("Chat something:\n\n");
    while (1) {
        memset(buff, 0, BUFF_SIZE);
        fgets(buff, BUFF_SIZE - 1, stdin);
        buff[strcspn(buff, "\n")] = '\0';

        printf("[You]: %s\n\n", buff);

        unsigned char cipher[BUFF_SIZE];
        int length = encrypt_aes(buff, cipher, KEY);

        bytes_sent = send(client_socket, cipher, length, 0);
    }

    close(client_socket);
    return 0;
}
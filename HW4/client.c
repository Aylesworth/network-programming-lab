#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define BUFF_SIZE 1024

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Wrong arguments!\nCorrect usage: %s <IP_ADDRESS> <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    const int SERV_PORT = atoi(argv[2]);

    int client_sock;
    char buff[BUFF_SIZE];
    struct sockaddr_in server_addr;
    int bytes_sent, bytes_received, sin_size;

    //Step 1: Construct a UDP socket
    if ((client_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {  /* calls socket() */
        perror("\nError: ");
        exit(0);
    }

    //Step 2: Define the address of the server
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERV_PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    //Step 3: Communicate with server
    printf("Enter @ or # to quit.\n");
    while (1) {
        // Send data
        printf("\nSend something: ");
        memset(buff, '\0', (strlen(buff) + 1));
        fgets(buff, BUFF_SIZE, stdin);
        buff[strcspn(buff, "\n")] = '\0';

        if (strcmp(buff, "@") == 0 || strcmp(buff, "#") == 0) {
            printf("\nProcess exited.\n");
            break;
        }

        sin_size = sizeof(struct sockaddr);

        bytes_sent = sendto(client_sock, buff, strlen(buff), 0, (struct sockaddr *) &server_addr, sin_size);
        if (bytes_sent < 0) {
            perror("Error: ");
            close(client_sock);
            return 0;
        }

        // Receive data
        bytes_received = recvfrom(client_sock, buff, BUFF_SIZE - 1, 0, (struct sockaddr *) &server_addr, &sin_size);
        if (bytes_received < 0) {
            perror("Error: ");
            close(client_sock);
            return 0;
        }

        buff[bytes_received] = '\0';
        printf("\nReply from server: %s\n", buff);
    }

    close(client_sock);
    return 0;
}

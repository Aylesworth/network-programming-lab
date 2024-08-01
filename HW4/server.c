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

#define BUFF_SIZE 1024

void encode(const char *data, char *result) {
    unsigned char digest[MD5_DIGEST_LENGTH];

    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, data, strlen(data));
    MD5_Final(digest, &context);

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&result[i * 2], "%02x", digest[i]);
    }
    result[2 * MD5_DIGEST_LENGTH] = '\0';
}

int split_alpha_digit(const char *str, char *alpha_str, char *digit_str) {
    int n = strlen(str);
    int i, j, k;
    i = j = k = 0;

    for (i = 0; i < n; i++) {
        if (isalpha(str[i])) {
            alpha_str[j] = str[i];
            j++;
        } else if (isdigit(str[i])) {
            digit_str[k] = str[i];
            k++;
        } else {
            return -1;
        }
    }

    alpha_str[j] = '\0';
    digit_str[k] = '\0';
    return 0;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Wrong arguments!\nCorrect usage: %s <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    const int PORT = atoi(argv[1]);
    int server_sock; /* file descriptors */
    char buff[BUFF_SIZE];
    int bytes_sent, bytes_received;
    struct sockaddr_in server; /* server's address information */
    struct sockaddr_in client1, client2; /* client's address information */
    int sin_size;

    //Step 1: Construct a UDP socket
    if ((server_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {  /* calls socket() */
        perror("\nError: ");
        exit(0);
    }

    //Step 2: Bind address to socket
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);   /* Remember htons() from "Conversions" section? =) */
    server.sin_addr.s_addr = INADDR_ANY;  /* INADDR_ANY puts your IP address automatically */
    bzero(&(server.sin_zero), 8); /* zero the rest of the structure */


    if (bind(server_sock, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) { /* calls bind() */
        perror("\nError: ");
        exit(0);
    }
    printf("Server listening on port %d.\n", PORT);

    //Step 3: Communicate with clients
    while (1) {
        sin_size = sizeof(struct sockaddr_in);

        char alpha_str_1[BUFF_SIZE / 2];
        char digit_str_1[BUFF_SIZE / 2];
        char alpha_str_2[BUFF_SIZE / 2];
        char digit_str_2[BUFF_SIZE / 2];

        // Receive from client 1
        bytes_received = recvfrom(server_sock, buff, BUFF_SIZE - 1, 0, (struct sockaddr *) &client1, &sin_size);

        if (bytes_received < 0)
            perror("\nError: ");
        else {
            buff[bytes_received] = '\0';
            char hash[BUFF_SIZE];
            encode(buff, hash);
            split_alpha_digit(hash, alpha_str_1, digit_str_1);
            printf("[From %s:%d]: %s\n", inet_ntoa(client1.sin_addr), ntohs(client1.sin_port), buff);
        }

        // Receive from client 2
        bytes_received = recvfrom(server_sock, buff, BUFF_SIZE - 1, 0, (struct sockaddr *) &client2, &sin_size);

        if (bytes_received < 0)
            perror("\nError: ");
        else {
            buff[bytes_received] = '\0';
            char hash[BUFF_SIZE];
            encode(buff, hash);
            split_alpha_digit(hash, alpha_str_2, digit_str_2);
            printf("[From %s:%d]: %s\n", inet_ntoa(client2.sin_addr), ntohs(client2.sin_port), buff);
        }

        // Send to client 2
        sprintf(buff, "%s %s", alpha_str_1, digit_str_1);
        bytes_sent = sendto(server_sock, buff, strlen(buff), 0, (struct sockaddr *) &client2,
                            sin_size);
        if (bytes_sent < 0)
            perror("\nError: ");
        else
            printf("[To   %s:%d]: %s\n", inet_ntoa(client2.sin_addr), ntohs(client2.sin_port), buff);

        // Send to client 1
        sprintf(buff, "%s %s", alpha_str_2, digit_str_2);
        bytes_sent = sendto(server_sock, buff, strlen(buff), 0, (struct sockaddr *) &client1,
                            sin_size);
        if (bytes_sent < 0)
            perror("\nError: ");
        else
            printf("[To   %s:%d]: %s\n", inet_ntoa(client1.sin_addr), ntohs(client1.sin_port), buff);
    }

    close(server_sock);
    return 0;
}

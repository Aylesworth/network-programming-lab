#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <openssl/sha.h>

#define MAX_LEN 256
#define BUFF_SIZE 1024

// Account data

typedef struct Account {
    char username[MAX_LEN];
    char password[MAX_LEN];
    int activated;
    int blocked;
    struct Account *next;
} Account;

Account *head = NULL, *tail = NULL;

// Add account to list
void add_acc(char *username, char *password, int activated, int blocked) {
    Account *acc = (Account *) malloc(sizeof(Account));
    strcpy(acc->username, username);
    strcpy(acc->password, password);
    acc->activated = activated;
    acc->blocked = blocked;
    acc->next = NULL;

    if (head == NULL) {
        head = acc;
        tail = acc;
    } else {
        tail->next = acc;
        tail = acc;
    }
}

// Find account
Account *find_acc(char *username) {
    for (Account *acc = head; acc != NULL; acc = acc->next) {
        if (strcmp(acc->username, username) == 0) return acc;
    }
    return NULL;
}

const char *FILE_NAME = "account.txt";

// Load accounts from file
void load_data() {
    FILE *f = fopen(FILE_NAME, "r");

    char username[MAX_LEN];
    char password[MAX_LEN];
    int activated;
    int blocked;

    while (fscanf(f, "%s %s %d %d", username, password, &activated, &blocked) > 0) {
        add_acc(username, password, activated, blocked);
    }

    fclose(f);
}

// Save accounts to file
void save_data() {
    FILE *f = fopen(FILE_NAME, "w");

    for (Account *acc = head; acc != NULL; acc = acc->next) {
        fprintf(f, "%s %s %d %d\n", acc->username, acc->password, acc->activated, acc->blocked);
    }

    fclose(f);
}

// Password processing

// Check if the password is valid (contains only letters and digits)
int validate_password(char *password) {
    int n = strlen(password);
    for (int i = 0; i < n; i++) {
        if (!isalnum(password[i]))
            return 0;
    }
    return 1;
}

// Encode the password using SHA-256
void sha256_encode(const char *input, char *output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input, strlen(input));
    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
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

void run_server(int port) {
    // Create socket
    struct sockaddr_in server;

    if ((server_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {  /* calls socket() */
        perror("\nError: ");
        exit(0);
    }

    // Bind socket
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero), 8);

    if (bind(server_sock, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("\nError: ");
        exit(0);
    }
    printf("Server listening on port %d.\n", port);

    // Communicate with client
    struct sockaddr_in client;
    int sin_size = sizeof(struct sockaddr_in);
    int bytes_sent, bytes_received;

    // Handle sign-in
    while (1) {
        int signed_in = 0;
        int count_wrong_pw = 0;
        char last_username[MAX_LEN];

        Account *acc;

        do {
            char buff[BUFF_SIZE];
            char username[MAX_LEN], password[MAX_LEN];

            // Receive username
            memset(buff, '\0', strlen(buff) + 1);
            bytes_received = recvfrom(server_sock, buff, BUFF_SIZE - 1, 0, (struct sockaddr *) &client, &sin_size);
            strcpy(username, buff);

            sprintf(buff, "Insert password:");
            bytes_sent = sendto(server_sock, buff, strlen(buff), 0, (struct sockaddr *) &client, sin_size);

            // Receive password
            memset(buff, '\0', strlen(buff) + 1);
            bytes_received = recvfrom(server_sock, buff, BUFF_SIZE - 1, 0, (struct sockaddr *) &client, &sin_size);
            strcpy(password, buff);

            acc = find_acc(username);

            if (acc == NULL) {
                // If account doesn't exist
                sprintf(buff, "Not OK\nEnter username: ");
            } else {
                if (strcmp(acc->password, password) == 0) {
                    // If password is correct
                    if (acc->activated == 1 && acc->blocked == 0) {
                        sprintf(buff, "OK\nEnter 'bye' to quit.\nChange password:");
                        signed_in = 1;
                        count_wrong_pw = 0;
                    } else {
                        sprintf(buff, "Account not ready\nEnter username: ");
                    }
                } else {
                    // If password is wrong
                    if (strcmp(acc->username, last_username) != 0)
                        count_wrong_pw = 0;
                    count_wrong_pw++;
                    if (count_wrong_pw < 3) {
                        sprintf(buff, "Not OK\nEnter username: ");
                    } else {
                        acc->blocked = 1;
                        save_data();
                        sprintf(buff, "Account is blocked\nEnter username: ");
                    }
                    strcpy(last_username, acc->username);
                }
            }

            // Send response
            bytes_sent = sendto(server_sock, buff, strlen(buff), 0, (struct sockaddr *) &client, sin_size);

        } while (!signed_in);


        // Handle changing password
        do {
            char buff[BUFF_SIZE];
            char new_password[MAX_LEN];

            // Receive new password
            memset(buff, '\0', BUFF_SIZE);
            bytes_received = recvfrom(server_sock, buff, BUFF_SIZE - 1, 0, (struct sockaddr *) &client, &sin_size);

            if (strcmp(buff, "bye") == 0) {
                sprintf(buff, "Goodbye %s\n", acc->username);
                bytes_sent = sendto(server_sock, buff, strlen(buff), 0, (struct sockaddr *) &client, sin_size);
                break;
            }

            // Validate
            strcpy(new_password, buff);
            if (validate_password(new_password) == 0) {
                sprintf(buff, "Error\nChange password:");
                bytes_sent = sendto(server_sock, buff, strlen(buff), 0, (struct sockaddr *) &client, sin_size);
                continue;
            }

            strcpy(acc->password, new_password);
            save_data();

            // Process password
            char alpha[MAX_LEN], digit[MAX_LEN], hash[2 * SHA256_DIGEST_LENGTH + 1];
            sha256_encode(new_password, hash);
            split(hash, alpha, digit);

            memset(buff, '\0', strlen(buff) + 1);
            sprintf(buff, "Hash: %s\nLetters: %s\nDigits: %s\nChange password:", hash, alpha, digit);

            // Send response
            bytes_sent = sendto(server_sock, buff, strlen(buff), 0, (struct sockaddr *) &client, sin_size);

        } while (1);
    }

    close(server_sock);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Wrong arguments!\nCorrect usage: %s <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    load_data();

    const int PORT = atoi(argv[1]);
    run_server(PORT);

    return 0;
}
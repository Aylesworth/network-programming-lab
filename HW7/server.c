#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

#define MAX_LEN 256
#define BUFF_SIZE 1024

typedef struct Account {
    char username[MAX_LEN];
    char password[MAX_LEN];
    int status;
    int online;
    struct Account *next;
} Account;

Account *head = NULL, *tail = NULL;

// Linked list operations

void add_acc(char *username, char *password, int status, int online) {
    Account *acc = (Account *) malloc(sizeof(Account));
    strcpy(acc->username, username);
    strcpy(acc->password, password);
    acc->status = status;
    acc->online = online;
    acc->next = NULL;

    if (head == NULL) {
        head = acc;
        tail = acc;
    } else {
        tail->next = acc;
        tail = acc;
    }
}

Account *find_acc(char *username) {
    for (Account *acc = head; acc != NULL; acc = acc->next) {
        if (strcmp(acc->username, username) == 0) return acc;
    }
    return NULL;
}

void clear_list() {
    Account *p = head;
    while (p != NULL) {
        Account *tmp = p;
        p = p->next;
        free(tmp);
    }
    head = tail = NULL;
}

// File operations

const char *FILE_NAME = "account.txt";

void load_data() {
    clear_list();

    FILE *f = fopen(FILE_NAME, "r");

    char username[MAX_LEN];
    char password[MAX_LEN];
    int status;
    int online;

    while (fscanf(f, "%s %s %d %d", username, password, &status, &online) > 0) {
        add_acc(username, password, status, online);
    }

    fclose(f);
}

void save_data() {
    FILE *f = fopen(FILE_NAME, "w");

    for (Account *acc = head; acc != NULL; acc = acc->next) {
        fprintf(f, "%s %s %d %d\n", acc->username, acc->password, acc->status, acc->online);
    }

    fclose(f);
}

// Server operations

int server_socket;

void handle(int client_socket) {
    int bytes_sent, bytes_received;
    char buff[BUFF_SIZE];

    int login = 0;
    int count_wrong_pw = 0;
    char last_username[MAX_LEN];
    Account *acc;

    // Handle login request
    do {
        char username[MAX_LEN], password[MAX_LEN];

        // Receive username
        bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
        if (bytes_received <= 0) return;
        buff[bytes_received] = '\0';
        strcpy(username, buff);

        usleep(1000);

        // Receive password
        bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
        if (bytes_received <= 0) return;
        buff[bytes_received] = '\0';
        strcpy(password, buff);

        // Check account
        load_data();
        acc = find_acc(username);

        memset(buff, 0, BUFF_SIZE);
        if (acc == NULL) {
            sprintf(buff, "Wrong username or password");
        } else {
            if (strcmp(acc->password, password) == 0) {
                if (acc->status == 1) {
                    if (acc->online) {
                        sprintf(buff, "Account logged in on another client");
                    } else {
                        login = 1;
                        acc->online = 1;
                        save_data();
                        sprintf(buff, "Login successfully");
                    }
                } else {
                    sprintf(buff, "Account not ready");
                }
            } else {
                if (strcmp(username, last_username) != 0) count_wrong_pw = 0;
                count_wrong_pw++;
                if (count_wrong_pw < 3) {
                    sprintf(buff, "Wrong username or password");
                } else {
                    acc->status = 0;
                    save_data();
                    count_wrong_pw = 0;
                    sprintf(buff, "Account is blocked");
                }
                strcpy(last_username, username);
            }
        }

        // Send response
        bytes_sent = send(client_socket, buff, strlen(buff), 0);
        if (bytes_sent < 0) {
            perror("Error");
            return;
        }
    } while (!login);

    // Receive logout request
    bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
    if (bytes_received <= 0) return;
    buff[bytes_received] = '\0';

    // Reset online status
    acc->online = 0;
    save_data();

    // Send response
    memset(buff, 0, BUFF_SIZE);
    sprintf(buff, "Logout successfully");
    bytes_sent = send(client_socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        perror("Error");
        return;
    }
}

void sig_chld(int signo) {
    pid_t pid;
    int stat;
    pid = waitpid(-1, &stat, WNOHANG);
    printf("Child %d terminated\n", pid);
}

void run_server(int port) {
    struct sockaddr_in server;
    struct sockaddr_in client;
    int sin_size = sizeof(struct sockaddr_in);

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  /* calls socket() */
        perror("\nError: ");
        exit(0);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero), 8);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("\nError: ");
        exit(0);
    }

    // Listen
    if (listen(server_socket, 5) == -1) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d.\n", port);

    signal(SIGCHLD, sig_chld);

    // Handle clients
    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *) &client, &sin_size);

        // Fork new process to handle each client
        int pid;
        if ((pid = fork()) == 0) {
            close(server_socket);

            // Get client info
            char ip_addr[MAX_LEN];
            int port;
            inet_ntop(AF_INET, &(client.sin_addr), ip_addr, MAX_LEN);
            port = ntohs(client.sin_port);
            printf("Client %s:%d connected\n", ip_addr, port);

            // Handle client
            handle(client_socket);

            // Close client socket
            close(client_socket);
            printf("Client %s:%d disconnected\n", ip_addr, port);
            exit(0);
        } else if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }

        close(client_socket);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Wrong arguments!\n");
        return 1;
    }

    load_data();

    const int PORT = atoi(argv[1]);
    run_server(PORT);

    close(server_socket);

    return 0;
}
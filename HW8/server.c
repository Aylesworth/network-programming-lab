#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>

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

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

void add_acc(char *username, char *password, int status, int online) {
    Account *acc = (Account *) malloc(sizeof(Account));
    strcpy(acc->username, username);
    strcpy(acc->password, password);
    acc->status = status;
    acc->online = online;
    acc->next = NULL;

    pthread_mutex_lock(&mutex1);
    if (head == NULL) {
        head = acc;
        tail = acc;
    } else {
        tail->next = acc;
        tail = acc;
    }
    pthread_mutex_unlock(&mutex1);
}

Account *find_acc(char *username) {
    pthread_mutex_lock(&mutex1);
    for (Account *acc = head; acc != NULL; acc = acc->next) {
        if (strcmp(acc->username, username) == 0) {
            pthread_mutex_unlock(&mutex1);
            return acc;
        }
    }
    pthread_mutex_unlock(&mutex1);
    return NULL;
}

void clear_list() {
    pthread_mutex_lock(&mutex1);
    Account *p = head;
    while (p != NULL) {
        Account *tmp = p;
        p = p->next;
        free(tmp);
    }
    head = tail = NULL;
    pthread_mutex_unlock(&mutex1);
}

// File operations

const char *FILE_NAME = "account.txt";

pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void load_data() {
    clear_list();

    pthread_mutex_lock(&mutex2);

    FILE *f = fopen(FILE_NAME, "r");
    char username[MAX_LEN];
    char password[MAX_LEN];
    int status;
    int online;

    while (fscanf(f, "%s %s %d %d", username, password, &status, &online) > 0) {
        add_acc(username, password, status, online);
    }

    fclose(f);

    pthread_mutex_unlock(&mutex2);
}

void save_data() {
    pthread_mutex_lock(&mutex2);

    FILE *f = fopen(FILE_NAME, "w");

    for (Account *acc = head; acc != NULL; acc = acc->next) {
        fprintf(f, "%s %s %d %d\n", acc->username, acc->password, acc->status, acc->online);
    }

    fclose(f);

    pthread_mutex_unlock(&mutex2);
}

// Server operations

int server_socket;

void handle_close_socket(int client_socket, char *username) {
    // Close socket
    close(client_socket);
    printf("Socket %d closed\n", client_socket);

    // Reset online status to false
    Account *acc = find_acc(username);
    acc->online = 0;
    save_data();
}

void *client_handler(void *arg) {
    pthread_detach(pthread_self());
    int client_socket = *(int *) arg;
    printf("Socket %d opened\n", client_socket);

    int bytes_sent, bytes_received;
    char buff[BUFF_SIZE];

    int login = 0;
    int count_wrong_pw = 0;
    char last_username[MAX_LEN];
    Account *acc;

    char username[MAX_LEN], password[MAX_LEN];

    // Handle login request
    do {
        // Receive username
        bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
        if (bytes_received == 0) {
            handle_close_socket(client_socket, username);
            return NULL;
        } else if (bytes_received < 0) {
            perror("Error");
            return NULL;
        }
        buff[bytes_received] = '\0';
        strcpy(username, buff);

        usleep(1000);

        // Receive password
        bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
        if (bytes_received == 0) {
            handle_close_socket(client_socket, username);
            return NULL;
        } else if (bytes_received < 0) {
            perror("Error");
            return NULL;
        }
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
            return NULL;
        }
    } while (!login);

    // Receive logout request
    bytes_received = recv(client_socket, buff, BUFF_SIZE - 1, 0);
    if (bytes_received == 0) {
        handle_close_socket(client_socket, username);
        return NULL;
    } else if (bytes_received < 0) {
        perror("Error");
        return NULL;
    }
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
        return NULL;
    }

    handle_close_socket(client_socket, username);
}

void sig_int(int signo) {
    close(server_socket);

    // Reset online status of clients to false
    for (Account *acc = head; acc != NULL; acc = acc->next) {
        if (acc->online) acc->online = 0;
    }
    save_data();

    exit(EXIT_SUCCESS);
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

    // Handle clients
    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *) &client, &sin_size);

        // Create new thread to handle each client
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, (void *) &client_socket);
    }

    close(server_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Wrong arguments!\nCorrect usage: %s <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    load_data();
    signal(SIGINT, sig_int);

    const int PORT = atoi(argv[1]);
    run_server(PORT);

    return 0;
}
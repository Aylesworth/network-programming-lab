#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define BUFF_SIZE 1024
#define MAX_LEN 256
#define MAX_CLIENTS 100

typedef struct Account {
    char username[MAX_LEN];
    char password[MAX_LEN];
    int status; // 0: not logged in, 1: logged in, 2: chatting, 3: sending file
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
        if (strcmp(acc->username, username) == 0) {
            return acc;
        }
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

    while (fscanf(f, "%s %s %d", username, password, &status) > 0) {
        add_acc(username, password, status, 0);
    }

    fclose(f);
}

void save_data() {
    FILE *f = fopen(FILE_NAME, "w");

    for (Account *acc = head; acc != NULL; acc = acc->next) {
        fprintf(f, "%s %s %d\n", acc->username, acc->password, acc->status);
    }

    fclose(f);
}

// Server operations

typedef struct Context {
    int status;
    int count_wrong_pw;
    char last_username[MAX_LEN];
    Account *acc;
    FILE *file;
    long filesize;
    long received_size;
} Context;

int listenfd;
fd_set readfds;
int maxfd, maxi;
int n_ready;
int clients[MAX_CLIENTS];
Context ctx[MAX_CLIENTS];

void handle_disconnect(int i) {
    // close socket
    close(clients[i]);
    printf("\nSocket %d closed\n", clients[i]);

    // re-calculate max values;
    if (i == maxi) {
        while (clients[--maxi] < 0);
    }
    if (clients[i] == maxfd) {
        maxfd = listenfd;
        for (int j = 0; j <= maxi; j++) {
            if (clients[j] > maxfd && j != i) {
                maxfd = clients[j];
            }
        }
    }

    // reset values
    FD_CLR(clients[i], &readfds);
    clients[i] = -1;
    ctx[i].status = 0;
    ctx[i].count_wrong_pw = 0;
    memset(ctx[i].last_username, 0, MAX_LEN);
    if (ctx[i].acc != NULL) {
        ctx[i].acc->online = 0;
        ctx[i].acc = NULL;
    }
}

void handle_login(int i, char *payload) {
    int client_socket = clients[i];
    int bytes_sent;
    char buff[BUFF_SIZE];

    // extract username and password
    char *username = strtok(payload, "\n");
    char *password = strtok(NULL, "\n");

    // check account
    Account *acc = find_acc(username);

    memset(buff, 0, BUFF_SIZE);
    if (acc == NULL) {
        sprintf(buff, "Wrong username or password");
    } else {
        if (strcmp(acc->password, password) == 0) {
            if (acc->status == 1) {
                if (acc->online) {
                    sprintf(buff, "Account logged in on another client");
                } else {
                    acc->online = 1;
                    ctx[i].acc = acc;
                    ctx[i].status = 1;
                    sprintf(buff, "Login successfully");
                }
            } else {
                sprintf(buff, "Account not ready");
            }
        } else {
            if (strcmp(username, ctx[i].last_username) != 0) ctx[i].count_wrong_pw = 0;
            ctx[i].count_wrong_pw++;
            if (ctx[i].count_wrong_pw < 3) {
                sprintf(buff, "Wrong username or password");
            } else {
                acc->status = 0;
                save_data();
                ctx[i].count_wrong_pw = 0;
                sprintf(buff, "Account is blocked");
            }
            strcpy(ctx[i].last_username, username);
        }
    }

    // send response
    bytes_sent = send(client_socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        perror("Error");
        exit(EXIT_FAILURE);
    }
}

void handle_logout(int i) {
    int client_socket = clients[i];
    int bytes_sent;
    char buff[BUFF_SIZE];

    // reset online status
    ctx[i].acc->online = 0;

    // send response
    memset(buff, 0, BUFF_SIZE);
    sprintf(buff, "Goodbye %s", ctx[i].acc->username);

    bytes_sent = send(client_socket, buff, strlen(buff), 0);
    if (bytes_sent < 0) {
        perror("Error");
        exit(EXIT_FAILURE);
    }

    handle_disconnect(i);
}

void handle_chat(int i, char *payload) {
    char buff[BUFF_SIZE];
    int bytes_sent;

    // case of joining
    if (strcmp(payload, "init\n\n") == 0) {
        ctx[i].status = 2;

        memset(buff, 0, BUFF_SIZE);
        sprintf(buff, "%s joined the chat", ctx[i].acc->username);

        for (int j = 0; j <= maxi; j++) {
            if (j != i && ctx[j].status == 2) {
                bytes_sent = send(clients[j], buff, strlen(buff), 0);
                if (bytes_sent < 0) {
                    perror("Error");
                    exit(EXIT_FAILURE);
                }
            }
        }

        return;
    }

    char *msg = strtok(payload, "\n");

    // case of leaving
    if (strcmp(msg, "exit") == 0) {
        ctx[i].status = 1;

        memset(buff, 0, BUFF_SIZE);
        sprintf(buff, "%s left the chat", ctx[i].acc->username);

        for (int j = 0; j <= maxi; j++) {
            if (j != i && ctx[j].status == 2) {
                bytes_sent = send(clients[j], buff, strlen(buff), 0);
                if (bytes_sent < 0) {
                    perror("Error");
                    exit(EXIT_FAILURE);
                }
            }
        }
        return;
    }

    // broadcast message to all users in chat
    memset(buff, 0, BUFF_SIZE);
    sprintf(buff, "[%s]: %s", ctx[i].acc->username, msg);
    for (int j = 0; j <= maxi; j++) {
        if (j != i && ctx[j].status == 2) {
            bytes_sent = send(clients[j], buff, strlen(buff), 0);
            if (bytes_sent < 0) {
                perror("Error");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void handle_file(int i, char *payload, int payload_size) {
    // case of beginning
    if (ctx[i].status != 3) {
        ctx[i].status = 3;

        char *filename = strtok(payload, "\n");
        long filesize = strtol(strtok(NULL, "\n"), NULL, 10);

        char filepath[MAX_LEN];
        memset(filepath, 0, MAX_LEN);
        sprintf(filepath, "server-files/%s_%s", ctx[i].acc->username, filename);

        ctx[i].file = fopen(filepath, "wb");
        if (ctx[i].file == NULL) {
            printf("Error while creating file\n");
            return;
        }
        ctx[i].filesize = filesize;

        printf("Receiving file %s from client %d...\n", filename, clients[i]);
        return;
    }

    // check if this is last segment
    int is_eof = 0;
    if (strcmp(payload + payload_size - 7, "<<EOF>>") == 0) {
        is_eof = 1;
        payload_size -= 7;
    }

    // write data to file
    int bytes_write = fwrite(payload, sizeof(char), payload_size, ctx[i].file);
    if (bytes_write <= 0) {
        return;
    }
    ctx[i].received_size += bytes_write;
    printf("Received %ld/%ld bytes\n", ctx[i].received_size, ctx[i].filesize);

    // reset status if is last segment
    if (is_eof) {
        printf("Finished receiving file\n");
        fclose(ctx[i].file);
        ctx[i].status = 1;
        ctx[i].file = NULL;
        ctx[i].filesize = -1;
        ctx[i].received_size = 0;
        return;
    }
}

void handle_client(int i) {
    // receive message
    char buff[BUFF_SIZE];
    int bytes_received = recv(clients[i], buff, BUFF_SIZE - 1, 0);
    if (bytes_received <= 0) {
        handle_disconnect(i);
        return;
    }
    buff[bytes_received] = '\0';

    // if is in progress of sending file
    if (ctx[i].status == 3) {
        handle_file(i, buff, bytes_received);
        return;
    }

    // switch by message type
    switch (buff[0]) {
        case '1':
            handle_login(i, buff + 2);
            break;
        case '2':
            handle_chat(i, buff + 2);
            break;
        case '3':
            handle_file(i, buff + 2, strlen(buff + 2));
            break;
        case '4':
            handle_logout(i);
            break;
    }
}

void sig_int(int signo) {
    close(listenfd);
    save_data();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Wrong arguments!\nCorrect usage: %s <PORT_NUMBER>\n", argv[0]);
        return 1;
    }

    load_data();
    signal(SIGINT, sig_int);

    const int PORT = atoi(argv[1]);

    struct sockaddr_in server;
    struct sockaddr_in client;
    int sin_size = sizeof(struct sockaddr_in);

    // create socket
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  /* calls socket() */
        perror("\nError: ");
        exit(0);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero), 8);

    // bind socket
    if (bind(listenfd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("\nError: ");
        exit(0);
    }

    // listen
    if (listen(listenfd, MAX_CLIENTS) == -1) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d.\n", PORT);

    // initialize values
    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);
    maxfd = listenfd;
    maxi = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1;
        ctx[i].status = 0;
        ctx[i].count_wrong_pw = 0;
        memset(ctx[i].last_username, 0, MAX_LEN);
        ctx[i].acc = NULL;
        ctx[i].file = NULL;
        ctx[i].filesize = -1;
        ctx[i].received_size = 0;
    }

    while (1) {
        // wait for events
        printf("\nWaiting for events...\n");
        n_ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (n_ready < 0) {
            perror("Error");
            exit(EXIT_FAILURE);
        }

        // check for new connection
        if (FD_ISSET(listenfd, &readfds)) {
            int connfd = accept(listenfd, (struct sockaddr *) &client, &sin_size);
            printf("\nClient %d connected\n", connfd);
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] < 0) {
                    clients[i] = connfd;
                    break;
                }
            }

            // calculate max values
            if (i == MAX_CLIENTS) {
                printf("Max clients reached!\n");
                close(connfd);
            } else {
                if (connfd > maxfd) maxfd = connfd;
                if (i > maxi) maxi = i;
            }
        }

        // check for read event on clients
        for (int i = 0; i <= maxi; i++) {
            if (clients[i] > 0) {
                if (FD_ISSET(clients[i], &readfds)) {
                    printf("\nHandling client %d\n", clients[i]);
                    handle_client(i);
                    if (clients[i] > 0) FD_SET(clients[i], &readfds);
                    continue;
                }
                FD_SET(clients[i], &readfds);
            }
        }

        FD_SET(listenfd, &readfds);
    }

    close(listenfd);

    return 0;
}
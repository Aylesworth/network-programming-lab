#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#define MAX_LEN 100

const char *FILE_NAME = "account.txt";



// Account node model

typedef struct Account {
    char username[MAX_LEN];
    char password[MAX_LEN];
    int status;
    struct Account *next; // next node in linked list
} Account;



// Linked list

Account *head = NULL; // first element in linked list



// Account services

Account *createAccount(char *username, char *password, int status) {
    Account *account = (Account *) malloc(sizeof(Account));

    strcpy(account->username, username);
    strcpy(account->password, password);
    account->status = status;
    account->next = NULL;

    return account;
}


void addAccount(Account *account) {
    // if list is empty, the new account will be the head
    if (head == NULL) {
        head = account;
        return;
    }

    // find last element
    Account *p = head;
    while (p->next != NULL)
        p = p->next;

    // add new account after the last element
    p->next = account;
}


Account *findAccount(char *username) {
    for (Account *p = head; p != NULL; p = p->next) {
        if (strcmp(p->username, username) == 0)
            return p;
    }
    return NULL;
}


void clearAccounts() {
    // free memory
    Account *p = head;
    while (p != NULL) {
        Account *temp = p;
        p = p->next;
        free(temp);
    }

    // re-initialize head
    head = NULL;
}



// File IO services

void readAccountsFromFile(const char *fileName) {
    // clear linked list
    clearAccounts();

    // open file
    FILE *f = fopen(fileName, "r");
    if (f == NULL) {
        printf("Error occurred while opening file");
        return;
    }

    char username[MAX_LEN];
    char password[MAX_LEN];
    int status;

    // read each line
    while (fscanf(f, "%s %s %d", username, password, &status) == 3) {
        Account *account = createAccount(username, password, status);
        addAccount(account);
    }

    // close file
    fclose(f);
}


void writeAccountToFile(const char *fileName, Account *account) {
    // open file
    FILE *f = fopen(fileName, "a");
    if (f == NULL) {
        printf("Error occurred while opening file");
        return;
    }

    // append new account
    fprintf(f, "%s %s %d\n", account->username, account->password, account->status);

    // close file
    fclose(f);
}


void rewriteAccountsToFile(const char *fileName) {
    // open file
    FILE *f = fopen(fileName, "w");
    if (f == NULL) {
        printf("Error occurred while opening file");
        return;
    }

    // overwrite accounts
    for (Account *p = head; p != NULL; p = p->next) {
        fprintf(f, "%s %s %d\n", p->username, p->password, p->status);
    }

    // close file
    fclose(f);
}



// Main program functions

void handleRegister() {
    // input username
    char username[MAX_LEN];
    printf("Username: ");
    scanf("%s", username);

    // check if account exists
    if (findAccount(username) != NULL) {
        printf("Account existed\n");
        return;
    }

    // input password
    char password[MAX_LEN];
    printf("Password: ");
    scanf("%s", password);

    // save account
    Account *account = createAccount(username, password, 1);
    addAccount(account);
    writeAccountToFile(FILE_NAME, account);

    printf("Successful registration\n");
}


int isUserSignedIn = 0; // login status

int incorrectPasswordCount = 0;

void handleSignIn() {
    // input username
    char username[MAX_LEN];
    printf("Username: ");
    scanf("%s", username);

    // find account
    Account *account = findAccount(username);
    if (account == NULL) {
        printf("Cannot find account\n");
        incorrectPasswordCount = 0;
        return;
    }

    // check status
    if (account->status == 0) {
        printf("Account is blocked\n");
        incorrectPasswordCount = 0;
        return;
    }

    // input password
    char password[MAX_LEN];
    printf("Password: ");
    scanf("%s", password);

    // check password
    if (strcmp(password, account->password) != 0) {
        incorrectPasswordCount++;
        if (incorrectPasswordCount > 3) {
            account->status = 0;
            rewriteAccountsToFile(FILE_NAME);
            incorrectPasswordCount = 0;

            printf("Password is incorrect. Account is blocked\n");
        } else {
            printf("Password is incorrect\n");
        }
        return;
    }

    // login successfully
    printf("Hello %s\n", username);
    incorrectPasswordCount = 0;
    isUserSignedIn = 1;
}


void handleSearch() {
    // check if user is signed in
    if (!isUserSignedIn) {
        printf("Account is not signed in\n");
        return;
    }

    // input username
    char username[MAX_LEN];
    printf("Username: ");
    scanf("%s", username);

    // find account
    Account *account = findAccount(username);
    if (account == NULL) {
        printf("Cannot find account\n");
        return;
    }

    printf("Account is %s\n", account->status == 1 ? "active" : "blocked");
}


void handleSignOut() {
    // input username
    char username[MAX_LEN];
    printf("Username: ");
    scanf("%s", username);

    // check if user is signed in
    if (!isUserSignedIn) {
        printf("Account is not signed in\n");
        return;
    }

    // find account
    Account *account = findAccount(username);
    if (account == NULL) {
        printf("Cannot find account\n");
        return;
    }

    printf("Goodbye %s\n", username);
    isUserSignedIn = 0;
}


void showMenu() {
    printf("\nUSER MANAGEMENT PROGRAM\n");
    printf("--------------------------------------\n");
    printf("1. Register\n2. Sign in\n3. Search\n4. Sign out\n");
    printf("Your choice (1-4), other to quit: ");

    int choice;
    scanf("%d", &choice);

    switch (choice) {
        case 1:
            handleRegister();
            break;
        case 2:
            handleSignIn();
            break;
        case 3:
            handleSearch();
            break;
        case 4:
            handleSignOut();
            break;
        default:
            printf("Bye!\n");
            return;
    }

    sleep(1);
    showMenu();
}


int main() {
    readAccountsFromFile(FILE_NAME);
    showMenu();
    return 0;
}

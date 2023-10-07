#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <regex.h>

// Function to retrieve the official and alias hostnames for a given IP address.
int get_hostname_by_ip(const char *ip_address) {
    struct in_addr addr;

    // Convert the IP address string to a binary format.
    if (inet_pton(AF_INET, ip_address, &addr) <= 0) {
        return 0;
    }

    // Perform reverse DNS lookup to get host information.
    struct hostent *host_entry = gethostbyaddr(&addr, sizeof(addr), AF_INET);

    if (host_entry == NULL) {
        return 0;
    }

    // Print the official hostname.
    printf("Official name: %s\n", host_entry->h_name);

    // Print alias hostnames, if any.
    if (*(host_entry->h_aliases) == NULL) return 1;
    printf("Alias name:\n");
    char **alias;
    for (alias = host_entry->h_aliases; *alias != NULL; alias++) {
        printf("%s\n", *alias);
    }

    return 1;
}

// Function to retrieve the IP addresses associated with a given hostname.
int get_ip_by_hostname(const char *hostname) {
    struct addrinfo hints, *result, *rp;
    int status;

    // Initialize hints for DNS resolution.
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_STREAM; // Use TCP

    // Perform DNS resolution for the given hostname.
    if ((status = getaddrinfo(hostname, NULL, &hints, &result)) != 0) {
        return 0;
    }

    int count = 0;

    // Iterate through the resolved IP addresses.
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        count++;
        struct sockaddr_in *ipv4 = (struct sockaddr_in *) rp->ai_addr;
        char ip_address[INET_ADDRSTRLEN];

        // Convert the binary IP address to a string format.
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip_address, INET_ADDRSTRLEN);

        if (count == 1) {
            printf("Official IP: ");
        }
        if (count == 2) {
            printf("Alias IP:\n");
        }
        printf("%s\n", ip_address);
    }

    // Free memory allocated for results.
    freeaddrinfo(result);
    return 1;
}

// Function to check if a given string matches a regular expression pattern.
int reg_check(const char *str, const char *pattern) {
    regex_t regex;
    int res;

    // Compile the regular expression.
    res = regcomp(&regex, pattern, REG_EXTENDED);

    // If compilation of the regex pattern failed, return 0
    if (res) {
        return 0;
    }

    // Execute the regular expression matching against the input string.
    res = regexec(&regex, str, 0, NULL, 0);

    // Free the regex resources.
    regfree(&regex);

    return res == 0; // Return 1 if there is a match, 0 otherwise.
}

int main(int argc, char *argv[]) {
    // Check the number of command-line arguments.
    if (argc != 2) {
        printf("Wrong number of arguments!\nCorrect usage: %s <Hostname/IP Address>\n", argv[0]);
        return 1; // Exit with an error code.
    }

    // Regular expressions for matching IPv4 addresses and hostnames.
    const char *ipv4_pattern = "^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
                               "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
                               "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
                               "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$";
    const char *hostname_pattern = "^[A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)+$";

    int res = 0;

    // Check if the command-line argument matches the IPv4 or hostname pattern.
    if (reg_check(argv[1], ipv4_pattern)) {
        res = get_hostname_by_ip(argv[1]);
    } else if (reg_check(argv[1], hostname_pattern)) {
        res = get_ip_by_hostname(argv[1]);
    }

    // Display an error message if no information was found.
    if (!res) {
        printf("No information found\n");
    }

    return 0; // Exit with a success code.
}

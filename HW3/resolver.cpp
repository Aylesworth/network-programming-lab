#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <regex.h>
#include <curl/curl.h>
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"

#define MAX_LEN 10000


// Structure to hold a string and its size
struct string_struct {
    char *value;
    size_t size;
};

// Callback function for comparing strings
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **) a, *(const char **) b);
}

// Function to sort and save an array of strings to a CSV file
void process_data(char **strings, int num_strings, const char *file_name) {
    FILE *f = fopen(file_name, "w");
    if (f == NULL) {
        printf("Error occurred while opening file.");
        return;
    }

    qsort(strings, num_strings, sizeof(strings[0]), compare_strings);

    for (int i = 0; i < num_strings; i++) {
        fprintf(f, "\"%s\"\n", strings[i]);
    }

    fclose(f);
}

// Function to extract links, text, and video sources from HTML content
void extract_data(struct string_struct html) {
    char *links[MAX_LEN];
    int num_links = 0;
    char *texts[MAX_LEN];
    int num_texts = 0;
    char *videos[MAX_LEN];
    int num_videos = 0;

    // Parse the HTML content
    htmlDocPtr doc = htmlReadMemory(html.value, static_cast<int>(html.size), NULL, NULL, HTML_PARSE_NOERROR);

    // Create an XPath context
    xmlXPathContextPtr context = xmlXPathNewContext(doc);

    // Extract links (href attributes of <a> elements)
    xmlXPathObjectPtr a_elements = xmlXPathEvalExpression((xmlChar *) "//a[@href]", context);

    for (int i = 0; i < a_elements->nodesetval->nodeNr; i++) {
        xmlNodePtr a_element = a_elements->nodesetval->nodeTab[i];
        links[num_links] = reinterpret_cast<char *>(xmlGetProp(a_element, (xmlChar *) "href"));
        num_links++;
    }

    // Extract text content (excluding <style> and <script> elements)
    xmlXPathObjectPtr text_elements = xmlXPathEvalExpression(
            (xmlChar *) "//*[not(self::style or self::script)]/text()[normalize-space()]", context);

    for (int i = 0; i < text_elements->nodesetval->nodeNr; i++) {
        xmlNodePtr text_element = text_elements->nodesetval->nodeTab[i];
        texts[num_texts] = reinterpret_cast<char *>(xmlNodeGetContent(text_element));
        num_texts++;
    }

    // Extract video sources (src attributes of <video> elements)
    xmlXPathObjectPtr video_elements = xmlXPathEvalExpression((xmlChar *) "//video[@src]", context);

    for (int i = 0; i < video_elements->nodesetval->nodeNr; i++) {
        xmlNodePtr video_element = video_elements->nodesetval->nodeTab[i];
        videos[num_videos] = reinterpret_cast<char *>(xmlGetProp(video_element, (xmlChar *) "src"));
        num_videos++;
    }

    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);

    // Process and save the extracted data to CSV files
    process_data(links, num_links, "links.csv");
    process_data(texts, num_texts, "texts.csv");
    process_data(videos, num_videos, "videos.csv");

    printf("\nData saved to files.\n");
    printf("Number of links: %d\n", num_links);
    printf("Number of texts: %d\n", num_texts);
    printf("Number of videos: %d\n", num_videos);
}

// Callback function to handle the received data
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    struct string_struct *mem = (struct string_struct *) userp;

    mem->value = (char *) realloc(mem->value, mem->size + real_size + 1);
    if (mem->value == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }

    memcpy(&(mem->value[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->value[mem->size] = 0;

    return real_size;
}

// Function to fetch and process HTML content from a URL
void process_html_content(const char *url) {
    char *full_url = (char *) malloc(strlen(url) + 8);
    if (full_url == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return;
    }

    strcpy(full_url, "https://");
    strcat(full_url, url);

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    if (curl) {
        struct string_struct html;
        html.value = (char *) malloc(1);
        html.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, full_url);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &html);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "Error while trying to fetch data: %s\n", curl_easy_strerror(res));
        } else {
            extract_data(html);
        }

        free(html.value);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}

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

    FILE *f = fopen("results.csv", "w");

    // Print the official hostname.
    printf("Official name: %s\n", host_entry->h_name);
    fprintf(f, "%s\n", host_entry->h_name);

    // Print alias hostnames, if any.
    if (*(host_entry->h_aliases) == NULL) return 1;
    printf("Alias name:\n");
    char **alias;
    for (alias = host_entry->h_aliases; *alias != NULL; alias++) {
        printf("%s\n", *alias);
        fprintf(f, "%s\n", *alias);
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

    FILE *f = fopen("results.csv", "w");

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
        fprintf(f, "%s\n", ip_address);
    }

    // Free memory allocated for results.
    freeaddrinfo(result);
    fclose(f);
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

    process_html_content(argv[1]);

    return 0; // Exit with a success code.
}

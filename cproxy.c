#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_BUF_SIZE 1024

void parse_url(const char *url, char *hostname, int *port, char *filepath) {
    char temp[MAX_BUF_SIZE];
    int default_port = 80;

//    // Check if the URL starts with "http://"
//    if (strncmp(url, "http://", 7) != 0) {
//        fprintf(stderr, "Error: URL must start with 'http://'\n");
//        exit(EXIT_FAILURE);
//    }

    // Remove "http://" from the URL
    strcpy(temp, url + 7);

    // Extract hostname
    char *port_separator = strchr(temp, ':');
    char *path_separator = strchr(temp, '/');
    if (port_separator != NULL) {
        // Extract hostname until the port separator
        strncpy(hostname, temp, port_separator - temp);
        hostname[port_separator - temp] = '\0';

        // Extract port from the substring after the port separator
        *port = atoi(port_separator + 1);
    } else {
        if (path_separator != NULL) {
            // Extract hostname until the path separator
            strncpy(hostname, temp, path_separator - temp);
            hostname[path_separator - temp] = '\0';
        } else {
            // The URL only contains the hostname
            strcpy(hostname, temp);
        }
        // Use the default port
        *port = default_port;
    }

    // Extract filepath
    if (path_separator != NULL) {
        strcpy(filepath, path_separator);
    } else {
        // No filepath specified, use "/"
        strcpy(filepath, "/");
    }
}
// Function to check if the file exists locally
int file_exists_locally(const char *hostname, const char *filepath) {
    char fullpath[MAX_BUF_SIZE];
    sprintf(fullpath, "./%s/%s", hostname, filepath);
    struct stat buffer;
    return (stat(fullpath, &buffer) == 0);
}

// Function to construct HTTP request based on URL
char *construct_request(const char *hostname, const char *filepath) {
//    char hostname[MAX_BUF_SIZE];
//    char filepath[MAX_BUF_SIZE];
//    int port;
//
//    // Parse the URL
//    parse_url(url, hostname, &port, filepath);
    char *request = (char *)malloc(MAX_BUF_SIZE);
    if (request == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }
//    if (strcmp(filepath, "/") == 0) {
//        strcpy(filepath, "/index.html");
//    }
    //sprintf(request, "GET %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", filepath, hostname, port);
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", filepath, hostname);
    return request;
}

// Function to create directories recursively
void create_directories(const char *path) {
    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");
    char dir[MAX_BUF_SIZE] = "";
    while (token != NULL) {
        strcat(dir, token);
        strcat(dir, "/");
        mkdir(dir, 0777);
        token = strtok(NULL, "/");
    }
    free(path_copy);
}

// Function to save file locally
void save_file_locally(const char *directory, const char *filename, char *response, int size) {
    char filepath[MAX_BUF_SIZE];
     //sprintf(filepath, "%s/%s", directory, filename);
   snprintf(filepath, sizeof(filepath), "%s/%s", directory, filename);

    FILE *file = fopen(filepath, "ab");
    if (file == NULL) {
        perror("Error opening file for writing");
        exit(1);
    }
    fwrite(response, 1, size, file);
    fclose(file);
}
void open_file(const char *hostname, const char *filepath){

    // Construct full path
    char fullpath[MAX_BUF_SIZE];
    sprintf(fullpath, "./%s/%s", hostname, filepath);

    // Construct HTTP response with the requested file
    FILE *file = fopen(fullpath, "rb");
    if (file == NULL) {
        perror("Error opening file for reading");
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Print the new HTTP response
    printf("HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n",file_size);


    if(file_size!=0) {
        // Read and print the file content in chunks
        char buffer[MAX_BUF_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            fwrite(buffer, 1, bytes_read, stdout);
        }
    }
    fclose(file);

//    printf("\nTotal response: %ld\n",content_size + strlen("HTTP/1.0 200 OK\r\nContent-Length: \r\n\r\n"));
    printf("\nTotal response: %ld\n",file_size + strlen("HTTP/1.0 200 OK\r\nContent-Length: \r\n\r\n"));
}

// Function to send HTTP request and receive response
void send_receive_request(const char *request, const char *hostname, const char *filepath, int port) {
    char response[MAX_BUF_SIZE];

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        herror("Failed to resolve hostname");
        exit(EXIT_FAILURE);
    }


    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    int bytes_sent = send(sockfd, request, strlen(request), 0);
    if (bytes_sent < 0) {
        perror("Error sending request");
        exit(EXIT_FAILURE);
    }

    int bytes_received;
    int flag=0;
    int not_continue=0;
    char local_directory[MAX_BUF_SIZE];

    while ((bytes_received = recv(sockfd, response , MAX_BUF_SIZE , 0)) > 0  && not_continue==0) {
      //  printf("%s\n",response);

        if(flag==0) {
            // Check if the HTTP response is 200 OK
            if (strstr(response, "HTTP/1.1 200 OK") == NULL && strstr(response, "HTTP/1.0 200 OK") == NULL) {
                // Print the first response line
                char *first_line_end = strchr(response, '\n');
                if (first_line_end != NULL) {
                    *first_line_end = '\0'; // Null terminate to print only the first line
                    printf("%s\n", response);
                }
                not_continue=1;
                break;
            }
            //keep the file locally
            // sprintf(local_directory, "./%s", hostname);
            snprintf(local_directory, sizeof(local_directory), "./%s", hostname);
            create_directories(local_directory);

            char *body_start = strstr(response, "\r\n\r\n");
            if (body_start != NULL) { //if there is body
                save_file_locally(local_directory, filepath, body_start + 4,
                                  MAX_BUF_SIZE - (body_start - response) - 4);
                flag = 1;
            } else{//if there is no body
                printf("\nThere is no body\n");
                save_file_locally(local_directory, filepath, NULL, 0);
                break;
            }

        }
        else{
            save_file_locally(local_directory, filepath, response, bytes_received);
        }
    }


    if (bytes_received < 0) {
        perror("Error receiving response");
        exit(EXIT_FAILURE);
    }

    if(not_continue!=1)
    open_file(hostname,filepath);


    close(sockfd);
}
int main(int argc, char *argv[]) {


    const char *url = argv[1];
    const char *flag = (argc == 3) ? argv[2] : NULL;

    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "-s") != 0) || (strncmp(url, "http://", 7) != 0)) {
        fprintf(stdout, "Usage: cproxy <URL> [-s]\n");
        //fprintf(stderr, "Usage: %s <URL> [-s]\n\n", argv[0]);
        return EXIT_FAILURE;
    }

    char hostname[MAX_BUF_SIZE];
    char filepath[MAX_BUF_SIZE];
    int port;

    parse_url(url, hostname, &port, filepath);


    if (strcmp(filepath, "/") == 0) {
        strcpy(filepath, "/index.html");
    }


    if (file_exists_locally(hostname,filepath)) {
        // File exists locally
        printf("File is given from local filesystem\n");
        open_file(hostname,filepath);

    } else {

        char *request = construct_request(hostname, filepath);
        printf("HTTP request =\n%s\nLEN = %d\n", request, (int)strlen(request));

        printf("Sending HTTP request...\n");
        send_receive_request(request, hostname, filepath, port);

        free(request);
    }


    if (flag && strcmp(flag, "-s") == 0) {

        printf("%s\n",hostname);
        printf("%d\n",port);
        printf("%s\n",filepath);

        printf("\nOpening browser to present the page...\n");
        char path[5000];
        //sprintf(path, "xdg-open %s", hostname);
        sprintf(path, "xdg-open %s/%s", hostname, filepath);
        //snprintf(path,MAX_BUF_SIZE,"xdg-open %s",temp);
        // snprintf(path,MAX_BUF_SIZE,"xdg-open %s",url);
        // Open browser to present the file
        if (system(path) == -1) {
            perror("Error opening browser");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

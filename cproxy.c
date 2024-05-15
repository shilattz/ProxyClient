#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#define MAX_BUF_SIZE 1024
//the update with the content
void parse_url(const char *url, char *hostname, int *port, char *filepath) {
    char temp[MAX_BUF_SIZE];
    int default_port = 80;

    // Remove "http://" from the URL
    strcpy(temp, url + 7);

    // Extract hostname
    char *port_separator = strchr(temp, ':');
    char *path_separator = strchr(temp, '/');
    if (port_separator != NULL) {
        // Extract hostname until the port separator
        strncpy(hostname, temp, port_separator - temp);
        hostname[port_separator - temp] = '\0';

        // Check if the port that insert is a number
        char *port_str = port_separator + 1;
        while (*port_str != '\0' && *port_str != '/') {
            if (!isdigit(*port_str)) {
                fprintf(stderr, "Invalid port number specified in the URL.\n");
                exit(EXIT_FAILURE);
            }
            port_str++;
        }

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
        // Check for consecutive slashes at the end of the URL
        const char *last_slash = strrchr(path_separator, '/');
        if (last_slash != NULL && *(last_slash + 1) == '\0') {
            // URL ends with slashes, set filepath to "/"
            strcpy(filepath, "/");
        } else {
            strcpy(filepath, path_separator);
        }
    } else {
        // No filepath specified, use "/"
        strcpy(filepath, "/");
    }
}

//Function to get directories path
char* get_directory_path(const char *hostname, const char *filepath){
    // Calculate the length of the concatenated string
    size_t length = strlen(hostname) + strlen(filepath) + 1; // +1 for the null terminator

    char concatenated[length];
    strcpy(concatenated, hostname);

    strcat(concatenated, filepath);

    // Find the last occurrence of '/'
    char *last_slash = strrchr(concatenated, '/');

    // Calculate the length of the new string until the last '/'
    size_t new_length = last_slash - concatenated + 1; // +1 to include the last '/'

    char *directory_path = (char *)malloc(new_length + 1); // +1 for null terminator
    if (directory_path == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }

    // Copy the substring until the last '/' to the new string
    strncpy(directory_path, concatenated, new_length - 1); // -1 to exclude the last '/'
    directory_path[new_length - 1] = '\0'; // Null-terminate the string

    // Print the new string
   // printf("New String: %s\n", directory_path);

    return directory_path;
}

// Function to check if the file exists locally
int file_exists_locally(const char *hostname, const char *filepath) {
    char fullpath[MAX_BUF_SIZE];
    sprintf(fullpath, "./%s/%s", hostname, filepath);
    struct stat buffer;
    return (stat(fullpath, &buffer) == 0);
}

// Function to construct HTTP request based on URL
char *construct_request(const char *hostname,  char *filepath ,int path_flag) {

    char *request = (char *)malloc(MAX_BUF_SIZE);
    if (request == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }

    if (strcmp(filepath, "/index.html") == 0 && path_flag==1) {
        strcpy(filepath, "/");
        sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", filepath, hostname);
        strcpy(filepath, "/index.html");
    }
    else{
        sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", filepath, hostname);
    }

    return request;
}

void create_directories(const char *path) {
    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");
    char dir[1024] = "";

    // Skip the first token if it's empty (indicating the current directory)
    if (token != NULL && strcmp(token, "") == 0) {
        token = strtok(NULL, "/");
    }

    while (token != NULL) {
        strcat(dir, token);
        strcat(dir, "/");
        mkdir(dir, 0777); // Create the directory
        token = strtok(NULL, "/");
    }
    free(path_copy);
}



// Function to save file locally
void save_file_locally(const char *directory, const char *filename, char *response, int size) {
    char filepath[MAX_BUF_SIZE];
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

    printf("\n Total response bytes: %ld\n",file_size + strlen("HTTP/1.0 200 OK\r\nContent-Length: \r\n\r\n"));
}

// Function to send HTTP request and receive response
void send_receive_request( char *request, const char *hostname, const char *filepath, int port) {

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



    char* directory_path = get_directory_path(hostname, filepath);

    char local_directory[5000];

    while ((bytes_received = recv(sockfd, response , MAX_BUF_SIZE , 0)) > 0  ) {
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
                //not_continue=1;
                free(directory_path);
                close(sockfd);
                free(request);
                exit(0);
            }
            //keep the file locally
            snprintf(local_directory, sizeof(local_directory), "./%s", directory_path);
            create_directories(local_directory);

            char *body_start = strstr(response, "\r\n\r\n");
            if (body_start != NULL) { //if there is body
                save_file_locally(hostname, filepath, body_start + 4,
                                  MAX_BUF_SIZE - (body_start - response) - 4);
                flag = 1;
            } else{//if there is no body
               // printf("\nThere is no body\n");
                save_file_locally(hostname, filepath, NULL, 0);
                break;
            }

        }
        else{
            save_file_locally(hostname, filepath, response, bytes_received);
        }
    }

    if (bytes_received < 0) {
        perror("Error receiving response");
        exit(EXIT_FAILURE);
    }

    //if(not_continue!=1)
    open_file(hostname,filepath);

    free(directory_path);
    close(sockfd);
}

int main(int argc, char *argv[]) {


    const char *url = argv[1];
    const char *flag = (argc == 3) ? argv[2] : NULL;

    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "-s") != 0) || (strncmp(url, "http://", 7) != 0)) {
        perror( "Usage: cproxy <URL> [-s]\n");
        return EXIT_FAILURE;
    }

    char hostname[MAX_BUF_SIZE];
    char filepath[MAX_BUF_SIZE];
    int port;

    parse_url(url, hostname, &port, filepath);
    int path_flag=0;

    if (strcmp(filepath, "/") == 0) {
        strcpy(filepath, "/index.html");
        path_flag=1;
    }
//    printf("%s\n",hostname);
//    printf("%d\n",port);
//    printf("%s\n",filepath);
    if (file_exists_locally(hostname,filepath)) {
        // File exists locally
        printf("File is given from local filesystem\n");
        open_file(hostname,filepath);

    } else {


        char *request = construct_request(hostname, filepath,path_flag);
        printf("HTTP request =\n%s\nLEN = %d\n", request, (int)strlen(request));

       // printf("Sending HTTP request...\n");
        send_receive_request(request, hostname, filepath, port);
        // send_receive_request(request, hostname, filepath, port,url);
        free(request);
    }

    if (flag && strcmp(flag, "-s") == 0) {
//        printf("%s\n",hostname);
//        printf("%d\n",port);
//        printf("%s\n",filepath);

       // printf("\nOpening browser to present the page...\n");
        char path[5000];
        sprintf(path, "xdg-open %s/%s", hostname, filepath);
        // Open browser to present the file
        if (system(path) == -1) {
            perror("Error opening browser");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}


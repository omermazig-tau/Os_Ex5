#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

const int MB_IN_BYTES = 1048576;
const int RESERVED_TCP_PORTS = 1024;

int open_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(1);
    }
    return sockfd;
}

struct sockaddr_in build_sockaddr_in(unsigned int port_number, char *ip) {
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_number);
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    return server_address;
}

int connect_to_server(unsigned int port_number, char *ip) {
    int sockfd = open_socket();
    // Build server address struct
    struct sockaddr_in server_address = build_sockaddr_in(port_number, ip);
    // Connect to the server
    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("Error connecting to server");
        exit(1);
    }
    return sockfd;
}

void write_chars_to_socket(char *characters, size_t chars_to_write, int sockfd) {
    // This is based on this SO answer - https://stackoverflow.com/a/9142150/2899096
    size_t left = chars_to_write;
    ssize_t temp_bytes_written;
    do {
        temp_bytes_written = write(sockfd, characters, left);
        if (temp_bytes_written < 0) {
            perror("Error writing to socket");
            exit(1);
        }
        characters += temp_bytes_written;
        left -= temp_bytes_written;
    } while (left > 0);
}

void read_chars_from_socket(char *characters, size_t chars_to_read, int sockfd) {
    // IMPORTANT - ASSUMES THAT `*characters` IS ALREADY ALLOCATED!!
    // This is based on this SO answer - https://stackoverflow.com/a/9142150/2899096
    size_t left = chars_to_read;
    ssize_t temp_bytes_read;
    do {
        temp_bytes_read = read(sockfd, characters, left);
        if (temp_bytes_read < 0) {
            perror("Error reading from socket");
            exit(1);
        }
        characters += temp_bytes_read;
        left -= temp_bytes_read;
    } while (left > 0);
}

void write_number_to_socket(uint32_t number, int sockfd) {
    uint32_t file_size_for_transfer = htonl(number);

    char *file_size_buffer_for_transfer = (char *) &file_size_for_transfer;
    write_chars_to_socket(file_size_buffer_for_transfer, sizeof(number), sockfd);
}

uint32_t read_number_from_socket(int sockfd) {
    uint32_t number_for_transfer;
    char *file_size_buffer_for_transfer = (char *) &number_for_transfer;
    read_chars_from_socket(file_size_buffer_for_transfer, sizeof(uint32_t), sockfd);
    uint32_t file_size = ntohl(number_for_transfer);
    return file_size;
}

uint32_t get_file_size(char *file_path) {
    struct stat st;
    stat(file_path, &st);
    return st.st_size;
}

int main(int argc, char *argv[]) {
    FILE *file;
    int batch_size = MB_IN_BYTES; // number of bytes to read per iteration
    char buffer[batch_size];
    size_t bytes_read;

    if (argc < 4) {
        fprintf(stderr, "usage %s ip port path_to_file\n", argv[0]);
        exit(1);
    }
    char *ip = argv[1];
    uint16_t port_number = atoi(argv[2]);
    if (port_number < RESERVED_TCP_PORTS) {
        fprintf(stderr, "Invalid port number %d\n", port_number);
        exit(1);
    }
    char *file_path = argv[3];
    file = fopen(file_path, "r");

    // check if file exists
    if (file == NULL) {
        fprintf(stderr, "Error opening file %s\n", file_path);
        exit(1);
    }

    // Connect to server and get the socket file descriptor
    int sockfd = connect_to_server(port_number, ip);

    // get the file size, convert it, and send it
    uint32_t file_size = get_file_size(file_path);
    write_number_to_socket(file_size, sockfd);

    // iterate over file and send batch_size bytes at a time
    while ((bytes_read = fread(buffer, 1, batch_size, file)) > 0) {
        // write the bytes read into the socket
        write_chars_to_socket(buffer, bytes_read, sockfd);
    }

    uint32_t C = read_number_from_socket(sockfd);
    // print the number of printable characters
    printf("# of printable characters: %u\n", C);

    fclose(file);
    close(sockfd);
    return 0;
}

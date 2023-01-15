#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <memory.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

const int MB_IN_BYTES = 1048576;
const int RESERVED_TCP_PORTS = 1024;
uint32_t pcc_total[126 + 1] = {0};
bool is_sig_int = false;

bool is_printable_character(char character) {
    return 32 <= character && character <= 126;
}

void update_pcc_total(const uint32_t temp_pcc_total[]) {
    for (int i = 32; i <= 126; i++) {
        pcc_total[i] += temp_pcc_total[i];
    }
}

void print_pcc_total() {
    // Print the statistics for each printable character
    for (int i = 32; i <= 126; i++) {
        printf("char '%c' : %u times\n", i, pcc_total[i]);
    }
}

void sig_int_handler() {
    //TODO - Add logic here
    is_sig_int = true;
}

void set_sig_int() {
    // This is copied from MY second assignment
    struct sigaction sig_int;
    memset(&sig_int, 0, sizeof(sig_int));
    sig_int.sa_handler = &sig_int_handler;
    sig_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sig_int, 0) < 0) {
        perror("Couldn't set signal for SIG_INT");
        exit(1);
    }
}

int open_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(1);
    }
    return sockfd;
}

void set_socket_reusable(int sockfd) {
    int one = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        perror("Error setting socket to be reusable");
        exit(1);
    }
}

struct sockaddr_in build_sockaddr_in(unsigned int port_number) {
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_number);
    // Accept connections on all network interface
    // TODO - htonl necessary?
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    return server_address;
}

void bind_socket(int sockfd, struct sockaddr_in server_address) {
    if (bind(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("Error binding socket");
        exit(1);
    }
}

void listen_to_connections(int sockfd) {
    if (listen(sockfd, 10) < 0) {
        perror("Error listening to socket");
        exit(1);
    }
}

int accept_connection(int sockfd) {
    int client_fd = accept(sockfd, NULL, NULL);
    if (client_fd < 0) {
        perror("Error accepting connection");
        exit(1);
    }
    return client_fd;
}

int connect_server(unsigned int port_number) {
    int sockfd = open_socket();
    set_socket_reusable(sockfd);
    struct sockaddr_in server_address = build_sockaddr_in(port_number);
    bind_socket(sockfd, server_address);
    listen_to_connections(sockfd);
    return sockfd;
}

size_t write_chars_to_socket(char *characters, size_t chars_to_write, int sockfd) {
    // This is based on this SO answer - https://stackoverflow.com/a/9142150/2899096
    size_t left = chars_to_write;
    ssize_t temp_bytes_written;
    do {
        temp_bytes_written = write(sockfd, characters, left);
        if (temp_bytes_written <= 0) {
            perror("Error writing to socket");
            return left;
        }
        characters += temp_bytes_written;
        left -= temp_bytes_written;
    } while (left > 0);
    return left;
}

size_t read_chars_from_socket(char *characters, size_t chars_to_read, int sockfd) {
    // IMPORTANT - ASSUMES THAT `*characters` IS ALREADY ALLOCATED!!
    // This is based on this SO answer - https://stackoverflow.com/a/9142150/2899096
    size_t left = chars_to_read;
    ssize_t temp_bytes_read;
    do {
        temp_bytes_read = read(sockfd, characters, left);
        if (temp_bytes_read <= 0) {
            perror("Error reading from socket");
            return left;
        }
        characters += temp_bytes_read;
        left -= temp_bytes_read;
    } while (left > 0);
    return left;
}

uint32_t write_number_to_socket(uint32_t number, int sockfd) {
    uint32_t file_size_for_transfer = htonl(number);

    char *file_size_buffer_for_transfer = (char *) &file_size_for_transfer;
    size_t bytes_left = write_chars_to_socket(file_size_buffer_for_transfer, sizeof(number), sockfd);
    return bytes_left;
}

uint32_t read_number_from_socket(uint32_t *number, int sockfd) {
    uint32_t number_for_transfer;
    char *file_size_buffer_for_transfer = (char *) &number_for_transfer;
    size_t bytes_left = read_chars_from_socket(file_size_buffer_for_transfer, sizeof(uint32_t), sockfd);
    *number = ntohl(number_for_transfer);
    return bytes_left;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage %s port\n", argv[0]);
        exit(1);
    }
    uint16_t port_number = atoi(argv[1]);
    if (port_number < RESERVED_TCP_PORTS) {
        fprintf(stderr, "Invalid port number %d\n", port_number);
        exit(1);
    }
    int sockfd = connect_server(port_number);
    //TODO - right place for this?
    set_sig_int();

    int batch_size = MB_IN_BYTES; // number of bytes to read per iteration
    char buffer[batch_size];
    while (!is_sig_int) {
        int client_fd = accept_connection(sockfd);
        uint32_t left;
        // Read the file size from the client
        uint32_t file_size;
        left = read_number_from_socket(&file_size, client_fd);

        unsigned int pcc = 0;
        uint32_t temp_pcc_total[126 + 1] = {0};
        size_t bytes_read = 0;
        // Read data from the client and count printable characters
        do {
            size_t bytes_to_read = batch_size < file_size - bytes_read ? batch_size : file_size - bytes_read;
            left = read_chars_from_socket(buffer, bytes_to_read, client_fd);
            bytes_read += bytes_to_read;

            for (int i = 0; i < bytes_to_read; ++i) {
                if (is_printable_character(buffer[i])) {
                    pcc++;
                    temp_pcc_total[(int)(buffer[i])]++;
                }
            }
        } while (bytes_read < file_size);
        // Write the result back to the client
        left = write_number_to_socket(pcc, client_fd);
        // Update pcc_total
        update_pcc_total(temp_pcc_total);
        close(client_fd);
    }
    print_pcc_total();
}

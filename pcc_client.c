#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

const int MB_IN_BYTES = 1048576;
const int RESERVED_TCP_PORTS = 1024;

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
        fprintf(stderr, "usage %s hostname port file\n", argv[0]);
        exit(1);
    }
    char *ip = argv[1];
    unsigned int port_number = atoi(argv[2]);
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

    // get the file size, convert it, and send it
    uint32_t file_size = get_file_size(file_path);
    uint32_t file_size_for_transfer = htonl(file_size);

    char file_size_buffer_for_transfer[batch_size];
    sprintf(file_size_buffer_for_transfer, "%d", file_size_for_transfer);
    write(1, file_size_buffer_for_transfer, strlen(file_size_buffer_for_transfer));

    // iterate over file and send batch_size bytes at a time
    while ((bytes_read = fread(buffer, 1, batch_size, file)) > 0) {
        // print the bytes read
        //TODO - Write to a socket instead
        write(1, buffer, bytes_read);
    }

    // print the number of printable characters
    printf("# of printable characters: %u\n", file_size);

    fclose(file);
    return 0;
}

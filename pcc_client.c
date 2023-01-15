#include <stdio.h>
#include <stdlib.h>

const int MB_IN_BYTES = 1048576;
const int RESERVED_TCP_PORTS = 1024;

int main(int argc, char *argv[]) {
    FILE *file;
    int batch_size = MB_IN_BYTES; // number of bytes to read per iteration
    char buffer[batch_size];
    size_t bytes_read;

    if (argc < 4) {
        fprintf(stderr,"usage %s hostname port file\n", argv[0]);
        exit(1);
    }
    char *ip = argv[1];
    unsigned int port_number = atoi(argv[2]);
    if (port_number < RESERVED_TCP_PORTS) {
        fprintf(stderr,"Invalid port number %d\n", port_number);
        exit(1);
    }
    char *file_path = argv[3];

    file = fopen(file_path, "r");

    // check if file exists
    if (file == NULL) {
        fprintf(stderr,"Error opening file %s\n", file_path);
        exit(1);
    }

    // iterate over file and write batch_size bytes at a time
    while ((bytes_read = fread(buffer, 1, batch_size, file)) > 0) {
        // print the bytes read
        //TODO - Write to a socket instead
        fwrite(buffer, 1, bytes_read, stdout);
    }

    fclose(file);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 516
#define MESSAGE_SIZE 512
#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DATA 3
#define OPCODE_ACK 4
#define OPCODE_ERROR 5

void handle_rrq(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, char *filename);
void handle_wrq(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, char *filename);
void send_error(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, int error_code, char *error_msg);
void send_ack(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, int block_number);

int main() {
    int server_fd;
    struct sockaddr_in address, client_address;
    int client_addrlen = sizeof(client_address);
    char buffer[BUFFER_SIZE] = {0};
    int opt = 1;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;         // dir IP
    address.sin_addr.s_addr = INADDR_ANY; // cuando hace el bind lo hace en todas las interfaces de la pc
    address.sin_port = htons(PORT);       // podría ir 0 y es un puerto al azar (usualmente en el cliente)

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, (socklen_t *)&client_addrlen);
        if (valread == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }

        int opcode = ntohs(*(uint16_t *)buffer);

        if (opcode == OPCODE_RRQ) {
            char *filename = buffer + 2;
            printf("Received reading request: %s\n", filename);
            handle_rrq(server_fd, &client_address, client_addrlen, filename);
        } else if (opcode == OPCODE_WRQ) {
            char *filename = buffer + 2;
            printf("Received writing request: %s\n", filename);
            handle_wrq(server_fd, &client_address, client_addrlen, filename);
        } else {
            send_error(server_fd, &client_address, client_addrlen, 4, "Illegal TFTP operation.");
        }
    }

    // Cerrar el socket después de salir del bucle
    close(server_fd);

    return 0;
}

void handle_rrq(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, char *filename) {
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "server_files/%s", filename);

    printf("Trying to open file: %s\n", filepath);
    int file_fd = open(filepath, O_RDONLY);

    if (file_fd < 0) {
        perror("open");
        send_error(sock, client_addr, client_addrlen, 1, "File not found.");
        return;
    }

    printf("File opened successfully: %s\n", filepath);
    char buffer[BUFFER_SIZE];
    char data_packet[BUFFER_SIZE];
    int block_number = 1;
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, buffer, MESSAGE_SIZE)) >= 0) {
        printf("Bytes read: %zd\n", bytes_read);  // Añadido para depuración

        if (bytes_read == 0) {
            // Llegó al final del archivo
            printf("End of file reached.\n");
            break;
        }

        memset(data_packet, 0, BUFFER_SIZE);
        *(uint16_t *)data_packet = htons(OPCODE_DATA);
        *(uint16_t *)(data_packet + 2) = htons(block_number);
        memcpy(data_packet + 4, buffer, bytes_read);

        sendto(sock, data_packet, bytes_read + 4, 0, (struct sockaddr *)client_addr, client_addrlen);
        printf("Sent block number: %d\n", block_number);  // Añadido para depuración
        block_number++;

        // Recibir ACK
        // recvfrom(sock, data_packet, BUFFER_SIZE, 0, (struct sockaddr *)client_addr, &client_addrlen);
        // int ack_opcode = ntohs(*(uint16_t *)data_packet);
        // int ack_block = ntohs(*(uint16_t *)(data_packet + 2));

        // if (ack_opcode != OPCODE_ACK || ack_block != block_number - 1) {
        //     send_error(sock, client_addr, client_addrlen, 0, "Unknown error.");
        //     close(file_fd);
        //     return;
        // }
    }

    printf("File send completed.\n");  // Añadido para depuración
    close(file_fd);
}

void handle_wrq(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, char *filename) {
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "server_files/%s", filename);

    printf("Trying to open file for writing: %s\n", filepath);
    int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (file_fd < 0) {
        perror("open");
        send_error(sock, client_addr, client_addrlen, 1, "Cannot create file.");
        return;
    }

    send_ack(sock, client_addr, client_addrlen, 0);

    char buffer[BUFFER_SIZE];
    int block_number = 1;
    ssize_t bytes_received;

    printf("Receiving data:");
    while (1) {
        bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)client_addr, &client_addrlen);
        if (bytes_received == -1) {
            perror("recvfrom");
            close(file_fd);
            return;
        }

        int opcode = ntohs(*(uint16_t *)buffer);
        int received_block_number = ntohs(*(uint16_t *)(buffer + 2));

        if (opcode == OPCODE_DATA && received_block_number == block_number) {
            write(file_fd, buffer + 4, bytes_received - 4);
            printf("...");

            send_ack(sock, client_addr, client_addrlen, block_number);

            block_number++;

            if (bytes_received < MESSAGE_SIZE + 4) {
                // Último paquete
                break;
            }
        } else if (opcode == OPCODE_ERROR) {
            printf("Error from client: %s\n", buffer + 4);
            close(file_fd);
            return;
        } else {
            printf("Protocol error: expected DATA block %d\n", block_number);
            close(file_fd);
            return;
        }
    }

    close(file_fd);
    printf("\nFile transfer completed\n");
}

void send_error(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, int error_code, char *error_msg) {
    char error_packet[BUFFER_SIZE];
    *(uint16_t *)error_packet = htons(OPCODE_ERROR);
    *(uint16_t *)(error_packet + 2) = htons(error_code);
    strcpy(error_packet + 4, error_msg);

    sendto(sock, error_packet, strlen(error_msg) + 4 + 1, 0, (struct sockaddr *)client_addr, client_addrlen);
}

void send_ack(int sock, struct sockaddr_in *client_addr, socklen_t client_addrlen, int block_number) {
    char ack[4];
    *(uint16_t *)ack = htons(OPCODE_ACK);
    *(uint16_t *)(ack + 2) = htons(block_number);

    sendto(sock, ack, 4, 0, (struct sockaddr *)client_addr, client_addrlen);
}

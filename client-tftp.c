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

void send_rrq(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename, const char *mode);
void send_wrq(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename, const char *mode);
void handle_data(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename);
void handle_ack(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename);

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <rrq/wrq> <filename>\n", argv[0]);
        return -1;
    }

    const char *operation = argv[1];
    const char *filename = argv[2];
    const char *mode = "octet";

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addr_len = sizeof(serv_addr);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (strcmp(operation, "rrq") == 0) {
        send_rrq(sock, &serv_addr, addr_len, filename, mode);
        handle_data(sock, &serv_addr, addr_len, filename);
    } else if (strcmp(operation, "wrq") == 0) {
        send_wrq(sock, &serv_addr, addr_len, filename, mode);
        handle_ack(sock, &serv_addr, addr_len, filename);
    } else {
        printf("Invalid operation. Use 'rrq' for read request or 'wrq' for write request.\n");
        return -1;
    }

    close(sock);
    return 0;
}

void send_rrq(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename, const char *mode) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    *(uint16_t *)buffer = htons(OPCODE_RRQ);
    strcpy(buffer + 2, filename);
    strcpy(buffer + 2 + strlen(filename) + 1, mode);

    sendto(sock, buffer, 2 + strlen(filename) + 1 + strlen(mode) + 1, 0, (struct sockaddr *)server_addr, server_addrlen);
}

void send_wrq(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename, const char *mode) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    *(uint16_t *)buffer = htons(OPCODE_WRQ);
    strcpy(buffer + 2, filename);
    strcpy(buffer + 2 + strlen(filename) + 1, mode);

    sendto(sock, buffer, 2 + strlen(filename) + 1 + strlen(mode) + 1, 0, (struct sockaddr *)server_addr, server_addrlen);
}

void handle_data(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename) {
    char buffer[BUFFER_SIZE];
    char ack[4];
    int block_number = 1;
    ssize_t bytes_received;
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "client_files/%s", filename);

    printf("Trying to open file: %s\n", filepath);
    int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (file_fd < 0) {
        perror("open");
        return;
    }
    
    printf("Receiving data:");
    while (1) {
        bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)server_addr, &server_addrlen);
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
            *(uint16_t *)ack = htons(OPCODE_ACK);
            *(uint16_t *)(ack + 2) = htons(block_number);
            sendto(sock, ack, 4, 0, (struct sockaddr *)server_addr, server_addrlen);
            printf("Sent ACK for block number: %d\n", block_number); // Añadido para depuración

            block_number++;

            if (bytes_received < MESSAGE_SIZE + 4) {
                // Último paquete
                break;
            }
        } else if (opcode == OPCODE_ERROR) {
            printf("Error from server: %s\n", buffer + 4);
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

void handle_ack(int sock, struct sockaddr_in *server_addr, socklen_t server_addrlen, const char *filename) {
    char buffer[BUFFER_SIZE];
    char data_packet[BUFFER_SIZE];
    int block_number = 0;
    ssize_t bytes_read;
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "client_files/%s", filename);

    printf("Trying to open file: %s\n", filepath);
    int file_fd = open(filepath, O_RDONLY);

    if (file_fd < 0) {
        perror("open");
        return;
    }
    
    printf("Sending data:");
    while (1) {
        bytes_read = read(file_fd, buffer, MESSAGE_SIZE);
        if (bytes_read == -1) {
            perror("read");
            close(file_fd);
            return;
        }

        block_number++;
        memset(data_packet, 0, BUFFER_SIZE);
        *(uint16_t *)data_packet = htons(OPCODE_DATA);
        *(uint16_t *)(data_packet + 2) = htons(block_number);
        memcpy(data_packet + 4, buffer, bytes_read);

        sendto(sock, data_packet, bytes_read + 4, 0, (struct sockaddr *)server_addr, server_addrlen);
        printf("Sent block number: %d\n", block_number);

        // Recibir ACK
        ssize_t ack_len = recvfrom(sock, data_packet, BUFFER_SIZE, 0, (struct sockaddr *)server_addr, &server_addrlen);
        if (ack_len == -1) {
            perror("recvfrom");
            close(file_fd);
            return;
        }

        int ack_opcode = ntohs(*(uint16_t *)data_packet);
        int ack_block_number = ntohs(*(uint16_t *)(data_packet + 2));

        if (ack_opcode != OPCODE_ACK || ack_block_number != block_number) {
            printf("Protocol error: expected ACK for block %d\n", block_number);
            close(file_fd);
            return;
        }

        printf("Received ACK for block number: %d\n", ack_block_number);

        if (bytes_read < MESSAGE_SIZE) {
            // Último paquete
            break;
        }
    }

    close(file_fd);
    printf("\nFile transfer completed\n");
}




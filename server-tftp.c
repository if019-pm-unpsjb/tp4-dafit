#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MESSAGE_SIZE 512
#define BUFFER_SIZE 512

int main()
{
    int server_fd;
    struct sockaddr_in address, client_address;
    int client_addrlen = sizeof(client_address);
    char buffer[BUFFER_SIZE] = {0};
    int opt = 1;
    short opcode;

    int mode;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;         // dir IP
    address.sin_addr.s_addr = INADDR_ANY; // cuando hace el bind lo hace en todas las interfaces de la pc
    address.sin_port = htons(PORT);       // podría ir 0 y es un puerto al azar (usualmente en el cliente)

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = recvfrom(server_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, (socklen_t *)&client_addrlen);
        if (valread == -1)
        {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }
        buffer[valread] = '\0'; // Asegurarse de que la cadena esté terminada en nulo
        printf("Received: %s\n", buffer);

        opcode = ntohs(buffer[0]);

        // sendto(server_fd, message, strlen(message), 0, (struct sockaddr *)&client_address, client_addrlen);
        // printf("Sent: %s\n", message);
    }

    // Cerrar el socket después de salir del bucle
    close(server_fd);

    return 0;
}
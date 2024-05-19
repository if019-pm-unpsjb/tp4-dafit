#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET; //dir IP
    address.sin_addr.s_addr = INADDR_ANY; //cuando hace el bind lo hace en todas las interfaces de la pc
    address.sin_port = htons( PORT ); //podría ir 0 y es un puerto al azar (usualmente en el cliente)

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) { //el 3 es cola de espera hasta 3
        perror("listen");
        exit(EXIT_FAILURE);
    }
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                       (socklen_t*)&addrlen))<0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // Mensaje de conexión establecida
    char *connection_message = "Connection established. Type 'disconnect' to close connection.\n";
    send(new_socket , connection_message , strlen(connection_message) , 0 );

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (valread == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf("Received: %s\n", buffer);
        
       
        send(new_socket, buffer, strlen(buffer), 0);
        printf("Sent: %s\n", buffer);
    }

    // Cerrar el socket después de salir del bucle
    close(new_socket);

    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char const *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addr_len = sizeof(serv_addr);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    

    while(1) {
        printf("Enter message: ");
        fgets(message, BUFFER_SIZE, stdin);
        
        // Eliminar el salto de línea del final de la cadena
        size_t len = strlen(message);
        if (len > 0 && message[len-1] == '\n') {
            message[--len] = '\0';
        }
        
        sendto(sock, message, strlen(message), 0, (struct sockaddr *)&serv_addr, addr_len);
        
        // Limpiar el buffer antes de recibir la respuesta del servidor
        memset(buffer, 0, BUFFER_SIZE);
        
        int valread = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serv_addr, &addr_len);
        if (valread == -1) {
            perror("recvfrom");
            return -1;
        }
        buffer[valread] = '\0'; 
        printf("Server replied: %s\n", buffer);
        
        break; // Salir del bucle para cerrar la conexión
        
    }

    // Cerrar el socket antes de salir del programa
    close(sock);

    return 0;
}



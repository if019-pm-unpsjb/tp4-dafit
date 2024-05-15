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

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    // Mensaje de conexión establecida
    int valread = read(sock, buffer, BUFFER_SIZE);
    if (valread == -1) {
        perror("read");
        return -1;
    }
    printf("Server replied: %s\n", buffer);

    while(1) {
        printf("Enter message: ");
        fgets(message, BUFFER_SIZE, stdin);
        
        // Eliminar el salto de línea del final de la cadena
        size_t len = strlen(message);
        if (len > 0 && message[len-1] == '\n') {
            message[--len] = '\0';
        }
        
        send(sock, message, strlen(message), 0);
        
        // Limpiar el buffer antes de recibir la respuesta del servidor
        memset(buffer, 0, BUFFER_SIZE);
        
        valread = read(sock, buffer, BUFFER_SIZE);
        if (valread == -1) {
            perror("read");
            return -1;
        }
        printf("Server replied: %s\n", buffer);
        
        // Verificar si el cliente quiere desconectar
        if (strcmp(message, "disconnect") == 0) {
            printf("Disconnecting from server...\n");
            break; // Salir del bucle para cerrar la conexión
        }
    }

    // Cerrar el socket antes de salir del programa
    close(sock);

    return 0;
}



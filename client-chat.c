#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char const *argv[])
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\nError al crear el socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nDirección no válida o no soportada \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nError de conexión \n");
        return -1;
    }

    while (1)
    {
        printf("Ingrese mensaje: ");
        fgets(message, BUFFER_SIZE, stdin);

        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n')
        {
            message[--len] = '\0';
        }

        send(sock, message, strlen(message), 0);

        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE);
        if (valread == -1)
        {
            perror("Error al leer del socket");
            return -1;
        }

        printf("Respuesta del servidor: %s\n", buffer);

        if (strcmp(message, "disconnect") == 0)
        {
            printf("Desconectando del servidor...\n");
            break;
        }
    }

    close(sock);
    return 0;
}

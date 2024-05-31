#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <IP> <puerto> <nombre>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int client_socket;
    struct sockaddr_in server_addr;
    char nombre[50];
    char buffer[BUFFER_SIZE];
    char mensaje[BUFFER_SIZE + 50]; // Buffer temporal para el mensaje completo

    // Crear socket del cliente
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket del cliente");
        exit(EXIT_FAILURE);
    }

    // Configurar la estructura del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]); // Dirección IP del servidor
    server_addr.sin_port = htons(atoi(argv[2])); // Puerto del servidor

    // Conectar al servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectar");
        exit(EXIT_FAILURE);
    }

    // Copiar el nombre del cliente desde los argumentos de la línea de comandos
    strncpy(nombre, argv[3], sizeof(nombre));
    nombre[sizeof(nombre) - 1] = '\0';

    printf("Conectado al servidor. Bienvenido, %s\n", nombre);

    // Bucle para enviar mensajes al servidor
    while (1) {
        printf("Ingrese el mensaje (o 'exit' para salir): ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Eliminar el carácter de nueva línea

        // Salir si el usuario ingresa 'exit'
        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        // Construir el mensaje completo (nombre: mensaje)
        snprintf(mensaje, sizeof(mensaje), "%s: %s", nombre, buffer);

        // Enviar mensaje al servidor
        if (send(client_socket, mensaje, strlen(mensaje), 0) != strlen(mensaje)) {
            perror("Error al enviar el mensaje");
            exit(EXIT_FAILURE);
        }
    }

    // Cerrar el socket del cliente
    close(client_socket);

    return 0;
}


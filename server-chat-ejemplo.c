#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Estructura para almacenar la información de los clientes
typedef struct {
    int socket;
    char nombre[50];
} Cliente;

int main() {
    int server_socket, client_sockets[MAX_CLIENTS], max_clients = MAX_CLIENTS, activity, max_sd, sd, i, valread;
    Cliente clientes[MAX_CLIENTS];
    fd_set readfds;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr;

    // Crear socket del servidor
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket del servidor");
        exit(EXIT_FAILURE);
    }

    // Configurar la estructura del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    // Enlazar el socket con la dirección y el puerto
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en el enlace");
        exit(EXIT_FAILURE);
    }

    // Escuchar las conexiones entrantes
    if (listen(server_socket, 3) < 0) {
        perror("Error al escuchar");
        exit(EXIT_FAILURE);
    }

    printf("Esperando conexiones...\n");

    // Inicializar clientes
    for (i = 0; i < max_clients; i++) {
        client_sockets[i] = 0;
    }

    // Bucle principal
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        // Añadir sockets de clientes al conjunto
        for (i = 0; i < max_clients; i++) {
            sd = client_sockets[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // Esperar actividad en alguno de los sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("Error en select");
        }

        // Nueva conexión entrante
        if (FD_ISSET(server_socket, &readfds)) {
            int new_socket;
            struct sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            if ((new_socket = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len)) < 0) {
                perror("Error al aceptar la conexión");
                exit(EXIT_FAILURE);
            }

            // Mostrar la dirección IP y el puerto del cliente
            printf("Nueva conexión, socket fd: %d, IP: %s, Puerto: %d\n", new_socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // Agregar el nuevo socket a la lista de sockets de clientes
            for (i = 0; i < max_clients; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        // Comprobar los sockets de los clientes en busca de datos
        for (i = 0; i < max_clients; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                // Leer datos del cliente
                if ((valread = read(sd, buffer, BUFFER_SIZE)) == 0) {
                    // Desconexión de un cliente
                    getpeername(sd, (struct sockaddr *)&server_addr, (socklen_t *)&addr_len);
                    printf("Cliente desconectado, IP: %s, Puerto: %d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    // Procesar mensaje recibido
                    buffer[valread] = '\0';
                    printf("Mensaje recibido: %s\n", buffer);
                    // Aquí puedes agregar la lógica para procesar el mensaje y enviarlo al destinatario adecuado
                    // (teclado: <nombre_destino> <mensaje> o <nombre_destino> archivo <nombre_archivo>)
                }
            }
        }
    }

    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define BUFFER_ENVIADO 2048
#define NOMBRE_SIZE 4
#define MAX_CLIENTS 10

typedef struct {
    int socket;
    char nombre[NOMBRE_SIZE];
} Cliente;

int clients[10]; // Array para guardar los descriptores de socket de los clientes
int client_count = 0;
Cliente clientes[MAX_CLIENTS];
pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER;
//sendfile pa mandar archivos


void *handle_client(void *arg)
{
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int bytes_received;
    char nombre[NOMBRE_SIZE + 1];
    bytes_received = recv(client_socket, nombre, NOMBRE_SIZE, 0);
    nombre[bytes_received] = '\0';

    pthread_mutex_lock(&clientes_mutex);
    strcpy(clientes[client_count].nombre, nombre);
    clientes[client_count].socket = client_socket;
    client_count++;
    pthread_mutex_unlock(&clientes_mutex);

    printf("Cliente conectado: %s\n", nombre);

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        buffer[bytes_received] = '\0';

        if (strcmp(buffer, "clientes") == 0)
        {
            char respuesta[BUFFER_SIZE] = "Lista de clientes:\n";
            pthread_mutex_lock(&clientes_mutex);
            for (int i = 0; i < client_count; ++i){
                char client_info[50];
                sprintf(client_info, "Cliente %d: %s\n", i + 1, clientes[i].nombre);
                strcat(respuesta, client_info);
            }
            pthread_mutex_unlock(&clientes_mutex);
            send(client_socket, respuesta, strlen(respuesta), 0);
        }else{
            // Extraer el nombre del cliente y el mensaje
            char target_nombre[NOMBRE_SIZE + 1];
            char message[BUFFER_SIZE];
            if (sscanf(buffer, "%4s %[^\n]", target_nombre, message) == 2)
            {
                // Encontrar el descriptor de socket del cliente objetivo
                int target_socket = -1;
                pthread_mutex_lock(&clientes_mutex);
                for (int i = 0; i < client_count; ++i)
                {
                    if (strcmp(clientes[i].nombre, target_nombre) == 0)
                    {
                        target_socket = clientes[i].socket;
                        break;
                    }
                }
                pthread_mutex_unlock(&clientes_mutex);

                if (target_socket != -1)
                {
                    char mensaje_enviado[BUFFER_ENVIADO];
                    snprintf(mensaje_enviado, BUFFER_ENVIADO, "mensaje enviado de %s: %s", nombre, message);
                    send(target_socket, mensaje_enviado, strlen(mensaje_enviado), 0);
                    printf("Mensaje enviado a %s: %s\n", target_nombre, message);
                }
                else
                {
                    const char *error_msg = "Cliente objetivo no encontrado.\n";
                    send(client_socket, error_msg, strlen(error_msg), 0);
                }
            }
            else
            {
                const char *error_msg = "Formato de mensaje incorrecto. Use: <Nombre> <mensaje>\n";
                send(client_socket, error_msg, strlen(error_msg), 0);
            }
        }
    }

    close(client_socket);
    pthread_mutex_lock(&clientes_mutex);
    for (int i = 0; i < client_count; ++i)
    {
        if (clientes[i].socket == client_socket)
        {
            for (int j = i; j < client_count - 1; ++j)
            {
                clientes[j] = clientes[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clientes_mutex);

    printf("Cliente desconectado: %s\n", nombre);
    return NULL;
}

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("Error al configurar el socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Error en bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("Error en listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Esperando conexiones...\n");

    while (client_count <= MAX_CLIENTS)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Error en accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        clients[client_count++] = new_socket;
        printf("Cliente %d conectado\n", client_count);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket);
    }

    // // Esperar que todos los clientes se desconecten
    // for (int i = 0; i < 2; ++i)
    // {
    //     close(clients[i]);
    // }

    close(server_fd);
    return 0;
}

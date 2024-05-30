#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define BUFFER_ENVIADO 2048
#define NOMBRE_SIZE 4
#define MAX_CLIENTS 10
#define FILE_SAVE_DIR "recibido/"
#define FULL_PATH_SIZE 2048 // Tamaño suficiente para combinar FILE_SAVE_DIR y file_path

typedef struct {
    int socket;
    char nombre[NOMBRE_SIZE + 1];
} Cliente;

Cliente clientes[MAX_CLIENTS]; // Array para guardar los descriptores de socket de los clientes
int client_count = 0;
pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    printf("Cliente %d conectado: %s\n", client_count, nombre);

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
        } else {
            char target_nombre[NOMBRE_SIZE + 1];
            char command[BUFFER_SIZE];
            if (sscanf(buffer, "%4s %s", target_nombre, command) == 2)
            {
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
                    if (strcmp(command, "archivo") == 0)
                    {
                        char file_path[BUFFER_SIZE];
                        long file_size;
                        sscanf(buffer + strlen(target_nombre) + strlen(command) + 2, "%s %ld", file_path, &file_size);

                        // Buscar el último '/' en file_path para obtener solo el nombre del archivo
                        char *file_name = strrchr(file_path, '/');
                        if (file_name != NULL)
                        {
                            file_name++; // Avanzar un carácter para saltar el '/'
                        }
                        else
                        {
                            file_name = file_path; // Si no hay '/', usar el file_path completo
                        }

                        struct stat st = {0};
                        if (stat(FILE_SAVE_DIR, &st) == -1)
                        {
                            mkdir(FILE_SAVE_DIR, 0700);
                        }

                        char full_path[FULL_PATH_SIZE];
                        snprintf(full_path, FULL_PATH_SIZE, "%s%s", FILE_SAVE_DIR, file_name);
                        printf("%s\n", full_path);
                        int file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if (file_fd < 0)
                        {
                            perror("Error al crear el archivo");
                            continue;
                        }

                        long remaining = file_size;
                        while (remaining > 0)
                        {
                            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
                            if (bytes_received <= 0)
                            {
                                perror("Error en recv");
                                break;
                            }
                            write(file_fd, buffer, bytes_received);
                            remaining -= bytes_received;
                        }

                        close(file_fd);
                        printf("Archivo %s recibido con éxito y guardado en %s\n", file_path, full_path);
                        char message[BUFFER_SIZE];
                        snprintf(message, BUFFER_ENVIADO, "[%s] envio un archivo: %s", nombre, file_name);
                        send(target_socket, message, strlen(message), 0);
                    }
                    else
                    {
                        char message[BUFFER_SIZE];
                        snprintf(message, BUFFER_SIZE, "[%s]: %s", nombre, buffer + strlen(target_nombre) + 1);
                        send(target_socket, message, strlen(message), 0);
                        printf("Mensaje enviado a %s: %s\n", target_nombre, buffer + strlen(target_nombre) + 1);
                    }
                }
                else
                {
                    const char *error_msg = "Cliente objetivo no encontrado.\n";
                    send(client_socket, error_msg, strlen(error_msg), 0);
                }
            }
            else
            {
                const char *error_msg = "Formato de mensaje incorrecto. Use: <Nombre> <mensaje/archivo>\n";
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

    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Error en accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&clientes_mutex);
        if (client_count >= MAX_CLIENTS)
        {
            close(new_socket);
            pthread_mutex_unlock(&clientes_mutex);
            continue;
        }
        pthread_mutex_unlock(&clientes_mutex);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket);
    }

    close(server_fd);
    return 0;
}

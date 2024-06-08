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
#define OPCODE_MSJE 1
#define OPCODE_ARCH 2
#define OPCODE_LIST 3
#define OPCODE_ACK 4
#define OPCODE_ERROR 5

#define BUFFER_SIZE 2048
#define FILEPATH_SIZE (BUFFER_SIZE + 14)
#define BUFFER_FILE 512
#define FILE_MESSAGE_SIZE (BUFFER_SIZE + 10)
#define NOMBRE_SIZE 4
#define MAX_CLIENTS 10
#define MESSAGE_SIZE 512
#define FILE_SAVE_DIR "server_files/"

typedef struct
{
    int socket;
    char nombre[NOMBRE_SIZE + 1];
} Cliente;

Cliente clientes[MAX_CLIENTS]; // Array para guardar los descriptores de socket de los clientes
int client_count = 0;
pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER;

void enviar_lista_de_clientes(int client_socket)
{
    char respuesta[BUFFER_SIZE] = "Lista de clientes:\n";
    pthread_mutex_lock(&clientes_mutex);
    for (int i = 0; i < client_count; ++i)
    {
        char client_info[50];
        sprintf(client_info, "Cliente %d: %s\n", i + 1, clientes[i].nombre);
        strcat(respuesta, client_info);
    }
    pthread_mutex_unlock(&clientes_mutex);
    send(client_socket, respuesta, strlen(respuesta), 0);
}

int obtener_socket_destinatario(const char *target_nombre)
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
    return target_socket;
}

void send_ack(const char *usuario, int target_sock, const char *filename)
{
    sleep(10);
    char ack[BUFFER_SIZE];
    uint16_t opcode = htons(OPCODE_ACK);
    memcpy(ack, &opcode, sizeof(opcode));
    strcpy(ack + sizeof(opcode), filename);

    send(target_sock, ack, sizeof(opcode) + strlen(filename) + 1, 0);
    printf("Enviando ACK a [%s] por archivo: %s.\n", usuario, filename);
}

void manejar_archivo(int client_sock, const char *usuario, const char *buffer2)
{
    char target_nombre[NOMBRE_SIZE + 1], filename[BUFFER_FILE];
    long file_size;

    if (sscanf(buffer2 + 2, "%4s %s %ld", target_nombre, filename, &file_size) != 3)
    {
        const char *error_message = "Error en el formato del mensaje.\n";
        send(client_sock, error_message, strlen(error_message), 0);
        return;
    }

    int target_socket = obtener_socket_destinatario(target_nombre);
    if (target_socket == -1)
    {
        const char *error_message = "Cliente objetivo no encontrado.\n";
        send(client_sock, error_message, strlen(error_message), 0);
        return;
    }

    printf("Reenviando archivo a: %s\n", target_nombre);

    send(target_socket, buffer2, BUFFER_SIZE, 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    long total_bytes_received = 0;
    ssize_t bytes_sent = 0;

    while (total_bytes_received < file_size)
    {
        bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received == -1)
        {
            perror("recv");
            return;
        }

        if (bytes_received == 0)
        {
            // Conexión cerrada por el cliente antes de recibir todos los datos
            printf("Conexión cerrada por el cliente antes de recibir todos los datos.\n");
            break;
        }

        bytes_sent += send(target_socket, buffer, bytes_received, 0);
        if (bytes_sent == -1)
        {
            perror("send");
            return;
        }

        total_bytes_received += bytes_received;
    }
    printf("Tamaño de archivo: %ld.\n", file_size);
    printf("Bytes recibidos: %ld.\n", total_bytes_received);
    printf("Bytes reenviados: %ld.\n", bytes_sent);

    if (total_bytes_received == file_size)
    {
        while (1)
        {
            bytes_received = recv(target_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received == -1)
            {
                perror("recv");
                return;
            }
            else if (bytes_received == 0)
            {
                printf("El cliente cerró la conexión antes de recibir el ACK.\n");
                return;
            }

            int opcode = ntohs(*(uint16_t *)buffer);
            if (opcode == OPCODE_ACK)
            {
                send_ack(usuario, client_sock, filename); // Enviar el ACK al remitente (client_sock)
                break;                                    // Salir del bucle una vez que se reciba el ACK
            }
            else
            {
                printf("Error: Se recibió un mensaje inesperado en lugar del ACK.\n");
                break;
            }
        }
    }
    else
    {
        printf("Error: Se recibieron %ld bytes, pero se esperaba %ld bytes.\n", total_bytes_received, file_size);
        const char *error_message = "Error al recibir el archivo.\n";
        send(client_sock, error_message, strlen(error_message), 0);
    }
}

void manejar_mensaje(int client_socket, const char *nombre, const char *buffer)
{
    char target_nombre[NOMBRE_SIZE + 1];
    sscanf(buffer + 2, "%4s", target_nombre);

    int target_socket = obtener_socket_destinatario(target_nombre);
    if (target_socket != -1)
    {
        char message[BUFFER_SIZE];
        snprintf(message, BUFFER_SIZE, "[%s]: %s", nombre, buffer + 2 + strlen(target_nombre) + 1);
        send(target_socket, message, strlen(message), 0);
        printf("Mensaje enviado de [%s] a [%s]: %s\n", nombre, target_nombre, buffer + 2 + strlen(target_nombre) + 1);
    }
    else
    {
        const char *error_msg = "Cliente objetivo no encontrado.\n";
        send(client_socket, error_msg, strlen(error_msg), 0);
    }
}

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

        int opcode = ntohs(*(uint16_t *)buffer);

        switch (opcode)
        {
        case OPCODE_MSJE:
            manejar_mensaje(client_socket, nombre, buffer);
            break;

        case OPCODE_ARCH:
            manejar_archivo(client_socket, nombre, buffer);
            break;

        case OPCODE_LIST:
            enviar_lista_de_clientes(client_socket);
            break;

        case OPCODE_ACK:
            printf("ACK recibido fuera de lugar.\n");
            break;

        default:
            const char *error_msg = "Código de operación incorrecto.\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
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
        pthread_detach(thread_id); // Separar el hilo para que no tenga que ser unido
    }

    close(server_fd);
    return 0;
}

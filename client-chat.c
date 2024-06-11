#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define BUFFER_SIZE 2048
#define NOMBRE_SIZE 4
#define PORT 8080

#define OPCODE_MSJE 1
#define OPCODE_ARCH 2
#define OPCODE_LIST 3
#define OPCODE_ACK 4
#define OPCODE_ERROR 5

char target_ip[INET_ADDRSTRLEN];
int target_port;

void *receive_messages(void *arg);
void receive_file(const char *client1_ip, int client1_port, const char *filename, long file_size);
void send_file(int sock, const char *target_name, const char *filename);
void send_ack(int sock, const char *filename);

void *receive_messages(void *arg)
{
    int sock = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0)
    {
        buffer[bytes_received] = '\0';
        int opcode = ntohs(*(uint16_t *)buffer);
        if (opcode == OPCODE_ARCH)
        {
            char target_nombre[NOMBRE_SIZE + 1], filename[BUFFER_SIZE];
            long file_size;
            int client1_port;
            char client1_ip[INET_ADDRSTRLEN];

            printf("Recibido mensaje OPCODE_ARCH.\n"); 
            if (sscanf(buffer + 2, "%4s %s %ld %d %s", target_nombre, filename, &file_size, &client1_port, client1_ip) == 5)
            {
                printf("Se quiere recibir el archivo: %s.\n", filename);
                receive_file(client1_ip, client1_port, filename, file_size);
            }
            else
            {
                printf("Error de formato del comando.\n");
            }
        }
        else
        {
            printf("%s\n", buffer); // Imprimir el mensaje después del OPCODE
        }
    }

    if (bytes_received < 0)
    {
        perror("Error en recv");
    }

    return NULL;
}

void receive_file(const char *client1_ip, int client1_port, const char *filename, long file_size)
{
    printf("Archivo entrante: %s\n", filename);
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "recibidos/%s", filename);

    printf("Creando archivo: %s\n", filepath);
    int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (file_fd < 0)
    {
        perror("open");
        return;
    }

    // Conectar con el cliente 1
    int client1_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client1_sock < 0)
    {
        perror("socket");
        close(file_fd);
        return;
    }

    struct sockaddr_in client1_addr;
    client1_addr.sin_family = AF_INET;
    client1_addr.sin_port = htons(client1_port);
    if (inet_pton(AF_INET, client1_ip, &client1_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(file_fd);
        close(client1_sock);
        return;
    }

    if (connect(client1_sock, (struct sockaddr *)&client1_addr, sizeof(client1_addr)) < 0)
    {
        perror("connect");
        close(file_fd);
        close(client1_sock);
        return;
    }

    printf("Conectado con el cliente 1 (%s:%d).\n", client1_ip, client1_port);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    long total_bytes_received = 0;
    printf("Recibiendo datos:\n");

    while (total_bytes_received < file_size)
    {
        bytes_received = recv(client1_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received == -1)
        {
            perror("recv");
            close(file_fd);
            close(client1_sock);
            return;
        }

        if (bytes_received == 0)
        {
            // Conexión cerrada por el cliente
            printf("Cliente cerró la conexión.\n");
            break;
        }

        ssize_t bytes_written = write(file_fd, buffer, bytes_received);
        if (bytes_written < 0)
        {
            perror("write");
            close(file_fd);
            close(client1_sock);
            return;
        }
        total_bytes_received += bytes_received;
    }

    close(file_fd);
    close(client1_sock);

    if (total_bytes_received == file_size)
    {
        printf("Se recibió correctamente el archivo: %s.\n", filename);
        // Envía una confirmación al cliente 1 si es necesario
    }
    else
    {
        printf("Error: Se recibieron %ld bytes, pero se esperaba %ld bytes.\n", total_bytes_received, file_size);
    }
}

void send_ack(int sock, const char *filename)
{
    char ack[BUFFER_SIZE];
    uint16_t opcode = htons(OPCODE_ACK);
    memcpy(ack, &opcode, sizeof(opcode));
    strcpy(ack + sizeof(opcode), filename);

    send(sock, ack, sizeof(opcode) + strlen(filename) + 1, 0);
    printf("Enviando ACK de archivo: %s.\n", filename);
}

void send_file(int sock, const char *target_name, const char *filename)
{
    printf("Ruta del archivo a enviar: %s\n", filename);

    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "client_files/%s", filename);

    printf("Abriendo archivo: %s\n", filepath);
    int file_fd = open(filepath, O_RDONLY);

    if (file_fd < 0)
    {
        perror("open");
        char *error_message = "No se encontró el archivo.\n";
        send(sock, error_message, strlen(error_message), 0);
        return;
    }

    // Obtener el tamaño del archivo
    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0)
    {
        perror("fstat");
        close(file_fd);
        return;
    }
    long file_size = file_stat.st_size;

    // Crear un nuevo socket para escuchar conexiones entrantes
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
    {
        perror("socket");
        close(file_fd);
        return;
    }

    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(8081); // Puerto para escuchar

    if (bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
    {
        perror("bind");
        close(file_fd);
        close(listen_sock);
        return;
    }

    if (listen(listen_sock, 1) < 0)
    {
        perror("listen");
        close(file_fd);
        close(listen_sock);
        return;
    }

    // Obtener la dirección IP del cliente
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getsockname(sock, (struct sockaddr *)&client_addr, &addr_len) != 0)
    {
        perror("Error al obtener la información del cliente");
        close(file_fd);
        close(listen_sock);
        return;
    }
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

    // Enviar comando al servidor para iniciar P2P
    printf("Enviando paquete de envío de archivo.\n");
    char command[BUFFER_SIZE];
    uint16_t opcode = htons(OPCODE_ARCH);
    memcpy(command, &opcode, sizeof(opcode));
    snprintf(command + 2, sizeof(command) - 2, "%s %s %ld %d %s", target_name, filename, file_size, ntohs(listen_addr.sin_port), client_ip);
    send(sock, command, sizeof(opcode) + strlen(command + 2), 0);

    printf("Esperando conexión del destinatario en el puerto %d.\n", ntohs(listen_addr.sin_port));

    // Esperar la conexión del cliente 2
    struct sockaddr_in client2_addr;
    socklen_t client2_addr_len = sizeof(client2_addr);
    int client2_sock = accept(listen_sock, (struct sockaddr *)&client2_addr, &client2_addr_len);
    if (client2_sock < 0)
    {
        perror("Error en accept");
        close(file_fd);
        close(listen_sock);
        return;
    }
    printf("Conexión aceptada desde %s:%d.\n", inet_ntoa(client2_addr.sin_addr), ntohs(client2_addr.sin_port));

    printf("Iniciando transferencia del archivo...\n");

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0)
    {
        ssize_t bytes_sent = send(client2_sock, buffer, bytes_read, 0);
        if (bytes_sent < 0)
        {
            perror("send");
            close(file_fd);
            close(client2_sock);
            close(listen_sock);
            return;
        }
        printf("Bytes enviados: %zd\n", bytes_sent);
    }

    close(file_fd);
    close(client2_sock);
    close(listen_sock);

    printf("Transferencia del archivo completada.\n");
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("El comando debe ser: %s <IP> <PUERTO> <NOMBRE>\n", argv[0]);
        printf("El nombre debe ser de máximo 4 letras\n");
        return -1;
    }

    strncpy(target_ip, argv[1], INET_ADDRSTRLEN);
    target_port = atoi(argv[2]);
    char nombre[NOMBRE_SIZE + 1];
    strncpy(nombre, argv[3], NOMBRE_SIZE);
    nombre[NOMBRE_SIZE] = '\0';

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Error al crear el socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_ip, &serv_addr.sin_addr) <= 0)
    {
        perror("Dirección inválida o no soportada");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Error al conectar");
        close(sock);
        return -1;
    }

    printf("Conectado al servidor\n");

    // Enviar el nombre al servidor
    send(sock, nombre, strlen(nombre), 0);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, receive_messages, (void *)&sock);

    while (1)
    {
        char buffer[BUFFER_SIZE];
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Eliminar el salto de línea

        // Buscar el comando "archivo" después del nombre del destinatario
        char target_name[NOMBRE_SIZE + 1];
        char command[BUFFER_SIZE];
        char file_name[BUFFER_SIZE];

        if (sscanf(buffer, "%4s %7s %s", target_name, command, file_name) == 3 && strcmp(command, "archivo") == 0)
        {
            if (sscanf(buffer, "%*s %*s %s", file_name) == 1)
            {
                printf("Enviando a [%s] archivo: %s\n", target_name, file_name);
                send_file(sock, target_name, file_name);
            }
            else
            {
                printf("Formato incorrecto. Use: <Nombre> archivo <ruta_del_archivo>\n");
            }
        }
        else if (sscanf(buffer, "%8s", command) == 1 && strcmp(command, "clientes") == 0)
        {
            // Enviar mensaje normal con OPCODE_LIST
            uint16_t opcode = htons(OPCODE_LIST);
            char message[BUFFER_SIZE];
            memcpy(message, &opcode, sizeof(opcode));
            send(sock, message, sizeof(opcode), 0);
        }
        else
        {
            // Enviar mensaje normal con OPCODE_MSJE
            uint16_t opcode = htons(OPCODE_MSJE);
            char message[BUFFER_SIZE];
            memcpy(message, &opcode, sizeof(opcode));
            strcpy(message + sizeof(opcode), buffer);
            send(sock, message, sizeof(opcode) + strlen(buffer), 0);
        }
    }
    close(sock);
    return 0;
}
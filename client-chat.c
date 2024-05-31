#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE 2048
#define NOMBRE_SIZE 4
#define MESSAGE_SIZE 512

void *receive_messages(void *arg);
void recieve_file(int sock, const char *filename, long file_size);
void send_file(int sock, const char *target_name, const char *filename);

void *receive_messages(void *arg)
{
    int sock = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0)
    {
        buffer[bytes_received] = '\0';
        if (strncmp(buffer, "FILE:", 5) == 0)
        {
            char *filename = buffer + 5;
            long file_size;
            sscanf(filename, "%ld", &file_size);
            printf("Archivo entrante: %s\n", filename);

            recieve_file(sock, filename, file_size);
        }
        else
        {
            printf("%s\n", buffer);
        }
    }

    if (bytes_received < 0)
    {
        perror("Error en recv");
    }

    return NULL;
}


void recieve_file(int sock, const char *filename, long file_size)
{
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "recibidos/%s", filename);

    printf("Creando archivo: %s\n", filepath);
    int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (file_fd < 0)
    {
        perror("open");
        const char *error_message = "No se pudo crear archivo.\n";
        send(sock, error_message, strlen(error_message), 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    long total_bytes_received = 0;
    printf("Recibiendo datos:\n");
    
    while (total_bytes_received < file_size)
    {
        bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received == -1)
        {
            perror("recv");
            close(file_fd);
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
            return;
        }
        total_bytes_received += bytes_received;
    }
    close(file_fd);
    if (total_bytes_received == file_size)
    {
        printf("Archivo recibió correctemente el archivo: %s.\n", filename);
    }
    else
    {
        printf("Error: Se recibieron %ld bytes, pero se esperaba %ld bytes.\n", total_bytes_received, file_size);
    }

}

void send_file(int sock, const char *target_name, const char *filename)
{
    // Imprimir la ruta completa del archivo para depuración
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

    // Enviar comando al servidor con el tamaño del archivo
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "%s archivo %s %ld", target_name, filename, file_size);
    send(sock, command, strlen(command), 0);

    printf("Se abrió el archivo: %s\n", filepath);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0)
    {
        printf("Bytes leídos: %zd\n", bytes_read);
        ssize_t bytes_sent = send(sock, buffer, bytes_read, 0);

        if (bytes_sent < 0)
        {
            perror("send");
            close(file_fd);
            return;
        }

        printf("Bytes enviados: %zd\n", bytes_sent);
    }

    if (bytes_read < 0)
    {
        perror("read");
    }
    else
    {
        printf("El archivo completo fue enviado.\n");
    }

    close(file_fd);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("El comando debe ser: %s <IP> <PUERTO> <NOMBRE>\n", argv[0]);
        printf("El nombre debe ser de máximo 4 letras\n");
        return -1;
    }

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char nombre[NOMBRE_SIZE + 1];

    strncpy(nombre, argv[3], NOMBRE_SIZE);
    nombre[NOMBRE_SIZE] = '\0';

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Error al crear el socket");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], (void *)&serv_addr.sin_addr) <= 0)
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
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Eliminar el salto de línea

        // Buscar el comando "archivo" después del nombre del destinatario
        char target_name[NOMBRE_SIZE + 1];
        char command[BUFFER_SIZE];
        char file_name[BUFFER_SIZE];

        if (sscanf(buffer, "%4s %7s %s", target_name, command, file_name) == 3 && strcmp(command, "archivo") == 0)
        {
            if (sscanf(buffer, "%*s %*s %s", file_name) == 1)
            {
                printf("target %s - file %s\n", target_name, file_name);
                send_file(sock, target_name, file_name);
                // Esperar confirmación del servidor
                recv(sock, buffer, BUFFER_SIZE, 0);
                printf("Respuesta del servidor: %s\n", buffer);
            }
            else
            {
                printf("Formato incorrecto. Use: <Nombre> archivo <ruta_del_archivo>\n");
            }
        }
        else
        {
            send(sock, buffer, strlen(buffer), 0);
        }
    }

    close(sock);
    return 0;
}

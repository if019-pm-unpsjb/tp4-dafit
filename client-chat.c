#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define NOMBRE_SIZE 4

void *receive_messages(void *arg)
{
    int sock = *((int *)arg);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0)
    {
        buffer[bytes_received] = '\0';
        printf("%s\n", buffer);
    }

    if (bytes_received < 0)
    {
        perror("Error en recv");
    }

    return NULL;
}

void send_file(int sock, const char *target_name, const char *file_path)
{
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0){
        perror("Error al abrir el archivo");
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0){
        perror("Error al obtener informacion del archivo");
        close(file_fd);
        return;
    }

    // Enviar la indicación de que se va a enviar un archivo y su tamaño
    char header[BUFFER_SIZE];
    snprintf(header, BUFFER_SIZE, "%s archivo %s %ld", target_name, file_path, file_stat.st_size);
    send(sock, header, strlen(header), 0);

    off_t offset= 0;
    size_t remaining = file_stat.st_size;

    while(remaining > 0){
        ssize_t sent = sendfile(sock, file_fd, &offset, remaining);
        if( sent <= 0){
            perror("Error al enviar archivo");
            break;
        }
        remaining -= sent;
    }

    close(file_fd);
}

int main(int argc, char *argv[]){

    if (argc != 4)
    {
        printf("El comando debe ser: %s <IP> <PUERTO> <NOMBRE\n", argv[0]);
        printf("El nombre debe ser de maximo 4 letras\n");
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

    // enviar el nombre al servidor
    send(sock, nombre, strlen(nombre), 0);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, receive_messages, (void *)&sock);

    while (1){
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Eliminar el salto de línea

        if (strncmp(buffer, "archivo", 7) == 0){
            char target_name[NOMBRE_SIZE + 1];
            char file_name[BUFFER_SIZE];

            memset(target_name, 0, sizeof(target_name));
            memset(file_name, 0, sizeof(file_name));

            if (sscanf(buffer, "archivo %4s %s", target_name, file_name) == 2)
            {
                printf("%s - %s\n", target_name, file_name);
                send_file(sock, target_name, file_name);
            }
            else
            {
                printf("Formato incorrecto. Use: <Nombre> archivo <ruta_del_archivo>\n");
            }
        }
        else{
            send(sock, buffer, strlen(buffer), 0);
            }
        }

    close(sock);
    return 0;
}

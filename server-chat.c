#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    int sockfd, max_sd, sd, new_socket, activity, i, valread;
    int client_socket[30] = {0};
    struct sockaddr_in servaddr, cliaddr;
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configurar dirección y puerto del servidor
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Vincular el socket con la dirección y puerto
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Error al vincular");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Escuchar por conexiones entrantes
    if (listen(sockfd, 3) < 0)
    {
        perror("Error al escuchar");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d\n", PORT);

    // Aceptar conexiones entrantes
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        max_sd = sockfd;

        for (i = 0; i < 30; i++)
        {
            sd = client_socket[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR))
        {
            perror("Error en select");
        }

        if (FD_ISSET(sockfd, &readfds))
        {
            socklen_t cli_len = sizeof(cliaddr);
            new_socket = accept(sockfd, (struct sockaddr *)&cliaddr, &cli_len);
            if (new_socket < 0)
            {
                perror("Error al aceptar conexión");
                exit(EXIT_FAILURE);
            }

            printf("Nueva conexión, socket fd es %d, IP es: %s, puerto: %d\n",
                   new_socket, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

            for (i = 0; i < 30; i++)
            {
                socklen_t cli_len = sizeof(cliaddr);
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    printf("Añadiendo a la lista de sockets como %d\n", i);
                    break;
                }
            }
        }

        for (i = 0; i < 30; i++)
        {
            sd = client_socket[i];
            if (FD_ISSET(sd, &readfds))
            {
                if ((valread = read(sd, buffer, BUFFER_SIZE)) == 0)
                {
                    getpeername(sd, (struct sockaddr *)&cliaddr, &cli_len);
                    printf("Host desconectado, IP %s, puerto %d\n",
                           inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                    close(sd);
                    client_socket[i] = 0;
                }
                else
                {
                    buffer[valread] = '\0';
                    printf("Mensaje recibido: %s\n", buffer);
                    send(sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }

    close(sockfd);
    return 0;
}

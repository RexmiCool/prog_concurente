#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

int main(int argc, char **argv)
{
    int sockfd, newsockfd;
    socklen_t clilen, servlen;
    struct sockaddr_un cli_addr;
    struct sockaddr_un serv_addr;
    char tampon[30];
    int nbOctets;
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        printf("Erreur de creation de socket\n");
        exit(1);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, "/tmp/socketLocale.1");
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, servlen) < 0)
    {
        printf("Erreur de bind\n");
        exit(1);
    }
    listen(sockfd, 5);
    while (1)
    {
        clilen = sizeof(cli_addr);
        printf("serveur: En attente...\n");
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            printf("serveur: Erreur de accept\n");
        nbOctets = 0;
        bzero((char *)tampon, 30);

        read(newsockfd, tampon, 20);
        printf("Serveur reçoit : %s\n", tampon);
        write(newsockfd, "Message reçu", 13);
        printf("Serveur envoie : Message recu\n");
        close(newsockfd);
    }
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>

#define PORT 8080
#define TAILLE 20

sem_t receive_plein;
sem_t receive_vide;
sem_t receive_mutex;

sem_t sending_plein;
sem_t sending_vide;
sem_t sending_mutex;

char *bufferServBrain;
char *bufferBrainClient;
int tourProd;
int tourCons;

int nbTour;
char *chaineAEnvoyer;
char *chaineRecue;

char* superMalloc(int size)
{
    char *buffer = (char *)malloc(size);
    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(buffer, ' ', size);
    return buffer;
}

void superFree(char *buffer)
{
    free(buffer);
}

void P(sem_t *val)
{
    sem_wait(val);
}

void V(sem_t *val)
{
    sem_post(val);
}

void placer(char *buffer)
{
    buffer[tourProd % TAILLE] = chaineAEnvoyer[tourProd];
}

void prendre(char *buffer)
{
    chaineRecue[tourCons] = buffer[tourCons % TAILLE];
}

void *thread_client(void *arg)
{
    int client_fd;
    struct sockaddr_in serv_addr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    printf("[ Process %d ] - Thread Client\n", getpid());
    while (tourCons < nbTour)
    {
        P(&sending_plein);
        P(&sending_mutex);

        prendre(bufferBrainClient);
        printf("Client: - %c\n", chaineRecue[tourCons]);

        send(client_fd, &chaineRecue[tourCons], 1, 0);

        V(&sending_mutex);
        V(&sending_vide);

        tourCons++;
    }
    close(client_fd);
    sleep(2);
    return NULL;
}

void *thread_server(void *arg)
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("[ Process %d ] - Thread Server\n", getpid());
    while (tourProd < nbTour)
    {
        P(&receive_vide);
        P(&receive_mutex);

        placer(bufferServBrain);
        printf("Server: + %c\n", chaineAEnvoyer[tourProd]);

        V(&receive_mutex);
        V(&receive_plein);

        tourProd++;
    }

    char buffer[1] = {0};
    while (tourCons < nbTour)
    {
        read(new_socket, buffer, 1);
        chaineRecue[tourCons] = buffer[0];
        printf("Server received: %c\n", chaineRecue[tourCons]);
        tourCons++;
    }

    close(new_socket);
    close(server_fd);
    sleep(2);
    return NULL;
}

void *thread_brain(void *arg)
{
    printf("[ Process %d ] - Thread Brain\n", getpid());
    while (tourCons < nbTour)
    {
        P(&receive_plein);
        P(&receive_mutex);

        prendre(bufferServBrain);
        printf("Brain: - %c\n", chaineRecue[tourCons]);

        V(&receive_mutex);
        V(&receive_vide);

        tourCons++;
    }

    tourProd = 0;
    tourCons = 0;
    nbTour++;

    while (tourProd < nbTour)
    {
        P(&sending_vide);
        P(&sending_mutex);

        placer(bufferBrainClient);
        printf("Brain: + %c\n", chaineAEnvoyer[tourProd]);

        V(&sending_mutex);
        V(&sending_plein);

        tourProd++;
    }
    
    sleep(2);
    return NULL;
}

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain)
{
    pthread_create(client, NULL, thread_client, NULL);
    pthread_create(server, NULL, thread_server, NULL);
    pthread_create(brain, NULL, thread_brain, NULL);
}

int main(int argc, char const *argv[])
{
    pid_t pid1;

    // Création du processus fils 1 (process1)
    if ((pid1 = fork()) == 0)
    {
        sem_init(&receive_plein, 0, 0);
        sem_init(&receive_vide, 0, TAILLE);
        sem_init(&receive_mutex, 0, 1);

        sem_init(&sending_plein, 0, 0);
        sem_init(&sending_vide, 0, TAILLE);
        sem_init(&sending_mutex, 0, 1);

        tourProd = 0;
        tourCons = 0;

        chaineAEnvoyer = "Bonjour Bonjour Bonjour";
        nbTour = strlen(chaineAEnvoyer);

        bufferServBrain = superMalloc(TAILLE);
        bufferBrainClient = superMalloc(TAILLE);
        chaineRecue = (char *)malloc(TAILLE + 1);

        printf("\n [ Process %d ] - Chaîne à envoyer : %s \n", getpid(), chaineAEnvoyer);

        // Création des threads Client, Server, Brain
        pthread_t tid_client, tid_server, tid_brain;
        create_threads(&tid_client, &tid_server, &tid_brain);

        pthread_join(tid_client, NULL);
        pthread_join(tid_server, NULL);
        pthread_join(tid_brain, NULL);

        chaineRecue[tourCons] = '\0';
        printf("\n [ Process %d ] - Chaine recue: %s\n", getpid(), chaineRecue);

        sem_destroy(&receive_plein);
        sem_destroy(&receive_vide);
        sem_destroy(&receive_mutex);
        sem_destroy(&sending_plein);
        sem_destroy(&sending_vide);
        sem_destroy(&sending_mutex);
        
        superFree(bufferServBrain);
        superFree(bufferBrainClient);
        free(chaineRecue);

        printf("Fin du processus fils 1\n");
        exit(0);
    }
    else if (pid1 < 0)
    {
        perror("fork");
        exit(1);
    }

    // Attente de la terminaison des processus fils
    waitpid(pid1, NULL, 0);

    printf("Fin du programme principal\n");
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#define BUFFER_SIZE 256

typedef struct {
    pthread_t thread;
    int sockfd;
    sem_t *semaphore;
    char buffer[BUFFER_SIZE];
} ThreadData;

sem_t semPlein, semVide, semMutex;

int fin = 0;

void arret(int signalRecu) {
    if (signalRecu == SIGUSR1) {
        fin = 1;
    }
}

void* client_thread(void* arg) {
    ThreadData* data = (ThreadData*) arg;

    while (!fin) {
        sem_wait(&semPlein);
        sem_wait(&semMutex);

        // Simuler l'envoi du message
        write(data->sockfd, data->buffer, strlen(data->buffer) + 1);
        printf("Client a envoyé: %s\n", data->buffer);

        sem_post(&semMutex);
        sem_post(&semVide);

        sleep(1); // Pour éviter une boucle infinie trop rapide
    }

    return NULL;
}

void* server_thread(void* arg) {
    ThreadData* data = (ThreadData*) arg;
    char buffer[BUFFER_SIZE];

    while (!fin) {
        read(data->sockfd, buffer, BUFFER_SIZE);
        sem_wait(&semVide);
        sem_wait(&semMutex);

        // Stocker le message reçu
        strncpy(data->buffer, buffer, BUFFER_SIZE);
        printf("Serveur a reçu: %s\n", buffer);

        sem_post(&semMutex);
        sem_post(&semPlein);

        sleep(1); // Pour éviter une boucle infinie trop rapide
    }

    return NULL;
}

void* brain_thread(void* arg) {
    ThreadData* data = (ThreadData*) arg;

    while (!fin) {
        sem_wait(&semPlein);
        sem_wait(&semMutex);

        // Simuler le traitement du message
        printf("Cerveau traite: %s\n", data->buffer);
        strcat(data->buffer, " [modifié par le cerveau]");

        sem_post(&semMutex);
        sem_post(&semVide);

        sleep(1); // Pour éviter une boucle infinie trop rapide
    }

    return NULL;
}

int main() {
    struct sigaction action;
    action.sa_handler = arret;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGUSR1, &action, NULL);

    sem_init(&semPlein, 0, 0);
    sem_init(&semVide, 0, BUFFER_SIZE);
    sem_init(&semMutex, 0, 1);

    pid_t pid1, pid2;

    pid1 = fork();
    if (pid1 == 0) {
        // Processus fils 1
        int sockfd;
        struct sockaddr_un serv_addr;

        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sun_family = AF_UNIX;
        strcpy(serv_addr.sun_path, "/tmp/socket1");
        bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
        listen(sockfd, 5);

        ThreadData data = { .sockfd = sockfd };

        pthread_create(&data.thread, NULL, server_thread, &data);
        pthread_create(&data.thread, NULL, brain_thread, &data);
        pthread_create(&data.thread, NULL, client_thread, &data);

        while (!fin) pause();

        close(sockfd);
        exit(0);
    }

    pid2 = fork();
    if (pid2 == 0) {
        // Processus fils 2
        int sockfd;
        struct sockaddr_un serv_addr;

        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        bzero((char *)&serv_addr, sizeof(serv_addr));
        serv_addr.sun_family = AF_UNIX;
        strcpy(serv_addr.sun_path, "/tmp/socket2");
        bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
        listen(sockfd, 5);

        ThreadData data = { .sockfd = sockfd };

        pthread_create(&data.thread, NULL, server_thread, &data);
        pthread_create(&data.thread, NULL, brain_thread, &data);
        pthread_create(&data.thread, NULL, client_thread, &data);

        while (!fin) pause();

        close(sockfd);
        exit(0);
    }

    // Processus père
    printf("Appuyez sur Ctrl+C pour arrêter le programme\n");
    while (!fin) pause();

    kill(pid1, SIGUSR1);
    kill(pid2, SIGUSR1);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    sem_destroy(&semPlein);
    sem_destroy(&semVide);
    sem_destroy(&semMutex);

    printf("Processus père arrêté\n");
    return 0;

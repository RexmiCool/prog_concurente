#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT1 12345
#define PORT2 12346
#define BUFFER_SIZE 1024

sem_t receive_plein;
sem_t receive_vide;
sem_t receive_mutex;

sem_t sending_plein;
sem_t sending_vide;
sem_t sending_mutex;

char bufferServBrain[BUFFER_SIZE];
char bufferBrainClient[BUFFER_SIZE];
char bufferClient[BUFFER_SIZE];
char bufferServer[BUFFER_SIZE];

void *thread_client(void *arg);
void *thread_server(void *arg);
void *thread_brain(void *arg);

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, int pid);

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    pid_t pid1, pid2;

    sem_init(&receive_plein, 0, 0);
    sem_init(&receive_vide, 0, 1);
    sem_init(&receive_mutex, 0, 1);

    sem_init(&sending_plein, 0, 0);
    sem_init(&sending_vide, 0, 1);
    sem_init(&sending_mutex, 0, 1);

    // Création du processus fils 1 (process1)
    if ((pid1 = fork()) == 0) {
        pthread_t tid_client, tid_server, tid_brain;
        create_threads(&tid_client, &tid_server, &tid_brain, 1);

        pthread_join(tid_client, NULL);
        pthread_join(tid_server, NULL);
        pthread_join(tid_brain, NULL);

        exit(0);
    } else if (pid1 < 0) {
        error("fork");
    }

    // Création du processus fils 2 (process2)
    if ((pid2 = fork()) == 0) {
        pthread_t tid_client, tid_server, tid_brain;
        create_threads(&tid_client, &tid_server, &tid_brain, 2);

        pthread_join(tid_client, NULL);
        pthread_join(tid_server, NULL);
        pthread_join(tid_brain, NULL);

        exit(0);
    } else if (pid2 < 0) {
        error("fork");
    }

    // Attente de la terminaison des processus fils
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    sem_destroy(&receive_plein);
    sem_destroy(&receive_vide);
    sem_destroy(&receive_mutex);
    sem_destroy(&sending_plein);
    sem_destroy(&sending_vide);
    sem_destroy(&sending_mutex);

    printf("Fin du programme principal\n");
    return 0;
}

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, int pid) {
    int *arg = malloc(sizeof(*arg));
    *arg = pid;

    pthread_create(client, NULL, thread_client, arg);
    pthread_create(server, NULL, thread_server, arg);
    pthread_create(brain, NULL, thread_brain, arg);
}

void *thread_client(void *arg) {
    int pid = *(int *)arg;
    int sockfd;
    struct sockaddr_in serv_addr;

    printf("[ Process %d ] - Thread Client\n", pid);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(pid == 1 ? PORT2 : PORT1);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR connecting");

    while (1) {
        sem_wait(&sending_plein);
        sem_wait(&sending_mutex);

        send(sockfd, bufferBrainClient, strlen(bufferBrainClient), 0);

        sem_post(&sending_mutex);
        sem_post(&sending_vide);

        sleep(1);
    }

    close(sockfd);
    free(arg);
    return NULL;
}

void *thread_server(void *arg) {
    int pid = *(int *)arg;
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    printf("[ Process %d ] - Thread Server\n", pid);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(pid == 1 ? PORT1 : PORT2);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0) error("ERROR on accept");

    while (1) {
        sem_wait(&receive_vide);
        sem_wait(&receive_mutex);

        recv(newsockfd, bufferServBrain, BUFFER_SIZE, 0);

        sem_post(&receive_mutex);
        sem_post(&receive_plein);

        sleep(1);
    }

    close(newsockfd);
    close(sockfd);
    free(arg);
    return NULL;
}

void *thread_brain(void *arg) {
    int pid = *(int *)arg;
    printf("[ Process %d ] - Thread Brain\n", pid);

    while (1) {
        sem_wait(&receive_plein);
        sem_wait(&receive_mutex);

        strcpy(bufferClient, bufferServBrain);
        sprintf(bufferBrainClient, "%s%d", bufferClient, pid);

        sem_post(&receive_mutex);
        sem_post(&receive_vide);

        sem_wait(&sending_vide);
        sem_wait(&sending_mutex);

        strcpy(bufferBrainClient, bufferClient);
        sprintf(bufferBrainClient, "%s%d", bufferClient, pid);

        sem_post(&sending_mutex);
        sem_post(&sending_plein);

        sleep(1);
    }

    free(arg);
    return NULL;
}

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
#include <signal.h>

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

int sockfd1, newsockfd1, sockfd2, newsockfd2;
pthread_t tid_client1, tid_server1, tid_brain1;
pthread_t tid_client2, tid_server2, tid_brain2;

void *thread_client(void *arg);
void *thread_server(void *arg);
void *thread_brain(void *arg);

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, int pid);
void error(const char *msg);
void cleanup(int signum);

int main(int argc, char *argv[]) {
    signal(SIGINT, cleanup);

    sem_init(&receive_plein, 0, 0);
    sem_init(&receive_vide, 0, 1);
    sem_init(&receive_mutex, 0, 1);

    sem_init(&sending_plein, 0, 0);
    sem_init(&sending_vide, 0, 1);
    sem_init(&sending_mutex, 0, 1);

    pid_t pid1, pid2;

    // Création du processus fils 1 (process1)
    if ((pid1 = fork()) == 0) {
        create_threads(&tid_client1, &tid_server1, &tid_brain1, 1);

        pthread_join(tid_client1, NULL);
        pthread_join(tid_server1, NULL);
        pthread_join(tid_brain1, NULL);

        exit(0);
    } else if (pid1 < 0) {
        error("fork");
    }

    // Création du processus fils 2 (process2)
    if ((pid2 = fork()) == 0) {
        create_threads(&tid_client2, &tid_server2, &tid_brain2, 2);

        pthread_join(tid_client2, NULL);
        pthread_join(tid_server2, NULL);
        pthread_join(tid_brain2, NULL);

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
    struct sockaddr_in serv_addr;

    printf("[ Process %d ] - Thread Client\n", pid);
    if ((sockfd1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(pid == 1 ? PORT2 : PORT1);

    while (connect(sockfd1, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Client connect");
        sleep(1);
    }

    while (1) {
        sem_wait(&sending_plein);
        sem_wait(&sending_mutex);

        printf("[ Process %d ] - Client envoie: %s\n", pid, bufferBrainClient);
        send(sockfd1, bufferBrainClient, strlen(bufferBrainClient), 0);

        sem_post(&sending_mutex);
        sem_post(&sending_vide);

        sleep(1);
    }

    close(sockfd1);
    free(arg);
    return NULL;
}

void *thread_server(void *arg) {
    int pid = *(int *)arg;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    printf("[ Process %d ] - Thread Server\n", pid);
    if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("ERROR opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(pid == 1 ? PORT1 : PORT2);

    if (bind(sockfd2, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    listen(sockfd2, 5);
    clilen = sizeof(cli_addr);

    if ((newsockfd1 = accept(sockfd2, (struct sockaddr *)&cli_addr, &clilen)) < 0) error("ERROR on accept");

    while (1) {
        sem_wait(&receive_vide);
        sem_wait(&receive_mutex);

        recv(newsockfd1, bufferServBrain, BUFFER_SIZE, 0);
        printf("[ Process %d ] - Server reçu: %s\n", pid, bufferServBrain);

        sem_post(&receive_mutex);
        sem_post(&receive_plein);

        sleep(1);
    }

    close(newsockfd1);
    close(sockfd2);
    free(arg);
    return NULL;
}

void *thread_brain(void *arg) {
    int pid = *(int *)arg;
    printf("[ Process %d ] - Thread Brain\n", pid);

    if (pid == 1) {
        strcpy(bufferBrainClient, "bonjour");
        sem_post(&sending_plein);
    }

    while (1) {
        sem_wait(&receive_plein);
        sem_wait(&receive_mutex);

        strcpy(bufferClient, bufferServBrain);
        sprintf(bufferBrainClient, "%s%d", bufferClient, pid);
        printf("[ Process %d ] - Brain modifié: %s\n", pid, bufferBrainClient);

        sem_post(&receive_mutex);
        sem_post(&receive_vide);

        sem_post(&sending_plein);
        sem_wait(&sending_mutex);

        strcpy(bufferBrainClient, bufferClient);
        sprintf(bufferBrainClient, "%s%d", bufferClient, pid);
        printf("[ Process %d ] - Brain envoie: %s\n", pid, bufferBrainClient);

        sem_post(&sending_mutex);
        sem_post(&sending_plein);

        sleep(1);
    }

    free(arg);
    return NULL;
}

void cleanup(int signum) {
    printf("\nCleaning up and exiting...\n");

    // Fermer les sockets
    close(sockfd1);
    close(newsockfd1);
    close(sockfd2);
    close(newsockfd2);

    exit(0);
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}

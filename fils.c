#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>

#define SOCKET_PATH_PREFIX "/tmp/socketUnix_"
#define BUFFER_SIZE 256
#define NUM_THREADS 3

char tamponReception[BUFFER_SIZE];
char tamponEmission[BUFFER_SIZE];
sem_t semReception, semEmission;

int sockfd;
int otherSockfd;
int fin = 0;

void stop_handler(int sig) {
    if (sig == SIGUSR1) {
        fin = 1;
        close(sockfd);
        close(otherSockfd);
        sem_destroy(&semReception);
        sem_destroy(&semEmission);
        exit(0);
    }
}

void *thread_client(void *arg) {
    while (!fin) {
        sem_wait(&semEmission);
        if (fin) break;

        struct sockaddr_un serv_addr;
        serv_addr.sun_family = AF_UNIX;
        strcpy(serv_addr.sun_path, SOCKET_PATH_PREFIX);
        strcat(serv_addr.sun_path, (char *)arg);

        if (connect(otherSockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Client: connect error");
            continue;
        }

        write(otherSockfd, tamponEmission, strlen(tamponEmission) + 1);
        sem_post(&semEmission);
    }
    return NULL;
}

void *thread_serveur(void *arg) {
    while (!fin) {
        struct sockaddr_un cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Server: accept error");
            continue;
        }

        read(newsockfd, tamponReception, BUFFER_SIZE);
        sem_post(&semReception);
        close(newsockfd);
    }
    return NULL;
}

void *thread_cerveau(void *arg) {
    while (!fin) {
        sem_wait(&semReception);
        if (fin) break;

        strcpy(tamponEmission, tamponReception);
        sem_post(&semEmission);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <1 or 2>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num = atoi(argv[1]);
    struct sigaction sa;
    sa.sa_handler = stop_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        exit(EXIT_FAILURE);
    }

    sem_init(&semReception, 0, 0);
    sem_init(&semEmission, 0, 0);

    char socketPath[100];
    snprintf(socketPath, sizeof(socketPath), "%s%d", SOCKET_PATH_PREFIX, num);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un serv_addr;
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, socketPath);

    unlink(socketPath);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5);

    pthread_t threads[NUM_THREADS];
    char otherSocket[100];
    snprintf(otherSocket, sizeof(otherSocket), "%d", 3 - num);

    if (pthread_create(&threads[0], NULL, thread_client, (void *)otherSocket) != 0 ||
        pthread_create(&threads[1], NULL, thread_serveur, NULL) != 0 ||
        pthread_create(&threads[2], NULL, thread_cerveau, NULL) != 0) {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

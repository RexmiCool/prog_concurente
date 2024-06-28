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
#include <time.h>

#define PORT_BASE 12345
#define BUFFER_SIZE 1024

sem_t *receive_plein;
sem_t *receive_vide;
sem_t *receive_mutex;

sem_t *sending_plein;
sem_t *sending_vide;
sem_t *sending_mutex;

char **bufferServBrain;
char **bufferBrainClient;

int *sockfds_client, *sockfds_server;
int num_processes;

pthread_t *tid_clients, *tid_servers, *tid_brains, *tid_trackers;
pid_t *pids;

void *thread_client(void *arg);
void *thread_server(void *arg);
void *thread_brain(void *arg);
void *thread_tracker(void *arg);

void create_threads(int pid);
void error(const char *msg);
void cleanup(int signum);
void signal_handler(int signum);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_processes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    num_processes = atoi(argv[1]);
    if (num_processes < 2) {
        fprintf(stderr, "Number of processes must be at least 2.\n");
        exit(EXIT_FAILURE);
    }
    
    signal(SIGINT, signal_handler);
    srand(time(NULL));

    receive_plein = malloc(num_processes * sizeof(sem_t));
    receive_vide = malloc(num_processes * sizeof(sem_t));
    receive_mutex = malloc(num_processes * sizeof(sem_t));

    sending_plein = malloc(num_processes * sizeof(sem_t));
    sending_vide = malloc(num_processes * sizeof(sem_t));
    sending_mutex = malloc(num_processes * sizeof(sem_t));

    bufferServBrain = malloc(num_processes * sizeof(char *));
    bufferBrainClient = malloc(num_processes * sizeof(char *));
    for (int i = 0; i < num_processes; ++i) {
        bufferServBrain[i] = malloc(BUFFER_SIZE * sizeof(char));
        bufferBrainClient[i] = malloc(BUFFER_SIZE * sizeof(char));
        sem_init(&receive_plein[i], 0, 0);
        sem_init(&receive_vide[i], 0, 1);
        sem_init(&receive_mutex[i], 0, 1);

        sem_init(&sending_plein[i], 0, 0);
        sem_init(&sending_vide[i], 0, 1);
        sem_init(&sending_mutex[i], 0, 1);
    }

    sockfds_client = malloc(num_processes * sizeof(int));
    sockfds_server = malloc(num_processes * sizeof(int));
    pids = malloc(num_processes * sizeof(pid_t));
    tid_clients = malloc(num_processes * sizeof(pthread_t));
    tid_servers = malloc(num_processes * sizeof(pthread_t));
    tid_brains = malloc(num_processes * sizeof(pthread_t));
    tid_trackers = malloc(num_processes * sizeof(pthread_t));

    for (int i = 0; i < num_processes; ++i) {
        if ((pids[i] = fork()) == 0) {
            create_threads(i);

            pthread_join(tid_clients[i], NULL);
            pthread_join(tid_servers[i], NULL);
            pthread_join(tid_brains[i], NULL);
            pthread_join(tid_trackers[i], NULL);

            exit(0);
        } else if (pids[i] < 0) {
            error("fork");
        }
    }

    for (int i = 0; i < num_processes; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    for (int i = 0; i < num_processes; ++i) {
        sem_destroy(&receive_plein[i]);
        sem_destroy(&receive_vide[i]);
        sem_destroy(&receive_mutex[i]);
        sem_destroy(&sending_plein[i]);
        sem_destroy(&sending_vide[i]);
        sem_destroy(&sending_mutex[i]);
        free(bufferServBrain[i]);
        free(bufferBrainClient[i]);
    }
    free(receive_plein);
    free(receive_vide);
    free(receive_mutex);
    free(sending_plein);
    free(sending_vide);
    free(sending_mutex);
    free(bufferServBrain);
    free(bufferBrainClient);
    free(sockfds_client);
    free(sockfds_server);
    free(pids);
    free(tid_clients);
    free(tid_servers);
    free(tid_brains);
    free(tid_trackers);

    printf("Fin du programme principal\n");
    return 0;
}

void create_threads(int pid) {
    int *arg = malloc(sizeof(*arg));
    *arg = pid;

    pthread_create(&tid_clients[pid], NULL, thread_client, arg);
    pthread_create(&tid_servers[pid], NULL, thread_server, arg);
    pthread_create(&tid_brains[pid], NULL, thread_brain, arg);
    pthread_create(&tid_trackers[pid], NULL, thread_tracker, arg);

    // Pause pour s'assurer que les serveurs sont prêts
    sleep(2);
}

void *thread_client(void *arg) {
    int pid = *(int *)arg;
    struct sockaddr_in serv_addr;

    printf("[ Process %d ] - Thread Client\n", pid);
    if ((sockfds_client[pid] = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("ERROR opening socket");

    int opt = 1;
    if (setsockopt(sockfds_client[pid], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        error("setsockopt failed");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_BASE + pid);

    while (1) {
        if (connect(sockfds_client[pid], (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Client connect");
            sleep(1);
            continue;  // Réessayer la connexion
        }

        while (1) {
            sem_wait(&sending_plein[pid]);
            sem_wait(&sending_mutex[pid]);

            printf("[ Process %d ] - Client envoie: %s\n", pid, bufferBrainClient[pid]);
            send(sockfds_client[pid], bufferBrainClient[pid], strlen(bufferBrainClient[pid]), 0);

            sem_post(&sending_mutex[pid]);
            sem_post(&sending_vide[pid]);

            sleep(1);
        }
    }

    close(sockfds_client[pid]);
    free(arg);
    return NULL;
}

void *thread_tracker(void *arg) {
    int pid = *(int *)arg;

    printf("[ Process %d ] - Thread Tracker\n", pid);

    while (1) {
        sem_wait(&sending_plein[pid]);
        sem_wait(&sending_mutex[pid]);

        printf("[ Process %d ] - Tracker envoie: %s\n", pid, bufferBrainClient[pid]);

        sem_post(&sending_mutex[pid]);
        sem_post(&sending_vide[pid]);

        sleep(1);
    }

    free(arg);
    return NULL;
}

void *thread_server(void *arg) {
    int pid = *(int *)arg;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    printf("[ Process %d ] - Thread Server\n", pid);
    if ((sockfds_server[pid] = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("ERROR opening socket");

    int opt = 1;
    if (setsockopt(sockfds_server[pid], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        error("setsockopt failed");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT_BASE + pid);

    if (bind(sockfds_server[pid], (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    listen(sockfds_server[pid], 5);
    clilen = sizeof(cli_addr);

    if ((sockfds_server[pid] = accept(sockfds_server[pid], (struct sockaddr *)&cli_addr, &clilen)) < 0) error("ERROR on accept");

    while (1) {
        sem_wait(&receive_vide[pid]);
        sem_wait(&receive_mutex[pid]);

        recv(sockfds_server[pid], bufferServBrain[pid], BUFFER_SIZE, 0);
        printf("[ Process %d ] - Server reçu: %s\n", pid, bufferServBrain[pid]);

        sem_post(&receive_mutex[pid]);
        sem_post(&receive_plein[pid]);

        sleep(1);
    }

    close(sockfds_server[pid]);
    free(arg);
    return NULL;
}

void *thread_brain(void *arg) {
    int pid = *(int *)arg;
    printf("[ Process %d ] - Thread Brain\n", pid);

    if (pid == 0) {
        strcpy(bufferBrainClient[pid], "bonjour");
        sem_post(&sending_plein[pid]);
    }

    while (1) {
        sem_wait(&receive_plein[pid]);
        sem_wait(&receive_mutex[pid]);

        strcpy(bufferBrainClient[pid], bufferServBrain[pid]);
        sprintf(bufferBrainClient[pid], "%s%d", bufferServBrain[pid], pid);
        printf("[ Process %d ] - Brain modifié: %s\n", pid, bufferBrainClient[pid]);

        sem_post(&receive_mutex[pid]);
        sem_post(&receive_vide[pid]);

        int target_pid = rand() % num_processes;
        printf("[ Process %d ] - Brain envoie a client: %d\n", pid, target_pid);

        sem_wait(&sending_vide[target_pid]);
        sem_wait(&sending_mutex[target_pid]);

        strcpy(bufferBrainClient[target_pid], bufferBrainClient[pid]);
        sem_post(&sending_mutex[target_pid]);
        sem_post(&sending_plein[target_pid]);

        sleep(1);
    }

    free(arg);
    return NULL;
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\nReceived SIGINT. Sending SIGUSR1 to child processes...\n");

        for (int i = 0; i < num_processes; ++i) {
            if (pids[i] > 0) kill(pids[i], SIGUSR1);
        }

        for (int i = 0; i < num_processes; ++i) {
            waitpid(pids[i], NULL, 0);
        }

        cleanup(signum);
    }
}

void cleanup(int signum) {
    printf("Cleaning up and exiting...\n");

    for (int i = 0; i < num_processes; ++i) {
        close(sockfds_client[i]);
        close(sockfds_server[i]);
    }

    for (int i = 0; i < num_processes; ++i) {
        sem_destroy(&receive_plein[i]);
        sem_destroy(&receive_vide[i]);
        sem_destroy(&receive_mutex[i]);
        sem_destroy(&sending_plein[i]);
        sem_destroy(&sending_vide[i]);
        sem_destroy(&sending_mutex[i]);
    }

    free(receive_plein);
    free(receive_vide);
    free(receive_mutex);
    free(sending_plein);
    free(sending_vide);
    free(sending_mutex);
    free(bufferServBrain);
    free(bufferBrainClient);
    free(sockfds_client);
    free(sockfds_server);
    free(pids);
    free(tid_clients);
    free(tid_servers);
    free(tid_brains);
    free(tid_trackers);

    exit(0);
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}

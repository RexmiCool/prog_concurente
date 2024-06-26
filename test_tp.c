#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

int taille = 20;

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

pthread_t tid_client, tid_server, tid_brain;

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
    buffer[tourProd % taille] = chaineAEnvoyer[tourProd];
}

void prendre(char *buffer)
{
    chaineRecue[tourCons] = buffer[tourCons % taille];
}

void *thread_client(void *arg)
{
    printf("[ Process %d ] - Thread Client\n", getpid());
    while (tourCons < nbTour)
    {
        P(&sending_plein);
        P(&sending_mutex);

        prendre(bufferBrainClient);
        printf("Client: - %c\n", chaineRecue[tourCons]);

        V(&sending_mutex);
        V(&sending_vide);

        tourCons++;
    }
    sleep(2);
    return NULL;
}

void *thread_server(void *arg)
{
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

void create_threads()
{
    pthread_create(&tid_client, NULL, thread_client, NULL);
    pthread_create(&tid_server, NULL, thread_server, NULL);
    pthread_create(&tid_brain, NULL, thread_brain, NULL);
}

void sigusr1_handler(int sig)
{
    printf("\nChild process received SIGUSR1. Cancelling threads...\n");
    pthread_cancel(tid_client);
    pthread_cancel(tid_server);
    pthread_cancel(tid_brain);
}

void sigint_handler(int sig)
{
    printf("\nParent process received SIGINT. Sending SIGUSR1 to child...\n");
    kill(0, SIGUSR1); // Send SIGUSR1 to all child processes
}

int main(int argc, char const *argv[])
{
    pid_t pid1;

    // Set up the SIGINT handler in the parent process
    signal(SIGINT, sigint_handler);

    // Création du processus fils 1 (process1)
    if ((pid1 = fork()) == 0)
    {
        // Set up the SIGUSR1 handler in the child process
        signal(SIGUSR1, sigusr1_handler);

        sem_init(&receive_plein, 0, 0);
        sem_init(&receive_vide, 0, taille);
        sem_init(&receive_mutex, 0, 1);

        sem_init(&sending_plein, 0, 0);
        sem_init(&sending_vide, 0, taille);
        sem_init(&sending_mutex, 0, 1);

        tourProd = 0;
        tourCons = 0;

        chaineAEnvoyer = "Bonjour Bonjour Bonjour";
        nbTour = strlen(chaineAEnvoyer);

        bufferServBrain = superMalloc(taille);
        bufferBrainClient = superMalloc(taille);
        chaineRecue = (char *)malloc(taille + 1);

        printf("\n [ Process %d ] - Chaîne à envoyer : %s \n", getpid(), chaineAEnvoyer);

        // Création des threads Client, Server, Brain
        create_threads();

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

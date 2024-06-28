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
#include <fcntl.h> // Pour les constantes O_WRONLY et O_NONBLOCK

//#define PORT1 12345
//#define PORT2 12346
#define BUFFER_SIZE 1024
#define NB_PROCESS 3

int ports[NB_PROCESS];


sem_t receive_plein;
sem_t receive_vide;
sem_t receive_mutex;

sem_t sending_plein;
sem_t sending_vide;
sem_t sending_mutex;

sem_t tracker_plein;
sem_t tracker_vide;
sem_t tracker_mutex;

char bufferServBrain[BUFFER_SIZE];
char bufferBrainClient[BUFFER_SIZE];
char bufferClient[BUFFER_SIZE];
char bufferBrainTracker[BUFFER_SIZE];
char bufferTracker[BUFFER_SIZE];
char bufferServer[BUFFER_SIZE];

// int sockfd1, newsockfd1, sockfd2, newsockfd2;
// pthread_t tid_client1, tid_server1, tid_brain1, tid_tracker1;
// pthread_t tid_client2, tid_server2, tid_brain2, tid_tracker2;
// pid_t pid1, pid2;

int sockfd, newsockfd;
pthread_t tid_client, tid_server, tid_brain, tid_tracker;
pid_t pids[NB_PROCESS];

int pipe_fd[2]; // Descripteurs de fichier pour le tube

void *thread_client(void *arg);
void *thread_server(void *arg);
void *thread_brain(void *arg);
void *thread_tracker(void *arg);

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, pthread_t *tracker, int pid);
void error(const char *msg);
void cleanup(int signum);
void signal_handler(int signum);
void create_pipe();
void create_observer_process();

int main(int argc, char *argv[])
{
    
    printf(ports);
    signal(SIGINT, signal_handler);

    sem_init(&receive_plein, 0, 0);
    sem_init(&receive_vide, 0, 1);
    sem_init(&receive_mutex, 0, 1);

    sem_init(&sending_plein, 0, 0);
    sem_init(&sending_vide, 0, 1);
    sem_init(&sending_mutex, 0, 1);

    sem_init(&tracker_plein, 0, 0);
    sem_init(&tracker_vide, 0, 1);
    sem_init(&tracker_mutex, 0, 1);

    // Création du tube (pipe)
    create_pipe();

    // Création du processus fils 1 (process1)
    /*if ((pid1 = fork()) == 0) {
        create_threads(&tid_client1, &tid_server1, &tid_brain1, &tid_tracker1, 1);

        pthread_join(tid_client1, NULL);
        pthread_join(tid_server1, NULL);
        pthread_join(tid_brain1, NULL);
        pthread_join(tid_tracker1, NULL);

        exit(0);
    } else if (pid1 < 0) {
        error("fork");
    }

    // Création du processus fils 2 (process2)
    if ((pid2 = fork()) == 0) {
        create_threads(&tid_client2, &tid_server2, &tid_brain2, &tid_tracker2, 2);

        pthread_join(tid_client2, NULL);
        pthread_join(tid_server2, NULL);
        pthread_join(tid_brain2, NULL);
        pthread_join(tid_tracker2, NULL);

        exit(0);
    } else if (pid2 < 0) {
        error("fork");
    }*/

    for (size_t i = 0; i < NB_PROCESS; i++)
    {
        ports[i] = 12345 + i;
    }

    

    for (size_t i = 0; i < NB_PROCESS; i++)
    {
        if ((pids[i] = fork()) == 0)
        {
            create_threads(&tid_client, &tid_server, &tid_brain, &tid_tracker, i);

            pthread_join(tid_client, NULL);
            pthread_join(tid_server, NULL);
            pthread_join(tid_brain, NULL);
            pthread_join(tid_tracker, NULL);

            exit(0);
        }
        else if (pids[i] < 0)
        {
            error("fork");
        }
    }

    // Création du processus Observer
    create_observer_process();

    // Attente de la terminaison des processus fils
    //waitpid(pid1, NULL, 0);
    //waitpid(pid2, NULL, 0);

    for (size_t i = 0; i < NB_PROCESS; i++){
        waitpid(pids[i], NULL, 0);
    }


    // Fermeture du tube
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    // Destruction des sémaphores
    sem_destroy(&receive_plein);
    sem_destroy(&receive_vide);
    sem_destroy(&receive_mutex);
    sem_destroy(&sending_plein);
    sem_destroy(&sending_vide);
    sem_destroy(&sending_mutex);
    sem_destroy(&tracker_plein);
    sem_destroy(&tracker_vide);
    sem_destroy(&tracker_mutex);

    printf("Fin du programme principal\n");
    return 0;
}

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, pthread_t *tracker, int pid)
{
    int *arg = malloc(sizeof(*arg));
    *arg = pid;

    pthread_create(client, NULL, thread_client, arg);
    pthread_create(server, NULL, thread_server, arg);
    pthread_create(brain, NULL, thread_brain, arg);
    pthread_create(tracker, NULL, thread_tracker, arg);
}

/*
void *thread_client(void *arg)
{
    int pid = *(int *)arg;
    struct sockaddr_in serv_addr;

    printf("[ Process %d ] - Thread Client\n", pid);
    if ((sockfd1 = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("ERROR opening socket");

    // Activer SO_REUSEADDR pour réutiliser l'adresse locale
    int opt = 1;
    if (setsockopt(sockfd1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        error("setsockopt failed");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(pid == 1 ? PORT2 : PORT1);

    while (1)
    {
        if (connect(sockfd1, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("Client connect");
            sleep(1);
            continue; // Réessayer la connexion
        }

        while (1)
        {
            sem_wait(&sending_plein);
            sem_wait(&sending_mutex);

            printf("[ Process %d ] - Client envoie: %s\n", pid, bufferBrainClient);
            send(sockfd1, bufferBrainClient, strlen(bufferBrainClient), 0);

            sem_post(&sending_mutex);
            sem_post(&sending_vide);

            sleep(1);
        }
    }

    close(sockfd1);
    free(arg);
    return NULL;
}*/

/*void *thread_client(void *arg)
{
    int pid = *(int *)arg;
    struct sockaddr_in serv_addr;

    printf("[ Process %d ] - Thread Client\n", pid);

    while (1)
    {
        sem_wait(&sending_plein);
        sem_wait(&sending_mutex);

        // Create socket
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
            error("ERROR opening socket");

        // Activer SO_REUSEADDR pour réutiliser l'adresse locale
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            error("setsockopt failed");

        // Randomly select which port to connect to
        int port;
        do
        {
            port = rand() % NB_PROCESS;
        } while (port == pid);
        

        port = ports[port];

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        printf("[ Process %d ] - Client se connecte à: %d\n", pid, port);

        // Attempt connection
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("Client connect");
            close(sockfd);
            sem_post(&sending_mutex);
            sem_post(&sending_vide);
            continue; // Retry connection
        }

        // Send message
        printf("[ Process %d ] - Client envoie: %s\n", pid, bufferBrainClient);
        send(sockfd, bufferBrainClient, strlen(bufferBrainClient), 0);

        // Close socket and release resources
        close(sockfd);
        sem_post(&sending_mutex);
        sem_post(&sending_vide);

        sleep(1); // Adjust as needed
    }

    free(arg);
    return NULL;
}*/

void *thread_client(void *arg)
{
    int pid = *(int *)arg;
    struct sockaddr_in serv_addr;
    int sockfd;
    int connected = 0;
    int port;

    printf("[ Process %d ] - Thread Client\n", pid);

    while (1)
    {
        if (!connected)
        {
            // Create socket
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0)
                error("ERROR opening socket");

            // Activer SO_REUSEADDR pour réutiliser l'adresse locale
            int opt = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
                error("setsockopt failed");

            // Randomly select which port to connect to
            do
            {
                port = rand() % NB_PROCESS;
            } while (port == pid);

            port = ports[port];

            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            serv_addr.sin_port = htons(port);

            printf("[ Process %d ] - Client se connecte à: %d\n", pid, port);

            // Attempt connection
            if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            {
                perror("Client connect");
                close(sockfd);
                sleep(1);
                continue; // Retry connection
            }
            connected = 1;
        }

        sem_wait(&sending_plein);
        sem_wait(&sending_mutex);

        // Send message
        printf("[ Process %d ] - Client envoie: %s\n", pid, bufferBrainClient);
        send(sockfd, bufferBrainClient, strlen(bufferBrainClient), 0);

        sem_post(&sending_mutex);
        sem_post(&sending_vide);

        sleep(1); // Adjust as needed
    }

    close(sockfd);
    free(arg);
    return NULL;
}



void *thread_tracker(void *arg)
{
    int pid = *(int *)arg;

    //printf("[ Process %d ] - Thread Tracker\n", pid);

    while (1)
    {
        sem_wait(&tracker_plein);
        sem_wait(&tracker_mutex);

        //printf("[ Process %d ] - Tracker envoie: %s\n", pid, bufferBrainTracker);
        write(pipe_fd[1], bufferBrainTracker, strlen(bufferBrainTracker));

        sem_post(&tracker_mutex);
        sem_post(&tracker_vide);

        sleep(1);
    }

    free(arg);
    return NULL;
}

void *thread_server(void *arg)
{
    int pid = *(int *)arg;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    printf("[ Process %d ] - Thread Server\n", pid);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error("ERROR opening socket");

    // Activer SO_REUSEADDR pour réutiliser l'adresse locale
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        error("setsockopt failed");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(ports[pid]);
    printf("[ Process %d ] - Server se crée sur: %d\n", pid, ports[pid]);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) < 0)
        error("ERROR on accept");

    while (1)
    {
        sem_wait(&receive_vide);
        sem_wait(&receive_mutex);

        recv(newsockfd, bufferServBrain, BUFFER_SIZE, 0);
        printf("[ Process %d ] - Server reçu: %s\n", pid, bufferServBrain);

        sem_post(&receive_mutex);
        sem_post(&receive_plein);

        sleep(1);
    }

    close(newsockfd);
    close(sockfd);
    free(arg);
    return NULL;
}

void *thread_brain(void *arg)
{
    int pid = *(int *)arg;
    printf("[ Process %d ] - Thread Brain\n", pid);

    if (pid == NB_PROCESS-1)
    {
        strcpy(bufferBrainClient, "bonjour");
        sem_post(&sending_plein);
    }

    while (1)
    {
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
        printf("[ Process %d ] - Brain envoie a client: %s\n", pid, bufferBrainClient);

        sem_post(&sending_mutex);
        sem_post(&sending_plein);

        sem_post(&tracker_plein);
        sem_wait(&tracker_mutex);

        strcpy(bufferBrainTracker, bufferClient);
        sprintf(bufferBrainTracker, "%s%d", bufferClient, pid);
        printf("[ Process %d ] - Brain envoie a tracker: %s\n", pid, bufferBrainTracker);

        sem_post(&tracker_mutex);
        sem_post(&tracker_plein);

        sleep(1);
    }

    free(arg);
    return NULL;
}

void signal_handler(int signum)
{
    if (signum == SIGINT)
    {
        printf("\nReceived SIGINT. Sending SIGUSR1 to child processes...\n");

        // Envoyer SIGUSR1 aux processus fils
        //if (pid1 > 0)
        //    kill(pid1, SIGUSR1);
        //if (pid2 > 0)
        //    kill(pid2, SIGUSR1);
        for (size_t i = 0; i < NB_PROCESS; i++)
        {
            if (pids[i] > 0)
                kill(pids[i], SIGUSR1);
        }
        

        // Attendre la terminaison des processus fils
        //waitpid(pid1, NULL, 0);
        //waitpid(pid2, NULL, 0);

        for (size_t i = 0; i < NB_PROCESS; i++)
        {
            waitpid(pids[i], NULL, 0);
        }

        // Nettoyage et sortie
        cleanup(signum);
    }
}

void cleanup(int signum)
{
    printf("Cleaning up and exiting...\n");

    // Fermer les sockets
    //close(sockfd1);
    //close(newsockfd1);
    //close(sockfd2);
    //close(newsockfd2);
    close(sockfd);
    close(newsockfd);

    // Fermer le tube
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    // Détruire les sémaphores
    sem_destroy(&receive_plein);
    sem_destroy(&receive_vide);
    sem_destroy(&receive_mutex);
    sem_destroy(&sending_plein);
    sem_destroy(&sending_vide);
    sem_destroy(&sending_mutex);
    sem_destroy(&tracker_plein);
    sem_destroy(&tracker_vide);
    sem_destroy(&tracker_mutex);

    exit(0);
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void create_pipe()
{
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(1);
    }
}

void create_observer_process()
{
    pid_t observer_pid;

    // Fork pour créer le processus Observer
    if ((observer_pid = fork()) == 0)
    {
        printf("[ Observer ] - Observer process started\n");

        // Fermer l'extrémité d'écriture du tube dans le processus Observer
        close(pipe_fd[1]);

        // Ouvrir un fichier pour écrire le log
        FILE *log_file = fopen("log.txt", "w");
        if (log_file == NULL)
        {
            perror("Failed to open log file");
            exit(1);
        }

        // Lecture depuis le tube et écriture dans le fichier log
        char buffer[BUFFER_SIZE];
        ssize_t nbytes;

        while ((nbytes = read(pipe_fd[0], buffer, BUFFER_SIZE)) > 0)
        {
            fwrite(buffer, 1, nbytes, log_file);
            fflush(log_file); // Assurer l'écriture immédiate dans le fichier
        }

        // Fermer le fichier et le tube
        fclose(log_file);
        close(pipe_fd[0]);

        printf("[ Observer ] - Observer process finished\n");
        exit(0);
    }
    else if (observer_pid < 0)
    {
        perror("Failed to fork observer process");
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 10

// Structure pour stocker le message
typedef struct {
  char message[100];
  pid_t pid;
} message_t;

// Mutex et variables de condition pour la synchronisation
pthread_mutex_t mutex_server_brain = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_server_brain = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_brain_client = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_brain_client = PTHREAD_COND_INITIALIZER;

// Tampon (file FIFO)
message_t buffer[BUFFER_SIZE];
int in = 0, out = 0;

void* thread_client(void* arg) {
  printf("Je suis le thread Client du processus %d\n", getpid());
  while (1) {
    pthread_mutex_lock(&mutex_brain_client);
    // Attendre qu'un message soit disponible
    while (in == out) {
      pthread_cond_wait(&cond_brain_client, &mutex_brain_client);
    }

    // Récupérer le message du tampon
    message_t msg = buffer[out];
    out = (out + 1) % BUFFER_SIZE;

    printf("Client (%d) reçoit: %s (from Brain - PID: %d)\n", getpid(), msg.message, msg.pid);

    pthread_mutex_unlock(&mutex_brain_client);
    sleep(1); // Simuler le traitement du message
  }
  return NULL;
}

void* thread_server(void* arg) {
  printf("Je suis le thread Server du processus %d\n", getpid());
  char message[100];
  while (1) {
    printf("Entrez un message pour Brain: ");
    fgets(message, sizeof(message), stdin);

    pthread_mutex_lock(&mutex_server_brain);
    // Attendre s'il y a de la place dans le tampon
    while ((in + 1) % BUFFER_SIZE == out) {
      printf("Buffer plein, attente\n");
      pthread_cond_wait(&cond_server_brain, &mutex_server_brain);
    }

    // Placer le message dans le tampon
    strcpy(buffer[in].message, message);
    buffer[in].pid = getpid();
    in = (in + 1) % BUFFER_SIZE;

    pthread_cond_signal(&cond_brain_client); // Signaler au Brain qu'il y a un message
    pthread_mutex_unlock(&mutex_server_brain);
    sleep(2); // Simuler le travail du Server
  }
  return NULL;
}

void* thread_brain(void* arg) {
  printf("Je suis le thread Brain du processus %d\n", getpid());
  while (1) {
    pthread_mutex_lock(&mutex_server_brain);
    // Attendre qu'un message soit disponible
    while (in == out) {
      pthread_cond_wait(&cond_server_brain, &mutex_server_brain);
    }

    // Récupérer le message du tampon
    message_t msg = buffer[out];
    out = (out + 1) % BUFFER_SIZE;

    // Modifier le message
    strcat(msg.message, " (modified by Brain)");

    pthread_mutex_unlock(&mutex_server_brain);

    pthread_mutex_lock(&mutex_brain_client);
    // Attendre s'il y a de la place dans le tampon
    while ((in + 1) % BUFFER_SIZE == out) {
      printf("Buffer plein, attente\n");
      pthread_cond_wait(&cond_brain_client, &mutex_brain_client);
    }

    // Placer le message modifié dans le tampon
    strcpy(buffer[in].message, msg.message);
    buffer[in].pid = msg.pid;
    in = (in + 1) % BUFFER_SIZE;

    pthread_cond_signal(&cond_client); // Signaler au Client qu'il y a un message
    pthread_mutex_unlock(&mutex_brain_client);
    sleep(2); // Simuler le travail du Brain
  }
  return NULL;
}

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain) {
    pthread_create(client, NULL, thread_client, NULL);
    pthread_create(server, NULL, thread_server, NULL);
    pthread_create(brain, NULL, thread_brain, NULL);
}

int main() {
  pid_t pid1, pid2;

  // Création du processus fils 1 (process1)
  pid1 = fork();
  if (pid1 == 0) {
    // Code exécuté dans le processus fils 1
    pthread_t tid_client, tid_server, tid_brain;

    create_threads(&tid_client, &tid_server, &tid_brain);

    pthread_join(tid_client, NULL);
    pthread_join(tid_server, NULL);
    pthread_join(tid_brain, NULL);

    printf("Fin du processus fils 1\n");
    exit(0);
  } else if (pid1 < 0) {
    perror("fork");
    exit(1);
  }

  // Création du processus fils 2 (process2)
  pid2 = fork();
  if (pid2 == 0) {
    // Code exécuté dans le processus fils 2
    pthread_t tid_client, tid_server, tid_brain;

    create_threads(&tid_client, &tid_server, &tid_brain);

    pthread_join(tid_client, NULL);
    pthread_join(tid_server, NULL);
    pthread_join(tid_brain, NULL);

    printf("Fin du processus fils 2\n");
    exit(0);
  } else if (pid2 < 0) {
    perror("fork");
    exit(1);
  }

  // Attente de la terminaison des processus fils
  waitpid(pid1, NULL, 0);
  waitpid(pid2, NULL, 0);

  printf("Fin du programme principal\n");
  return 0;
}

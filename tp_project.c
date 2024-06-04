#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

void* thread_client(void* arg) {
  printf("Je suis le thread Client du processus %d\n", getpid());
  // Simuler le travail du thread Client
  sleep(2);
  return NULL;
}

void* thread_server(void* arg) {
  printf("Je suis le thread Server du processus %d\n", getpid());
  // Simuler le travail du thread Server
  sleep(2);
  return NULL;
}

void* thread_brain(void* arg) {
  printf("Je suis le thread Brain du processus %d\n", getpid());
  // Simuler le travail du thread Brain
  sleep(2);
  return NULL;
}

int main() {
  pid_t pid1, pid2;

  // Création du processus fils 1 (process1)
  pid1 = fork();
  if (pid1 == 0) {
    // Code exécuté dans le processus fils 1
    pthread_t tid_client, tid_server, tid_brain;

    pthread_create(&tid_client, NULL, thread_client, NULL);
    pthread_create(&tid_server, NULL, thread_server, NULL);
    pthread_create(&tid_brain, NULL, thread_brain, NULL);

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

    pthread_create(&tid_client, NULL, thread_client, NULL);
    pthread_create(&tid_server, NULL, thread_server, NULL);
    pthread_create(&tid_brain, NULL, thread_brain, NULL);

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

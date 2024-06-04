#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 256

typedef struct {
  int sockfd;
  int process_id;
} thread_args;

void *client_thread_function(void *arg) {
  char *modified_buffer = (char *)arg;
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("connect");
    exit(1);
  }

  printf("Thread client: Envoi du message modifié: %s\n", modified_buffer);
  send(sockfd, modified_buffer, strlen(modified_buffer), 0);

  close(sockfd);

  pthread_exit(NULL);
}

void *brain_thread_function(void *arg) {
  char *modified_buffer = (char *)arg;
  printf("Thread brain: Message modifié: %s\n", modified_buffer);

  // Envoyer le message modifié au thread client
  pthread_t client_thread;
  pthread_create(&client_thread, NULL, client_thread_function, (void *)modified_buffer);
  pthread_join(client_thread, NULL);

  pthread_exit(NULL);
}

void *server_thread(void *arg) {
  thread_args *args = (thread_args *)arg;
  int sockfd = args->sockfd;
  int process_id = args->process_id;

  char buffer[BUFFER_SIZE];
  int bytes_received;

  while (1) {
    bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
      break;
    }

    printf("Processus %d: Message reçu: %s\n", process_id, buffer);

    // Envoyer le message reçu au thread brain
    pthread_t brain_thread;
    char modified_buffer[BUFFER_SIZE];
    sprintf(modified_buffer, "%s - %d", buffer, process_id);
    pthread_create(&brain_thread, NULL, brain_thread_function, (void *)modified_buffer);
    pthread_join(brain_thread, NULL);
  }

  pthread_exit(NULL);
}

int main() {
  int sockfd1, sockfd2;
  struct sockaddr_in server_addr;

  // Créer la socket pour le processus 1
  sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd1 < 0) {
    perror("socket");
    exit(1);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sockfd1, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind");
    exit(1);
  }

  if (listen(sockfd1, 2) < 0) {
    perror("listen");
    exit(1);
  }

  // Créer la socket pour le processus 2
  sockfd2 = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd2 < 0) {
    perror("socket");
    exit(1);
  }

  if (connect(sockfd2, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("connect");
    exit(1);
  }

  // Créer les threads pour le processus 1
  pthread_t server_thread1, brain_thread1, client_thread1;
  thread_args args1 = {sockfd1, 1};

  pthread_create(&server_thread1, NULL, server_thread, (void *)&args1);
  pthread_create(&brain_thread1, NULL, brain_thread_function, NULL);
  pthread_create(&client_thread1, NULL, client_thread_function, "Bonjour");

  // Attendre la fin des threads du processus 1
  pthread_join(server_thread1, NULL);
  pthread_join(brain_thread1, NULL);
  pthread_join(client_thread1, NULL);

  // Fermer les sockets
  close(sockfd1);
  close(sockfd2);

  return 0;
}

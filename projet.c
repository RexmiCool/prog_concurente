#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

#define SOCK_PATH1 "socket1"
#define SOCK_PATH2 "socket2"
#define BUFFER_SIZE 1024

int stop = 0;

void handle_signal(int signal) {
    stop = 1;
}

typedef struct {
    char buffer[BUFFER_SIZE];
    int size;
} Buffer;

Buffer recv_buffer1 = { .size = 0 };
Buffer send_buffer1 = { .size = 0 };
Buffer recv_buffer2 = { .size = 0 };
Buffer send_buffer2 = { .size = 0 };

pthread_mutex_t recv_mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t recv_cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t send_cond1 = PTHREAD_COND_INITIALIZER;

pthread_mutex_t recv_mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t recv_cond2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t send_cond2 = PTHREAD_COND_INITIALIZER;

void *client_thread(void *arg) {
    int sockfd;
    struct sockaddr_un addr;
    char *path = (char *)arg;
    char *process_id = strstr(path, "1") ? "Process 1" : "Process 2";

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, strstr(path, "1") ? SOCK_PATH2 : SOCK_PATH1);

    while (!stop) {
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("connect");
            sleep(1);
            continue;
        }

        if (strstr(path, "1")) {
            pthread_mutex_lock(&send_mutex1);
            while (send_buffer1.size == 0 && !stop) {
                pthread_cond_wait(&send_cond1, &send_mutex1);
            }

            if (stop) {
                pthread_mutex_unlock(&send_mutex1);
                break;
            }

            printf("[%s - Client] Sending message: %s\n", process_id, send_buffer1.buffer);
            if (send(sockfd, send_buffer1.buffer, send_buffer1.size, 0) == -1) {
                perror("send");
            }

            send_buffer1.size = 0;
            pthread_mutex_unlock(&send_mutex1);
        } else {
            pthread_mutex_lock(&send_mutex2);
            while (send_buffer2.size == 0 && !stop) {
                pthread_cond_wait(&send_cond2, &send_mutex2);
            }

            if (stop) {
                pthread_mutex_unlock(&send_mutex2);
                break;
            }

            printf("[%s - Client] Sending message: %s\n", process_id, send_buffer2.buffer);
            if (send(sockfd, send_buffer2.buffer, send_buffer2.size, 0) == -1) {
                perror("send");
            }

            send_buffer2.size = 0;
            pthread_mutex_unlock(&send_mutex2);
        }
    }

    close(sockfd);
    return NULL;
}

void *server_thread(void *arg) {
    int sockfd, clientfd;
    struct sockaddr_un addr;
    char *path = (char *)arg;
    char *process_id = strstr(path, "1") ? "Process 1" : "Process 2";

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(path);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    if ((clientfd = accept(sockfd, NULL, NULL)) == -1) {
        perror("accept");
        exit(1);
    }

    while (!stop) {
        if (strstr(path, "1")) {
            pthread_mutex_lock(&recv_mutex1);
            recv_buffer1.size = recv(clientfd, recv_buffer1.buffer, BUFFER_SIZE, 0);
            if (recv_buffer1.size == -1) {
                perror("recv");
            } else if (recv_buffer1.size > 0) {
                printf("[%s - Server] Received message: %s\n", process_id, recv_buffer1.buffer);
                pthread_cond_signal(&recv_cond1);
            }
            pthread_mutex_unlock(&recv_mutex1);
        } else {
            pthread_mutex_lock(&recv_mutex2);
            recv_buffer2.size = recv(clientfd, recv_buffer2.buffer, BUFFER_SIZE, 0);
            if (recv_buffer2.size == -1) {
                perror("recv");
            } else if (recv_buffer2.size > 0) {
                printf("[%s - Server] Received message: %s\n", process_id, recv_buffer2.buffer);
                pthread_cond_signal(&recv_cond2);
            }
            pthread_mutex_unlock(&recv_mutex2);
        }
    }

    close(clientfd);
    close(sockfd);
    unlink(path);
    return NULL;
}

void *brain_thread(void *arg) {
    char *process_id = (char *)arg;

    while (!stop) {
        if (strcmp(process_id, "Process 1") == 0) {
            pthread_mutex_lock(&recv_mutex1);
            while (recv_buffer1.size == 0 && !stop) {
                pthread_cond_wait(&recv_cond1, &recv_mutex1);
            }

            if (stop) {
                pthread_mutex_unlock(&recv_mutex1);
                break;
            }

            pthread_mutex_lock(&send_mutex2);
            memcpy(send_buffer2.buffer, recv_buffer1.buffer, recv_buffer1.size);
            send_buffer2.size = recv_buffer1.size;
            recv_buffer1.size = 0;
            printf("[%s - Brain] Processing message: %s\n", process_id, send_buffer2.buffer);
            pthread_cond_signal(&send_cond2);
            pthread_mutex_unlock(&send_mutex2);
            pthread_mutex_unlock(&recv_mutex1);
        } else {
            pthread_mutex_lock(&recv_mutex2);
            while (recv_buffer2.size == 0 && !stop) {
                pthread_cond_wait(&recv_cond2, &recv_mutex2);
            }

            if (stop) {
                pthread_mutex_unlock(&recv_mutex2);
                break;
            }

            pthread_mutex_lock(&send_mutex1);
            memcpy(send_buffer1.buffer, recv_buffer2.buffer, recv_buffer2.size);
            send_buffer1.size = recv_buffer2.size;
            recv_buffer2.size = 0;
            printf("[%s - Brain] Processing message: %s\n", process_id, send_buffer1.buffer);
            pthread_cond_signal(&send_cond1);
            pthread_mutex_unlock(&send_mutex1);
            pthread_mutex_unlock(&recv_mutex2);
        }
    }

    return NULL;
}

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, char *path) {
    pthread_create(client, NULL, client_thread, (void *)path);
    pthread_create(server, NULL, server_thread, (void *)path);
    pthread_create(brain, NULL, brain_thread, (void *)strstr(path, "1") ? "Process 1" : "Process 2");
}

int main() {
    signal(SIGUSR1, handle_signal);

    pthread_t client1, server1, brain1;
    pthread_t client2, server2, brain2;

    pid_t pid1, pid2;

    if ((pid1 = fork()) == 0) {
        create_threads(&client1, &server1, &brain1, SOCK_PATH1);

        pthread_mutex_lock(&send_mutex1);
        strcpy(send_buffer1.buffer, "Bonjour");
        send_buffer1.size = strlen("Bonjour") + 1;
        pthread_cond_signal(&send_cond1);
        pthread_mutex_unlock(&send_mutex1);

        pthread_join(client1, NULL);
        pthread_join(server1, NULL);
        pthread_join(brain1, NULL);
        exit(0);
    }

    if ((pid2 = fork()) == 0) {
        create_threads(&client2, &server2, &brain2, SOCK_PATH2);
        pthread_join(client2, NULL);
        pthread_join(server2, NULL);
        pthread_join(brain2, NULL);
        exit(0);
    }

    while (!stop) {
        sleep(1);
    }

    kill(pid1, SIGUSR1);
    kill(pid2, SIGUSR1);

    wait(NULL);
    wait(NULL);

    pthread_mutex_destroy(&recv_mutex1);
    pthread_mutex_destroy(&send_mutex1);
    pthread_cond_destroy(&recv_cond1);
    pthread_cond_destroy(&send_cond1);

    pthread_mutex_destroy(&recv_mutex2);
    pthread_mutex_destroy(&send_mutex2);
    pthread_cond_destroy(&recv_cond2);
    pthread_cond_destroy(&send_cond2);

    return 0;
}

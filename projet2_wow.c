#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
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

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;

void *client_thread(void *arg) {
    int sockfd;
    struct sockaddr_un addr;
    char *path = (char *)arg;
    char *process_id = strstr(path, "1") ? "Process 1" : "Process 2";

    while (!stop) {
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            exit(1);
        }

        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, strstr(path, "1") ? SOCK_PATH2 : SOCK_PATH1);

        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("connect");
            close(sockfd);
            sleep(1);
            continue;
        }

        if (strstr(path, "1")) {
            pthread_mutex_lock(&mutex1);
            while (send_buffer1.size == 0 && !stop) {
                pthread_cond_wait(&cond1, &mutex1);
            }

            if (stop) {
                pthread_mutex_unlock(&mutex1);
                break;
            }

            printf("[%s - Client] Sending message: %s\n", process_id, send_buffer1.buffer);
            if (send(sockfd, send_buffer1.buffer, send_buffer1.size, 0) == -1) {
                perror("send");
            }

            send_buffer1.size = 0;
            pthread_mutex_unlock(&mutex1);
        } else {
            pthread_mutex_lock(&mutex2);
            while (send_buffer2.size == 0 && !stop) {
                pthread_cond_wait(&cond2, &mutex2);
            }

            if (stop) {
                pthread_mutex_unlock(&mutex2);
                break;
            }

            printf("[%s - Client] Sending message: %s\n", process_id, send_buffer2.buffer);
            if (send(sockfd, send_buffer2.buffer, send_buffer2.size, 0) == -1) {
                perror("send");
            }

            send_buffer2.size = 0;
            pthread_mutex_unlock(&mutex2);
        }

        close(sockfd);
    }

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

    while (!stop) {
        if ((clientfd = accept(sockfd, NULL, NULL)) == -1) {
            perror("accept");
            exit(1);
        }

        if (strstr(path, "1")) {
            pthread_mutex_lock(&mutex2);
            recv_buffer1.size = recv(clientfd, recv_buffer1.buffer, BUFFER_SIZE, 0);
            if (recv_buffer1.size == -1) {
                perror("recv");
            } else if (recv_buffer1.size > 0) {
                printf("[%s - Server] Received message: %s\n", process_id, recv_buffer1.buffer);
                pthread_cond_signal(&cond1);
            }
            pthread_mutex_unlock(&mutex1);
        } else {
            pthread_mutex_lock(&mutex2);
            recv_buffer2.size = recv(clientfd, recv_buffer2.buffer, BUFFER_SIZE, 0);
            if (recv_buffer2.size == -1) {
                perror("recv");
            } else if (recv_buffer2.size > 0) {
                printf("[%s - Server] Received message: %s\n", process_id, recv_buffer2.buffer);
                pthread_cond_signal(&cond2);
            }
            pthread_mutex_unlock(&mutex2);
        }

        close(clientfd);
    }

    close(sockfd);
    unlink(path);
    return NULL;
}

void *brain_thread(void *arg) {
    char *path = (char *)arg;
    char *process_id = strstr(path, "1") ? "1" : "2";

    while (!stop) {
        if (strstr(path, "1")) {
            printf("[%s - Brain] start brain\n", process_id);
            while (recv_buffer1.size == 0 && !stop) {
                pthread_cond_wait(&cond1, &mutex1);
            }

            pthread_mutex_lock(&mutex1);

            if (stop) {
                pthread_mutex_unlock(&mutex1);
                break;
            }

            strcat(recv_buffer1.buffer, process_id);
            recv_buffer1.size = strlen(recv_buffer1.buffer) + 1;

            pthread_mutex_lock(&mutex1);
            memcpy(send_buffer1.buffer, recv_buffer1.buffer, recv_buffer1.size);
            send_buffer1.size = recv_buffer1.size;
            pthread_cond_signal(&cond1);
            pthread_mutex_unlock(&mutex1);

            recv_buffer1.size = 0;
            pthread_mutex_unlock(&mutex1);
            printf("[%s - Brain] stop brain\n", process_id);
        } else {
            printf("[%s - Brain] start brain\n", process_id);
            pthread_mutex_lock(&mutex2);
            while (recv_buffer2.size == 0 && !stop) {
                pthread_cond_wait(&cond2, &mutex2);
            }

            if (stop) {
                pthread_mutex_unlock(&mutex2);
                break;
            }

            strcat(recv_buffer2.buffer, process_id);
            recv_buffer2.size = strlen(recv_buffer2.buffer) + 1;

            pthread_mutex_lock(&mutex2);
            memcpy(send_buffer2.buffer, recv_buffer2.buffer, recv_buffer2.size);
            send_buffer2.size = recv_buffer2.size;
            pthread_cond_signal(&cond2);
            pthread_mutex_unlock(&mutex2);

            recv_buffer2.size = 0;
            pthread_mutex_unlock(&mutex2);
            printf("[%s - Brain] stop brain\n", process_id);
        }
    }

    return NULL;
}

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, char *path) {
    pthread_create(client, NULL, client_thread, (void *)path);
    pthread_create(server, NULL, server_thread, (void *)path);
    pthread_create(brain, NULL, brain_thread, (void *)path);
}

int main() {
    signal(SIGINT, handle_signal);

    pthread_t client1, server1, brain1;
    pthread_t client2, server2, brain2;

    pid_t pid1, pid2;

    if ((pid1 = fork()) == 0) {
        create_threads(&client1, &server1, &brain1, SOCK_PATH1);

        pthread_mutex_lock(&mutex1);
        strcpy(send_buffer1.buffer, "Bonjour");
        send_buffer1.size = strlen("Bonjour") + 1;
        pthread_cond_signal(&cond1);
        pthread_mutex_unlock(&mutex1);

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

    pthread_mutex_destroy(&mutex1);
    pthread_mutex_destroy(&mutex2);
    pthread_cond_destroy(&cond1);
    pthread_cond_destroy(&cond2);

    return 0;
}

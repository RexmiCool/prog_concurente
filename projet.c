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

Buffer recv_buffer = { .size = 0 };
Buffer send_buffer = { .size = 0 };

pthread_mutex_t recv_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t recv_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t send_cond = PTHREAD_COND_INITIALIZER;

void *client_thread(void *arg) {
    int sockfd;
    struct sockaddr_un addr;
    char *path = (char *)arg;

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);

    while (!stop) {
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("connect");
            sleep(1);
            continue;
        }

        pthread_mutex_lock(&send_mutex);
        while (send_buffer.size == 0 && !stop) {
            pthread_cond_wait(&send_cond, &send_mutex);
        }

        if (stop) {
            pthread_mutex_unlock(&send_mutex);
            break;
        }

        if (send(sockfd, send_buffer.buffer, send_buffer.size, 0) == -1) {
            perror("send");
        }

        send_buffer.size = 0;
        pthread_mutex_unlock(&send_mutex);
    }

    close(sockfd);
    return NULL;
}

void *server_thread(void *arg) {
    int sockfd, clientfd;
    struct sockaddr_un addr;
    char *path = (char *)arg;

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
        pthread_mutex_lock(&recv_mutex);
        recv_buffer.size = recv(clientfd, recv_buffer.buffer, BUFFER_SIZE, 0);
        if (recv_buffer.size == -1) {
            perror("recv");
        } else if (recv_buffer.size > 0) {
            pthread_cond_signal(&recv_cond);
        }
        pthread_mutex_unlock(&recv_mutex);
    }

    close(clientfd);
    close(sockfd);
    unlink(path);
    return NULL;
}

void *brain_thread(void *arg) {
    (void)arg; // Suppress unused parameter warning

    while (!stop) {
        pthread_mutex_lock(&recv_mutex);
        while (recv_buffer.size == 0 && !stop) {
            pthread_cond_wait(&recv_cond, &recv_mutex);
        }

        if (stop) {
            pthread_mutex_unlock(&recv_mutex);
            break;
        }

        pthread_mutex_lock(&send_mutex);
        memcpy(send_buffer.buffer, recv_buffer.buffer, recv_buffer.size);
        send_buffer.size = recv_buffer.size;
        recv_buffer.size = 0;
        pthread_cond_signal(&send_cond);
        pthread_mutex_unlock(&send_mutex);
        pthread_mutex_unlock(&recv_mutex);
    }

    return NULL;
}

void create_threads(pthread_t *client, pthread_t *server, pthread_t *brain, char *path) {
    pthread_create(client, NULL, client_thread, (void *)path);
    pthread_create(server, NULL, server_thread, (void *)path);
    pthread_create(brain, NULL, brain_thread, NULL);
}

int main() {
    signal(SIGUSR1, handle_signal);

    pthread_t client1, server1, brain1;
    pthread_t client2, server2, brain2;

    pid_t pid1, pid2;

    if ((pid1 = fork()) == 0) {
        create_threads(&client1, &server1, &brain1, SOCK_PATH1);
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

    pthread_mutex_destroy(&recv_mutex);
    pthread_mutex_destroy(&send_mutex);
    pthread_cond_destroy(&recv_cond);
    pthread_cond_destroy(&send_cond);

    return 0;
}

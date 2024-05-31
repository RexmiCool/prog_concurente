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
            recv_buffer2.size = recv(clientfd, recv_buffer2.buffer, BUFFER_SIZE, 0);
            if (recv_buffer2.size == -1) {
                perror("recv");
            } else if (recv_buffer2.size > 0) {
                printf("[%s - Server] Received message: %s\n", process_id, recv_buffer2.buffer);
                pthread_cond_signal(&cond2);
                pthread_mutex_lock(&mutex1);
                memcpy(send_buffer1.buffer, recv_buffer2.buffer, recv_buffer2.size);
                send_buffer1.size = recv_buffer2.size;
                pthread_cond_signal(&cond1);
                pthread_mutex_unlock(&mutex1);
            }
            pthread_mutex_unlock(&mutex2);
        } else {
            pthread_mutex_lock(&mutex1);
            recv_buffer1.size = recv(clientfd, recv_buffer1.buffer, BUFFER_SIZE, 0);
            if (recv_buffer1.size == -1) {
                perror("recv");
            } else if (recv_buffer1.size > 0) {
                printf("[%s - Server] Received message: %s\n", process_id, recv_buffer1.buffer);
                pthread_cond_signal(&cond1);
                pthread_mutex_lock(&mutex2);
                memcpy(send_buffer2.buffer, recv_buffer1.buffer, recv_buffer1.size);
                send_buffer2.size = recv_buffer1.size;
                pthread_cond_signal(&cond2);
                pthread_mutex_unlock(&mutex2);
            }
            pthread_mutex_unlock(&mutex1);
        }

        close(clientfd);
    }

    close(sockfd);
    unlink(path);
    return NULL;
}

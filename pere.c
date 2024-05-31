#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NUM_CHILDREN 2

pid_t children[NUM_CHILDREN];

void stop_children(int sig) {
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        kill(children[i], SIGUSR1);
    }
}

void cleanup_and_exit() {
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        waitpid(children[i], NULL, 0);
    }
    printf("Father process cleaning up and exiting.\n");
    exit(0);
}

void signal_handler(int sig) {
    if (sig == SIGINT) {
        stop_children(sig);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting signal handler");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_CHILDREN; ++i) {
        if ((children[i] = fork()) == 0) {
            char arg[2];
            sprintf(arg, "%d", i + 1);
            execl("fils", "fils", arg, NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        }
    }

    atexit(cleanup_and_exit);

    while (1) {
        pause();
    }

    return 0;
}

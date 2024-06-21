#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <string.h>


int main() {

    while (1) {
        fork();
        printf("Hi\n");
    }

    return 0;
}
//fin

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

void fct (int signalRecu, siginfo_t* info, void* inutile){
    switch (signalRecu)
    {
    case SIGUSR1:
        printf("Le processus fils a reçu le signal SIGUSR1, et envoie SIGUSR2 au processus père\n");
        kill(info->si_pid, SIGUSR2);
    case SIGUSR2:
        printf("Le processus père a reçu le signal SIGUSR2, et envoie SIGTERM au processus fils\n");
        kill(info->si_pid, SIGTERM);
    case SIGTERM:
        printf("Le processus fils a reçu le signal SIGTERM, et va donc s'arrêter\n");
        exit(0);
    }
}

int main(int argc, char** argv){
    struct sigaction prepaSignal;
    prepaSignal.sa_sigaction=&fct;
    prepaSignal.sa_flags=SA_SIGINFO;

    pid_t pid = fork();

    switch (pid)
    {
    case -1:
        perror("fork");
        break;
    case 0:
        printf("Processus fils :\n");

        sigaction(SIGUSR1, &prepaSignal, NULL);
        sigaction(SIGTERM, &prepaSignal, NULL);

        while (1)
            pause();
        
        break;
    
    default:
        printf("Process père :\n");

        sleep(2);

        sigaction(SIGUSR2, &prepaSignal, NULL);

        kill(pid, SIGUSR1);

        wait(NULL);

        printf("Process père détecte fin du fils\n");
        break;
    }
}
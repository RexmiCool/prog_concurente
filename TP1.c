#include <string.h>
#include <stdio.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>


typedef struct SharedMem
    {
        int descriptor;
        char* adresse;
        int taille;
    }
    shared_mem;

shared_mem superMalloc (int taille){
    printf("superMalloc start");
    shared_mem m;

    m.descriptor = shmget(IPC_PRIVATE, taille, IPC_CREAT|IPC_EXCL|0600);
    m.adresse = (void*) - 1;
    if (m.descriptor != -1){
        m.adresse = shmat(m.descriptor, NULL, 0);
    }
    printf("superMalloc end");
    return m;
}

int superFree(shared_mem m){
    printf("superFree start");

    int retour = shmdt(m.adresse);
    if (retour != -1){
        retour = shmctl(m.descriptor, IPC_RMID, 0);
    }
    printf("superFree end");
    return retour;
}

int creerSemaphore(int compteur){
    printf("creerSemaphore start");
    int idSem = semget(IPC_PRIVATE, 1, 0600|IPC_CREAT|IPC_EXCL);
    semctl(idSem, 0, SETVAL, compteur);
    printf("creerSemaphore end");
    return idSem;
}

int detruireSemaphore(int idSem){
    printf("detruireSemaphore");

    return semctl(idSem, 0, IPC_RMID, 0);
}


void P(int val){
    printf("P start");
    struct sembuf sem;
    sem.sem_num = 0;
    sem.sem_op = -1;
    sem.sem_flg = 0;
    printf("P end");
    return semop(val, &sem, 1);
}

void V(int val){
    printf("V start");
    struct sembuf sem;
    sem.sem_num = 0;
    sem.sem_op = 1;
    sem.sem_flg = 0;
    printf("V end");
    return semop(val, &sem, 1);
}


void placer(shared_mem mem, char lettre){
    printf("placer start");
    *(mem.adresse + mem.taille-1 - *mem.adresse) = lettre;
    *mem.adresse = (char) ((int) *mem.adresse + 1);
    printf("placer end");
}

char prendre(shared_mem mem){
    printf("prendre start");
    char lettre = *(mem.adresse + mem.taille-1);
    for (int i = mem.taille-1; i > 1; i--){
        *(mem.adresse+i) = *(mem.adresse+i-1);
    }
    *mem.adresse = (char) ((int) *mem.adresse - 1);
    printf("prendre end");
    return lettre;
}


void producteur(shared_mem mem_p, int mutex, int vide, int plein, char* message_to_send){
    printf("producteur start");
    int num_char = 0;
    char lettre;

    do
    {
        lettre = message_to_send[num_char];
        P(vide);
        P(mutex);

        placer(mem_p, lettre);

        V(mutex);
        V(plein);

        num_char++;
    } while (lettre != "/0");

    printf("producteur end");
}

void consomateur(shared_mem mem_p, int mutex, int vide, int plein, char* message_to_receive){
    printf("consomateur start");
    int num_char = 0;
    char lettre;

    do
    {
        P(plein);
        P(mutex);

        lettre = prendre(mem_p);

        V(mutex);
        V(vide);

        message_to_receive[num_char] = lettre;
        num_char++;
    } while (lettre != "/0");

    printf("consomateur end");
}

int main(int argc, char const *argv[])
{
    printf("1");

    int taille = 4;
    
    shared_mem mem = superMalloc(taille + 1);
    int mutex = creerSemaphore(1);
    int plein = creerSemaphore(0);
    int vide = creerSemaphore(taille);
    int etat;
    char *message_to_send = "BONJOUR";

    printf("2");

    pid_t pid = fork();

    switch (pid)
    {
    case -1:
        printf("ERREUR CREATION FILS");
        break;

    case 0:
        producteur(mem, mutex, vide, plein, message_to_send);
        break;
    
    default:
        char message_to_receive[40];
        consomateur(mem, mutex, vide, plein, message_to_receive);
        wait(&etat);
        printf("Chaine Recue : %s", message_to_receive);
        superFree(mem);
        detruireSemaphore(mutex);
        detruireSemaphore(vide);
        detruireSemaphore(plein);
        break;
    }




    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

int taille = 8;

sem_t* plein;
sem_t* vide;
sem_t* mutex;

char* bufferCommun;
int tourProd;
int tourCons;
int nbTour;
char* chaineAEnvoyer;
char* chaineRecue;


void superMalloc (){
    bufferCommun = (void*) malloc(taille+1);
    int index = 0;
    *bufferCommun = 0;
    for (index = 1; index <= taille; index++)
    {
        *(bufferCommun + index) = ' ';
    }
}


void superFree(){
    free(bufferCommun);
}

unsigned int getCompteur(sem_t* sem){
    unsigned int val = 0;
    sem_getvalue(sem, &val);
    return val;
}

sem_t* creerSemaphore(unsigned int compteur){
    sem_t* sem = (sem_t*) malloc(sizeof(sem_t));
    if(sem_init(sem, 0, compteur) == 0){
        printf("Creation de sem avec valeur initiale de %d \n", getCompteur(sem));
    }
    else{
        perror("Echec de cretion de semaphore");
    }
    return sem;
}

int detruireSemaphore(sem_t *sem){
    unsigned int res = sem_destroy(sem);
    free(sem);
    return res;
}


void P(sem_t* val){
    return sem_wait(val);
}

void V(sem_t* val){
    return sem_post(val);
}



void placer(){
    *(bufferCommun + taille - ((int) *bufferCommun)) = *(chaineAEnvoyer+tourProd);
    *bufferCommun = (char) ((int) *bufferCommun + 1);
}

void prendre(){
    char lettre = *(bufferCommun + taille);
    int i;
    for (int i = taille; i > 1; i--){
        *(bufferCommun + i) = *(bufferCommun + i - 1);
    }
    *bufferCommun = (char) ((int) *bufferCommun - 1);


    *(chaineRecue+tourCons) = lettre;
}


void producteur(){
    while (tourProd < nbTour)
    {
        printf("producteur while est executee\n");
        P(vide);
        P(mutex);

        placer();

        V(mutex);
        V(plein);

        tourProd++;
    }
    printf("fin prod\n");

}

void consomateur(){
    printf("debut cons\n");
    while (tourCons < nbTour)
    {
        P(plein);
        P(mutex);

        prendre();

        V(mutex);
        V(vide);

        tourCons++;
    }
    printf("fin cons\n");
}

int main(int argc, char const *argv[])
{
    pthread_t thread_conso;

    plein = creerSemaphore(0);
    vide = creerSemaphore(taille);
    mutex = creerSemaphore(1);

    tourProd = 0;
    tourCons = 0;

    int cumulLongueurs = 0;
    int mot = 1;

    chaineAEnvoyer = "BoNjOuR";

    printf("\n Chaine a envoyer : %s \n", chaineAEnvoyer);

    nbTour = strlen(chaineAEnvoyer);

    superMalloc(taille);

    printf("AVANT\n");
    if(pthread_create(&thread_conso, NULL, (void *(*)()) consomateur, NULL) == -1){
        perror("Cant create thread");
    }
    printf("APRES\n");

    producteur();

    printf("after product\n");

    pthread_join(thread_conso, NULL);
    
    printf("after join\n");

    detruireSemaphore(plein);
    detruireSemaphore(vide);
    detruireSemaphore(mutex);
    superFree();


    return 0;
}
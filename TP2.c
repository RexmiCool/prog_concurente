#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

int taille = 8;

sem_t plein;
sem_t vide;
sem_t mutex;

char* bufferCommun;
int tourProd;
int tourCons;
int nbTour;
char* chaineAEnvoyer;
char* chaineRecue;

void superMalloc (){
    bufferCommun = (char*) malloc(taille);
    chaineRecue = (char*) malloc(taille + 1);
    for (int i = 0; i < taille; i++)
    {
        bufferCommun[i] = ' ';
    }
}

void superFree(){
    free(bufferCommun);
    free(chaineRecue);
}

unsigned int getCompteur(sem_t* sem){
    unsigned int val = 0;
    sem_getvalue(sem, &val);
    return val;
}

void P(sem_t* val){
    sem_wait(val);
}

void V(sem_t* val){
    sem_post(val);
}

void placer(){
    bufferCommun[tourProd % taille] = chaineAEnvoyer[tourProd];
}

void prendre(){
    chaineRecue[tourCons] = bufferCommun[tourCons % taille];
}

void* producteur(void* arg){
    while (tourProd < nbTour)
    {
        P(&vide);
        P(&mutex);

        placer();
        printf("Producteur a placé: %c\n", chaineAEnvoyer[tourProd]);

        V(&mutex);
        V(&plein);

        tourProd++;
    }
    printf("fin prod\n");
    return NULL;
}

void* consomateur(void* arg){
    printf("debut cons\n");
    while (tourCons < nbTour)
    {
        P(&plein);
        P(&mutex);

        prendre();
        printf("Consommateur a pris: %c\n", chaineRecue[tourCons]);

        V(&mutex);
        V(&vide);

        tourCons++;
    }
    printf("fin cons\n");
    return NULL;
}

int main(int argc, char const *argv[])
{
    pthread_t thread_prod, thread_conso;

    sem_init(&plein, 0, 0);
    sem_init(&vide, 0, taille);
    sem_init(&mutex, 0, 1);

    tourProd = 0;
    tourCons = 0;

    chaineAEnvoyer = "BoNjOuR";
    nbTour = strlen(chaineAEnvoyer);

    superMalloc();

    printf("\n Chaine a envoyer : %s \n", chaineAEnvoyer);

    pthread_create(&thread_conso, NULL, consomateur, NULL);
    pthread_create(&thread_prod, NULL, producteur, NULL);

    pthread_join(thread_prod, NULL);
    pthread_join(thread_conso, NULL);

    printf("Chaine recue: %s\n", chaineRecue);

    sem_destroy(&plein);
    sem_destroy(&vide);
    sem_destroy(&mutex);
    superFree();

    return 0;
}

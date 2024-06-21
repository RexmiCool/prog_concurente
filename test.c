#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

#define N 4  // Taille du buffer commun

// Sémaphores
sem_t *plein, *vide, *mutex;

// Buffer commun et ses indices
char bufferCommun[N];
int nbElem = 0;  // Nombre d'éléments dans le buffer

// Fonctions utilitaires pour les sémaphores
unsigned int getCompteur(sem_t *_semaphore) {
    int valeur = 0;
    sem_getvalue(_semaphore, &valeur);
    return valeur;
}

sem_t *creerSemaphore(unsigned int _compteur) {
    sem_t *semaphore = (sem_t *)malloc(sizeof(sem_t));

    if (sem_init(semaphore, 0, _compteur) == 0) {
        printf("Création d'un semaphore ayant pour valeur initiale %d\n", getCompteur(semaphore));
    } else {
        perror("Échec dans la création du sémaphore ! \n");
    }
    return semaphore;
}

int detruireSemaphore(sem_t *_semaphore) {
    int resultat = sem_destroy(_semaphore);
    free(_semaphore);
    return resultat;
}

int P(sem_t *_semaphore) {
    return sem_wait(_semaphore);
}

int V(sem_t *_semaphore) {
    return sem_post(_semaphore);
}

// Fonctions pour manipuler le buffer commun
void addElement(char _caractereAAjouter) {
    bufferCommun[nbElem] = _caractereAAjouter;
    nbElem++;
}

char getElement() {
    nbElem--;
    return bufferCommun[nbElem];
}

char *getAffichageDuBufferCommun() {
    static char chaineDeCaracteres[N * 2 + 4]; // [N caractères + N-1 virgules + 2 crochets + 1 '\0']
    int indexDansLaChaineDeCaracteres = 0;
    int indexDansLeBufferCommun;
    int indexDuProchainCaractereDansLeBufferCommun = N - nbElem;

    chaineDeCaracteres[indexDansLaChaineDeCaracteres++] = '[';

    for (indexDansLeBufferCommun = 0; indexDansLeBufferCommun < N; indexDansLeBufferCommun++) {
        if (indexDansLeBufferCommun >= indexDuProchainCaractereDansLeBufferCommun) {
            chaineDeCaracteres[indexDansLaChaineDeCaracteres++] = bufferCommun[indexDansLeBufferCommun];
        } else {
            chaineDeCaracteres[indexDansLaChaineDeCaracteres++] = ' ';
        }
        if (indexDansLeBufferCommun < N - 1) {
            chaineDeCaracteres[indexDansLaChaineDeCaracteres++] = ',';
        }
    }

    chaineDeCaracteres[indexDansLaChaineDeCaracteres++] = ']';
    chaineDeCaracteres[indexDansLaChaineDeCaracteres] = '\0';

    return chaineDeCaracteres;
}

// Fonctions exécutées par les threads

void *serveur(void *arg) {
    while (1) {
        char item = 'A' + rand() % 26;  // Produit un caractère aléatoire

        P(vide);  // Attente d'une place vide
        P(mutex); // Accès exclusif au buffer

        addElement(item);
        printf("[Serveur] Produit : %c\n", item);

        V(mutex); // Libération de l'accès au buffer
        V(plein); // Indique qu'il y a un élément de plus dans le buffer

        usleep(rand() % 1000000);  // Délai aléatoire pour la production
    }
    return NULL;
}

void *cerveau(void *arg) {
    while (1) {
        P(plein);  // Attente d'un élément à consommer
        P(mutex);  // Accès exclusif au buffer

        char item = getElement();
        printf("[Cerveau] Consomme : %c\n", item);

        V(mutex);  // Libération de l'accès au buffer
        V(vide);   // Indique qu'il y a une place vide de plus dans le buffer

        usleep(rand() % 1000000);  // Délai aléatoire pour la consommation
    }
    return NULL;
}

void *client(void *arg) {
    while (1) {
        printf("[Client] État actuel du buffer : %s\n", getAffichageDuBufferCommun());
        usleep(500000);  // Délai pour rafraîchir l'affichage
    }
    return NULL;
}

int main() {
    pthread_t serveur1, serveur2, serveur3;
    pthread_t cerveau1, cerveau2, cerveau3;
    pthread_t client1, client2, client3;

    srand(time(NULL));  // Initialisation de la graine pour les nombres aléatoires

    // Initialisation des sémaphores
    plein = creerSemaphore(0);
    vide = creerSemaphore(N);
    mutex = creerSemaphore(1);

    // Création des threads
    pthread_create(&serveur1, NULL, serveur, NULL);
    pthread_create(&serveur2, NULL, serveur, NULL);
    pthread_create(&serveur3, NULL, serveur, NULL);

    pthread_create(&cerveau1, NULL, cerveau, NULL);
    pthread_create(&cerveau2, NULL, cerveau, NULL);
    pthread_create(&cerveau3, NULL, cerveau, NULL);

    pthread_create(&client1, NULL, client, NULL);
    pthread_create(&client2, NULL, client, NULL);
    pthread_create(&client3, NULL, client, NULL);

    // Attente des threads (en pratique, on ne termine jamais dans ce modèle)
    pthread_join(serveur1, NULL);
    pthread_join(serveur2, NULL);
    pthread_join(serveur3, NULL);

    pthread_join(cerveau1, NULL);
    pthread_join(cerveau2, NULL);
    pthread_join(cerveau3, NULL);

    pthread_join(client1, NULL);
    pthread_join(client2, NULL);
    pthread_join(client3, NULL);

    // Destruction des sémaphores
    detruireSemaphore(plein);
    detruireSemaphore(vide);
    detruireSemaphore(mutex);

    return 0;
}

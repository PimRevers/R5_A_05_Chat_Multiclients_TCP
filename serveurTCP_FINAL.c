#include <arpa/inet.h> // Structures et conversions réseau (htons, struct sockaddr_in)
#include <netdb.h>     // Résolution d'adresses (gethostbyname, struct hostent)
#include <pthread.h>   // Threads et mutex POSIX (pthread_create, pthread_detach, etc.)
#include <signal.h>    // Gestion des signaux (sigaction, SIGINT)
#include <stdio.h>     // Entrées / sorties (printf, perror)
#include <stdlib.h>    // Fonctions standards (malloc, free, exit, EXIT_SUCCESS, EXIT_FAILURE)
#include <string.h>    // Manipulation de chaînes (strlen, strcat)
#include <sys/socket.h>// Sockets POSIX (socket, bind, listen, accept)
#include <unistd.h>    // Appels système POSIX (read, write, close)

// Port TCP sur lequel le serveur écoute
#define PORTSERV 64100

#define MAX_CLIENTS 50

// Liste des sockets des clients connectés
int clients[MAX_CLIENTS];
int nb_clients = 0;
pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;

#define MAX_HISTORIQUE 1000 // Taille maximale de l'historique des messages

// Historique partagé entre tous les threads
char historique_global[MAX_HISTORIQUE] = ""; 
pthread_mutex_t mutex_histo = PTHREAD_MUTEX_INITIALIZER;

// Ctrl + C : arrêt propre du serveur
static void corSignalHandler(int li_sigNum) {
    if (li_sigNum == SIGINT) {
        write(STDOUT_FILENO, "\n[SIGINT capturé] Fermeture propre du serveur!\n", 49);
        _exit(EXIT_SUCCESS);
    }
}

static void *traiter_client(void *arg) {
    int scom = *(int *)arg;
    free(arg);
    pthread_detach(pthread_self());
    
    printf("[Thread %lu] Nouveau client connecté.\n", (unsigned long)pthread_self());

    // Enregistrement du client pour la diffusion
    pthread_mutex_lock(&mutex_clients);
    if (nb_clients < MAX_CLIENTS) {
        clients[nb_clients++] = scom;
    }
    pthread_mutex_unlock(&mutex_clients);

    // Envoi de l'historique actuel dès la connexion
    pthread_mutex_lock(&mutex_histo);
    int taille_histo = (int)strlen(historique_global);
    write(scom, &taille_histo, sizeof(taille_histo));
    if (taille_histo > 0) {
        write(scom, historique_global, taille_histo);
    }
    pthread_mutex_unlock(&mutex_histo);

    // Boucle de réception des messages
    int nb_caractere = 0;
    while (read(scom, &nb_caractere, sizeof(nb_caractere)) > 0) {
        // Allocation dynamique demandée : taille exacte reçue
        char *buffer = (char *)malloc(nb_caractere + 1);
        if (!buffer) {
            perror("Erreur malloc buffer");
            break;
        }

        int lus = read(scom, buffer, nb_caractere);
        if (lus <= 0) {
            free(buffer);
            break;
        }
        buffer[lus] = '\0';
        printf("[Thread %lu] Reçu : \"%s\"\n", (unsigned long)pthread_self(), buffer);

        // Sauvegarde dans l'historique global partagé
        pthread_mutex_lock(&mutex_histo);
        if (strlen(historique_global) + strlen(buffer) + 2 < MAX_HISTORIQUE) {
            strcat(historique_global, buffer);
            strcat(historique_global, "\n");
        }
        pthread_mutex_unlock(&mutex_histo);

        // Diffusion (broadcast) à tous les clients connectés SAUF l'expéditeur
        pthread_mutex_lock(&mutex_clients);
        for (int i = 0; i < nb_clients; i++) {
            if (clients[i] != scom) {
                write(clients[i], buffer, lus);
            }
        }
        pthread_mutex_unlock(&mutex_clients);

        free(buffer);
    }

    // Retrait du client lors de la déconnexion
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i] == scom) {
            clients[i] = clients[nb_clients - 1]; // Remplace par le dernier
            nb_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);

    printf("[Thread %lu] Client déconnecté.\n", (unsigned long)pthread_self());
    close(scom);
    return NULL;
}

int main(void) {
    int sc;   // Socket d'écoute (maître)
    int scom; // Socket de communication dédiée au client accepté

    // Configuration de l'adresse du serveur
    struct sockaddr_in sin = {
        .sin_family = AF_INET,
        .sin_port = htons(PORTSERV),
        .sin_addr.s_addr = INADDR_ANY
    };

    // Configuration du gestionnaire pour Ctrl+C (SIGINT)
    struct sigaction ls_sa;
    sigemptyset(&ls_sa.sa_mask);
    ls_sa.sa_flags = 0;
    ls_sa.sa_handler = corSignalHandler;
    if (sigaction(SIGINT, &ls_sa, NULL) < 0) {
        perror("Erreur sigaction");
    }

    signal(SIGPIPE, SIG_IGN);

    // Création de la socket d'écoute
    if ((sc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erreur création socket d'écoute");
        exit(1);
    }

    // SO_REUSEADDR : évite l'erreur "Address already in use"
    int opt = 1;
    if (setsockopt(sc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Erreur setsockopt SO_REUSEADDR");
    }

    // Attachement de la socket au port (bind)
    if (bind(sc, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Erreur bind");
        close(sc);
        exit(1);
    }

    // Mise en écoute (listen)
    if (listen(sc, 5) < 0) {
        perror("Erreur listen");
        close(sc);
        exit(1);
    }
    printf("Serveur de tchat (Threads + Historique) en écoute sur le port %d...\n", PORTSERV);

    // Boucle principale d'acceptation des clients
    while (1) {
        if ((scom = accept(sc, NULL, NULL)) < 0) {
            continue;
        }

        // Allocation dynamique pour transmettre scom de manière sûre au thread
        int *arg_scom = malloc(sizeof(int));
        if (!arg_scom) {
            perror("Erreur malloc arg_scom");
            close(scom);
            continue;
        }
        *arg_scom = scom;

        // Création du thread dédié au client
        pthread_t th;
        if (pthread_create(&th, NULL, traiter_client, arg_scom) != 0) {
            perror("Erreur pthread_create");
            free(arg_scom);
            close(scom);
            continue;
        }
    }

    close(sc);
    return 0;
}
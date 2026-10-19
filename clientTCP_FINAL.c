#include <arpa/inet.h> // Structures et conversions réseau (htons, struct sockaddr_in)
#include <netdb.h>     // Résolution d'adresses (gethostbyname, struct hostent)
#include <signal.h>    // Gestion des signaux (kill, SIGTERM)
#include <stdio.h>     // Entrées / sorties (printf, perror, fgets)
#include <stdlib.h>    // Fonctions standards (malloc, free, exit, EXIT_SUCCESS, EXIT_FAILURE)
#include <string.h>    // Manipulation de chaînes (strlen, strcpy, strcspn, strcmp)
#include <sys/socket.h>// Sockets POSIX (socket, connect, shutdown)
#include <sys/wait.h>  // Synchronisation des processus (waitpid)
#include <unistd.h>    // Appels système POSIX (read, write, close, fork, getppid)

// Port TCP sur lequel le serveur écoute
#define PORTSERV 64100
// Taille maximale de la saisie utilisateur
#define TAILLE_MAX 512

int main(int argc, char *argv[]) {

    char *hote = (argc >= 2) ? argv[1] : "localhost";

    struct hostent *hp = gethostbyname(hote);
    if (hp == NULL) {
        herror("Erreur résolution hôte");
        exit(EXIT_FAILURE);
    }

    // Préparation coordonnées du serveur
    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(PORTSERV),
        .sin_addr = *(struct in_addr *)hp->h_addr
    };

    // Création de la socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erreur création socket");
        exit(EXIT_FAILURE);
    }

    // Connexion au serveur
    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("Erreur connect");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Réception de l'historique initial
    int taille_histo = 0;
    if (read(sock, &taille_histo, sizeof(taille_histo)) > 0 && taille_histo > 0) {
        char *historique = malloc(taille_histo + 1);
        if (historique) {
            int lus = read(sock, historique, taille_histo);
            if (lus > 0) {
                historique[lus] = '\0';
                printf("=== HISTORIQUE DES MESSAGES ===\n%s===============================\n", historique);
            }
            free(historique);
        }
    } else {
        printf("(Aucun message dans l'historique)\n");
    }

    // Séparation : un processus pour l'envoi, un pour la réception
    pid_t pid = fork();
    if (pid < 0) {
        perror("Erreur fork");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // --- PROCESSUS FILS : Saisie et envoi au serveur ---
        char saisie[TAILLE_MAX];

        while (1) {
            printf("\nMessage (\"quit\" pour quitter) : ");
            if (fgets(saisie, sizeof(saisie), stdin) == NULL) {
                break;
            }

            saisie[strcspn(saisie, "\r\n")] = '\0'; // Retrait du retour chariot

            if (strcmp(saisie, "quit") == 0) {
                printf("Déconnexion...\n");
                break;
            }

            int nb_caractere = (int)strlen(saisie);
            if (nb_caractere == 0) {
                continue;
            }

            // Allocation dynamique demandée : allocation exacte pour la chaîne
            char *message = (char *)malloc(nb_caractere + 1);
            if (!message) {
                perror("Erreur malloc message");
                break;
            }
            strcpy(message, saisie);

            // 1. Envoi du nombre de caractères puis 2. Envoi du message
            if (write(sock, &nb_caractere, sizeof(nb_caractere)) <= 0 ||
                write(sock, message, nb_caractere) <= 0) {
                perror("Erreur écriture socket");
                free(message);
                break;
            }
            free(message);
        }

        // Fin de l'envoi : on ferme l'écriture et prévient le père
        shutdown(sock, SHUT_WR);
        kill(getppid(), SIGTERM);
        close(sock);
        exit(EXIT_SUCCESS);
    } else {
        // --- PROCESSUS PÈRE : Écoute et affichage des messages en direct ---
        char buffer[TAILLE_MAX];
        int octets_lus;

        while ((octets_lus = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[octets_lus] = '\0';
            printf("\n[Nouveau message] : %s\n", buffer);
            printf("Message (\"quit\" pour quitter) : ");
            fflush(stdout);
        }

        close(sock);
        kill(pid, SIGTERM);    // Arrête le fils s'il attendait une saisie
        waitpid(pid, NULL, 0); // Évite un processus zombie
        return EXIT_SUCCESS;
    }
}
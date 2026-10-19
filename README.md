# Chat Multi-Clients TCP en C

Ce projet implémente un système de messagerie instantanée (tchat) multi-clients en langage C sous environnement Linux/POSIX, basé sur des sockets TCP.

Il illustre les concepts clés de la programmation système et réseau avancée :
- **Sockets TCP (IPv4)** pour la communication réseau en mode connecté.
- **Multithreading (`pthread`)** côté serveur pour gérer plusieurs clients en simultané sans blocage.
- **Synchronisation par mutex (`pthread_mutex_t`)** pour protéger les ressources partagées (liste des clients, historique global).
- **Multiprocessus (`fork`)** côté client pour découpler l'écoute des messages réseau de la saisie utilisateur.
- **Gestion des signaux POSIX** (`SIGINT`, `SIGTERM`, `SIGPIPE`) pour une terminaison propre et sécurisée.

---

## Sommaire

1. [Principe de fonctionnement](#principe-de-fonctionnement)
   - [Architecture globale](#architecture-globale)
   - [Le Serveur (Multi-thread)](#le-serveur-multi-thread)
   - [Le Client (Multi-processus)](#le-client-multi-processus)
   - [Protocole de communication](#protocole-de-communication)
2. [Structure du projet](#structure-du-projet)
3. [Compilation](#compilation)
4. [Guide d'utilisation](#guide-dutilisation)
   - [1. Démarrer le serveur](#1-démarrer-le-serveur)
   - [2. Démarrer un ou plusieurs clients](#2-démarrer-un-ou-plusieurs-clients)
   - [3. Échanger des messages](#3-échanger-des-messages)
   - [4. Quitter le chat](#4-quitter-le-chat)
5. [Détails techniques et robustesse](#détails-techniques-et-robustesse)

---

## Principe de fonctionnement

### Architecture globale

L'application repose sur le modèle client-serveur :
- Le **serveur** centralise la connexion, maintient la liste des clients actifs, enregistre un historique et rediffuse les messages reçus à l'ensemble des participants.
- Chaque **client** se connecte au serveur, reçoit l'historique des discussions antérieures, puis peut envoyer et recevoir des messages en direct.

```
       +---------------------------------------------+
       |             SERVEUR TCP                     |
       |  Port : 64100                               |
       |  - Thread principal : accept()              |
       |  - 1 Thread détaché par client              |
       |  - Historique global (protégé par mutex)    |
       |  - Diffusion (broadcast)                    |
       +----------------------+----------------------+
                              |
      +-----------------------+-----------------------+
      | (Socket TCP)                                  | (Socket TCP)
      v                                               v
+-----------------------------+         +-----------------------------+
|          CLIENT 1           |         |          CLIENT 2           |
|  - Processus Père (Écoute)  |         |  - Processus Père (Écoute)  |
|  - Processus Fils (Saisie)  |         |  - Processus Fils (Saisie)  |
+-----------------------------+         +-----------------------------+
```

---

### Le Serveur (Multi-thread)

Fichier : `serveurTCP_FINAL.c`

1. **Initialisation** : 
   - Crée une socket de flux (`SOCK_STREAM`) liée au port `64100` (`PORTSERV`).
   - Applique l'option `SO_REUSEADDR` pour pouvoir redémarrer immédiatement le serveur sans attendre l'expiration du délai `TIME_WAIT`.
   - Ignore le signal `SIGPIPE` pour éviter l'arrêt brutal du serveur lorsqu'un client se déconnecte inopinément.
2. **Acceptation concurrente** :
   - La boucle principale attend les connexions entrantes avec `accept()`.
   - Pour chaque client, un nouveau thread POSIX indépendant est lancé (`pthread_create`), puis détaché (`pthread_detach`) afin de libérer automatiquement ses ressources à sa terminaison.
3. **Synchronisation** :
   - `mutex_clients` : protège le tableau `clients[]` (limité à `MAX_CLIENTS = 50`) lors des ajouts, déconnexions et diffusions.
   - `mutex_histo` : protège le tampon `historique_global` (taille max de 1000 caractères).
4. **Diffusion (Broadcast)** :
   - Lorsqu'un message est reçu d'un client, le thread l'ajoute à l'historique, puis l'envoie à tous les autres clients connectés (`clients[i] != scom`). L'expéditeur ne reçoit pas son propre écho.

---

### Le Client (Multi-processus)

Fichier : `clientTCP_FINAL.c`

Pour permettre la saisie au clavier et la réception asynchrone des messages sans blocage d'E/S, le client utilise `fork()` après s'être connecté :

- **Processus Fils (Émission / Saisie)** :
  - Lit la saisie de l'utilisateur sur `stdin` via `fgets()`.
  - Effectue une allocation dynamique à la taille exacte de la chaîne saisie.
  - Envoie d'abord la taille du texte, puis le contenu du message.
  - Si l'utilisateur tape `quit`, ferme l'écriture de la socket (`shutdown`), envoie `SIGTERM` au père et se termine.
- **Processus Père (Réception / Affichage)** :
  - Écoute en continu la socket TCP via `read()`.
  - Dès qu'un message arrive, l'affiche instantanément à l'écran (`[Nouveau message] : ...`).
  - Si la connexion est rompue ou le serveur arrêté, interrompt le processus fils avec `kill(pid, SIGTERM)` et l'attend avec `waitpid()` afin d'éviter tout processus zombie.

---

### Protocole de communication

1. **Phase de bienvenue (Historique)** :
   - Dès la connexion, le serveur envoie un entier `sizeof(int)` représentant la taille en octets de l'historique disponible.
   - Si cette taille est supérieure à 0, le serveur envoie ensuite la chaîne brute de l'historique, que le client lit et affiche.
2. **Phase d'échange de messages** :
   - Chaque message est transmis en deux étapes :
     1. Un entier `nb_caractere` (`sizeof(int)`) indiquant la longueur de la chaîne.
     2. Le tampon de `nb_caractere` octets contenant le texte.
   - Cette approche évite les problèmes de découpage de flux TCP (framing) et permet au récepteur d'allouer dynamiquement la taille exacte en mémoire (`malloc(nb_caractere + 1)`).

---

## Structure du projet

```text
chat_multiclients_tcp/
├── clientTCP_FINAL.c    # Code source du client (fork, saisie & affichage)
├── serveurTCP_FINAL.c   # Code source du serveur (threads, mutex & broadcast)
├── clientTCP_FINAL      # Binaire exécutable du client (généré)
├── serveurTCP_FINAL     # Binaire exécutable du serveur (généré)
├── .gitignore           # Règles d'exclusion Git
└── README.md            # Documentation du projet
```

---

## Compilation

Le projet utilise le compilateur `gcc` avec la bibliothèque de threads POSIX (`-lpthread`).

### Compiler le serveur
```bash
gcc -Wall -Wextra serveurTCP_FINAL.c -o serveurTCP_FINAL -lpthread
```

### Compiler le client
```bash
gcc -Wall -Wextra clientTCP_FINAL.c -o clientTCP_FINAL
```

---

## Guide d'utilisation

### 1. Démarrer le serveur

Dans un premier terminal, lancez le serveur :

```bash
./serveurTCP_FINAL
```

Le serveur affiche :
```text
Serveur de tchat (Threads + Historique) en écoute sur le port 64100...
```

---

### 2. Démarrer un ou plusieurs clients

Ouvrez un ou plusieurs autres terminaux pour lancer les clients.

#### Connexion en local (sur la même machine) :
```bash
./clientTCP_FINAL
# ou explicitement :
./clientTCP_FINAL localhost
```

#### Connexion à distance (sur une autre machine du réseau) :
Indiquez l'adresse IP ou le nom d'hôte de la machine exécutant le serveur :
```bash
./clientTCP_FINAL 192.168.1.42
```

À la connexion, le client affiche l'historique des échanges précédents (s'il existe) puis propose l'invite de saisie :
```text
(Aucun message dans l'historique)

Message ("quit" pour quitter) : 
```

---

### 3. Échanger des messages

- Tapez votre message dans le terminal du client et appuyez sur **Entrée**.
- Le message est envoyé au serveur, enregistré dans l'historique et diffusé en temps réel aux autres clients connectés.
- Les autres clients voient apparaître le message immédiatement :
  ```text
  [Nouveau message] : Bonjour tout le monde !
  Message ("quit" pour quitter) : 
  ```

---

### 4. Quitter le chat

- **Côté Client** :
  - Tapez `quit` dans la saisie du message :
    ```text
    Message ("quit" pour quitter) : quit
    Déconnexion...
    ```
  - Le client se déconnecte proprement et libère les processus.
- **Côté Serveur** :
  - Appuyez sur **Ctrl + C** dans le terminal du serveur.
  - Le gestionnaire de signal `corSignalHandler` intercepte `SIGINT` et assure une sortie propre :
    ```text
    ^C
    [SIGINT capturé] Fermeture propre du serveur!
    ```

---

## Détails techniques et robustesse

| Mécanisme | Rôle / Avantage |
| :--- | :--- |
| **`pthread_detach`** | Libération automatique des ressources allouées à chaque thread sans nécessiter de `pthread_join`. |
| **`SO_REUSEADDR`** | Réutilisation immédiate du port `64100` après redémarrage du serveur. |
| **`signal(SIGPIPE, SIG_IGN)`** | Empêche le crash du serveur si un client se déconnecte brutalement pendant une écriture réseau. |
| **`sigaction(SIGINT, ...)`** | Interception propre de l'arrêt par `Ctrl+C` avec vidage et sortie immédiate. |
| **`fork()` + `SIGTERM`** | Séparation claire lecture/écriture dans le client et destruction propre des deux processus à la sortie. |
| **Allocation dynamique** | Allocation exacte mémoire (`malloc`) selon la taille reçue, libérée dès diffusion (`free`). |

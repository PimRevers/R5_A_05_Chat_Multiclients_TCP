# Chat Multi-Clients TCP en C

Ce projet implémente un système de messagerie instantanée (tchat) multi-clients en langage C sous environnement Linux (développé et testé sous **WSL - Windows Subsystem for Linux**), basé sur les sockets TCP.

---

## 📌 Fonctionnalités implémentées

* **Chat multi-clients en temps réel** : plusieurs clients peuvent se connecter simultanément au serveur et échanger des messages.
* **Diffusion (Broadcast)** : lorsqu'un client envoie un message, le serveur le redistribue automatiquement à tous les autres clients connectés.
* **Historique des messages partagé** :
  * Tous les messages envoyés sont enregistrés côté serveur dans un historique partagé.
  * Dès qu'un nouveau client se connecte, il reçoit automatiquement la totalité de l'historique des discussions précédentes.
* **Communication asynchrone côté client (`fork`)** : séparation entre la saisie clavier (processus fils) et la réception des messages réseau (processus père) pour ne jamais bloquer l'affichage.
* **Concurrence côté serveur (`threads POSIX`)** : chaque client est pris en charge par un thread dédié avec exclusion mutuelle (`mutex`) pour protéger les données partagées (liste des sockets, historique).
* **Déconnexion propre** : saisie de `quit` pour fermer la session client proprement, et gestion des signaux (`Ctrl + C` / `SIGINT`, `SIGTERM`, `SIGPIPE`).

---

## ⚠️ Fonctionnalités non implémentées

Dans le cadre de cette version du projet :
* **Pas de gestion des channels / salons** : tous les utilisateurs échangent dans un salon unique sur un seul port. Le choix et la création dynamique de canaux associés à des ports distincts n'ont pas été réalisés.
* **Pas de gestion de pseudos (noms d'utilisateurs)** : les messages sont diffusés sous la forme brute reçue (sans identifiant ou préfixe de pseudo pour l'émetteur).

---

## 💻 Environnement de développement

Le projet a été développé et testé sous **WSL** (*Windows Subsystem for Linux*), une machine virtuelle Linux intégrée sous Windows, avec les outils standards GCC et POSIX.

---

## 🛠️ Compilation

Pour compiler le code source, ouvrez un terminal Linux (ou WSL) dans le dossier du projet et utilisez le compilateur `gcc` avec l'option `-Wall` (qui active les avertissements pour vérifier la qualité du code).

### 1. Compiler le serveur
Le serveur utilise la bibliothèque de threads POSIX, on ajoute donc `-lpthread` :
```bash
gcc -Wall serveurTCP_FINAL.c -o serveurTCP_FINAL -lpthread
```

### 2. Compiler le client
```bash
gcc -Wall clientTCP_FINAL.c -o clientTCP_FINAL
```

Ces commandes génèrent les deux fichiers exécutables compilés :
* `serveurTCP_FINAL`
* `clientTCP_FINAL`

---

## 🚀 Utilisation / Exécution

Pour lancer les programmes, il suffit d'écrire le nom du fichier exécutable précédé de `./` dans le terminal.

### Étape 1 : Lancer le serveur
Dans un premier terminal, exécutez le serveur :
```bash
./serveurTCP_FINAL
```
Le serveur se met en écoute sur le port `64100` :
```text
Serveur de tchat (Threads + Historique) en écoute sur le port 64100...
```

### Étape 2 : Lancer un ou plusieurs clients
Ouvrez un nouveau terminal (ou plusieurs pour simuler plusieurs utilisateurs) et lancez le client :

* **Sur la même machine (en local) :**
```bash
./clientTCP_FINAL
```
*(ou en précisant l'adresse : `./clientTCP_FINAL localhost`)*

* **Depuis une autre machine sur le réseau :**
```bash
./clientTCP_FINAL <adresse_ip_du_serveur>
```

À la connexion, le client affiche l'historique des discussions précédentes (s'il y en a) puis invite à saisir un message :
```text
=== HISTORIQUE DES MESSAGES ===
Bonjour !
Bienvenue sur le chat.
===============================

Message ("quit" pour quitter) : 
```

### Étape 3 : Échanger et quitter
* Tapez votre message et appuyez sur **Entrée** : il sera instantanément envoyé au serveur et affiché chez les autres participants.
* Pour quitter le chat proprement, tapez simplement :
```text
quit
```
* Pour arrêter le serveur, faites la combinaison de touches **Ctrl + C** dans son terminal.

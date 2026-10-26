# bhttpd

Serveur HTTP/1.0 en C (module Programmation Systeme et Reseaux). Il ecoute sur un port, lit une requete, repond.
Mon port HTTP : **26130** (le port SSH 13022 sert seulement a se connecter a iot).

## Environnements

- **Local** : WSL2 Debian sur mon PC. Projet dans `~/bhttpd` (jamais dans /mnt/c).
- **Distant** : `iot.devinci.fr`, accessible par `ssh iot`. Machine partagee, utiliser uniquement mon port.

Arborescence (identique des deux cotes pour le projet) :

    ~/bhttpd/Makefile
    ~/bhttpd/src/bhttpd.c
    ~/opt/bhttpd/{etc,sbin,srv,var/log}     (distant : installation)

## 1. Developper

    cd ~/bhttpd
    code .                  # ouvre VS Code dans WSL (barre en bas : "WSL: Debian")

Modifier `src/bhttpd.c`, puis `Ctrl+S`. Terminal integre de VS Code : `Ctrl+`` (c'est un terminal Linux).

## 2. Compiler et tester en local

    make                              # gcc -Wall -Werror -> ./bhttpd
    ./bhttpd -d -p 9000               # lance le serveur (bloque : normal). Ctrl+C pour l'arreter

Dans un 2e terminal (bouton + du terminal VS Code) :

    echo "GET /index.html HTTP/1.0" | nc localhost 9000      # attendu : 200
    echo "FOO /index.html HTTP/1.0" | nc localhost 9000      # attendu : 400
    echo "BLABLA" | nc localhost 9000                        # attendu : 400
    curl --http1.0 -i http://localhost:9000/                 # attendu : 200

Attention : `curl` sans `--http1.0` envoie `HTTP/1.1`, donc recoit un 400 (le serveur n'accepte que HTTP/1.0).

Navigateur Windows : http://localhost:9000/ (un navigateur parle en HTTP/1.1 : il recevra un 400, c'est normal pour l'instant).

Tester les options :

    ./bhttpd -h ; echo $?             # aide, 0
    ./bhttpd -x ; echo $?             # "Invalid option" + aide, != 0
    ./bhttpd -p abc ; echo $?         # "Invalid argument", != 0

Options : `-c` fichier de config, `-d` debug (affiche les requetes), `-h` aide, `-l` fichier de logs,
`-p` port, `-s` fichier socket UNIX. (`-c`, `-l`, `-s` sont lues mais pas encore utilisees.)
Sans `-p`, le port par defaut est 26130.

## 3. Envoyer et deployer sur iot (depuis WSL)

    scp ~/bhttpd/src/bhttpd.c iot:bhttpd/src/
    scp ~/bhttpd/Makefile iot:bhttpd/
    ssh iot 'cd bhttpd && make && make install'

`scp` copie, `make` compile sur iot (obligatoire), `make install` copie le binaire dans `~/opt/bhttpd/sbin/`.

## 4. Lancer et tester sur iot

Si un ancien serveur tourne (ancien binaire), l'arreter avant : `pkill -u $USER bhttpd`.

    ssh iot
    tmux new -s bhttpd                          # session qui survit a la deconnexion
    ~/opt/bhttpd/sbin/bhttpd -p 26130 -d        # lance le serveur
    # Ctrl+B puis D : detacher (le serveur continue)
    # tmux attach -t bhttpd : revenir ; Ctrl+C dedans : arreter

Tester depuis iot (le port n'est pas joignable depuis l'exterieur) :

    echo "GET /index.html HTTP/1.0" | nc localhost 26130
    echo "FOO /index.html HTTP/1.0" | nc localhost 26130
    curl --http1.0 -i http://localhost:26130/

Voir la page dans le navigateur Windows (tunnel SSH, a lancer depuis WSL et laisser ouvert) :

    ssh -L 9000:localhost:26130 iot
    # puis http://localhost:9000/

Verifier / arreter :

    ps -u $USER | grep bhttpd
    ss --family inet --listening --numeric | grep 26130
    pkill -u $USER bhttpd

## 5. Tests automatiques (page web)

Page de mes resultats : http://iot.devinci.fr/psr/eb222724
Identifiant : `eb222724`, mot de passe : C5WQ2t5v
Archives des anciens tests : https://iot.devinci.fr/pub/psr/archives/

Les tests sont lances periodiquement : apres un deploiement, **le serveur doit tourner dans tmux sur iot**, puis recharger la page plus tard.
Une case a `1` = test reussi, `0` = echec.

| Test | Verifie | Etat |
|---|---|---|
| T01 | `~/bhttpd/src/bhttpd.c` existe | ok |
| T02 | `~/bhttpd/Makefile` avec une regle `install` | ok |
| T03 | executable dans `~/opt/bhttpd/sbin` avec droit d'execution | ok |
| T04 | binaire au format ELF 64-bit LSB pie executable | ok |
| T05 | un programme ecoute sur mon port | ok |
| T06 | 200 sur une requete valide | ok |
| T07 | 400 sur une requete invalide | v4 (regex) |
| T08 | 403 sur un fichier existant non lisible | a faire |
| T09 | 404 sur un fichier inexistant | a faire |

Si un test passe a `0` : le reproduire a la main avec les commandes de la section 4 (le plus souvent, le serveur n'est plus lance ou l'ancien binaire tourne encore).

## Nouvelles commandes a retenir

- `tmux new -s nom` / `tmux attach -t nom` / `tmux ls` : sessions persistantes.
- `ssh -L local:localhost:distant iot` : tunnel pour voir le serveur distant dans un navigateur.
- `ss --family inet --listening --numeric` : ports en ecoute.
- `ps -u $USER`, `pkill -u $USER nom` : mes processus uniquement.
- `curl --http1.0 -i` : requete HTTP/1.0 avec les en-tetes de reponse.

## 6. Bloc-notes — Tests bonus et améliorations à réaliser

Pistes à explorer une fois les tests principaux validés :

- Connexion bloquante : tester le cas où un client monopolise le serveur en se connectant sans envoyer de requête, empêchant ainsi le traitement des autres clients.

- Timeout client : mettre en place un délai maximal d'attente pour déconnecter les clients qui restent bloqués sans envoyer de requête.

- Sockets UNIX : implémenter l'option -s pour permettre au serveur d'utiliser un socket UNIX au lieu d'un socket TCP.

- Gestion de plusieurs connexions avec poll() : faire évoluer le serveur pour qu'il puisse surveiller plusieurs sockets et traiter plusieurs clients sans rester bloqué sur un seul.
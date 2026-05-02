/* Célia HAMAM 12309320
	Je déclare qu'il s'agit de mon propre travail.
	Ce travail a été réalisé intégralement par un être humain. 
*/


#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include "buffer/buffer.h"
#include "utils.h"
#include <string.h>
#include <time.h>

#define PORT_FREESCORD 4321
#define BUFF_SIZE 1024

// ==== codes couleurs ANSI ====
// on les met en define pour ne pas les éparpiller partout dans le code
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"

// couleurs pour les pseudos des autres
#define CYAN        "\033[38;5;87m"
// couleur pour nos propres messages (quand on les reçoit en echo)
#define GREEN       "\033[38;5;120m"
// couleur pour le prompt de saisie
#define YELLOW      "\033[38;5;221m"
// couleur pour les messages systeme (connexion, deconnexion, erreurs)
#define GREY        "\033[38;5;245m"
// couleur pour la banniere
#define MAGENTA     "\033[38;5;213m"

// notre pseudo global, on le connait une fois que le serveur a répondu '0'
static char mon_pseudo[17] = {0};

/** se connecter au serveur TCP d'adresse donnée en argument sous forme de
 * chaîne de caractère et au port donné en argument
 * retourne le descripteur de fichier de la socket obtenue ou -1 en cas
 * d'erreur. */
int connect_serveur_tcp(char *adresse, uint16_t port);

// afficher un separateur horizontal discret
static void print_separateur(void)
{
	printf(GREY "  ─────────────────────────────────────────\n" RESET);
}

// afficher l'heure actuelle entre crochets (ex: [14:32])
static void print_heure(void)
{
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	printf(DIM "[%02d:%02d]" RESET " ", tm_info->tm_hour, tm_info->tm_min);
}

// afficher la banniere de bienvenue au lancement du client
static void print_banniere(void)
{
	printf("\n");
	printf(MAGENTA BOLD "  ╔══════════════════════════════════════╗\n" RESET);
	printf(MAGENTA BOLD "  ║          " RESET BOLD "    freescord     " MAGENTA BOLD "          ║\n" RESET);
	printf(MAGENTA BOLD "  ╚══════════════════════════════════════╝\n" RESET);
	printf("\n");
}

// afficher un message recu depuis le serveur en le mettant bien en forme
// on detecte si c'est notre propre message (echo) ou celui d'un autre
static void afficher_message(const char *msg)
{
	// effacer la ligne du prompt en cours pour que le message s'affiche proprement
	printf("\r\033[2K");

	// extraire le pseudo : le message est de la forme "pseudo: contenu"
	const char *sep = strstr(msg, ": ");
	if (sep != NULL) {
		size_t nick_len = (size_t)(sep - msg);
		char nick[17] = {0};
		if (nick_len < sizeof(nick))
			strncpy(nick, msg, nick_len);

		// le contenu du message commence apres ": "
		const char *contenu = sep + 2;

		print_heure();

		// si c'est notre propre message on l'affiche en vert, sinon en cyan
		if (strcmp(nick, mon_pseudo) == 0) {
			printf(GREEN BOLD "  %-16s" RESET GREEN " %s" RESET, nick, contenu);
		} else {
			printf(CYAN BOLD "  %-16s" RESET " %s" RESET, nick, contenu);
		}
	} else {
		// message sans pseudo (messages systeme du serveur)
		print_heure();
		printf(GREY "  %s" RESET, msg);
	}

	fflush(stdout);
}

// afficher le prompt de saisie
static void print_prompt(void)
{
	printf(YELLOW BOLD "\n  ✏  vous > " RESET);
	fflush(stdout);
}

int main(int argc, char *argv[])
{	
	// verifier qu'on a bien une addresse ip en argument
	if (argc < 2) {
		fprintf(stderr, "Usage : %s <adresse_ip> \n", argv[0]);
		return 1;
	}

	print_banniere();

	// se connecter au serveur
	int fd = connect_serveur_tcp(argv[1], PORT_FREESCORD);
	if (fd == -1) return 1;

	printf(GREY "  Connecté au serveur %s:%d\n" RESET, argv[1], PORT_FREESCORD);
	print_separateur();

	// +2 sur buf_envoi pour avoir de la place apres lf_to_crlf (qui ajoute un char)
	char buf_envoi[BUFF_SIZE + 2];
	char buf_reponse[BUFF_SIZE];

	// exo 6: on crée un buffer pour lire depuis la socket de façon bufferisée
	buffer *b = buff_create(fd, BUFF_SIZE);
	if (b == NULL) {
		perror("buff_create");
		close(fd);
		return 1;
	}

	// EXO 7 : lire et afficher le message de bienvenue du serveur
	// le serveur envoie des lignes jusqu'a une ligne vide (juste \r\n)
	while (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) != NULL) {
		crlf_to_lf(buf_reponse);

		// une ligne vide = fin du message de bienvenue
		if (strcmp(buf_reponse, "\n") == 0) break;

		printf(GREY "  %s" RESET, buf_reponse);
	}
	print_separateur();

	// EXO 7 : phase de negociation du nickname
	// le protocole dit que c'est l'utilisateur qui tape "nickname <pseudo>" lui-meme
	// on boucle jusqu'à ce que le serveur reponde '0' (pseudo accepté)
	printf(GREY "  Tapez : nickname <votre_pseudo>\n" RESET);
	print_prompt();

	while (mon_pseudo[0] == '\0') {

		// lire ce que l'utilisateur tape dans le terminal
		if (fgets(buf_envoi, sizeof(buf_envoi), stdin) == NULL) {
			printf(GREY "\n  Abandon.\n" RESET);
			buff_free(b);
			close(fd);
			return 0;
		}

		// exo 6: convertir en CRLF avant d'envoyer au serveur
		lf_to_crlf(buf_envoi);
		size_t len = strlen(buf_envoi);
		if (write(fd, buf_envoi, len) != (ssize_t)len) {
			perror("write");
			buff_free(b);
			close(fd);
			return 1;
		}

		// lire la reponse du serveur (code 0, 1, 2 ou 3 suivi d'un message)
		if (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) == NULL) {
			printf(GREY "  Serveur déconnecté.\n" RESET);
			buff_free(b);
			close(fd);
			return 1;
		}
		crlf_to_lf(buf_reponse);

		if (buf_reponse[0] == '0') {
			// pseudo accepté : on extrait le pseudo depuis ce que l'utilisateur a tapé
			// buf_envoi contient "nickname <pseudo>\r\n" donc on saute "nickname "
			// on enleve d'abord le CRLF qu'on avait ajouté
			crlf_to_lf(buf_envoi);
			size_t envoi_len = strlen(buf_envoi);
			if (envoi_len > 0 && buf_envoi[envoi_len - 1] == '\n')
				buf_envoi[envoi_len - 1] = '\0';

			if (strncmp(buf_envoi, "nickname ", 9) == 0) {
				strncpy(mon_pseudo, buf_envoi + 9, 16);
				mon_pseudo[16] = '\0';
			}

			print_separateur();
			printf(GREEN BOLD "  ✓ Pseudo '%s' accepté !\n" RESET, mon_pseudo);
			print_separateur();
			printf(GREY "  Tapez vos messages ci-dessous. Ctrl+D pour quitter.\n" RESET);
			print_separateur();
			printf("\n");

		} else {
			// pseudo refusé : afficher la réponse du serveur telle quelle (code + message)
			printf(GREY "  ✗ %s" RESET, buf_reponse);
			print_prompt();
		}
	}

	// exo3: on utilise poll pour surveiller stdin et la socket en meme temps
	struct pollfd fds[2];
	// fds[0] = stdin
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	// fds[1] = la socket connectee au serveur
	fds[1].fd = fd;
	fds[1].events = POLLIN;

	print_prompt();

	for(;;) {

		// exo6 : on traite d'abord les lignes deja présentes dans le buffer 
		if (buff_ready(b)) {
			if (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) != NULL) {
				// le protocole freescord envoie du CRLF donc on convertit en LF pour l'affichage unix
				crlf_to_lf(buf_reponse);
				afficher_message(buf_reponse);
				print_prompt();
				continue;
			} else if (buff_eof(b)) {
				printf(GREY "\n  Le serveur a fermé la connexion\n" RESET);
				break;
			}
		}

		// exo 3: poll est bloquant jusqu'à ce qu'un des descripteurs soit pret
		// -1 = on attend a l'infini
		int ret = poll(fds, 2, -1);
		if (ret == -1) {
			perror("poll"); break;
		}

		// cas1 : si stdin a qlq chose à lire
		if (fds[0].revents & POLLIN) {
			if (fgets(buf_envoi, sizeof(buf_envoi), stdin) == NULL) {
				printf(GREY "\n  Déconnexion. À bientôt !\n" RESET);
				print_separateur();
				break;
			}

			// exo 6: le protocole freescord veut des lignes en CRLF 
			// fgets nous donne du LF (unix) donc il faut convertir avant d'envoyer
			lf_to_crlf(buf_envoi);

			// envoyer le message au serveur
			size_t len = strlen(buf_envoi);
			if (write(fd, buf_envoi, len) != (ssize_t)len) {
				perror("write"); break;
			}

			// re-afficher le prompt apres envoi
			print_prompt();
		}

		// cas2: la socket a recu des données du serveur
		if (fds[1].revents & POLLIN) {
			if (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) != NULL) {
				crlf_to_lf(buf_reponse);
				afficher_message(buf_reponse);
				print_prompt();
			} else if (buff_eof(b)) {
				printf(GREY "\n  Le serveur a fermé la connexion\n" RESET);
				break;
			}
		}
	}

	buff_free(b);
	close(fd);
	return 0;
}

int connect_serveur_tcp(char *adresse, uint16_t port)
{
	//creation socket TCP ipv4
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		return -1;
	}

	// remplir la struct d'adresse du serveur
	struct sockaddr_in addr_srv;
	memset(&addr_srv, 0, sizeof(addr_srv));
	addr_srv.sin_family = AF_INET; // ipv4
	addr_srv.sin_port = htons(port); // port

	// convertir l'adresse en binaire et la mettre dans la struct
	int addr_bin = inet_pton(AF_INET, adresse, &addr_srv.sin_addr);
	if (addr_bin == 0) { // adresse non valide
		fprintf(stderr, "adresse non valide : %s \n", adresse);
		close(fd);
		return -1;
	}

	if (addr_bin == -1) { // erreur
		perror("inet_pton");
		close(fd);
		return -1;
	}

	// connect (on etablie la connexion tcp avec le serveur)
	if (connect(fd, (struct sockaddr *)&addr_srv, sizeof(addr_srv)) == -1) {
		perror("connect");
		close(fd);
		return -1;
	}

	return fd; // on retourne le fd de la socket connectee au serveur
}
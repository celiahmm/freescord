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
#include "client_display.h"
#include "utils.h"
#include <string.h>

#define PORT_FREESCORD 4321
#define BUFF_SIZE 1024

//codes couleurs ANSI pour personnaliser l'interface du client
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define GREEN       "\033[38;5;120m"
#define GREY        "\033[38;5;245m"

// notre pseudo global, on le connait une fois que le serveur a répondu '0'
static char mon_pseudo[17] = {0};

/** se connecter au serveur TCP d'adresse donnée en argument sous forme de
 * chaîne de caractère et au port donné en argument
 * retourne le descripteur de fichier de la socket obtenue ou -1 en cas
 * d'erreur. */
int connect_serveur_tcp(char *adresse, uint16_t port);

int main(int argc, char *argv[])
{	
	// verifier qu'on a bien une addresse ip en argument
	if (argc < 2) {
		fprintf(stderr, "Usage : %s <adresse_ip> \n", argv[0]);
		return 1;
	}

	client_print_banner();

	// se connecter au serveur
	int fd = connect_serveur_tcp(argv[1], PORT_FREESCORD);
	if (fd == -1) return 1;

	printf(GREY "  Connecté au serveur %s:%d\n" RESET, argv[1], PORT_FREESCORD);
	client_print_separator();

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
	client_print_separator();

	// EXO 7 : phase du nickname
	// le protocole dit que c'est l'utilisateur qui tape "nickname <pseudo>" lui-meme
	// on boucle jusqu'à ce que le serveur reponde '0' (pseudo accepté)
	printf(GREY "  Tapez : nickname <votre_pseudo>\n" RESET);
	client_print_prompt();

	while (mon_pseudo[0] == '\0') {

		// lire ce que l'utilisateur tape dans le terminal
		if (fgets(buf_envoi, sizeof(buf_envoi), stdin) == NULL) {
			printf("\n");
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
			printf("\r\033[2K" GREY "  Serveur déconnecté.\n\n" RESET);
			fflush(stdout);
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

			client_print_separator();
			printf(GREEN BOLD "  Pseudo '%s' accepté !\n" RESET, mon_pseudo);
			client_print_separator();
			printf(GREY "  Tapez vos messages ci-dessous. Ctrl+D pour quitter.\n" RESET);
			client_print_separator();
			printf("\n");

		} else {
			// pseudo refusé : afficher la reponse du serveur en utilisant
			// la fonction d'affichage (gère les erreurs en rouge)
			client_afficher_message(buf_reponse, mon_pseudo);
			client_print_prompt();
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

	client_print_prompt();

	for(;;) {

		// exo6 : on traite d'abord les lignes deja présentes dans le buffer 
		if (buff_ready(b)) {
			if (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) != NULL) {
				// le protocole freescord envoie du CRLF donc on convertit en LF pour l'affichage unix
				crlf_to_lf(buf_reponse);
				client_afficher_message(buf_reponse, mon_pseudo);
				client_print_prompt();
				continue;
			} else if (buff_eof(b)) {
				printf("\r\033[2K" GREY "\n  Le serveur a fermé la connexion\n\n" RESET);
				fflush(stdout);
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
				printf("\n");
				printf(GREY "\n  Déconnexion. A bientôt !\n" RESET);
				client_print_separator();
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
			client_print_prompt();
		}

		// cas2: la socket a recu des données du serveur
		if (fds[1].revents & POLLIN) {
			if (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) != NULL) {
				crlf_to_lf(buf_reponse);
				client_afficher_message(buf_reponse, mon_pseudo);
				client_print_prompt();
			} else if (buff_eof(b)) {
				printf("\n");
				client_print_separator();
				printf("\r\033[2K" GREY "  Le serveur a fermé la connexion\n" RESET);
				client_print_separator();
				fflush(stdout);
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
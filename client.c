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

#define PORT_FREESCORD 4321
#define BUFF_SIZE 1024


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

	// se connecter au serveur
	int fd = connect_serveur_tcp(argv[1], PORT_FREESCORD);
	if (fd == -1) return 1;
	printf("Connecté au serveur %s sur le port %d \n", argv[1], PORT_FREESCORD);

	char buf_envoi[BUFF_SIZE];
	char buf_reponse[BUFF_SIZE];

// exo 6: on crée un buffer pour lire depuis la socket de façon bufferisée
	buffer *b = buff_create(fd, BUFF_SIZE);
	if (b == NULL) {
		perror("buff_create");
		close(fd);
		return 1;
	}

// exo3: on utilise poll pour surveiller stdin et la socket en meme temps
	struct pollfd fds[2];
	// fds[0] = stdin
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	// fds[1] = la socket connectee au serveur
	fds[1].fd = fd;
	fds[1].events = POLLIN;


	for(;;) {

// exo6 : on traite d'abord les lignes deja présentes dans le buffer 

		if (buff_ready(b)) {
			if (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) != NULL) {
				// le protocole freescord envoie du CRLF donc on convertit en LF pour l'affichage unix
				crlf_to_lf(buf_reponse);

				printf("\r\033[1;36mServeur > \033[0m%s", buf_reponse);
				fflush(stdout);
				continue;
			} else if (buff_eof(b)) {
				printf("\nLe serveur a fermé la connexion\n");
				break;
			}
		}


		printf("\033[1;35mVous > \033[0m");
		fflush(stdout);

// exo 3: poll est bloquant jusqu'à ce qu'un des descripteurs soit pret
		// -1 = on attend a l'infini
		int ret = poll(fds, 2, -1);
		if (ret == -1) {
			perror("poll"); break;
		}

		// cas1 : si stdin a qlq chose à lire
		if (fds[0].revents & POLLIN) {
			if (fgets(buf_envoi, sizeof(buf_envoi), stdin) == NULL) {
				printf("\nFin de saisie (ctrl+D).\nDeconnexion.\n");
				break;
			}

	// exo 6: le protocole freescord veut des lignes en CRLF 
			// fgets nous donne du LF (unix) donc l faut convertir avant d'envoyer
			lf_to_crlf(buf_envoi);

			// envoyer le message au serveur
			size_t len = strlen(buf_envoi);
			if (write(fd, buf_envoi, len) != (ssize_t)len) {
				perror("write"); break;
			}
		}

		// cas2: la socket a recu des données du serveur
		if (fds[1].revents & POLLIN) {

			if (buff_fgets_crlf(b, buf_reponse, sizeof(buf_reponse)) != NULL) {
				crlf_to_lf(buf_reponse);
				printf("\r\033[1;36mServeur > \033[0m%s", buf_reponse);
				fflush(stdout);
			} else if (buff_eof(b)) {
				printf("\nLe serveur a fermé la connexion\n");
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
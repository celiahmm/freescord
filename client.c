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


	for(;;) {
		printf("Vous >  ");
		fflush(stdout);

		if (fgets(buf_envoi, sizeof(buf_envoi), stdin) == NULL) {
			printf("\nFin de saisie (ctrl+D).\nDeconnexion.\n");
			break;
		}

		// envoyer le message au serveur
		size_t len = strlen(buf_envoi);
        if (write(fd, buf_envoi, len) != (ssize_t)len) {
            perror("write");
            break;
        }

		// lire la reponse du serveur dans buf_reception
		// read est bloquant, on attend la reponse du serveur
		ssize_t n = read(fd, buf_reponse, sizeof(buf_reponse) - 1);
		if(n <= 0) {
			if (n == 0) printf("Le serveur a fermé la connexion.\n"); 
			else perror("read");
			break;
		}
		buf_reponse[n] = '\0'; // ajouter un \0 pour faire une chaine de caractere
		
		//printf("Serveur > %s\n", buf_reponse); 
		printf("\033[1;34mServeur > \033[0m%s\n", buf_reponse);
		fflush(stdout);
	}

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

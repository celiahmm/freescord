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
#include <string.h>
#include "list/list.h"
#include "user.h"
#include "user.c"
#include <string.h>


#define PORT_FREESCORD 4321
#define BUFF_SIZE 1024

/** Gérer toutes les communications avec le client renseigné dans
 * user, qui doit être l'adresse d'une struct user */
void *handle_client(void *user);
/** Créer et configurer une socket d'écoute sur le port donné en argument
 * retourne le descripteur de cette socket, ou -1 en cas d'erreur */
int create_listening_sock(uint16_t port);

int main(int argc, char *argv[])
{

	int sock = create_listening_sock(PORT_FREESCORD);
	if (sock == -1)  return 1; 

	for(;;) { 
		// ===CODE EXO 1===
		/* struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int *client_fd = malloc(sizeof(int));
		if(!client_fd) {
			perror("malloc");
			continue;
		}

		*client_fd = accept(sock, (struct sockaddr *)&client_addr, &client_len);
		if (*client_fd == -1) {
			perror("accept");
			free(client_fd);
			continue;
		}

		// afficher l'ip du client qui vinet de se connecter 
		printf("Nouveau client connecté :  %s (fd = %d) \n", 
			inet_ntoa(client_addr.sin_addr),  *client_fd);

		// exo 1 : on gere le client directement dabs la boucle (exo 2: on delegue a un thread)
		handle_client(client_fd);
 */

		// ===CODE EXO 2===
		struct user *us = user_accept(sock);
		if (!us) continue; // si echec on reesaye

		// afficher l'ip du client qui vient de se connecter
		struct sockaddr_in *addr_in = (struct sockaddr_in *)us->address;
		printf("Nouveau client connecté :  %s (fd = %d) \n", 
			inet_ntoa(addr_in->sin_addr),  us->sock);

		// on lance un thread pour gerer le client pour que main puisse retourner a accept() immediatement 
		// et accepter un 2eme client pendant que le thread traite le 1er  
		pthread_t tid; 
		if (pthread_create(&tid, NULL, handle_client, us) != 0) {
			perror("pthread_create");
			user_free(us);
			continue;
		}

		pthread_detach(tid); // on detache le thread pour que ses ressources soient liberees automatiquement qd il se termine
	}

	close(sock);
	return 0;
}

void *handle_client(void *clt)
{
	// exo 1: clt est directment le fd du client (exo 2: on remplace par une struct user)
	/* int client_fd = *(int *)clt;
	free(clt);

	char buffer[BUFF_SIZE];
	ssize_t n;

	// boucle d'echo exo 1
	while((n= read(client_fd, buffer, sizeof(buffer))) > 0) {
		if(write(client_fd, buffer, n) != n) {
			perror("write");
			break;
		}
	}
	if(n == -1)  perror("read");

	printf("Client déconnecté (fd = %d) \n", client_fd);
	close(client_fd);
	return NULL; */

	// exo 2: clt est mtn une strutc user*
	struct user *us = (struct user *)clt;
	char buffer[BUFF_SIZE];
	ssize_t n;

	// boucle d'echo exo 2
	while((n= read(us->sock, buffer, sizeof(buffer))) > 0) {
		if(write(us->sock, buffer, n) != n) {
			perror("write");
			break;
		}
	}
	if(n == -1)  perror("read");

	printf("Client déconnecté (fd = %d) \n", us->sock);
	user_free(us); // ferme la socket et libere la memoire de us
	return NULL;
}

int create_listening_sock(uint16_t port) {
	// creation socket TCP ipv4
	int sock =socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		return -1;
	}

	// option pour reutiliser le port rapidement apres la fermeture du serveur
	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		perror("setsockopt");
		close(sock);
		return -1;
	}

	// adresse d'ecoute
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET; // ipv4
	addr.sin_port = htons(port); // port
	addr.sin_addr.s_addr = INADDR_ANY;

	// bind
	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind");
		close(sock);
		return -1;
	}

	// listen
	if (listen(sock, 10) == -1) {
		perror("listen");
		close(sock);
		return -1;
	} 

	printf("Le serveur écoute sur le port %d \n", port);
	return sock; // descripteur de  la socket 
}

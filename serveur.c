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


#define PORT_FREESCORD 4321
#define BUFF_SIZE 1024

/** Gérer toutes les communications avec le client renseigné dans
 * user, qui doit être l'adresse d'une struct user */
void *handle_client(void *user);
/** Créer et configurer une socket d'écoute sur le port donné en argument
 * retourne le descripteur de cette socket, ou -1 en cas d'erreur */
int create_listening_sock(uint16_t port);

// EXO 3: variables globales
int pipe_fds[2]; // pipe_fds[0] = lecture, et pipe_fds[1] = ecriture (threads clients)

// la liste des utilisateurs connctés
struct list *users_connectes;

// un mutex pour protéger l'acces à la liste (ajout/suppression d'utilisateurs)
pthread_mutex_t users_mutex;

//thread répéteur qui lit le pipe et renvoie chaque message à tous les clients
void *th_repeteur(void *arg);


int main(int argc, char *argv[]) {

	// == CODE EXO 3 == 
	if (pipe(pipe_fds) == -1) {
		perror("pipe");
		return 1;
	}

	if (pthread_mutex_init(&users_mutex, NULL) != 0) {
		perror("init mutex");
		return 1;
	}

	users_connectes = list_create();

	pthread_t th_rep;
	if (pthread_create(&th_rep, NULL, th_repeteur, NULL) != 0) {
		perror("pthread_create repeater");
		return 1;
	}
	pthread_detach(th_rep);

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


		// ===CODE EXO 3 : ajouter le new user a la liste partagée===
		pthread_mutex_lock(&users_mutex);
		list_add(users_connectes, us);
		pthread_mutex_unlock(&users_mutex);

		pthread_t tid; 
		if (pthread_create(&tid, NULL, handle_client, us) != 0) {
			perror("pthread create");

			// ajouts exo3
			pthread_mutex_lock(&users_mutex);
			list_remove_element(users_connectes, us);
			pthread_mutex_unlock(&users_mutex);

			user_free(us);
			continue;
		}

		pthread_detach(tid); // on detache le thread pour que ses ressources soient liberees automatiquement qd il se termine
	}

	close(sock);
	pthread_mutex_destroy(&users_mutex);
	return 0;
}

void *handle_client(void *clt)
{
	// code exo 1: clt est directment le fd du client (exo 2: on remplace par une struct user)

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
	return NULL; 
	*/

	// code exo 2: clt est mtn une strutc user*
	struct user *us = (struct user *)clt;
	char buffer[BUFF_SIZE];
	ssize_t n;

	// boucle d'echo
	while((n= read(us->sock, buffer, sizeof(buffer))) > 0) {
		// exo 2
		/* if(write(us->sock, buffer, n) != n) {
			perror("write");
			break;
		} */
		
		// exo3
		if(write(pipe_fds[1], buffer, n) != n) {
			perror("write pipe"); break;
		}
	}
	if(n == -1)  perror("read");

	printf("Client déconnecté (fd = %d) \n", us->sock);

	// exo3: on retire le client de la liste avant de liberer sa memoire
	pthread_mutex_lock(&users_mutex);
	list_remove_element(users_connectes, us);
	pthread_mutex_unlock(&users_mutex);

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

//thread répéteur (EXO 3)
void *th_repeteur(void *arg) {
	(void)arg;
	char buffer[BUFF_SIZE];
	ssize_t n;

	// on lit en boucle ce qui arrive dans le pipe
	while ((n = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {

		pthread_mutex_lock(&users_mutex);
		
		//envoyer le message a tous les clients de la liste
		struct node *curr = users_connectes->first;
		while (curr != NULL) {
			struct user *us = (struct user *)curr->elt;
			if (write(us->sock, buffer, n) == -1) {
				perror("write to client");
			}
			curr = curr->next;
		}

		pthread_mutex_unlock(&users_mutex);
	}

	return NULL;
}
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
#include "buffer/buffer.h"
#include "utils.h"


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

// la liste des utilisateurs connectés
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

// exo 7 : verifier qu'un nickname est valide (pas trop long, pas de ':', pas vide)
// retourne 0 si ok, le code d'erreur (2) sinon
static int check_nickname(const char *nick)
{
	if (nick == NULL || nick[0] == '\0') return 2;

	size_t len = strlen(nick);
	if (len > NICKNAME_MAX) return 2; // trop long

	// le caractere ':' est interdit dans un pseudo
	for (size_t i = 0; i < len; i++) {
		if (nick[i] == ':') return 2;
	}

	return 0;
}

// exo 7 : verifier qu'un nickname n'est pas déjà pris par un autre user connecté
// retourne 1 si déjà pris, 0 sinon — à appeler avec le mutex déjà pris
static int nickname_existe(const char *nick)
{
	struct node *curr = users_connectes->first;
	while (curr != NULL) {
		struct user *u = (struct user *)curr->elt;
		// on compare les pseudos (sensible à la casse)
		if (strcmp(u->nickname, nick) == 0) return 1;
		curr = curr->next;
	}
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

	// code exo 2: clt est mtn une struct user*
	struct user *us = (struct user *)clt;

	// exo 7 : on cree un buffer pour lire les lignes du client proprement
	buffer *b = buff_create(us->sock, BUFF_SIZE);
	if (b == NULL) {
		perror("buff_create");
		pthread_mutex_lock(&users_mutex);
		list_remove_element(users_connectes, us);
		pthread_mutex_unlock(&users_mutex);
		user_free(us);
		return NULL;
	}

	// == EXO 7 : message de bienvenue ==
	// le serveur envoie quelques lignes de bienvenue, terminées par une ligne vide
	const char *welcome =
		"Bienvenue sur freescord !\r\n"
		"Pour choisir votre pseudo, tapez : nickname <votre_pseudo>\r\n"
		"\r\n"; // ligne vide = fin du message de bienvenue
	write(us->sock, welcome, strlen(welcome));

	// == EXO 7 : phase de negociation du nickname ==
	// on boucle jusqu'à ce que le client envoie un pseudo valide et dispo
	char line[BUFF_SIZE];
	us->nickname[0] = '\0'; // pas encore de pseudo

	while (us->nickname[0] == '\0') {
		// lire une ligne du client (format CRLF)
		if (buff_fgets_crlf(b, line, sizeof(line)) == NULL) {
			// client déconnecté avant d'avoir choisi un pseudo
			buff_free(b);
			pthread_mutex_lock(&users_mutex);
			list_remove_element(users_connectes, us);
			pthread_mutex_unlock(&users_mutex);
			user_free(us);
			return NULL;
		}

		// enlever le CRLF final pour travailler sur la chaine propre
		crlf_to_lf(line);
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

		// la commande doit commencer par "nickname "
		if (strncmp(line, "nickname ", 9) != 0) {
			// code 3 : la commande n'est pas "nickname "
			write(us->sock, "3 Commande invalide, utilisez : nickname <pseudo>\r\n", 51);
			continue;
		}

		// extraire le pseudo (ce qui suit "nickname ")
		char *nick = line + 9;

		// verifier que le pseudo est valide (longueur, caracteres interdits)
		int code = check_nickname(nick);
		if (code != 0) {
			// code 2 : pseudo invalide (trop long ou contient ':')
			write(us->sock, "2 Pseudo invalide (trop long ou contient ':')\r\n", 47);
			continue;
		}

		// verifier que le pseudo n'est pas déjà pris par quelqu'un d'autre
		pthread_mutex_lock(&users_mutex);
		int taken = nickname_existe(nick);
		if (taken) {
			pthread_mutex_unlock(&users_mutex);
			// code 1 : pseudo déjà pris
			write(us->sock, "1 Pseudo deja pris, choisissez-en un autre\r\n", 44);
			continue;
		}

		// tout est ok : on enregistre le pseudo dans la struct user
		strncpy(us->nickname, nick, NICKNAME_MAX);
		us->nickname[NICKNAME_MAX] = '\0'; // securite au cas où
		pthread_mutex_unlock(&users_mutex);

		// code 0 : pseudo accepté, on le confirme au client
		char ok_msg[BUFF_SIZE];
		snprintf(ok_msg, sizeof(ok_msg), "0 Pseudo '%s' accepte\r\n", us->nickname);
		write(us->sock, ok_msg, strlen(ok_msg));

		printf("Nouveau pseudo enregistré : %s\n", us->nickname);
	}

	// == EXO 7 : boucle principale de reception des messages ==
	// maintenant que le client a un pseudo, on lit ses messages et on les
	// préfixe par "pseudo: message" avant de les envoyer dans le pipe
	while (buff_fgets_crlf(b, line, sizeof(line)) != NULL) {

		// enlever le CRLF du message recu pour le recoller proprement apres
		crlf_to_lf(line);
		size_t llen = strlen(line);
		if (llen > 0 && line[llen - 1] == '\n') line[llen - 1] = '\0';

		// construire le message préfixé : "pseudo: message\r\n"
		// +4 pour ": " + "\r\n"
		char prefixed[BUFF_SIZE + NICKNAME_MAX + 4];
		snprintf(prefixed, sizeof(prefixed), "%s: %s\r\n", us->nickname, line);

		// on envoie le message préfixé dans le pipe pour que le répéteur le broadcast
		size_t plen = strlen(prefixed);
		if (write(pipe_fds[1], prefixed, plen) != (ssize_t)plen) {
			perror("write pipe"); break;
		}
	}

	printf("Client déconnecté : %s (fd = %d) \n", us->nickname, us->sock);

	// on retire le client de la liste avant de liberer sa memoire
	pthread_mutex_lock(&users_mutex);
	list_remove_element(users_connectes, us);
	pthread_mutex_unlock(&users_mutex);

	buff_free(b);
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
	return sock; // descripteur de la socket 
}

//thread répéteur (EXO 3)
// il lit le pipe en boucle et envoie chaque message a tous les clients connectes
void *th_repeteur(void *arg) {
	(void)arg;
	char buffer[BUFF_SIZE];
	ssize_t n;

	// on lit en boucle ce qui arrive dans le pipe
	while ((n = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {

		pthread_mutex_lock(&users_mutex);
		
		// envoyer le message a tous les clients de la liste
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
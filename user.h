/* Célia HAMAM 12309320
	Je déclare qu'il s'agit de mon propre travail.
	Ce travail a été réalisé intégralement par un être humain. 
*/

#ifndef USER_H
#define USER_H
#include <sys/socket.h>
#include <netinet/in.h>

#define NICKNAME_MAX 16

struct user {
	struct sockaddr *address; // adresse du client
	socklen_t addr_len; 	  // taille de l'adresse
	int sock; 				  // descripteur de fichier de la socket du client

	// exo 7 : ajout du nickname
	char nickname[NICKNAME_MAX + 1]; // pseudo de l'user (+ 1 pour \0)
};

struct user *user_accept(int sl);
void user_free(struct user *user);

#endif /* ifndef USER_H */

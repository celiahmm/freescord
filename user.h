#ifndef USER_H
#define USER_H
#include <sys/socket.h>
#include <netinet/in.h>

struct user {
	struct sockaddr *address; // adresse du client
	socklen_t addr_len; 	  // taille de l'adresse
	int sock; 				  // descripteur de fichier de la socket du client
	/* autres champs éventuels */
};

struct user *user_accept(int sl);
void user_free(struct user *user);

#endif /* ifndef USER_H */

/* Célia HAMAM 12309320
	Je déclare qu'il s'agit de mon propre travail.
	Ce travail a été réalisé intégralement par un être humain. 
*/

#include "user.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

#include <unistd.h>

/** accepter une connection TCP depuis la socket d'écoute sl et retourner un
 * pointeur vers un struct user, dynamiquement alloué et convenablement
 * initialisé */
struct user *user_accept(int sl)
{
	// allouer la struct user
	struct user *us = malloc(sizeof(struct user));
	if (!us) {
		perror("malloc");
		return NULL;
	}
	
	// allouer la struct sockaddr pour stocker l'adresse du client
	// on utilise sockaddr_storage (pour ipv4 ou ipv6)
	us->addr_len = sizeof(struct sockaddr_storage);
	us->address = malloc(us->addr_len);
	if (!us->address) {
		perror("malloc");
		free(us);
		return NULL;	
	}

	// accepter la connection et remplir l'adresse du client
	us->sock = accept(sl, us->address, &us->addr_len);
	if (us->sock == -1) {
		perror("accept");
		free(us->address);
		free(us);
		return NULL;
	}

	return us; // retourne un nouveau fd pour le client
}

/** libérer toute la mémoire associée à user */
void user_free(struct user *user)
{
	if(user != NULL) {
		close(user->sock); // fermer la socket du client
		free(user->address); // liberer la memoire de l'adresse
		free(user); // liberer la struct user
	}
}

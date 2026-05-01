/* Célia HAMAM 12309320
	Je déclare qu'il s'agit de mon propre travail.
	Ce travail a été réalisé intégralement par un être humain. 
*/

#include "buffer.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct buffer {
	int fd;
	/* autres champs, à vous */
	char *data; // le tampon
	size_t taille; // taille du tampon
	size_t start; // indice du 1er octet par encore consommé
	size_t len; // nbr d'octets encore valide dans le tampon
	int eof; // flag qui determine si on a atteint la fin du fichier
	int ungot; // caractere remis par buff_ungetc 
	int has_ungot; // caractere en attente ou non (1 ou 0)
};

buffer *buff_create(int fd, size_t buffsz)
{
	// allouer la structure
	buffer *b = malloc(sizeof(buffer));
	if(b == NULL) return NULL;

	// allouter le tampon
	b->data = malloc(buffsz);
	if(b->data == NULL) {
		free(b) ; return NULL;
	}

	// initialiser tous les champs
	b->fd = fd;
	b->taille= buffsz;
	b->start = 0;
	b->len = 0;
	b->eof = 0;
	b->has_ungot = 0;

	return b;
}



int buff_getc(buffer *b)
{
	// si un caractere a été remis avec ungetc alor on le rend en priorité
	if(b->has_ungot) {
		b->has_ungot = 0;
		return b->ungot;
	}

	// si tampon vide on le rechagre
	if(b->len == 0) {
		if(b->eof) return EOF;

		ssize_t n = read(b->fd, b->data, b->taille);
		if(n <=0) {
			// n == 0 : fin du fichier
			// n == -1  : erreur
			b->eof = 1;
			return EOF;
		}
	
		b->start = 0;
		b->len = (size_t) n;
	}

	// consommer un carractere tampon
	unsigned char c = (unsigned char) b->data[b->start];
	b->start++;
	b->len--;

	return (int) c;
}

int buff_ungetc(buffer *b, int c)
{
	b->ungot = c;
	b->has_ungot = 1;
	return c;
}

void buff_free(buffer *b)
{
	if(b!= NULL) {
		free(b->data);
		free(b);
	}
}

int buff_eof(const buffer *buff)
{
	return  buff->eof 
			&& buff->len == 0
			&& !buff->has_ungot;
}

int buff_ready(const buffer *buff)
{
	return buff->len > 0 || buff->has_ungot;
}

char *buff_fgets(buffer *b, char *dest, size_t size)
{
	if (size==0) return NULL;

	size_t i = 0;
	int c;

	// on lit caractere par caractere jusqu'a '\n', EOF, ou saturation du dest
	while (i < size-1) {
		c = buff_getc(b);
		if(c == EOF) break;

		dest[i++] = (char)c;

		if(c== '\n') break;
	} 

	// si on a rien lu et qu'on est en EOF: fin
	if (i==0 && c==EOF) return NULL;

	dest[i] = '\0';
	return dest;
}

char *buff_fgets_crlf(buffer *b, char *dest, size_t size)
{
	if (size == 0) return NULL;

	size_t i = 0;
	int c;

	while (i < size - 1) {
		c = buff_getc(b);
		if (c == EOF) break;

		dest[i++] = (char)c;

		// si on tombe sur '\r' on verifie si '\n' suit
		if (c == '\r') {
			int d = buff_getc(b);

			if (d == '\n') { // CRLF complet trouvé donc on garde le '\n' et on s'arrete

				if (i < size - 1)  // verifier qu'on a encore de la place pour '\n'
					dest[i++] = (char)d;
				break;
			} 
			// ce n'etait pas une fin de ligne donc on remet le caractere et on continue à lire
			else if (d != EOF)  buff_ungetc(b, d);
		}
	}

	if (i == 0 && c == EOF) return NULL;

	dest[i] = '\0';
	return dest;
}
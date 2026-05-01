#include "buffer.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

/* la structure interne du buffer, cachée dans le .c
 * (c'est un type opaque, le client ne voit que le typedef dans le .h) */
struct buffer {
	int fd;              // le fichier qu'on lit
	char *data;          // le tampon (tableau alloué dynamiquement)
	size_t size;         // capacité totale du tampon
	size_t start;        // index du premier octet pas encore consommé
	size_t len;          // nombre d'octets encore valides dans le tampon
	int eof;             // flag : est-ce qu'on a atteint la fin du fichier ?
	int ungot;           // caractère remis par buff_ungetc (un seul garanti)
	int has_ungot;       // booléen : est-ce qu'on a un caractère en attente ?
};

/** Créer un buffer associé au fichier fd, avec un tampon de taille buffsz */
buffer *buff_create(int fd, size_t buffsz)
{
	// allouer la structure
	buffer *b = malloc(sizeof(buffer));
	if (b == NULL)
		return NULL;

	// allouer le tampon
	b->data = malloc(buffsz);
	if (b->data == NULL) {
		free(b);
		return NULL;
	}

	// initialiser tous les champs
	b->fd = fd;
	b->size = buffsz;
	b->start = 0;
	b->len = 0;
	b->eof = 0;
	b->has_ungot = 0;

	return b;
}

/** Lire le prochain caractère depuis le buffer.
 *  Si le tampon est vide, on recharge avec read() */
int buff_getc(buffer *b)
{
	// si un caractère a été remis avec ungetc, on le rend en priorité
	if (b->has_ungot) {
		b->has_ungot = 0;
		return b->ungot;
	}

	// si le tampon est vide, il faut recharger
	if (b->len == 0) {
		if (b->eof)
			return EOF;

		ssize_t n = read(b->fd, b->data, b->size);
		if (n <= 0) {
			// n == 0  -> fin de fichier
			// n == -1 -> erreur (on traite les deux pareil ici)
			b->eof = 1;
			return EOF;
		}

		b->start = 0;
		b->len = (size_t)n;
	}

	// consommer un caractère du tampon
	unsigned char c = (unsigned char)b->data[b->start];
	b->start++;
	b->len--;

	return (int)c;
}

/** Remettre un caractère dans le buffer.
 *  La spec garantit que ça marche pour un seul caractère */
int buff_ungetc(buffer *b, int c)
{
	// on stocke dans le champ dédié, c'est plus simple que de reculer dans le tampon
	b->ungot = c;
	b->has_ungot = 1;
	return c;
}

/** Libérer le buffer et son tampon */
void buff_free(buffer *b)
{
	if (b != NULL) {
		free(b->data);
		free(b);
	}
}

/** Retourne vrai si on a atteint la fin du fichier ET qu'il ne reste rien à lire */
int buff_eof(const buffer *b)
{
	return b->eof && b->len == 0 && !b->has_ungot;
}

/** Retourne vrai s'il reste des octets dans le tampon sans faire de read */
int buff_ready(const buffer *b)
{
	return b->len > 0 || b->has_ungot;
}

/** Lire une ligne terminée par '\n' (LF) */
char *buff_fgets(buffer *b, char *dest, size_t size)
{
	if (size == 0)
		return NULL;

	size_t i = 0;
	int c;

	// on lit caractère par caractère jusqu'à '\n', EOF, ou saturation du dest
	while (i < size - 1) {
		c = buff_getc(b);
		if (c == EOF)
			break;

		dest[i++] = (char)c;

		if (c == '\n')
			break;
	}

	// si on n'a rien lu et qu'on est en EOF, c'est une fin pure
	if (i == 0 && c == EOF)
		return NULL;

	dest[i] = '\0';
	return dest;
}

/** Lire une ligne terminée par "\r\n" (CRLF).
 *  La séquence "\r\n" est conservée dans dest si elle est trouvée */
char *buff_fgets_crlf(buffer *b, char *dest, size_t size)
{
	if (size == 0)
		return NULL;

	size_t i = 0;
	int c;

	while (i < size - 1) {
		c = buff_getc(b);
		if (c == EOF)
			break;

		dest[i++] = (char)c;

		// si on tombe sur '\r', il faut vérifier si '\n' suit
		if (c == '\r') {
			int d = buff_getc(b);
			if (d == '\n') {
				// CRLF complet trouvé, on garde le '\n' et on s'arrête
				dest[i++] = (char)d;
				break;
			} else if (d != EOF) {
				// ce n'était pas une fin de ligne, on remet le caractère
				// et on continue à lire (on cherchera un vrai CRLF plus loin)
				buff_ungetc(b, d);
			}
			// si d == EOF, la boucle s'arrêtera au prochain tour
		}
	}

	if (i == 0 && c == EOF)
		return NULL;

	dest[i] = '\0';
	return dest;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "buffer.h"

int main(void)
{
	// on cree un fichier tmp avec des lignes CRLF et LF
	const char *tmpname = "/tmp/test_buffer.txt";
	int fd = open(tmpname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	// on ecrit dedans : ligne 1 en CRLF et ligne 2 en LF
	const char *contenu = "bonjour\r\nmonde\nfin";
	write(fd, contenu, strlen(contenu));

	// on remet le curseur au debut pour relire
	lseek(fd, 0, SEEK_SET);

	// on bufferise la lecture
	buffer *b = buff_create(fd, 4);

	printf("Test buff_fgets_crlf\n");
	char ligne[64];
	while (buff_fgets_crlf(b, ligne, sizeof(ligne)) != NULL) {
		printf("lue : [%s] (len=%zu)\n", ligne, strlen(ligne));
	}

	printf("eof atteint ? %d\n", buff_eof(b));

	buff_free(b);
	close(fd);
	unlink(tmpname);

	// test avec l'entrée standard 
	/* printf("\nTest avec stdin (buff_fgets)\n");
	buffer *bin = buff_create(STDIN_FILENO, 16);
	while (buff_fgets(bin, ligne, sizeof(ligne)) != NULL) {
		printf("Lu : [%s]\n", ligne);
	}
	buff_free(bin); */
	
	return 0;
}
/* Célia HAMAM 12309320
	Je déclare qu'il s'agit de mon propre travail.
	Ce travail a été réalisé intégralement par un être humain. 
*/


#include <stdio.h>
#include <string.h>
#include <time.h>

#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define CYAN        "\033[38;5;87m"
#define BLUE       "\033[38;5;159m"
#define GREEN       "\033[38;5;71m"
#define YELLOW      "\033[38;5;221m"
#define GREY        "\033[38;5;245m"
#define PURPLE    "\033[38;5;141m"
#define RED		  	"\033[38;5;88m"

// pour afficher l'heure avant chaque message
static void print_heure(void)
{
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	printf(DIM "[%02d:%02d]" RESET " ", tm_info->tm_hour, tm_info->tm_min);
}

// affiche une ligne de separtion 
void client_print_separator(void)
{
	printf(GREY "──────────────────────────────────────────────────────────────\n" RESET);
}

// affiche le message de bienvenue du serveur
void client_print_banner(void)
{
	printf("\n");
	printf(PURPLE BOLD "  +=========================================+\n" RESET);
	printf(PURPLE BOLD "  |             " RESET BOLD "    FREESCORD     " PURPLE BOLD "          |\n" RESET);
	printf(PURPLE BOLD "  +=========================================+\n" RESET);
	printf("\n");
}

// affiche le prompt "vous > " pour indiquer que l'utilisateur peut taper un message
void client_print_prompt(void)
{
	printf(YELLOW BOLD "\nvous > " RESET);
	fflush(stdout);
}


// affiche un message reçu du serveur en mettant le pseudo en couleur
void client_afficher_message(const char *msg, const char *mon_pseudo)
{
	// effacer la ligne de prompt courante avant d'afficher le message
	printf("\r\033[2K");

	// afficher les messages d'erreur en rouge
	if (strncmp(msg, "Erreur (", 8) == 0) {
		client_print_separator();
		printf(RED BOLD "  %s" RESET, msg);
		client_print_separator();
		fflush(stdout);
		return;
	}
	
// le message du serveur est de la forme "<pseudo> : <contenu>" 
//donc on cherche ": " pour separer le pseudo du contenu
	const char *sep = strstr(msg, ": "); 
	if (sep != NULL) {
		size_t nick_len = (size_t)(sep - msg); // longueur du pseudo
		char nick[17] = {0};
		if (nick_len < sizeof(nick)) // on copie le pseudo dans une variable a part pour l'afficher en couleur
			strncpy(nick, msg, nick_len);

		const char *contenu = sep + 2;

		print_heure();

		// si le message vient de moi, on affiche le pseudo en violet, sinon en bleu
		size_t name_len = strlen(nick); // pour aligner les msgs on calcule un padding pour que le contenu commence toujours a la meme colonne
		int pad = 16 - (int)name_len;
		if (pad < 0) pad = 0;
		if (strcmp(nick, mon_pseudo) == 0)
			printf(PURPLE BOLD "  %s:" RESET "%*s " "%s" RESET, nick, pad, "", contenu);
		else
			printf(BLUE BOLD "  %s:" RESET "%*s " "%s" RESET, nick, pad, "", contenu);

	} else {
		print_heure();
		printf(GREY "  %s" RESET, msg);
	}

	fflush(stdout);
}

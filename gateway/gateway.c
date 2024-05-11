#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <unistd.h>

#define PORT 8080
#define DRIBBLINGPORT 8033
#define INFORTUNIOPORT 8041
#define TIROPORT 8077
#define BUFDIM 1024

/*
	In questo script verranno gestiti i giocatori e gli eventi della partita.
	Gestirà i giocatori come thread, insieme all'arbitro che è sempre attivo,
	il giocatore con la palla è l'unico thread giocatore attivo che dopo un intervallo casuale
	genera un evento di dribbling (vedi dribbling.c).
	*/

//risorsa pallone, solo un thread giocatore attivo
pthread_mutex_t pallone;

void* playerThread(void* arg) {
	//codice thread giocatore
}

void* refereeThread(void* arg) {
	//codice thread arbitro
}

int main(int argc, char* argv[]) {
	//inserisci codice
	return 0;
}
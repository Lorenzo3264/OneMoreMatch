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
	Gestira' i giocatori come thread, insieme all'arbitro che e' sempre attivo,
	il giocatore con la palla e' l'unico thread giocatore attivo che dopo un intervallo casuale
	genera un evento di dribbling (vedi dribbling.c).
	*/

//risorsa pallone, solo un thread giocatore attivo
pthread_mutex_t pallone;

//al loro interno ci sono gli id delle squadre
pthread_t squadraA[5];
pthread_t squadraB[5];

//tempo della partita inteso come numero di eventi, a 0 la partita termina.
int N = 50; 

//indica quale giocatore ha il possesso del pallone va da 0 a 9
int activePlayer = 0;

void* playerThread(void* arg) {
	//codice thread giocatore
	int id = *(int*)arg;
	free(arg);
	printf("ciao sono il thread del giocatore numero %d\n", id);
	/*
	while (0) { //fino a quando non finisce la partita
		while (pthread_mutex_trylock(&pallone)) {
			//deve interrompersi in attesa del suo turno
		}
		pthread_mutex_unlock(&pallone);
	}*/
}

void* refereeThread(void* arg) {
	//codice thread arbitro
}

int main(int argc, char* argv[]) {

	printf("Starting...\n");

	//punteggio delle due squadre
	int puntiA = 0;
	int puntiB = 0;

	

	//definizione dati e procedura per le socket
	struct sockaddr_in myaddr, client;
	char buffer[BUFDIM];
	int mySocket, clientSocket, len;

	mySocket = socket(AF_INET, SOCK_STREAM, 0);

	//host byte order
	myaddr.sin_family = AF_INET;

	//short, network byte order
	myaddr.sin_port = htons(PORT);

	//long, network byte order
	inet_aton("0.0.0.0", &(myaddr.sin_addr));

	// a zero tutto il resto
	memset(&(myaddr.sin_zero), '\0', 8);

	len = sizeof(client);

	bind(mySocket, (struct sockaddr*)&myaddr, sizeof(myaddr));
	listen(mySocket, 12);

	//mysocket e' pronta ad accettare richieste

	inet_ntop(AF_INET, &myaddr.sin_addr, buffer, sizeof(buffer));

	printf("Accepting as %s with port %d...\n", buffer, PORT);
	clientSocket = accept(mySocket, (struct sockaddr*)&client, &len);
	inet_ntop(AF_INET, &client.sin_addr, buffer, sizeof(buffer));
	printf("request from client %s\n", buffer);

	read(clientSocket, buffer, BUFDIM);
	write(clientSocket, "hewwo! uwu\0", 12);
	/*
		Qui bisogna stabilire il formato del messaggio e il modo di
		interpretarlo. esempio messaggio 1538042967 i primi cinque
		numeri sono la prima squadra e gli altri sono l'altra squadra
	*/

	pthread_mutex_init(&pallone, NULL);
	int* id; //id del giocatore
	for (int i = 0; i < 10; i++) {
		id = malloc(sizeof(int));
		*id = buffer[i] - '0'; //ottengo il valore intero del carattere
		if (i < 5) {
			pthread_create(&squadraA[i], NULL, playerThread, (void*)id);
			printf("thread id: %d lanciato\n", *id);
		}
		else {
			pthread_create(&squadraB[i%5], NULL, playerThread, (void*)id);
			printf("thread id: %d lanciato\n", *id);
		}
	}
	for (int i = 0; i < 5; i++) {
		pthread_join(squadraA[i],NULL);
		pthread_join(squadraB[i],NULL);
	}
	close(mySocket);
	return 0;
}
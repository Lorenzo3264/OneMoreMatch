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
#include <time.h>

#define PORT 8080
#define DRIBBLINGPORT 8033
#define INFORTUNIOPORT 8041
#define TIROPORT 8077
#define BUFDIM 1024
#define NPLAYERS 10
#define WAIT 5

//servono per identificare il tipo di evento per l'arbitro
#define TIRO 0
#define INFORTUNIO 1
#define DRIBBLING 2


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
int N = 90; 

//indica quale giocatore ha il possesso del pallone va da 0 a 9
int activePlayer = -1;

//indica quale azione avviene
int azione = -1;

void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, int port);

void* playerThread(void* arg) {
	//codice thread giocatore
	int id = *(int*)arg;
	free(arg);
	printf("ciao sono il thread del giocatore numero %d\n", id);

	//per permettere singole operazioni di accesso e liberazione risorsa
	int possesso = -1; 

	struct sockaddr_in addrTiro, addrInfortunio, addrDribbling;
	int socketTiro, socketInfortunio, socketDribbling;

	//serviceInit(&socketTiro, &addrTiro, TIROPORT);
	//serviceInit(&socketInfortunio, &addrInfortunio, INFORTUNIOPORT);
	//serviceInit(&socketDribbling, &addrDribbling, DRIBBLINGPORT);

	while (N) { //fino a quando non finisce la partita
		if (activePlayer == id && possesso) {
			pthread_mutex_lock(&pallone);
			printf("il giocatore %d ha la palla, possesso: %d\n", id, possesso);
			possesso++;
			
		}
		if (activePlayer != id && !possesso) {
			pthread_mutex_unlock(&pallone);
			printf("il giocatore %d e' scarso :(, possesso: %d\n", id, possesso);
			possesso--;
		}
	}

	//partita terminata

}

/*
	l'arbitro gestisce le comunicazioni con il thread arbitro lato client. 
	Manda a quest'ultimo gli esiti di ogni evento. In qualche modo deve, quindi,
	ricevere i risultati delle azioni dai thread giocatori
*/
void* refereeThread(void* arg) {
	//codice thread arbitro
	int s_fd = *(int*)arg;
	char buf[BUFDIM];
	
	int s_fd = *(int*)arg; //socket file descriptor del client/arbitro
	while (N) {
		switch (azione) {
			case TIRO:
				break;
			case DRIBBLING:
				break;
			case INFORTUNIO:
				break;
			default:
				break;
		}
	}
	
	strcpy(buf,"partitaTerminata\0");
	
	write(s_fd, buf, BUFDIM);
}

void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, int port) {
	*serviceSocket = socket(AF_INET, SOCK_STREAM, 0);

	serviceAddr->sin_family = AF_INET;

	serviceAddr->sin_port = htons(port);

	inet_aton("127.0.0.1", &serviceAddr->sin_addr);

	if (connect(*serviceSocket, (struct sockaddr*)serviceAddr, sizeof(*serviceAddr))) {
		perror("connect() failed\n");
		return;
	}
}

int main(int argc, char* argv[]) {

	printf("Starting...\n");

	//punteggio delle due squadre
	int puntiA = 0;
	int puntiB = 0;

	pthread_t arbitro;

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
	int* id; //id del giocatore
	inet_ntop(AF_INET, &myaddr.sin_addr, buffer, sizeof(buffer));

	//indici per inserire i giocatori nelle squadre
	int i = 0;
	int j = 0;

	//attesa di richieste per i giocatori
	while (i >=0) {
		id = malloc(sizeof(int));
		printf("Accepting as %s with port %d...\n", buffer, PORT);
		clientSocket = accept(mySocket, (struct sockaddr*)&client, &len);
		inet_ntop(AF_INET, &client.sin_addr, buffer, sizeof(buffer));
		printf("request from client %s\n", buffer);
		read(clientSocket, buffer, BUFDIM);
		/*
			Qui bisogna stabilire il formato del messaggio e il modo di
			interpretarlo. esempio messaggio A3 indicano la squadra (A, B) 
			e id giocatore (0..9)
		*/
		

		if (buffer[0] = 'A') {
			*id = buffer[1] - '0';
			pthread_create(&squadraA[i], NULL, playerThread, (void*)id);
			i++;
		}
		else{
			if (buffer[0] = 'B') {
				*id = buffer[1] - '0';
				pthread_create(&squadraB[j], NULL, playerThread, (void*)id);
				j++;
			}
			else {
				//creato il thread dell'arbitro, gestisce la comunicazione col client.
				pthread_create(&arbitro, NULL, refereeThread, (void*)clientSocket);
				i = -1;
			}
		}
		
		
	}
	

	/* Intializes random number generator */
	time_t t;
	srand((unsigned)time(&t));

	pthread_mutex_init(&pallone, NULL);
	
	pthread_mutex_lock(&pallone);
	for (int i = 0; i < NPLAYERS; i++) {
		
		
		*id = buffer[i] - '0'; //ottengo il valore intero del carattere
		if (i < 5) {
			
			printf("thread id: %d lanciato\n", *id);
		}
		else {
			pthread_create(&squadraB[i%5], NULL, playerThread, (void*)id);
			printf("thread id: %d lanciato\n", *id);
		}
		
	}
	
	activePlayer = rand() % 10;
	pthread_mutex_unlock(&pallone);
	printf("al giocatore %d viene dato il pallone\n", activePlayer);
	
	for (int i = 0; i < 5; i++) {
		pthread_join(squadraA[i],NULL);
		pthread_join(squadraB[i],NULL);
	}
	close(mySocket);
	return 0;
}
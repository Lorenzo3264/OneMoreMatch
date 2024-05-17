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

char squadre[10];
int tempoFallo[10];
int tempoInfortunio[10];

//tempo della partita inteso come numero di eventi, a 0 la partita termina.
volatile int N = 90; 

//indica quale giocatore ha il possesso del pallone va da 0 a 9
volatile int activePlayer = -1;

//indica quale azione avviene
int azione = -1;

void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, int port);

void* playerThread(void* arg) {
	//codice thread giocatore
	char* player = (char *)arg;
	char squadra = player[0];
	int id = player[1] - '0';
	tempoFallo[id] = 0;
	tempoInfortunio[id] = 0;
	squadre[id] = squadra;
	char buffer[BUFDIM];
	//printf("giocatore %d, squadra %c\n", id, squadra);
	free(player);

	time_t t;
	srand((unsigned)time(&t));

	struct sockaddr_in addrTiro, addrInfortunio, addrDribbling;
	int socketTiro, socketInfortunio, socketDribbling;

	serviceInit(&socketTiro, &addrTiro, TIROPORT);
	serviceInit(&socketInfortunio, &addrInfortunio, INFORTUNIOPORT);
	serviceInit(&socketDribbling, &addrDribbling, DRIBBLINGPORT);

	int chance;
	int altroPlayer;
	char altraSquadra;
	int i = 0;
	int j = 0;
	char time[5];

	while (N) { //fino a quando non finisce la partita
		
		pthread_mutex_lock(&pallone);
		while (activePlayer == id) {
			
			printf("Il giocatore %d della squadra %c ha la palla!.\n", id, squadra);
			sprintf_s(buffer, BUFDIM, "%c%d\0", squadra, id);
			write(socketDribbling, buffer, BUFDIM);
			read(socketDribbling, buffer, BUFDIM);
			/*
				formato messaggio dribbling: "x%c%d" 
				(x = s = successo, x = f = fallimento, x = i = infortunio)
				%c%d = squadra e giocatore avversari
			*/
			altroPlayer = buffer[2] - '0';
			altraSquadra = buffer[1];
			if (buffer[0] == 'i') {
				//messaggio inviato a infortunio per decidere i tempi
				sprintf_s(buffer, BUFDIM, "%c%d%c%d\0", squadra, id, altraSquadra, altroPlayer);
				write(socketInfortunio, buffer, BUFDIM);
				read(socketInfortunio, buffer, BUFDIM);
				/*
					formato messaggio infortunio: IXXXPXXX\0
					I precede il tempo di infortunio
					P precede il tempo di penalità
				*/
				
				
				i++;
				while (buffer[i] != 'P') {
					time[j] = buffer[i];
					i++;
					j++;
				}
				time[j] = '\0';
				j = 0;
				tempoInfortunio[id] = atoi(time);
				i++;
				while (buffer[i] != '\0') {
					time[j] = buffer[i];
					i++;
					j++;
				}
				time[j] = '\0';
				j = 0;
				i = 0;
				tempoFallo[altroPlayer] = atoi(time);
				
				/*
					inviamo ai servizi informazioni sullo stato
					dei giocatori in fallo e infortunati
				*/
				sprintf_s(buffer, BUFDIM, "i%c%d",squadra,id);
				write(socketInfortunio, buffer, BUFDIM);
				write(socketTiro, buffer, BUFDIM);
				write(socketDribbling, buffer, BUFDIM);
				sprintf_s(buffer, BUFDIM, "f%c%d", altraSquadra, altroPlayer);
				write(socketInfortunio, buffer, BUFDIM);
				write(socketTiro, buffer, BUFDIM);
				write(socketDribbling, buffer, BUFDIM);
				
				while(squadre[activePlayer] != squadra && tempoInfortunio[activePlayer] > 0 && tempoFallo[activePlayer] > 0{
					activePlayer = rand() % 10;
				}
				//adesso ad avere il pallone e' un giocatore diverso della propria squadra
			}
			else {
				if (buffer[0] == 'f') {
					activePlayer = altroPlayer;
					//il giocatore a perso la palla a un giocatore avversario
				}
				else {
					if (buffer[0] == 's') {
						//non perde la palla (tenta un tiro?)
					}
				}
			}
			
			
			

			//prima che perda il pallone o ricominci
			N--;
			if (tempoInfortunio[id] > 0) tempoInfortunio[id]--;
			else {
				if (tempoInfortunio[id] == 0) {
					sprintf_s(buffer, BUFDIM, "a%c%d", squadra, id);
					write(socketInfortunio, buffer, BUFDIM);
					write(socketTiro, buffer, BUFDIM);
					write(socketDribbling, buffer, BUFDIM);
				}
			}
			if (tempoFallo[id] > 0) tempoFallo[id]--;
			else {
				if (tempoFallo[id] == 0) {
					sprintf_s(buffer, BUFDIM, "a%c%d", squadra, id);
					write(socketInfortunio, buffer, BUFDIM);
					write(socketTiro, buffer, BUFDIM);
					write(socketDribbling, buffer, BUFDIM);
				}
			}
		}
		pthread_mutex_unlock(&pallone);
		// Yield per evitare consumi eccessivi di CPU
		sched_yield();
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
	char buf[BUFDIM];

	while (activePlayer == -1);
	strcpy(buf, "La partita è cominciata!\n");
	write(s_fd, buf, BUFDIM);

	int s_fd = *(int*)arg; //socket file descriptor del client/arbitro
	while (N) {
		switch (azione) {
			case TIRO:
				sprintf_s(buf, BUFDIM, "il giocatore %d ha effettuato un tiro ed\n", activePlayer);
				write(s_fd, buf, BUFDIM);
				azione = -1;
				break;
			case DRIBBLING:
				sprintf_s(buf, BUFDIM, "il giocatore %d scarta l'avversario\n", activePlayer);
				write(s_fd, buf, BUFDIM);
				azione = -1;
				break;
			case INFORTUNIO:
				sprintf_s(buf, BUFDIM, "il giocatore %d e' vittima di un infortunio\n", activePlayer);
				write(s_fd, buf, BUFDIM);
				azione = -1;
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
	/*
			inet_ntop(AF_INET, &myaddr.sin_addr, buffer, sizeof(buffer));
			printf("Accepting as %s with port %d...\n", buffer, PORT);
	*/
	clientSocket = accept(mySocket, (struct sockaddr*)&client, &len);
	/*
		inet_ntop(AF_INET, &client.sin_addr, buffer, sizeof(buffer));
		printf("request from client %s\n", buffer);
	*/
	read(clientSocket, buffer, BUFDIM);
	/*
		Qui bisogna stabilire il formato del messaggio e il modo di
		interpretarlo. esempio messaggio A3 indicano la squadra (A, B)
		e id giocatore (0..9)
	*/

	struct sockaddr_in addrTiro, addrInfortunio, addrDribbling;
	int socketTiro, socketInfortunio, socketDribbling;

	serviceInit(&socketTiro, &addrTiro, TIROPORT);
	serviceInit(&socketInfortunio, &addrInfortunio, INFORTUNIOPORT);
	serviceInit(&socketDribbling, &addrDribbling, DRIBBLINGPORT);

	//inviamo anche ai servizi le informazioni delle squadre
	write(socketTiro, buffer, BUFDIM);
	write(socketInfortunio, buffer, BUFDIM);
	write(socketDribbling, buffer, BUFDIM);

	pthread_mutex_init(&pallone, NULL);
	pthread_mutex_lock(&pallone); //i giocatori aspettano l'inizio della partita

	//indici per inserire i giocatori nelle squadre
	int i = 0;
	int j = 0;
	short ref = 0;
	//attesa di richieste per i giocatori
	while (i < 5 || j < 5 || !ref) {
		char* player = malloc(2*sizeof(char)); //info del giocatore
		
		
		if (buffer[0] == 'A') {
			player[0] = 'A';
			player[1] = buffer[1];
			pthread_create(&squadraA[i], NULL, playerThread, (void*)player);
			i++;
		}
		else{
			if (buffer[0] == 'B') {
				player[0] = 'B';
				player[1] = buffer[1];
				pthread_create(&squadraB[j], NULL, playerThread, (void*)player);
				j++;
			}
			else {
				//creato il thread dell'arbitro, gestisce la comunicazione col client.
				pthread_create(&arbitro, NULL, refereeThread, (void*)&clientSocket);
				ref = 1;
			}
		}
	}
	
	

	/* Intializes random number generator */
	time_t t;
	srand((unsigned)time(&t));
	
	activePlayer = rand() % 10;
	pthread_mutex_unlock(&pallone);

	for (int i = 0; i < 5; i++) {
		pthread_join(squadraA[i],NULL);
		pthread_join(squadraB[i],NULL);
	}
	close(mySocket);
	return 0;
}
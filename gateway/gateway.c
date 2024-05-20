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
#include <wchar.h>
#include <netdb.h>

#define PORT 8080
#define REFEREEPORT 8088
#define DRIBBLINGPORT 8033
#define INFORTUNIOPORT 8041
#define TIROPORT 8077
#define BUFDIM 1024
#define NPLAYERS 10
#define WAIT 0

//servono per identificare il tipo di evento per l'arbitro
#define TIRO 't'
#define INFORTUNIO 'i'
#define DRIBBLING 'd'


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

volatile char squadre[10] = { 0 };
volatile int tempoFallo[10] = { -1 };
volatile int tempoInfortunio[10] = { -1 };

//tempo della partita inteso come numero di eventi, a 0 la partita termina.
volatile int N = 90; 

//indica quale giocatore ha il possesso del pallone va da 0 a 9
volatile int activePlayer = -1;

//la partita non comincia se il server non e' pronto
volatile short refServer = -1;
volatile short playerCount = 10;


void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, char* ip, int port);
void serverInit(int* serverSocket, struct sockaddr_in* serverAddr, char* ip, int port);

void* playerThread(void* arg) {
	printf("player thread: 1\n");

	//codice thread giocatore
	struct hostent* hentDribbling, *hentInfortunio, *hentTiro;
	char ipTiro[40], ipDribbling[40], ipInfortunio[40];

	hentDribbling = gethostbyname("dribbling");
	if (hentDribbling == NULL) {
		perror("gethostbyname");
		pthread_exit(NULL); // Exit the thread if gethostbyname fails
	}
	inet_ntop(AF_INET, (void*)hentDribbling->h_addr_list[0], ipDribbling, 15);
	hentInfortunio = gethostbyname("infortunio");
	if (hentInfortunio == NULL) {
		perror("gethostbyname");
		pthread_exit(NULL); // Exit the thread if gethostbyname fails
	}
	inet_ntop(AF_INET, (void*)hentInfortunio->h_addr_list[0], ipInfortunio, 15);
	hentTiro = gethostbyname("tiro");
	if (hentTiro == NULL) {
		perror("gethostbyname");
		pthread_exit(NULL); // Exit the thread if gethostbyname fails
	}
	inet_ntop(AF_INET, (void*)hentTiro->h_addr_list[0], ipTiro, 15);

	printf("player thread: 2\n");

	//informazioni giocatore
	char* player = (char *)arg;
	char squadra = player[0];
	int id = player[1] - '0';
	if (id > 9)
	{
		perror("wrong id");
		exit(EXIT_FAILURE);
	}

	//inizializzo informazioni globali
	squadre[id] = squadra;

	char buffer[BUFDIM]; //buffer per le comunicazioni coi servizi
	//printf("giocatore %d, squadra %c\n", id, squadra);
	free(player); //libero la struttura usata per le info del giocatore

	struct sockaddr_in addrTiro, addrInfortunio, addrDribbling;
	int socketTiro, socketInfortunio, socketDribbling;
	

	int chance; //chance di tiro

	//info del giocatore che entra in tackle
	int altroPlayer;
	char altraSquadra;

	//strutture di supporto per infortunio e fallo
	int i = 0;
	int j = 0;
	char time[5];

	//dato del numero di dribbling riusciti, finalizzato per il tiro
	short nDribbling = 0;

	printf("player %d thread: 3\n",id);
	playerCount--;
	while (N > 0) { //fino a quando non finisce la partita
		
		pthread_mutex_lock(&pallone); //attende di ricevere possesso del pallone
		while (activePlayer == id && N > 0) { //controlla se il possesso del pallone e' legale

			printf("player %d thread: 4\n",id);

			printf("Il giocatore %d della squadra %c ha la palla!\n", id, squadra);

			//informo il servizio Dribbling dell'evento
			
			serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
			snprintf(buffer, BUFDIM, "%d\0", id);
			write(socketDribbling, buffer, BUFDIM);
			printf("player %d thread: read dribbling\n",id);
			read(socketDribbling, buffer, BUFDIM);
			printf("player %d thread: dribbling buffer = %s\n", id, buffer);
			close(socketDribbling);
			/*
				formato messaggio dribbling: "x%d" 
				(x = s = successo, x = f = fallimento, x = i = infortunio)
				%d = giocatore avversario
			*/

			//inizializzo i dati dell'altro giocatore
			altroPlayer = buffer[1] - '0';
			if (altroPlayer > 9)
			{
				perror("wrong id");
				exit(EXIT_FAILURE);
			}
			altraSquadra = squadre[altroPlayer];


			printf("player %d thread: 5\n",id);

			//switch per l'evento del dribbling
			switch (buffer[0]) {
			case 'i':
				//messaggio inviato a infortunio per decidere i tempi
				serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);
				snprintf(buffer, BUFDIM, "%d%d\0", id, altroPlayer);
				write(socketInfortunio, buffer, BUFDIM);
				printf("player %d thread: 5.1 buffer = %s\n", id, buffer);
				read(socketInfortunio, buffer, BUFDIM);
				printf("player %d thread: 5.2 buffer = %s\n", id, buffer);
				close(socketInfortunio);
				/*
					formato messaggio infortunio: IXXXPXXX\0
					I precede il tempo di infortunio
					P precede il tempo di penalita'
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
					Non e' necessario inviare a dribbling la notifica dell'infortunio o fallo
					perche' dribbling ha generato l'evento e gia' lo sa
				*/

				while (squadre[activePlayer] != squadra && tempoInfortunio[activePlayer] > 0 && tempoFallo[activePlayer] > 0){
					activePlayer = rand() % 10;
				}
				//adesso ad avere il pallone e' un giocatore della propria squadra
				break;
			case 'f':
				activePlayer = altroPlayer;
				//il giocatore a perso la palla a un giocatore avversario
				break;
			case 's':
				//non perde la palla (tenta un tiro?)
				chance = rand() % 100;
				chance = chance + (nDribbling * 30);
				nDribbling++;
				if (chance > 70) {
					//tenta un tiro
					serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
					snprintf(buffer, BUFDIM, "%d\0", id);
					write(socketTiro, buffer, BUFDIM);
					close(socketTiro);
					printf("player %d thread: 5.3 buffer = %s\n", id, buffer);
					while (squadre[activePlayer] == squadra && tempoInfortunio[activePlayer] > 0 && tempoFallo[activePlayer] > 0) {
						activePlayer = rand() % 10;
					}
					//il pallone viene dato ad un giocatore della squadra avversaria quando avviene un tiro.

				}
				break;
			}


			printf("player %d thread: 6\n",id);

			//prima che perda il pallone o ricominci
			sleep(WAIT);
			N--;
			for (int k = 0; k < 10; k++) {
				printf("%c%d=%d:%d, ", squadre[k], k, tempoInfortunio[k],tempoFallo[k]);
				
				if (tempoInfortunio[k] > 0) tempoInfortunio[k]--;
				else {
					if (tempoInfortunio[k] == 0) {
						tempoInfortunio[k] = -1;
						snprintf(buffer, BUFDIM, "a%d\0", k);
						close(socketDribbling);
						serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
						write(socketDribbling, buffer, BUFDIM);
						printf("player %d thread: 6.1 buffer = %s\n", id, buffer);
						close(socketDribbling);
					}
				}
				if (tempoFallo[k] > 0) tempoFallo[k]--;
				else {
					if (tempoFallo[k] == 0) {
						tempoFallo[k] = -1;
						snprintf(buffer, BUFDIM, "a%d\0", k);
						close(socketDribbling);
						serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
						write(socketDribbling, buffer, BUFDIM);
						printf("player %d thread: 6.1 buffer = %s\n", id, buffer);
						close(socketDribbling);
					}
				}
			}
			
			


			printf("player %d thread: tempo rimanente %d\n", id, N);
		}
		
		nDribbling = 0;
		pthread_mutex_unlock(&pallone);
	}

	//partita terminata

}

/*
	l'arbitro gestisce le comunicazioni con il thread arbitro lato client. 
	Manda a quest'ultimo gli esiti di ogni evento. In qualche modo deve, quindi,
	ricevere i risultati delle azioni dai thread giocatori
*/
void* refereeThread(void* arg) {
	printf("referee thread: inizio\n");

	//codice thread arbitro
	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	struct hostent* hent;
	hent = gethostbyname(hostname);
	char ip[40];
	inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15);


	char azione;
	char buf[BUFDIM];
	int s_fd = *(int*)arg; //socket file descriptor del client/arbitro
	int eventSocket, serviceSocket, len, player, opponent;
	struct sockaddr_in eventAddr, serviceAddr;

	serverInit(&eventSocket, &eventAddr, ip, REFEREEPORT);
	bind(eventSocket, (struct sockaddr*)&eventAddr, sizeof(eventAddr));
	listen(eventSocket, 12);
	len = sizeof(serviceAddr);
	
	printf("referee thread: server inizializzato\n");

	while (activePlayer == -1);
	strcpy(buf, "La partita e' cominciata!\n");
	write(s_fd, buf, BUFDIM);

	printf("referee thread: la partita � cominciata!\n");
	
	refServer = 0;
	while (N > 0) {
		printf("referee thread: waiting for event\n");
		serviceSocket = accept(eventSocket, (struct sockaddr*)&serviceAddr, &len);
		buf[0] = '\0';
		read(serviceSocket, buf, BUFDIM);
		printf("referee thread: from service buffer = %s\n",buf);
		/*
			formato messaggio: x%d(%d)(r)\0
			x = tipo di azione
			%d = giocatore attivo
			(%d) opzionale = giocatore in tackle/fallo
			(r) opzionale = risultato tiro/dribbling (y = successo, f = fallimento)
		*/
		azione = buf[0];
		player = buf[1] - '0';
		if (player > 9)
		{
			perror("wrong id");
			exit(EXIT_FAILURE);
		}
		switch (azione) {
			case TIRO:
				if (buf[2] == 'y') {
					snprintf(buf, BUFDIM, "il giocatore %d tira... ed e' GOAL!!!\0", player);
				}
				else {
					snprintf(buf, BUFDIM, "il giocatore %d tira... e ha mancato la porta...\0", player);
				}
				write(s_fd, buf, BUFDIM);
				printf("referee thread: to client buffer = %s\n", buf);
				azione = -1;
				break;

			case DRIBBLING:
				opponent = buf[2] - '0';
				if (opponent > 9)
				{
					perror("wrong id");
					exit(EXIT_FAILURE);
				}
				if (buf[3] == 'y') {
					snprintf(buf, BUFDIM, "il giocatore %d scarta il giocatore %d\0", player, opponent);
				}
				else {
					snprintf(buf, BUFDIM, "il giocatore %d prende la palla da %d\0", opponent, player);
				}
				write(s_fd, buf, BUFDIM);
				printf("referee thread: to client buffer = %s\n", buf);
				azione = -1;
				break;

			case INFORTUNIO:
				opponent = buf[2] - '0';
				if (opponent > 9) 
				{
					perror("wrong id");
					exit(EXIT_FAILURE);
				}
				snprintf(buf, BUFDIM, "il giocatore %d e' vittima di un infortunio da parte di %d\0", player, opponent);
				write(s_fd, buf, BUFDIM);
				printf("referee thread: to client buffer = %s\n", buf);
				azione = -1;
				break;
			case 'e':
				snprintf(buf, BUFDIM, "partitaTerminata\0");
				write(s_fd, buf, BUFDIM);
				printf("referee thread: to client buffer = %s\n", buf);
				break;
			default:
				break;
		}
		printf("referee thread: chiudo la socket dell'evento puntatore: %p\n",&serviceSocket);
		close(serviceSocket);
	}
	
	
	
}

void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, char* ip, int port) {
	*serviceSocket = socket(AF_INET, SOCK_STREAM, 0);

	serviceAddr->sin_family = AF_INET;

	serviceAddr->sin_port = htons(port);

	inet_aton(ip, &serviceAddr->sin_addr);

	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &serviceAddr->sin_addr, buf, sizeof(buf));

	if (connect(*serviceSocket, (struct sockaddr*)serviceAddr, sizeof(*serviceAddr))) {
		printf("connect() failed for %s:%d\n",ip,port);
		return;
	}

}

void serverInit(int* serverSocket, struct sockaddr_in* serverAddr,char* ip, int port) {
	printf("server init: inizio\n");

	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr->sin_family = AF_INET;
	serverAddr->sin_port = htons(port);
	inet_aton(ip, &(serverAddr->sin_addr));
	memset(&(serverAddr->sin_zero), '\0', 8);

	printf("server init: fine\n");
}

int main(int argc, char* argv[]) {

	//inizializzo il random number generator
	time_t t;
	srand((unsigned)time(&t));

	/* Set IP address to localhost */
	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	struct hostent* hent, *hentDribbling, *hentInfortunio, *hentTiro;
	char ip[40], ipTiro[40], ipDribbling[40], ipInfortunio[40];
	hent = gethostbyname(hostname);
	if (hent == NULL) {
		perror("gethostbyname");
		exit(1); // Exit the thread if gethostbyname fails
	}
	inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15);
	hentDribbling = gethostbyname("dribbling");
	if (hentDribbling == NULL) {
		perror("gethostbyname");
		exit(1); // Exit the thread if gethostbyname fails
	}
	inet_ntop(AF_INET, (void*)hentDribbling->h_addr_list[0], ipDribbling, 15);
	hentInfortunio = gethostbyname("infortunio");
	if (hentInfortunio == NULL) {
		perror("gethostbyname");
		exit(1); // Exit the thread if gethostbyname fails
	}
	inet_ntop(AF_INET, (void*)hentInfortunio->h_addr_list[0], ipInfortunio, 15);
	hentTiro = gethostbyname("tiro");
	if (hentTiro == NULL) {
		perror("gethostbyname");
		exit(1); // Exit the thread if gethostbyname fails
	}
	inet_ntop(AF_INET, (void*)hentTiro->h_addr_list[0], ipTiro, 15);
	
	
	
	
	

	//inizializzo i servizi
	struct sockaddr_in addrTiro, addrInfortunio, addrDribbling;
	int socketTiro, socketInfortunio, socketDribbling;

	


	printf("Starting...\n");
	//punteggio delle due squadre
	int puntiA = 0;
	int puntiB = 0;

	pthread_t arbitro;

	//definizione dati e procedura per le socket
	struct sockaddr_in myaddr, client;
	char buffer[BUFDIM];
	int mySocket, clientSocket, len;

	/*
		mySocket = socket(AF_INET, SOCK_STREAM, 0);

		//host byte order
		myaddr.sin_family = AF_INET;

		//short, network byte order
		myaddr.sin_port = htons(PORT);

		//long, network byte order
		inet_aton("0.0.0.0", &(myaddr.sin_addr));

		// a zero tutto il resto
		memset(&(myaddr.sin_zero), '\0', 8);
	*/
	
	serverInit(&mySocket, &myaddr, ip, PORT);

	len = sizeof(client);

	bind(mySocket, (struct sockaddr*)&myaddr, sizeof(myaddr));
	listen(mySocket, 12);

	
	
	/*
		inet_ntop(AF_INET, &client.sin_addr, buffer, sizeof(buffer));
		printf("request from client %s\n", buffer);
	*/
	


	inet_ntop(AF_INET, &myaddr.sin_addr, buffer, sizeof(buffer));
	printf("Accepting as %s with port %d...\n", buffer, PORT);

	//inviamo anche ai servizi le informazioni delle squadre
	

	pthread_mutex_init(&pallone, NULL);
	pthread_mutex_lock(&pallone); //i giocatori aspettano l'inizio della partita

	//indici per inserire i giocatori nelle squadre
	int i = 0;
	int j = 0;
	short ref = 0;
	//attesa di richieste per i giocatori
	while (i < 5 || j < 5 || ref != 1) {

		clientSocket = accept(mySocket, (struct sockaddr*)&client, &len);

		char* player = malloc(2*sizeof(char)); //info del giocatore

		read(clientSocket, buffer, BUFDIM);
		/*
			Qui bisogna stabilire il formato del messaggio e il modo di
			interpretarlo. esempio messaggio A3 indicano la squadra (A, B)
			e id giocatore (0..9)
		*/
		

		if (buffer[0] == 'A') {
			player[0] = 'A';
			player[1] = buffer[1];
			pthread_create(&squadraA[i], NULL, playerThread, (void*)player);
			i++;

			serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
			serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);
			serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);

			write(socketTiro, buffer, BUFDIM);
			write(socketInfortunio, buffer, BUFDIM);
			write(socketDribbling, buffer, BUFDIM);

		}
		else{
			if (buffer[0] == 'B') {
				player[0] = 'B';
				player[1] = buffer[1];
				pthread_create(&squadraB[j], NULL, playerThread, (void*)player);
				j++;

				serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
				serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);
				serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);

				write(socketTiro, buffer, BUFDIM);
				write(socketInfortunio, buffer, BUFDIM);
				write(socketDribbling, buffer, BUFDIM);

			}
			else {

				//creato il thread dell'arbitro, gestisce la comunicazione col client.
				pthread_create(&arbitro, NULL, refereeThread, (void*)&clientSocket);
				ref = 1;
			}
		}
	}
	
	printf("la partita sta per cominciare\n");

	
	
	activePlayer = rand() % 10;
	while (refServer != 0);
	while (playerCount > 0);
	pthread_mutex_unlock(&pallone);

	printf("la partita e' cominciata!\n");
	for (int i = 0; i < 5; i++) {
		pthread_join(squadraA[i],NULL);
		pthread_join(squadraB[i],NULL);
	}
	serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
	serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);
	serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);

	snprintf(buffer, BUFDIM, "partita terminata\0");

	write(socketTiro, buffer, BUFDIM);
	write(socketInfortunio, buffer, BUFDIM);
	write(socketDribbling, buffer, BUFDIM);

	pthread_join(arbitro, NULL);

	

	close(mySocket);
	close(clientSocket);
	close(socketTiro);
	close(socketDribbling);
	close(socketInfortunio);
	return 0;
}
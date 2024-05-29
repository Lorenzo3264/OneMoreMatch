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
#include <errno.h>

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
#define TIMEOUT 'h'


/*
	In questo script verranno gestiti i giocatori e gli eventi della partita.
	Gestira' i giocatori come thread, insieme all'arbitro che e' sempre attivo,
	il giocatore con la palla e' l'unico thread giocatore attivo che dopo un intervallo casuale
	genera un evento di dribbling (vedi dribbling.c).
	*/

//risorsa pallone, solo un thread giocatore attivo
pthread_mutex_t pallone;
pthread_mutex_t globalVar; //trattare in maniera sicura variabili globali
pthread_mutex_t eventMutex; //multipli thread event manager hanno accesso concorrente alla socket

volatile char squadre[10] = { 0 };
volatile int tempoFallo[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
volatile int tempoInfortunio[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

//tempo della partita inteso come numero di eventi, a 0 la partita termina.
volatile int N = 360;

//indica quale giocatore ha il possesso del pallone va da 0 a 9
volatile int activePlayer = -1;

//la partita non comincia se il server non e' pronto
volatile short refServer = -1;
volatile short playerCount = 10;


void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, const char* ip, int port);
void serverInit(int* serverSocket, struct sockaddr_in* serverAddr, char* ip, int port);
int resolve_hostname(const char* hostname, char* ip, size_t ip_len);
void writeRetry(int* socket, struct sockaddr_in* addr, const char* ip, int port, char* buffer);

void* playerThread(void* arg) {
	
	//informazioni giocatore
	int id = *(int*)arg;
	printf("player %d thread: 1\n",id);
	if (id > 9 || id < 0)
	{
		perror("player: wrong id");
		exit(EXIT_FAILURE);
	}
	pthread_mutex_lock(&globalVar);
	printf("player %d: ha preso possesso del mutex globalVar\n",id);
	char squadra = squadre[id];
	//codice thread giocatore
	char ipTiro[40], ipDribbling[40], ipInfortunio[40];

	while (resolve_hostname("dribbling", ipDribbling, sizeof(ipDribbling)) != 0) {
		printf("player %d: retrying resolve hostname\n",id);
	}
	while (resolve_hostname("infortunio", ipInfortunio, sizeof(ipInfortunio)) != 0) {
		printf("player %d: retrying resolve hostname\n", id);
	}
	while (resolve_hostname("tiro", ipTiro, sizeof(ipTiro)) != 0) {
		printf("player %d: retrying resolve hostname\n", id);
	}

	pthread_mutex_unlock(&globalVar);
	printf("player %d: ha rilasciato il mutex globalVar\n",id);

	

	

	char buffer[BUFDIM]; //buffer per le comunicazioni coi servizi
	//printf("giocatore %d, squadra %c\n", id, squadra);


	

	printf("player %d thread: 2\n", id);

	

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

	//modifichiamo informazioni globali
	pthread_mutex_lock(&globalVar);
	printf("player: ha preso possesso del mutex globalVar\n");
	playerCount--;
	pthread_mutex_unlock(&globalVar);
	printf("player: ha rilasciato il mutex globalVar\n");

	while (N > 0) { //fino a quando non finisce la partita

		pthread_mutex_lock(&pallone); //attende di ricevere possesso del pallone
		while (activePlayer == id && N > 0) { //controlla se il possesso del pallone e' legale

			printf("player %d thread: 4\n",id);

			printf("Il giocatore %d della squadra %c ha la palla!\n", id, squadra);

			//informo il servizio Dribbling dell'evento

			serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
			
			//gestisco il possibile errore di questo snprintf
			snprintf(buffer, BUFDIM, "%d\0", id);

			writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
			
			printf("player %d thread: %s sent to dribbling\n",id,buffer);
			recv(socketDribbling, buffer, BUFDIM, 0);
			printf("player %d thread: dribbling buffer = %s\n", id, buffer);
			//close(socketDribbling);

			
			/*
				formato messaggio dribbling: "x%d"
				(x = s = successo, x = f = fallimento, x = i = infortunio)
				%d = giocatore avversario
			*/

			//inizializzo i dati dell'altro giocatore
			altroPlayer = buffer[1] - '0';
			while (altroPlayer > 9 || altroPlayer < 0)
			{
				printf("opponent: wrong id, retrying...\n");
				serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);

				//gestisco il possibile errore di questo snprintf
				snprintf(buffer, BUFDIM, "%d\0", id);
				writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
				printf("player %d thread: retrying dribbling buffer send = %s\n",id,buffer);
				recv(socketDribbling, buffer, BUFDIM, 0);
				printf("player %d thread: retrying dribbling buffer recv = %s\n", id, buffer);
				altroPlayer = buffer[1] - '0';
			}
			altraSquadra = squadre[altroPlayer];


			sleep(WAIT);


			printf("player %d thread: 5\n",id);

			//switch per l'evento del dribbling
			switch (buffer[0]) {
			case 'i':
				//messaggio inviato a infortunio per decidere i tempi
				serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);
				snprintf(buffer, BUFDIM, "%d%d\0", id, altroPlayer);
				writeRetry(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT, buffer);
				printf("player %d thread: 5.1 buffer = %s\n", id, buffer);
				recv(socketInfortunio, buffer, BUFDIM, 0);
				//close(socketInfortunio);


				printf("player %d thread: 5.2 buffer = %s\n", id, buffer);
				/*
					formato messaggio infortunio: IXXXPXXX\0
					I precede il tempo di infortunio
					P precede il tempo di penalita'
				*/

				i++;
				while (buffer[i] != 'P' && buffer[i] != '\0' && j<4) {
					time[j] = buffer[i];
					i++;
					j++;
				}
				time[j] = '\0';
				j = 0;
				tempoInfortunio[id] = atoi(time);
				i++;
				while (buffer[i] != '\0' && j<4) {
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

				while (squadre[activePlayer] != squadra || tempoInfortunio[activePlayer] > 0 || tempoFallo[activePlayer] > 0){
					activePlayer = rand() % 10;
				}
				printf("player %d thread: la palla viene passata a %d per infortunio\n", id, activePlayer);
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
					writeRetry(&socketTiro, &addrTiro, ipTiro, TIROPORT, buffer);
					recv(socketTiro, buffer, BUFDIM, 0);
					//close(socketTiro);
					while (buffer[0] != 'a') {
						serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
						snprintf(buffer, BUFDIM, "%d\0", id);
						writeRetry(&socketTiro, &addrTiro, ipTiro, TIROPORT, buffer);
						recv(socketTiro, buffer, BUFDIM, 0);
						//close(socketTiro);
					}


					printf("player %d thread: 5.3 buffer = %s\n", id, buffer);
					while (squadre[activePlayer] == squadra || tempoInfortunio[activePlayer] > 0 || tempoFallo[activePlayer] > 0) {
						activePlayer = rand() % 10;
					}
					//il pallone viene dato ad un giocatore della squadra avversaria quando avviene un tiro.
					printf("player %d thread: la palla viene passata a %d per tiro\n", id, activePlayer);
				}
				break;
			}


			printf("player %d thread: 6\n",id);

			//prima che perda il pallone o ricominci

			/*
				bisogna evitare il deadlock imposto dal fatto che troppi giocatori si infortunino o vadano in fallo
				quando succede bisogna in qualche modo liberare la situazione in modo da avere giocatori in campo.
				IDEA: evento di timeout, resetta lo stato e dunque il tempoInfortunio e tempoFallo di tutti i giocatori.
			*/



			sleep(WAIT);
			//N--;
			pthread_mutex_lock(&globalVar); 
			for (int k = 0; k < 10; k++) {
				printf("%c%d=%d:%d, ", squadre[k], k, tempoInfortunio[k],tempoFallo[k]);

				
				if (tempoInfortunio[k] > 0) tempoInfortunio[k]--;
				else {
					if (tempoInfortunio[k] == 0) {
						tempoInfortunio[k] = -1;
						snprintf(buffer, BUFDIM, "a%d\0", k);
						serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
						writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
						recv(socketDribbling, buffer, BUFDIM, 0);
						while (buffer[0] != 'a') {
							serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
							writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
							recv(socketDribbling, buffer, BUFDIM, 0);
						}
						//close(socketDribbling);
						printf("player %d thread: 6.1 buffer = %s\n", id, buffer);
					}
				}
				if (tempoFallo[k] > 0) tempoFallo[k]--;
				else {
					if (tempoFallo[k] == 0) {
						tempoFallo[k] = -1;
						snprintf(buffer, BUFDIM, "a%d\0", k);
						serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
						writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
						recv(socketDribbling, buffer, BUFDIM, 0);
						while (buffer[0] != 'a') {
							serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
							writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
							recv(socketDribbling, buffer, BUFDIM, 0);
						}
						//close(socketDribbling);
						printf("player %d thread: 6.1 buffer = %s\n", id, buffer);
					}
				}
			}
			pthread_mutex_unlock(&globalVar);




			printf("player %d thread: tempo rimanente %d\n", id, N);
			sched_yield();
		}

		nDribbling = 0;
		pthread_mutex_unlock(&pallone);
		sched_yield();
	}

	//partita terminata
	printf("player %d thread: terminato\n", id);
}

void* eventManager(void* arg) {

	int* sockets;
	sockets = (int*)arg;
	int s_fd = sockets[0];
	int serviceSocket = sockets[1];
	free(sockets);
	char buf[BUFDIM];
	int player,opponent;
	char azione;

	
	buf[0] = '\0';
	
	recv(serviceSocket, buf, BUFDIM, 0);
	printf("event manager: from service buffer = %s\n", buf);
	if (buf[0] == '\0') {
		//close(serviceSocket);
		printf("event manager: received nothing from buffer\n");
		pthread_exit(NULL);
	}
	/*
		formato messaggio: x%d(%d)(r)\0
		x = tipo di azione
		%d = giocatore attivo
		(%d) opzionale = giocatore in tackle/fallo
		(r) opzionale = risultato tiro/dribbling (y = successo, f = fallimento)
	*/

	if (buf[0] == 'e') {
		snprintf(buf, BUFDIM, "partitaTerminata\0");
		printf("event manager: to client buffer = %s\n", buf);
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		pthread_mutex_unlock(&eventMutex);
		//close(serviceSocket);
		pthread_exit(NULL);
	}
	azione = buf[0];
	player = buf[1] - '0';
	if (player > 9 || player < 0)
	{
		printf("event manager: player wrong id");
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
		printf("event manager: to client buffer = %s\n", buf);
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		pthread_mutex_unlock(&eventMutex);
		
		azione = -1;
		break;

	case DRIBBLING:
		opponent = buf[2] - '0';
		if (opponent > 9 || opponent < 0)
		{
			perror("event manager: opponent wrong id");
			exit(EXIT_FAILURE);
		}
		if (buf[3] == 'y') {
			snprintf(buf, BUFDIM, "il giocatore %d scarta il giocatore %d\0", player, opponent);
		}
		else {
			snprintf(buf, BUFDIM, "il giocatore %d prende la palla da %d\0", opponent, player);
		}
		printf("event manager: to client buffer = %s\n", buf);
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		pthread_mutex_unlock(&eventMutex);
		
		azione = -1;
		break;

	case INFORTUNIO:
		opponent = buf[2] - '0';
		if (opponent > 9 || opponent < 0)
		{
			perror("event manager: opponent wrong id");
			exit(EXIT_FAILURE);
		}
		snprintf(buf, BUFDIM, "il giocatore %d e' vittima di un infortunio da parte di %d\0", player, opponent);
		printf("event manager: to client buffer = %s\n", buf);
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		pthread_mutex_unlock(&eventMutex);
		
		azione = -1;
		break;
	case TIMEOUT:
		snprintf(buf, BUFDIM, "Si e' deciso di tenere un timeout, i giocatori rientreranno in campo a breve...\0", player, opponent);
		printf("event manager: to client buffer = %s\n", buf);
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		pthread_mutex_unlock(&eventMutex);
		pthread_mutex_lock(&globalVar);
		for (int i = 0; i < 10; i++) {
			tempoInfortunio[i] = -1;
			tempoFallo[i] = -1;
		}
		pthread_mutex_unlock(&globalVar);
		break;
	default:
		printf("event manager: caso non gestito\n");
		recv(serviceSocket, buf, BUFDIM, 0);
		printf("event manager: evento scartato = %s\n",buf);
		break;
	}
	printf("event manager: chiudo la socket del servizio\n");
	//close(serviceSocket);
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
	char ip[40];
	pthread_mutex_lock(&globalVar);
	resolve_hostname(hostname, ip, sizeof(ip));
	pthread_mutex_unlock(&globalVar);

	char azione;
	char buf[BUFDIM];
	int s_fd = *(int*)arg; //socket file descriptor del client/arbitro
	int eventSocket, serviceSocket, len, player, opponent;
	struct sockaddr_in eventAddr, serviceAddr;

	serverInit(&eventSocket, &eventAddr, ip, REFEREEPORT);
	bind(eventSocket, (struct sockaddr*)&eventAddr, sizeof(eventAddr));
	listen(eventSocket, N);
	len = sizeof(serviceAddr);

	printf("referee thread: server inizializzato\n");
	pthread_mutex_lock(&globalVar);
	refServer = 0;
	pthread_mutex_unlock(&globalVar);
	while (activePlayer == -1);
	strcpy(buf, "La partita e' cominciata!\n");
	send(s_fd, buf, strlen(buf) + 1, 0);

	printf("referee thread: la partita e' cominciata!\n");
	pthread_t eventReq;

	int* sockets;
	while (1) {
		printf("referee thread: waiting for event\n");
		if (N < 1) {
			snprintf(buf, BUFDIM, "partitaTerminata\0");
			send(s_fd, buf, strlen(buf) + 1, 0);
			//close(s_fd);
			//close(eventSocket);
			pthread_exit(NULL);
		}
		
		serviceSocket = accept(eventSocket, (struct sockaddr*)&serviceAddr, &len);
		printf("referee thread: event received\n");
		sockets = (int*)malloc(2 * sizeof(int));
		sockets[0] = s_fd;
		sockets[1] = serviceSocket;
		pthread_create(&eventReq, NULL, eventManager, (void*)sockets);
		N--;
	}
	
	printf("referee thread: terminato N = %d\n",N);

}



void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, const char* ip, int port) {
	// Chiudi la socket precedente, se aperta
	if (*serviceSocket != -1) {
		close(*serviceSocket);
	}

	// Crea una nuova socket
	*serviceSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*serviceSocket == -1) {
		perror("socket() failed");
		return;
	}

	// Configura l'indirizzo del servizio
	memset(serviceAddr, 0, sizeof(*serviceAddr));
	serviceAddr->sin_family = AF_INET;
	serviceAddr->sin_port = htons(port);

	if (inet_aton(ip, &serviceAddr->sin_addr) == 0) {
		fprintf(stderr, "Invalid IP address: %s\n", ip);
		close(*serviceSocket);
		*serviceSocket = -1;
		return;
	}

	// Connessione al server
	if (connect(*serviceSocket, (struct sockaddr*)serviceAddr, sizeof(*serviceAddr)) == -1) {
		perror("connect() failed");
		close(*serviceSocket);
		*serviceSocket = -1;
		return;
	}

	// Debug: Stampa l'indirizzo IP e la porta
	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &serviceAddr->sin_addr, buf, sizeof(buf));
	printf("Connected to %s:%d\n", buf, port);
}

void serverInit(int* serverSocket, struct sockaddr_in* serverAddr,char* ip, int port) {
	printf("server init: inizio\n");
	int opt = 1;
	*serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (setsockopt(*serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("Errore nel settaggio delle opzioni del socket");
		exit(EXIT_FAILURE);
	}
	serverAddr->sin_family = AF_INET;
	serverAddr->sin_port = htons(port);
	inet_aton(ip, &(serverAddr->sin_addr));
	memset(&(serverAddr->sin_zero), '\0', 8);

	printf("server init: fine\n");
}

int resolve_hostname(const char* hostname, char* ip, size_t ip_len) {
	struct addrinfo hints, * res;
	int errcode;
	void* ptr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // For IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	errcode = getaddrinfo(hostname, NULL, &hints, &res);
	if (errcode != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
		return errcode;
	}

	ptr = &((struct sockaddr_in*)res->ai_addr)->sin_addr;

	if (inet_ntop(res->ai_family, ptr, ip, ip_len) == NULL) {
		perror("inet_ntop");
		freeaddrinfo(res);
		pthread_exit(NULL);
	}

	freeaddrinfo(res);
	return errcode;
}

void writeRetry(int *socket, struct sockaddr_in *addr, const char* ip, int port, char* buffer) {
	int result = send(*socket, buffer, strlen(buffer) + 1, 0);
	for (int i = 0; i < 5; i++) {
		if (result == -1) {
			perror("thread error: write error, retrying");
			//close(*socket);
			serviceInit(socket, addr, ip, port);
			result = send(*socket, buffer, strlen(buffer) + 1, 0);
		}
		else {
			i = 5;
		}
	}
	if (result == -1) {
		perror("thread error: too many retries");
		N = 0;
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char* argv[]) {

	//thread dei giocatori
	pthread_t squadraA[5];
	pthread_t squadraB[5];

	//inizializzo il random number generator
	time_t t;
	srand((unsigned)time(&t));

	/* Set IP address to localhost */
	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	char ip[40], ipTiro[40], ipDribbling[40], ipInfortunio[40];
	while (resolve_hostname(hostname, ip, sizeof(ip))!=0) {
		printf("main: retrying resolve hostname\n");
	}
	while (resolve_hostname("dribbling", ipDribbling, sizeof(ipDribbling)) != 0) {
		printf("main: retrying resolve hostname\n");
	}
	while (resolve_hostname("infortunio", ipInfortunio, sizeof(ipInfortunio)) != 0) {
		printf("main: retrying resolve hostname\n");
	}
	while (resolve_hostname("tiro", ipTiro, sizeof(ipTiro)) != 0) {
		printf("main: retrying resolve hostname\n");
	}






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
	pthread_mutex_init(&globalVar, NULL);
	pthread_mutex_init(&eventMutex, NULL);
	pthread_mutex_lock(&pallone); //i giocatori aspettano l'inizio della partita

	//indici per inserire i giocatori nelle squadre
	int i = 0;
	int j = 0;
	short ref = 0;
	//attesa di richieste per i giocatori
	int players[10];
	
	
	while (i < 5 || j < 5 || ref != 1) {

		printf("main: index i=%d j=%d ref=%d\n", i, j, ref);
		clientSocket = accept(mySocket, (struct sockaddr*)&client, &len);
		recv(clientSocket, buffer, BUFDIM, 0);
		char bufferTiro[3];
		char bufferDribbling[3];
		char bufferInfortunio[3];
		printf("main: client buffer = %s\n", buffer);
		snprintf(bufferTiro, sizeof(bufferTiro), "%c%c\0", buffer[0], buffer[1]);
		snprintf(bufferDribbling, sizeof(bufferDribbling), "%c%c\0", buffer[0], buffer[1]);
		snprintf(bufferInfortunio, sizeof(bufferInfortunio), "%c%c\0", buffer[0], buffer[1]);
		/*
			Qui bisogna stabilire il formato del messaggio e il modo di
			interpretarlo. esempio messaggio A3 indicano la squadra (A, B)
			e id giocatore (0..9)
		*/

		if (buffer[0] == 'A' && i<5) {
			//close(clientSocket);
			players[i + j] = buffer[1] - '0';
			pthread_mutex_lock(&globalVar);
			squadre[players[i + j]] = 'A';
			pthread_mutex_unlock(&globalVar);
			
			pthread_create(&squadraA[i], NULL, playerThread, (void*)&players[i+j]);
			i++;

			serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
			serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
			serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);

			if (send(socketTiro, bufferTiro, strlen(bufferTiro) + 1, MSG_CONFIRM) < 0) perror("errore nella scrittura");
			if (send(socketDribbling, bufferDribbling, strlen(bufferDribbling) + 1, MSG_CONFIRM) < 0) perror("errore nella scrittura");
			if (send(socketInfortunio, bufferInfortunio, strlen(bufferInfortunio) + 1, MSG_CONFIRM) < 0) perror("errore nella scrittura");

			printf("main: waiting for ack\n");
			recv(socketTiro, buffer, BUFDIM, 0);
			recv(socketDribbling, buffer, BUFDIM, 0);
			recv(socketInfortunio, buffer, BUFDIM, 0);
			printf("main: %s received\n",buffer);

		}
		else{
			if (buffer[0] == 'B' && j < 5) {
				//close(clientSocket);
				players[i + j] = buffer[1] - '0';
				pthread_mutex_lock(&globalVar);
				squadre[players[i + j]] = 'B';
				pthread_mutex_unlock(&globalVar);
				pthread_create(&squadraB[j], NULL, playerThread, (void*)&players[i+j]);
				j++;

				serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
				serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
				serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);

				if (send(socketTiro, bufferTiro, strlen(bufferTiro) + 1, MSG_CONFIRM) < 0) perror("errore nella scrittura");
				if (send(socketDribbling, bufferDribbling, strlen(bufferDribbling) + 1, MSG_CONFIRM) < 0) perror("errore nella scrittura");
				if (send(socketInfortunio, bufferInfortunio, strlen(bufferInfortunio) + 1, MSG_CONFIRM) < 0) perror("errore nella scrittura");

				recv(socketTiro, buffer, BUFDIM, 0);
				recv(socketDribbling, buffer, BUFDIM, 0);
				recv(socketInfortunio, buffer, BUFDIM, 0);
			}
			else {
				if (buffer[0] == 's') {
					//creato il thread dell'arbitro, gestisce la comunicazione col client.
					pthread_create(&arbitro, NULL, refereeThread, (void*)&clientSocket);
					ref = 1;
					printf("main: referee started\n");
				}
			}
		}
	}

	//close(mySocket);

	printf("la partita sta per cominciare\n");



	
	while (refServer != 0);
	while (playerCount > 0);
	activePlayer = rand() % 10;
	pthread_mutex_unlock(&pallone);

	printf("la partita e' cominciata!\n");
	for (int i = 0; i < 5; i++) {
		pthread_join(squadraA[i],NULL);
		pthread_join(squadraB[i],NULL);
	}

	serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
	serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);
	serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);

	snprintf(buffer, BUFDIM, "t\0");

	send(socketTiro, buffer, strlen(buffer)+1, 0);
	send(socketInfortunio, buffer, strlen(buffer) + 1, 0);
	send(socketDribbling, buffer, strlen(buffer) + 1, 0);
	printf("main: messaggio di terminazione inviato ai servizi\n");

	pthread_join(arbitro, NULL);

	//if (clientSocket != -1) close(clientSocket);
	return 0;
}

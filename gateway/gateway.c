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
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h> 

#define PORT 8080
#define REFEREEPORT 8088
#define DRIBBLINGPORT 8033
#define INFORTUNIOPORT 8041
#define TIROPORT 8077
#define BUFDIM 1024
#define TEAMSIZE 10
#define WAIT 0

//servono per identificare il tipo di evento per l'arbitro
#define TIRO 't'
#define INFORTUNIO 'i'
#define DRIBBLING 'd'
#define TIMEOUT 'h'

/*
	In questo script verranno gestiti i giocatori e gli eventi della partita.
	Gestira' i giocatori come thread, insieme al processo arbitro,
	il giocatore con la palla e' l'unico thread giocatore attivo, che
	genera un evento di dribbling e cosi' via (vedi dribbling.c).
	*/


pthread_mutex_t pallone; //risorsa pallone, solo un thread giocatore attivo
pthread_mutex_t* globalVar; //mutex per trattare in maniera sicura variabili globali
pthread_mutex_t eventMutex; //multipli thread event manager hanno accesso concorrente alla socket

volatile char squadre[TEAMSIZE] = { 0 }; //indica a che squadra appartiene un giocatore

//tempo di attesa dei giocatori inizializzati nel main
volatile int* tempoFallo;
volatile int* tempoInfortunio;

//tempo della partita inteso come numero di eventi, a 0 la partita termina.
int* N; //inizializzato nel main

//indica quale giocatore ha il possesso del pallone va da 0 a 9
volatile int activePlayer = -1;

volatile short playerCount = TEAMSIZE; //variabile per preparare i giocatori

//semafori per la sincronizzazione dei thread
sem_t refServer;
sem_t playerSemaphore;

//semafori per la sincronizzazione dei processi
sem_t *eventSemaphore;
sem_t *processSemaphore;
sem_t *timeoutSemaphore;

//funzioni di supporto
void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, const char* ip, int port);
void serverInit(int* serverSocket, struct sockaddr_in* serverAddr, char* ip, int port);
int resolve_hostname(const char* hostname, char* ip, size_t ip_len);
void writeRetry(int* socket, struct sockaddr_in* addr, const char* ip, int port, char* buffer);

//definizione del comportamento del thread giocatore
void* playerThread(void* arg) {
	int id = *(int*)arg; //identificativo del giocatore
	
	if (id > 9 || id < 0)
	{
		perror("player: wrong id");
		exit(EXIT_FAILURE);
	}

	pthread_mutex_lock(globalVar);
	char squadra = squadre[id]; //ottiene la squadra e la conserva in una varabile semplice

	//inizializzazione dei servizi
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
	pthread_mutex_unlock(globalVar);
	
	char buffer[BUFDIM]; //buffer per le comunicazioni coi servizi
	
	//variabili necessari per l'iniziallizzazione dei servizi
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

	//informiamo agli altri thread che un giocatore e' pronto
	pthread_mutex_lock(globalVar);
	playerCount--;
	if (playerCount <= 0) {
		sem_post(&playerSemaphore);
		printf("giocatori tutti pronti");
	}
	pthread_mutex_unlock(globalVar);

	while (*N > 0) { //fino a quando non finisce la partita

		pthread_mutex_lock(&pallone); //attende di ricevere possesso del pallone
		while (activePlayer == id && *N > 0) { //controlla se il possesso del pallone e' legale
			
			//informo il servizio Dribbling dell'evento
			serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);
			
			//gestisco il possibile errore di questo snprintf
			snprintf(buffer, BUFDIM, "%d\0", id);
			writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
			
			printf("player %d thread: %s sent to dribbling\n",id,buffer);
			recv(socketDribbling, buffer, BUFDIM, 0);
			printf("player %d thread: dribbling buffer = %s\n", id, buffer);

			/*
				formato messaggio dribbling: "x%d"
				(x = s = successo, x = f = fallimento, x = i = infortunio)
				%d = giocatore avversario
			*/

			//inizializzo i dati dell'altro giocatore
			altroPlayer = buffer[1] - '0';

			//gestisco un eventuale errore
			while ((altroPlayer > 9 || altroPlayer < 0) || 
				(squadra == squadre[altroPlayer] || tempoInfortunio[activePlayer] > 0 || tempoFallo[activePlayer] > 0))
			{
				printf("player %d thread: oppnent wrong id=%d, retrying...\n",id,altroPlayer);
				serviceInit(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT);

				//gestisco il possibile errore di questo snprintf
				snprintf(buffer, BUFDIM, "%d\0", id);
				writeRetry(&socketDribbling, &addrDribbling, ipDribbling, DRIBBLINGPORT, buffer);
				recv(socketDribbling, buffer, BUFDIM, 0);
				altroPlayer = buffer[1] - '0';
			}
			altraSquadra = squadre[altroPlayer];

			//switch per l'evento del dribbling
			switch (buffer[0]) {
			case 'i':
				//messaggio inviato a infortunio per decidere i tempi
				serviceInit(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT);
				snprintf(buffer, BUFDIM, "%d%d\0", id, altroPlayer);
				writeRetry(&socketInfortunio, &addrInfortunio, ipInfortunio, INFORTUNIOPORT, buffer);
				printf("player %d thread: 5.1 buffer = %s\n", id, buffer);
				recv(socketInfortunio, buffer, BUFDIM, 0);
				sem_wait(eventSemaphore); //attende che l'arbitro gestisca l'evento

				/*
					formato messaggio infortunio: IXXXPXXX\0
					I precede il tempo di infortunio
					P precede il tempo di penalita'
				*/

				//imposto i tempi di attesa di entrambi i giocatori
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

				//TODO: verificare la necessita' di un timeout

				while (squadre[activePlayer] != squadra || tempoInfortunio[activePlayer] > 0 || tempoFallo[activePlayer] > 0){
					activePlayer = rand() % TEAMSIZE;
				}
				printf("player %d thread: la palla viene passata a %d per infortunio\n", id, activePlayer);
				//adesso ad avere il pallone e' un giocatore della propria squadra

				break;
			case 'f':
				activePlayer = altroPlayer;
				sem_wait(eventSemaphore); //attende che l'arbitro gestisca l'evento
				//il giocatore ha perso la palla a un giocatore avversario

				break;
			case 's':
				//non perde la palla (tenta un tiro?)
				sem_wait(eventSemaphore); //attende che l'arbitro gestisca l'evento

				//inizializzata chance di tiro
				chance = rand() % 100;
				chance = chance + (nDribbling * 30);
				nDribbling++;
				if (chance > 70) {
					//tenta un tiro
					serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
					snprintf(buffer, BUFDIM, "%d\0", id);
					writeRetry(&socketTiro, &addrTiro, ipTiro, TIROPORT, buffer);
					recv(socketTiro, buffer, BUFDIM, 0);
					//gestione errori
					while (buffer[0] != 'a') {
						serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
						snprintf(buffer, BUFDIM, "%d\0", id);
						writeRetry(&socketTiro, &addrTiro, ipTiro, TIROPORT, buffer);
						recv(socketTiro, buffer, BUFDIM, 0);
					}
					sem_wait(eventSemaphore); //attende che l'arbitro gestisca l'evento

					while (squadre[activePlayer] == squadra || tempoInfortunio[activePlayer] > 0 || tempoFallo[activePlayer] > 0) {
						activePlayer = rand() % TEAMSIZE;
					}

					//il pallone viene dato ad un giocatore della squadra avversaria quando avviene un tiro.
					printf("player %d thread: la palla viene passata a %d per tiro\n", id, activePlayer);
				}
				break;
			}

			
			printf("player %d thread: 6\n",id);

			//prima che lasci il pallone o ricominci

			//riduce tempi di attesa e informa dribbling in caso di ritorno in campo
			for (int k = 0; k < TEAMSIZE; k++) {
				if (tempoInfortunio[k] > 0) tempoInfortunio[k]--;
				else {
					if (tempoInfortunio[k] == 0) {
						tempoInfortunio[k] = -1;
					}
				}
				if (tempoFallo[k] > 0) tempoFallo[k]--;
				else {
					if (tempoFallo[k] == 0) {
						tempoFallo[k] = -1;
					}
				}
			}
			
			
			sched_yield(); //permette di operare ad altri thread o processi
		}

		nDribbling = 0;
		pthread_mutex_unlock(&pallone);
		sched_yield(); //altri giocatori devono avere l'opportunità di prendere la risorsa pallone
	}

	//partita terminata
	printf("player %d thread: terminato\n", id);
}

//thread per la gestione del timeout, presente nel processo main
void* timeoutEvent(void* argv) {
	while (1) {
		sem_wait(timeoutSemaphore); //attende che l'arbitro dichiari timeout
		printf("TimeouEvent thread: c'e' un timeout, azzero i tempi");
		for (int i = 0; i < TEAMSIZE; i++) {
			tempoInfortunio[i] = -1;
			tempoFallo[i] = -1;
		}
	}
}

//thread per la gestione degli eventi, presente nel processo arbitro
/*
	nel thread vi e' la presenza di un mutex per i singoli eventi, messo
	per irrobustire il programma, poiche' i player attendono comunque
	la fine di un evento prima di generarne un altro
*/
void* eventManager(void* arg) {

	//inizializzazione delle variabili
	int* sockets;
	sockets = (int*)arg;
	int s_fd = sockets[0];
	int serviceSocket = sockets[1];
	free(sockets); //liberazione di memoria, causa malloc prima della generazione del thread
	char buf[BUFDIM];
	int player,opponent;
	char azione;

	
	buf[0] = '\0';//inizializzo la stringa
	
	recv(serviceSocket, buf, BUFDIM, 0);
	if (buf[0] == '\0') {
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

	//il caso in cui il messaggio e' 'e' allora termina la partita
	if (buf[0] == 'e') {
		snprintf(buf, BUFDIM, "partitaTerminata\0");

		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		recv(s_fd, buf, strlen(buf) + 1, 0);
		if (strncmp(buf, "ack", 3)) perror("event manager: errore ack da client");
		pthread_mutex_unlock(&eventMutex);

		pthread_exit(NULL);
	}

	azione = buf[0];
	player = buf[1] - '0';
	
	//gestione errori
	if (azione != TIRO && azione != INFORTUNIO &&
		azione != DRIBBLING && azione != TIMEOUT)
	{
		printf("event manager: wrong event");
		azione = 'k';
	}

	switch (azione) {
	case TIRO:
		if (buf[2] == 'y') {
			snprintf(buf, BUFDIM, "il giocatore %d tira... ed e' GOAL!!!\0", player);
		}
		else {
			snprintf(buf, BUFDIM, "il giocatore %d tira... e ha mancato la porta...\0", player);
		}
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		recv(s_fd, buf, strlen(buf) + 1, 0);
		if (strncmp(buf, "ack", 3)) perror("event manager: errore ack da client");
		pthread_mutex_unlock(&eventMutex);
		
		azione = -1;
		break;
	case DRIBBLING:
		opponent = buf[2] - '0';
		if (opponent > 9 || opponent < 0 || squadre[opponent] == squadre[player])
		{
			printf("event manager %d: opponent wrong id, opp=%d player=%d\n",*N,opponent,player);
			*N = *N + 1;
			pthread_exit(NULL);
		}
		if (buf[3] == 'y') {
			snprintf(buf, BUFDIM, "il giocatore %d scarta il giocatore %d\0", player, opponent);
		}
		else {
			snprintf(buf, BUFDIM, "il giocatore %d prende la palla da %d\0", opponent, player);
		}
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		recv(s_fd, buf, strlen(buf) + 1, 0);
		if (strncmp(buf, "ack", 3)) perror("event manager: errore ack da client");
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
		pthread_mutex_lock(&eventMutex);
		send(s_fd, buf, strlen(buf) + 1, 0);
		recv(s_fd, buf, strlen(buf) + 1, 0);
		if (strncmp(buf, "ack", 3)) perror("event manager: errore ack da client");
		pthread_mutex_unlock(&eventMutex);
		
		azione = -1;
		break;
	default:
		printf("event manager: caso non gestito\n");
		break;
	}
	sem_post(eventSemaphore);//informa il giocatore che l'evento e' stato gestito
}


/*
	l'arbitro gestisce le comunicazioni con il thread arbitro lato client.
	Manda a quest'ultimo gli esiti di ogni evento. In qualche modo deve, quindi,
	ricevere i risultati delle azioni dai thread giocatori. Ci riesce grazie alle
	comunicazioni da parte dei servizi
*/
void refereeProcess(int* arg) {

	//risoluzione indirizzo ip
	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	char ip[40];
	resolve_hostname(hostname, ip, sizeof(ip));
	printf("referee process: ip impostato: %s\n", ip);

	//definizione e inizializzazione delle veriabili
	char buf[BUFDIM];
	int s_fd = *arg; //socket file descriptor del client/arbitro
	int eventSocket, serviceSocket, len, player, opponent;
	struct sockaddr_in eventAddr, serviceAddr;

	serverInit(&eventSocket, &eventAddr, ip, REFEREEPORT);
	bind(eventSocket, (struct sockaddr*)&eventAddr, sizeof(eventAddr));
	listen(eventSocket, *N);
	len = sizeof(serviceAddr);

	sem_post(&refServer); //informa il main che il referee e' pronto
	
	strcpy(buf, "La partita e' cominciata!\n");
	send(s_fd, buf, strlen(buf) + 1, 0);
	pthread_t eventReq;

	int* sockets; //puntatore per le socket da inviare all'event manager
	while (1) {
		if (*N < 1) {
			printf("referee process %d: partita terminata...\n",*N);
			snprintf(buf, BUFDIM, "partitaTerminata\0");
			send(s_fd, buf, strlen(buf) + 1, 0);
			sem_post(processSemaphore);
			sem_unlink("eSem");
			sem_close(eventSemaphore);
			sem_unlink("pSem");
			sem_close(processSemaphore);
			exit(1);
		}

		printf("referee process %d: waiting for event\n",*N);
		serviceSocket = accept(eventSocket, (struct sockaddr*)&serviceAddr, &len);
		printf("referee process %d: event received\n",*N);
		sockets = (int*)malloc(2 * sizeof(int));
		sockets[0] = s_fd;
		sockets[1] = serviceSocket;
		pthread_create(&eventReq, NULL, eventManager, (void*)sockets);
		pthread_join(eventReq, NULL);
		*N = *N - 1;
	}

}


//metodo di supporto per l'inizializzazione di socket client pronte all'uso
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

//metodo per inizializzare una socket server
void serverInit(int* serverSocket, struct sockaddr_in* serverAddr,char* ip, int port) {

	int opt = 1; //opzioni

	//inizializzazione e settaggio
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
}

//metodo per la risoluzione di un hostname
int resolve_hostname(const char* hostname, char* ip, size_t ip_len) {
	//definizione delle variabili
	struct addrinfo hints, * res;
	int errcode;
	void* ptr;

	//inizializzazione hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // Per IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	//ottiene l'hostname
	errcode = getaddrinfo(hostname, NULL, &hints, &res);
	if (errcode != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
		return errcode;
	}

	//puntatore per l'impostazione dell'indirizzo
	ptr = &((struct sockaddr_in*)res->ai_addr)->sin_addr;

	//scrittura dell'indirizzo ip
	if (inet_ntop(res->ai_family, ptr, ip, ip_len) == NULL) {
		perror("inet_ntop");
		freeaddrinfo(res);
		pthread_exit(NULL);
	}

	freeaddrinfo(res);
	return errcode;
}

//metodo di supporto per tentare nuovamente la comunicazione con un servizio
void writeRetry(int *socket, struct sockaddr_in *addr, const char* ip, int port, char* buffer) {
	int result = send(*socket, buffer, strlen(buffer) + 1, 0);
	for (int i = 0; i < 5; i++) {
		if (result == -1) {
			perror("thread error: write error, retrying");
			serviceInit(socket, addr, ip, port);
			result = send(*socket, buffer, strlen(buffer) + 1, 0);
		}
		else {
			i = 5;
		}
	}
	//per non procedere all'infinito allora termina
	if (result == -1) {
		perror("thread error: too many retries");
		*N = 0;
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char* argv[]) {

	//inizializzo il random number generator
	time_t t;
	srand((unsigned)time(&t));

	char buffer[BUFDIM];

	// Impostiamo gli ip dei servizi e del gateway
	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	char ip[40], ipTiro[40], ipDribbling[40], ipInfortunio[40];
	while (resolve_hostname(hostname, ip, sizeof(ip)) != 0) {
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

	//definisco variabili per le socket
	struct sockaddr_in addrTiro, addrInfortunio, addrDribbling;
	int socketTiro, socketInfortunio, socketDribbling;
	int mySocket, clientSocket, len;
	struct sockaddr_in myaddr, client;

	serverInit(&mySocket, &myaddr, ip, PORT);

	len = sizeof(client);

	bind(mySocket, (struct sockaddr*)&myaddr, sizeof(myaddr));
	listen(mySocket, 12);

	inet_ntop(AF_INET, &myaddr.sin_addr, buffer, sizeof(buffer));
	printf("Accepting as %s with port %d...\n", buffer, PORT);

	//thread dei giocatori
	pthread_t squadraA[5];
	pthread_t squadraB[5];

	//inizializzazione delle variabili e mutex in memoria condivisa
	N = (int*)mmap(
		NULL, sizeof(int), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	tempoFallo = (int*)mmap(
		NULL, TEAMSIZE*sizeof(int), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	tempoInfortunio = (int*)mmap(
		NULL, TEAMSIZE*sizeof(int), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	globalVar = (pthread_mutex_t*)mmap(
		NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	for (int i = 0; i < TEAMSIZE; i++) {
		tempoFallo[i] = -1;
		tempoInfortunio[i] = -1;
	}

	*N = 60;

	//inizializzazione di semafori e mutex
	pthread_mutex_init(&pallone, NULL);
	pthread_mutex_init(globalVar, NULL);
	pthread_mutex_init(&eventMutex, NULL);
	sem_init(&playerSemaphore, 0, 1);
	sem_init(&refServer, 1, 1);
	eventSemaphore = sem_open("eSem", O_CREAT | O_EXCL, 0644, 0);
	processSemaphore = sem_open("pSem", O_CREAT | O_EXCL, 0644, 0);
	timeoutSemaphore = sem_open("tSem", O_CREAT | O_EXCL, 0644, 0);
	pthread_mutex_lock(&pallone); //i giocatori aspettano l'inizio della partita

	//indici per inserire i giocatori nelle squadre
	int i = 0;
	int j = 0;
	short ref = 0;

	//array di richieste di accesso per i giocatori
	int players[TEAMSIZE];
	
	//URGE GRANDI CAMBIAMENTI
	while (i < 5 || j < 5 || ref != 1) {

		printf("main: index i=%d j=%d ref=%d\n", i, j, ref);
		clientSocket = accept(mySocket, (struct sockaddr*)&client, &len);
		recv(clientSocket, buffer, BUFDIM, 0);
		printf("main: client buffer = %s\n", buffer);

		/*
			Qui bisogna interpretare il formato del messaggio. 
			esempio messaggio 'A3' indicano la squadra (che puo' essere A o B)
			e id giocatore (da 0 a 9)
		*/

		//inserisco giocatore nella squadra A
		if (buffer[0] == 'A' && i<5) {
			players[i + j] = buffer[1] - '0';

			//metto la squadra corretta all'interno dell'array globale
			pthread_mutex_lock(globalVar);
			squadre[players[i + j]] = 'A';
			pthread_mutex_unlock(globalVar);
			
			pthread_create(&squadraA[i], NULL, playerThread, (void*)&players[i+j]);
			i++;
		}
		else{
			if (buffer[0] == 'B' && j < 5) {
				players[i + j] = buffer[1] - '0';

				//metto la squadra corretta all'interno dell'array globale
				pthread_mutex_lock(globalVar);
				squadre[players[i + j]] = 'B';
				pthread_mutex_unlock(globalVar);

				pthread_create(&squadraB[j], NULL, playerThread, (void*)&players[i+j]);
				j++;
			}
			else {
				if (buffer[0] == 's') {
					//creato il processo dell'arbitro, gestisce la comunicazione col client.
					if (fork() == 0) {

						//inizializzazione semafori nel nuovo processo
						eventSemaphore = sem_open("eSem", 0);
						processSemaphore = sem_open("pSem", 0);
						timeoutSemaphore = sem_open("tSem", 0);
						refereeProcess(&clientSocket);

						//non necessari ma irrobustisce il programma
						sem_unlink("eSem");
						sem_close(eventSemaphore);
						sem_unlink("pSem");
						sem_close(processSemaphore);
						sem_unlink("tSem");
						sem_close(timeoutSemaphore);
						exit(1);
					}
					ref = 1;
					printf("main: referee started\n");
				}
			}
		}
	}

	//attende che il referee e i giocatori siano pronti
	sem_wait(&refServer);
	sem_wait(&playerSemaphore);

	//determina il primo giocatore ad avere la palla
	activePlayer = rand() % TEAMSIZE;

	//comincia la partita
	pthread_mutex_unlock(&pallone);

	printf("la partita e' cominciata!\n");
	pthread_t timeoutThread;
	pthread_create(&timeoutThread, NULL, timeoutEvent, (void*)argv);

	//attende la fine della partita
	while (sem_wait(processSemaphore) != 0);

	//chiude e libera i semafori
	sem_destroy(&refServer);
	sem_destroy(&playerSemaphore);
	sem_unlink("eSem");
	sem_close(eventSemaphore);
	sem_unlink("pSem");
	sem_close(processSemaphore);
	sem_unlink("tSem");
	sem_close(timeoutSemaphore);
	return 0;
}

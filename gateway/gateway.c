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
#include <signal.h>

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
sem_t playerSemaphore;

int pipe_fd[2];
int event_pipe[2];

//funzioni di supporto
void serviceInit(int* serviceSocket, struct sockaddr_in* serviceAddr, const char* ip, int port);
void serverInit(int* serverSocket, struct sockaddr_in* serverAddr, char* ip, int port);
int resolve_hostname(const char* hostname, char* ip, size_t ip_len);
void writeRetry(int* socket, struct sockaddr_in* addr, const char* ip, int port, char* buffer);
int checkTimeout();

//pid del referee, necessario per il segnale di timeout
pid_t refPid;

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
			
			printf("giocatore %d inizia il turno nella partita %d\n", id, getpid());

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
				(squadra == squadre[altroPlayer] || tempoInfortunio[altroPlayer] > 0 || tempoFallo[altroPlayer] > 0))
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

				snprintf(buffer, BUFDIM, "i%d%d\0",id,altroPlayer);
				write(pipe_fd[1], buffer, BUFDIM);
				printf("attendo evento infortunio di partita %d\n", getpid());
				read(event_pipe[0], buffer, BUFDIM);
				printf("completato evento infortunio di partita %d\n", getpid());

				while (squadre[activePlayer] != squadra || tempoInfortunio[activePlayer] > 0 || tempoFallo[activePlayer] > 0){
					activePlayer = rand() % TEAMSIZE;
				}
				printf("player %d thread: la palla viene passata a %d per infortunio\n", id, activePlayer);
				//adesso ad avere il pallone e' un giocatore della propria squadra

				break;
			case 'f':
				activePlayer = altroPlayer;

				snprintf(buffer, BUFDIM, "d%d%df\0",id,altroPlayer);
				write(pipe_fd[1], buffer, BUFDIM);
				printf("attendo evento scarto di partita %d\n", getpid());
				read(event_pipe[0], buffer, BUFDIM);
				printf("completato evento scarto di partita %d\n", getpid());

				//il giocatore ha perso la palla a un giocatore avversario

				break;
			case 's':
				//non perde la palla (tenta un tiro?)

				snprintf(buffer, BUFDIM, "d%d%dy\0",id,altroPlayer);
				write(pipe_fd[1], buffer, BUFDIM);
				printf("attendo evento dribbling di partita %d\n", getpid());
				read(event_pipe[0], buffer, BUFDIM);
				printf("completato evento dribbling di partita %d\n", getpid());

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
					while (strcmp(buffer,"err") == 0) {
						serviceInit(&socketTiro, &addrTiro, ipTiro, TIROPORT);
						snprintf(buffer, BUFDIM, "%d\0", id);
						writeRetry(&socketTiro, &addrTiro, ipTiro, TIROPORT, buffer);
						recv(socketTiro, buffer, BUFDIM, 0);
					}

					write(pipe_fd[1], buffer, BUFDIM);
					printf("attendo evento tiro di partita %d\n", getpid());
					read(event_pipe[0], buffer, BUFDIM);
					printf("completato evento tiro di partita %d\n", getpid());

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

			printf("giocatore %d finisce il turno nella partita %d\n", id, getpid());
		}

		nDribbling = 0;
		pthread_mutex_unlock(&pallone);
		sched_yield(); //altri giocatori devono avere l'opportunità di prendere la risorsa pallone
	}

	//partita terminata
	printf("player %d thread: terminato\n", id);
}


//thread per la gestione degli eventi, presente nel processo arbitro
/*
	nel thread vi e' la presenza di un mutex per i singoli eventi, messo
	per irrobustire il programma, poiche' i player attendono comunque
	la fine di un evento prima di generarne un altro
*/
void* eventManager(void* arg) {

	typedef struct ref_event {
		int sock;
		char buff[BUFDIM];
	} my_event;

	//inizializzazione delle variabili
	int* sockets;
	sockets = (int*)arg;
	my_event ev = *(my_event*)arg;
	int s_fd = ev.sock;
	char buf[BUFDIM];
	int player,opponent;
	char azione;

	
	buf[0] = '\0';//inizializzo la stringa
	
	strcpy(buf, ev.buff);
	printf("event manager: received %s from pipe\n", buf);
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
			printf("event manager %d: opponent wrong id, opp=%c%d player=%c%d\n", *N, squadre[opponent], opponent, squadre[player], player);
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

		int tim = checkTimeout();
		if (tim == 1) {
			for (int i = 0; i < 10; i++) {
				tempoFallo[i] = -1;
				tempoInfortunio[i] = -1;
			}
			strcpy(buf, "A causa delle recenti vicissitudini viene stabilito un timeout!\0");
			send(s_fd, buf, strlen(buf) + 1, 0);
			recv(s_fd, buf, strlen(buf) + 1, 0);
		}
		
		azione = -1;
		break;
	default:
		printf("event manager: caso non gestito\n");
		break;
	}
	strcpy(buf, "akc\0");
	write(event_pipe[1], buf, BUFDIM);
}


/*
	l'arbitro gestisce le comunicazioni con il thread arbitro lato client.
	Manda a quest'ultimo gli esiti di ogni evento. In qualche modo deve, quindi,
	ricevere i risultati delle azioni dai thread giocatori. Ci riesce grazie alle
	comunicazioni da parte dei servizi
*/
void refereeProcess(int* arg) {

	typedef struct ref_event {
		int sock;
		char buff[BUFDIM];
	} my_event;

	//risoluzione indirizzo ip
	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	char ip[40];
	resolve_hostname(hostname, ip, sizeof(ip));
	printf("referee process: ip impostato: %s\n", ip);

	//definizione e inizializzazione delle veriabili
	char buf[BUFDIM];
	int s_fd = *arg; //socket file descriptor del client/arbitro
	int serviceSocket, len, player, opponent;
	struct sockaddr_in serviceAddr;

	recv(s_fd, buf, BUFDIM, 0);
	printf("referee process: received %s\n", buf);
	
	strcpy(buf, "La partita e' cominciata!\n");
	send(s_fd, buf, strlen(buf) + 1, 0);
	pthread_t eventReq;

	int* sockets; //puntatore per le socket da inviare all'event manager
	while (1) {
		if (*N < 1) {
			printf("referee process %d: partita terminata...\n",*N);
			snprintf(buf, BUFDIM, "partitaTerminata\0");
			send(s_fd, buf, strlen(buf) + 1, 0);
			printf("LA PARTITA NUMERO %d TERMINA!\n", getppid());
			kill(getppid(), SIGKILL);
			close(event_pipe[1]);
			exit(1);
		}
		my_event ev;
		printf("referee process %d partita %d: waiting for event\n",*N,getppid());
		read(pipe_fd[0], buf, BUFDIM);
		printf("referee process %d partita %d: event received\n",*N,getppid());
		ev.sock = s_fd;
		strcpy(ev.buff, buf);
		pthread_create(&eventReq, NULL, eventManager, (void*)&ev);
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

int checkTimeout() {
	int acc = 0, j = 0, k = 0, ret = 0;
	int squadraA[5];
	int squadraB[5];
	for(int i=0;i<10;i++){
		if (squadre[i] == 'A') {
			squadraA[j] = i;
			j++;
		}
		else {
			squadraB[k] = i;
			k++;
		}
	}
	for (int i = 0; i < 5; i++) {
		if (tempoFallo[squadraA[i]] > 0 || tempoInfortunio[squadraA[i]] > 0) acc++;
	}
	if (acc == 5) {
		ret = 1;
		kill(getpid(), SIGUSR1);
		kill(refPid, SIGUSR2);
	}
	acc = 0;
	for (int i = 0; i < 5; i++) {
		if (tempoFallo[squadraB[i]] > 0 || tempoInfortunio[squadraB[i]] > 0) acc++;
	}
	if (acc == 5) {
		ret = 1;
		kill(getpid(), SIGUSR1);
		kill(refPid, SIGUSR2);
	}
	return ret;
}

//handler per il segnale di timeout
void handler(int sig) {
	if (sig == SIGUSR1) {
		for (int i = 0; i < 10; i++) {
			tempoFallo[i] = -1;
			tempoInfortunio[i] = -1;
		}
	}
	else if(sig == SIGUSR2){
		printf("referee process: timeout!\n");
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


	signal(SIGINT, SIG_IGN);

	int pipe_check[2];

	while (1) {
		printf("pid %d: sto per avere un figlio!\n", getpid());
		if (pipe(pipe_check) < 0) perror("pipe check error"), exit(1);
		pid_t pid = fork();
		if (pid < 0) perror("fork error"), exit(1);
		if (pid == 0) {
			
			close(pipe_check[0]);

			signal(SIGUSR1, handler);
			signal(SIGUSR2, handler);

			if (pipe(pipe_fd) < 0) perror("pipe error"), exit(1);
			if (pipe(event_pipe) < 0) perror("pipe error"), exit(1);

			printf("pid %d: Hello World!\n", getpid());

			//definisco variabili per le socket
			int mySocket, clientSocket, len, refereeSocket;
			struct sockaddr_in myaddr, client;

			printf("pid %d: starting...\n", getpid());

			serverInit(&mySocket, &myaddr, ip, PORT);

			len = sizeof(client);


			printf("pid %d: socket ready.\n", getpid());
			bind(mySocket, (struct sockaddr*)&myaddr, sizeof(myaddr));
			listen(mySocket, 12);

			inet_ntop(AF_INET, &myaddr.sin_addr, buffer, sizeof(buffer));
			printf("pid %d: Accepting as %s with port %d...\n", getpid(), buffer, PORT);

			int refCheck = 0;
			int playerCheck = 0;
			printf("pid %d: Waiting for players...\n",getpid());
			char playerBuf[8][128];
			for (int i = 0; i < 9; i++) {
				clientSocket = accept(mySocket, (struct sockaddr*)&client, &len);
				recv(clientSocket, buffer, BUFDIM, 0);
				int cmp = strncmp(buffer, "referee", 7);
				printf("la stringa '%s', confrontata con 'referee', risulta cmp=%d\n", buffer, cmp);
				if (cmp == 0) {
					if (refCheck == 1) {
						strcpy(buffer, "wait\0");
						send(clientSocket, buffer, BUFDIM, 0);
						close(clientSocket);
						i--;
						continue;
					}
					else {
						printf("pid %d: referee accolto\n", getpid());
						refereeSocket = clientSocket;
						refCheck = 1;
						}
				}
				else {
					if (playerCheck < 8) {
						strcpy(playerBuf[i], buffer);
						printf("pid %d recv: %s\n", getpid(), buffer);
						snprintf(buffer, BUFDIM, "%d\0", getpid());
						send(clientSocket, buffer, BUFDIM, 0);
						close(clientSocket);
					}
					else {
						i--;
						snprintf(buffer, BUFDIM, "err\0");
						send(clientSocket, buffer, BUFDIM, 0);
						close(clientSocket);
					}
					playerCheck++;
				}
					
				
				
				
			}

			close(mySocket);
			write(pipe_check[1], buffer, BUFDIM);
			close(pipe_check[1]);
			printf("signal sent\n");
			//printf("pid %d: allowing new match!\n", getpid());

			for (int i = 0; i < 8; i++) {
				strcpy(buffer, playerBuf[i]);
				printf("pid %d: matrix row %d: %s\n", getpid(), i, buffer);
				send(refereeSocket, buffer, BUFDIM, 0);
				recv(refereeSocket, buffer, BUFDIM, 0);
			}
			strcpy(buffer, "end\0");
			send(refereeSocket, buffer, BUFDIM, 0);
			recv(refereeSocket, buffer, BUFDIM, 0);
			snprintf(buffer, BUFDIM, "%d\0", getpid());
			send(refereeSocket, buffer, BUFDIM, 0);
			
			printf("pid %d: sockets received...\n", getpid());
			//thread dei giocatori
			pthread_t squadraA[5];
			pthread_t squadraB[5];

			//inizializzazione delle variabili e mutex in memoria condivisa
			N = (int*)mmap(
				NULL, sizeof(int), PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS, -1, 0);

			tempoFallo = (int*)mmap(
				NULL, TEAMSIZE * sizeof(int), PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS, -1, 0);

			tempoInfortunio = (int*)mmap(
				NULL, TEAMSIZE * sizeof(int), PROT_READ | PROT_WRITE,
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
			pthread_mutex_lock(&pallone); //i giocatori aspettano l'inizio della partita

			

			//array di richieste di accesso per i giocatori
			int players[TEAMSIZE];

			printf("pid %d: startup complete, accepting player numbers...\n", getpid());

			char idBuf[10][2];
			for (int k = 0; k < 10; k++) {
				printf("pid %d: processing player %d",getpid(),k);
				if(recv(refereeSocket, buffer, BUFDIM, 0)<0) perror("recieve error"), exit(1);
				printf(" being %s\n", buffer);
				idBuf[k][0] = buffer[0];
				idBuf[k][1] = buffer[1];
				strcpy(buffer, "ack\0");
				send(refereeSocket, buffer, BUFDIM, 0);
			}

			for (int k = 0; k < 10; k++) printf("pid %d: received matrix row %d: %c%c\n", getpid(), k, idBuf[k][0], idBuf[k][1]);

			//indici per inserire i giocatori nelle squadre
			int i = 0;
			int j = 0;
			short ref = 0;

			//URGE GRANDI CAMBIAMENTI
			for (int k = 0; k < 10; k++) {
				printf("pid %d: index i=%d j=%d ref=%d\n", getpid(), i, j, ref);


				/*
					Qui bisogna interpretare il formato del messaggio.
					esempio messaggio 'A3' indicano la squadra (che puo' essere A o B)
					e id giocatore (da 0 a 9)
				*/
				
				//inserisco giocatore nella squadra A
				if (idBuf[k][0] == 'A' && i < 5) {
					players[i + j] = idBuf[k][1] - '0';

					//metto la squadra corretta all'interno dell'array globale
					pthread_mutex_lock(globalVar);
					squadre[players[i + j]] = 'A';
					pthread_mutex_unlock(globalVar);

					pthread_create(&squadraA[i], NULL, playerThread, (void*)&players[i + j]);
					i++;
				}
				else {
					if (idBuf[k][0] == 'B' && j < 5) {
						players[i + j] = idBuf[k][1] - '0';

						//metto la squadra corretta all'interno dell'array globale
						pthread_mutex_lock(globalVar);
						squadre[players[i + j]] = 'B';
						pthread_mutex_unlock(globalVar);

						pthread_create(&squadraB[j], NULL, playerThread, (void*)&players[i + j]);
						j++;
					}
				}
				
			}

			for (int k = 0; k < 10; k++) {
				printf("player: %c%d\n", squadre[k], k);
			}

			//creato il processo dell'arbitro, gestisce la comunicazione col client.
			if ((refPid = fork()) == 0) {
				close(pipe_fd[1]);
				close(event_pipe[0]);
				for (int k = 0; k < 10; k++) {
					printf("referee got player: %c%d\n", squadre[k], k);
				}

				refereeProcess(&refereeSocket);
			}
			printf("main: referee started\n");
			close(pipe_fd[0]);
			close(event_pipe[1]);

			//attende che il referee e i giocatori siano pronti
			sem_wait(&playerSemaphore);

			//determina il primo giocatore ad avere la palla
			activePlayer = rand() % TEAMSIZE;

			//comincia la partita
			pthread_mutex_unlock(&pallone);

			printf("la partita e' cominciata!\n");

			while (1) pause();

			//chiude e libera i semafori e pipe
			sem_destroy(&playerSemaphore);
			close(event_pipe[0]);
		}
		close(pipe_check[1]);
		printf("preparazione partita, attendere...\n");
		read(pipe_check[0], buffer, BUFDIM);
		close(pipe_check[0]);
		printf("ricevuto il segnale %s\n",buffer);
	}

	
	return 0;
}

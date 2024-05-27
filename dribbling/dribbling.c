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
#include <netdb.h>


#define PORT 8033
#define BUFDIM 1024
#define REFEREEPORT 8088
#define QUEUE 360

volatile char squadre[10];

pthread_mutex_t globalVar;
volatile char stato[10];

volatile short stop = -1;

/*
	Ogni giocatore crea una connessione quindi ogni giocatore e' in attesa
	di un servizio a cui provvede questo metodo. Solo il giocatore attivo puo'
	inviare un messaggio quindi qui non ci preoccupiamo di capire chi e' il giocatore
	attivo.
*/

int timeout();

void resolve_hostname(const char* hostname, char* ip, size_t ip_len) {
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
		pthread_exit(NULL);
	}

	ptr = &((struct sockaddr_in*)res->ai_addr)->sin_addr;

	if (inet_ntop(res->ai_family, ptr, ip, ip_len) == NULL) {
		perror("inet_ntop");
		freeaddrinfo(res);
		pthread_exit(NULL);
	}

	freeaddrinfo(res);
}


void* service(void* arg) {
	/*
		in arg possiamo mettere il socket fd
		in attesa con read, quando riceve info esegue codice e risponde con write
		rimane attivo finche' il giocatore non termina l'evento
	*/
	
	int s_fd = *(int*)arg;
	pthread_mutex_lock(&globalVar);
	printf("service: starting...\n");

	struct hostent* hent;
	char ip[40];
	resolve_hostname("gateway", ip, sizeof(ip));

	char buffer[BUFDIM];
	int player, opponent, chance, c_fd;
	struct sockaddr_in c_addr;

	printf("service: waiting for player\n");
	//bisogna capire se avviene un dribbling o un giocatore diventa attivo
	recv(s_fd, buffer, BUFDIM, 0);
	if (strcmp(buffer, "partita terminata\0") == 0) {
		stop = 0;
		pthread_exit(NULL);
	}
	int con = buffer[0] - '0';
	char buf[BUFDIM];
	if ((con < 0 || con > 9) && buffer[0] != 'a') {
		printf("service: wrong buffer %s\n", buffer);
		
		snprintf(buf, BUFDIM, "err\0");
		send(s_fd, buf, BUFDIM, 0);
		sleep(1);
		close(s_fd);
		pthread_exit(NULL);
	}

	printf("service: received message: %s\n",buffer);

	for (int i = 0; i < 10; i++) {
		printf("%c%d=%c, ", squadre[i], i, stato[i]);
	}

	if (buffer[0] != 'a') {

		printf("service: player action\n");
		player = buffer[0] - '0';
		
		/*
			probabilita' fallimento = 35%
			probabilita' successo = 60%
			probabilita' infortunio = 5%
		*/
		int t = timeout();
		if (t == 1) {
			for (int i = 0; i < 10; i++) {
				stato[i] = 'a';
			}
			printf("service: connecting to referee %s:%d...\n", ip, REFEREEPORT);

			c_fd = socket(AF_INET, SOCK_STREAM, 0);
			c_addr.sin_family = AF_INET;
			c_addr.sin_port = htons(REFEREEPORT);
			inet_aton(ip, &c_addr.sin_addr);
			if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
				perror("connect failed to referee\n");
			}
			snprintf(buffer, BUFDIM, "h\n");
			if (send(c_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");
			printf("service: send message to referee = %s\n",buffer);
			close(c_fd);
		}
		do {
			opponent = rand() % 10;
		} while (squadre[opponent] == squadre[player] || stato[opponent] != 'a');


		if (opponent < 0 || opponent > 10) {
			printf("service: wrong opponent id\n");
			pthread_exit(NULL);
		}
		printf("service: opponent = %c%d\n", squadre[opponent], opponent);
		printf("service: player = %c%d\n", squadre[player], player);
		chance = rand() % 100;
		if (chance >= 0 && chance < 35) {
			snprintf(buffer, BUFDIM, "f%d\0", opponent);
			if(send(s_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");
			close(s_fd);

			snprintf(buffer, BUFDIM, "d%d%df\0", player, opponent);
			printf("service: connecting to referee %s:%d...\n", ip, REFEREEPORT);

			c_fd = socket(AF_INET, SOCK_STREAM, 0);
			c_addr.sin_family = AF_INET;
			c_addr.sin_port = htons(REFEREEPORT);
			inet_aton(ip, &c_addr.sin_addr);
			if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
				perror("connect failed to referee\n");
			}
			if (send(c_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");
			printf("service: sent message to referee: %s\n", buffer);
			close(c_fd);
		}
		if (chance >= 35 && chance < 95) {
			snprintf(buffer, BUFDIM, "s%d\0", opponent);
			send(s_fd, buffer, BUFDIM, 0);
			close(s_fd);

			snprintf(buffer, BUFDIM, "d%d%dy\0", player, opponent);
			printf("service: connecting to referee %s:%d...\n", ip, REFEREEPORT);

			c_fd = socket(AF_INET, SOCK_STREAM, 0);
			c_addr.sin_family = AF_INET;
			c_addr.sin_port = htons(REFEREEPORT);
			inet_aton(ip, &c_addr.sin_addr);
			if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
				perror("connect failed to referee\n");
			}
			if (send(c_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");
			printf("service: sent message to referee: %s\n", buffer);
			close(c_fd);
		}
		if (chance >= 95 && chance < 100) {
			snprintf(buffer, BUFDIM, "i%d\0", opponent);
			if (send(s_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");
			close(s_fd);
			stato[player] = 'i';
			stato[opponent] = 'f';
		}
		
	}
	else {
		printf("service: player status update\n");
		player = buffer[1] - '0';
		stato[player] = 'a';
		snprintf(buf, BUFDIM, "ack\0");
		send(s_fd, buf, BUFDIM, 0);
		close(s_fd);
	}

	pthread_mutex_unlock(&globalVar);
}

int timeout() { //ritorna 1 se si deve fare il timeout 
	int squadraA[5], squadraB[5];
	int a = 0, b = 0;
	for (int i = 0; i < 10; i++) {
		if (squadre[i] == 'A') squadraA[a++] = i;
		else squadraB[b++] = i;
	}
	int timeoutA = 0, timeoutB = 0; //se uguale a -1 significa che c'e' almeno un giocatore disponibile
	for (int i = 0; i < 5; i++) {
		if (stato[squadraA[i]] == 'a') timeoutA = -1;
		if (stato[squadraB[i]] == 'a') timeoutB = -1;
	}
	return ((timeoutA + timeoutB) > -2) ? 1 : 0;
}

int main(int argc, char* argv[]) {

	time_t t;
	srand((unsigned)time(&t));

	pthread_mutex_init(&globalVar, NULL);

	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	char ip[40];
	resolve_hostname(hostname, ip, sizeof(ip));

	int serverSocket, client, len;
	struct sockaddr_in serverAddr, clientAddr;
	char buffer[BUFDIM];
	len = sizeof(client);
	pthread_t player;

	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	int opt = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("Errore nel settaggio delle opzioni del socket");
		exit(EXIT_FAILURE);
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_aton(ip, &(serverAddr.sin_addr));
	memset(&(serverAddr.sin_zero), '\0', 8);

	bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	listen(serverSocket, QUEUE);

	char buf[BUFDIM];
	inet_ntop(AF_INET, &serverAddr.sin_addr, buf, sizeof(buf));
	printf("main: Accepting as %s:%d...\n",buf,PORT);

	int i = 0;
	int j = 0;
	int id;
	while (i < 5 || j < 5) {
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		if(recv(client, buffer, BUFDIM, 0) < 0) perror("main: si è verificato un errore al recv\n");
		snprintf(buf, BUFDIM, "ack\0");
		if (send(client, buf, BUFDIM, 0) < 0) perror("main: send ack fallito\n");
		printf("main: buffer ricevuto = %s, ", buffer);
		printf("main: creazione squadre: %c%c\n", buffer[0],buffer[1]);
		id = buffer[1] - '0';
		if (buffer[0] == 'A') {
			squadre[id] = 'A';
			i++;
		}
		if (buffer[0] == 'B') {
			squadre[id] = 'B';
			j++;
		}
		stato[id] = 'a';
	}

	while (stop == -1) {
		printf("main: waiting for player...\n");
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		printf("main: connection accepted!\n");
		pthread_create(&player, NULL, service, (void*)&client);
		printf("main: thread creato!\n");
	}

	
	char refip[40];
	resolve_hostname("gateway", refip, sizeof(refip));

	struct sockaddr_in c_addr;

	int c_fd;

	c_fd = socket(AF_INET, SOCK_STREAM, 0);

	c_addr.sin_family = AF_INET;

	c_addr.sin_port = htons(REFEREEPORT);

	inet_aton(refip, &c_addr.sin_addr);

	if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
		printf("connect() failed to %s:%d\n", refip, REFEREEPORT);
	}

	snprintf(buffer, BUFDIM, "e\0");
	if (send(c_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");
	printf("main: closing...\n");
	sleep(1);
	close(c_fd);
	close(client);

	return 0;
}

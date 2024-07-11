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
#include <signal.h>
#include <errno.h>

#define PORT 8033
#define BUFDIM 1024
#define REFEREEPORT 8088
#define QUEUE 2048

pthread_mutex_t globalVar;

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

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	
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

	char buf[BUFDIM];
	recv(s_fd, buffer, BUFDIM, 0);
	if (buffer[0] == 't') {

		c_fd = socket(AF_INET, SOCK_STREAM, 0);
		c_addr.sin_family = AF_INET;
		c_addr.sin_port = htons(REFEREEPORT);
		inet_aton(ip, &c_addr.sin_addr);
		if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
			perror("connection to referee failed, retrying...");
		}
		snprintf(buffer, BUFDIM, "end\0");
		send(c_fd, buffer, BUFDIM, 0);

		snprintf(buf, BUFDIM, "ack\0");
		send(s_fd, buf, BUFDIM, 0);

		exit(1);
	}

	int con = buffer[0] - '0';
	if (con < 0 || con > 9) {
		printf("service: wrong buffer %s\n", buffer);
		
		snprintf(buf, BUFDIM, "err\0");
		if (send(s_fd, buf, BUFDIM, 0) < 0) perror("service: send error\n");
		pthread_mutex_unlock(&globalVar);
		
		pthread_exit(NULL);
	}

	printf("service: received message: %s\n",buffer);

	printf("service: player action\n");
	player = buffer[0] - '0';
		
	/*
		probabilita' fallimento = 35%
		probabilita' successo = 60%
		probabilita' infortunio = 5%
	*/
		
	do {
		opponent = rand() % 10;
	} while (opponent == player);

	printf("service: opponent = %d\n", opponent);
	printf("service: player = %d\n", player);
	chance = rand() % 100;
	if (chance >= 0 && chance < 35) {
		snprintf(buffer, BUFDIM, "f%d\0", opponent);
		if(send(s_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");

		snprintf(buffer, BUFDIM, "d%d%df\0", player, opponent);
		printf("service: connecting to referee %s:%d...\n", ip, REFEREEPORT);

		c_fd = socket(AF_INET, SOCK_STREAM, 0);
		c_addr.sin_family = AF_INET;
		c_addr.sin_port = htons(REFEREEPORT);
		inet_aton(ip, &c_addr.sin_addr);
		if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
			perror("connect failed to referee\n");
		}
		if (send(c_fd, buffer, BUFDIM, 0) < 0) {
			switch (errno) {
			case EPIPE:
				int result = -1;
				for (int i = 0; i < 5; i++) {
					if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
						perror("connection to referee failed, retrying...");
					}
					else {
						result = send(c_fd, buffer, BUFDIM, 0);
					}
					if (result != -1) i = 5;
				}
				if (result == -1) perror("too many retries");
				break;
			}
		}
		printf("service: sent message to referee: %s\n", buffer);
	}
	if (chance >= 35 && chance < 85) {
		snprintf(buffer, BUFDIM, "s%d\0", opponent);
		send(s_fd, buffer, BUFDIM, 0);

		snprintf(buffer, BUFDIM, "d%d%dy\0", player, opponent);
		printf("service: connecting to referee %s:%d...\n", ip, REFEREEPORT);

		c_fd = socket(AF_INET, SOCK_STREAM, 0);
		c_addr.sin_family = AF_INET;
		c_addr.sin_port = htons(REFEREEPORT);
		inet_aton(ip, &c_addr.sin_addr);
		if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
			perror("connect failed to referee\n");
		}
		if (send(c_fd, buffer, BUFDIM, 0) < 0) {
			switch (errno) {
			case EPIPE:
				int result = -1;
				for (int i = 0; i < 5; i++) {
					if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
						perror("connection to referee failed, retrying...");
					}
					else {
						result = send(c_fd, buffer, BUFDIM, 0);
					}
					if (result != -1) i = 5;
				}
				if (result == -1) perror("too many retries");
				break;
			}
		}
		printf("service: sent message to referee: %s\n", buffer);
	}
	if (chance >= 85 && chance < 100) {
		snprintf(buffer, BUFDIM, "i%d\0", opponent);
		if (send(s_fd, buffer, BUFDIM, 0) < 0) perror("service: write error\n");
	}
	pthread_mutex_unlock(&globalVar);
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

	while (1) {
		printf("main: waiting for player...\n");
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		printf("main: connection accepted!\n");
		pthread_create(&player, NULL, service, (void*)&client);
		printf("main: thread creato!\n");
	}

	close(client);

	return 0;
}

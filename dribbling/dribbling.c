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

char squadre[10];
char stato[10];

/*
	Ogni giocatore crea una connessione quindi ogni giocatore e' in attesa
	di un servizio a cui provvede questo metodo. Solo il giocatore attivo puo'
	inviare un messaggio quindi qui non ci preoccupiamo di capire chi e' il giocatore
	attivo.
*/
void* service(void* arg) {
	/*
		in arg possiamo mettere il socket fd
		in attesa con read, quando riceve info esegue codice e risponde con write
		rimane attivo finche' il giocatore non termina l'evento
	*/
	printf("service: starting...\n");

	struct hostent* hent;
	hent = gethostbyname("gateway");
	char ip[40];
	inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15);

	char buffer[BUFDIM];
	int s_fd, player, opponent, chance, c_fd;
	struct sockaddr_in c_addr;
	s_fd = *(int*)arg;

	c_fd = socket(AF_INET, SOCK_STREAM, 0);

	c_addr.sin_family = AF_INET;

	c_addr.sin_port = htons(REFEREEPORT);

	inet_aton(ip, &c_addr.sin_addr);

	printf("service: connecting to referee %s:%d...\n",ip,REFEREEPORT);
	if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
		printf("connect() failed to %s:%d\n",ip,REFEREEPORT);
	}
	
	printf("service: waiting for player\n");
	//bisogna capire se avviene un dribbling o un giocatore diventa attivo
	read(s_fd, buffer, BUFDIM); 

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
		
		do {
			opponent = rand() % 10;
		} while (squadre[opponent] == squadre[player] || stato[opponent] != 'a');
		printf("service: opponent = %c%d\n", squadre[opponent], opponent);
		printf("service: player = %c%d\n", squadre[player], player);
		chance = rand() % 100;
		if (chance >= 0 && chance < 35) {
			sprintf(buffer, "d%d%df\0", player,opponent);
			write(c_fd, buffer, BUFDIM);
			printf("service: sent message to referee: %s\n", buffer);
			sprintf(buffer, "f%d\0", opponent);
			
		}
		if (chance >= 35 && chance < 95) {
			sprintf(buffer, "d%d%dy\0",player,opponent);
			write(c_fd, buffer, BUFDIM);
			printf("service: sent message to referee: %s\n", buffer);
			sprintf(buffer, "s%d\0", opponent);
			
		}
		if (chance >= 95 && chance < 100) {
			sprintf(buffer, "i%d\0", opponent);
			stato[player] = 'i';
			stato[opponent] = 'f';
		}
		write(s_fd, buffer, BUFDIM);
		printf("service: sent message to player: %s\n", buffer);
	}
	else {
		printf("service: player status update\n");
		player = buffer[1];
		stato[player] = 'a';
	}
	close(c_fd);

}



int main(int argc, char* argv[]) {

	time_t t;
	srand((unsigned)time(&t));

	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	struct hostent* hent;
	hent = gethostbyname(hostname);
	char ip[40];
	inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15);

	int serverSocket, client, len;
	struct sockaddr_in serverAddr, clientAddr;
	char buffer[BUFDIM];
	len = sizeof(client);
	pthread_t player;

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_aton(ip, &(serverAddr.sin_addr));
	memset(&(serverAddr.sin_zero), '\0', 8);

	bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	listen(serverSocket, 12);

	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &serverAddr.sin_addr, buf, sizeof(buf));
	printf("main: Accepting as %s:%d...\n",buf,PORT);
	
	int i = 0;
	int j = 0;
	int id;
	while (i < 5 || j < 5) {
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		read(client, buffer, BUFDIM);
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

	while (1) {
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		printf("main: connection accepted!\n");
		pthread_create(&player, NULL, service, (void*)&client);
		printf("main: thread creato!\n");
		pthread_join(player, NULL);
		printf("main: richiesta del player terminata\n");
	}

	close(client);
	
	return 0;
}
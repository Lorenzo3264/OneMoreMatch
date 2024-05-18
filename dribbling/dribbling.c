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
	char buffer[BUFDIM];
	int s_fd, player, opponent, chance, c_fd;
	struct sockaddr_in c_addr;
	s_fd = *(int*)arg;

	c_fd = socket(AF_INET, SOCK_STREAM, 0);

	c_addr.sin_family = AF_INET;

	c_addr.sin_port = htons(REFEREEPORT);

	inet_aton("127.0.0.1", &c_addr.sin_addr);

	if (connect(c_fd, (struct sockaddr*)&c_addr, sizeof(c_addr))) {
		perror("non sono riuscito a connettermi con l'arbitro\n");
	}

	//bisogna capire se avviene un dribbling o un giocatore diventa attivo
	read(s_fd, buffer, BUFDIM); 

	if (buffer[0] != 'a') {
		player = buffer[0];

		time_t t;
		srand((unsigned)time(&t));
		do {
			opponent = rand() % 10;
		} while (squadre[opponent] == squadre[player] && stato[opponent] != 'a');
		chance = rand() % 100;
		if (chance >= 0 && chance < 30) {
			sprintf(buffer, "d%d%df\0", player,opponent);
			write(c_fd, buffer, BUFDIM);
			sprintf(buffer, "f%d", opponent);
			
		}
		if (chance >= 30 && chance < 85) {
			sprintf(buffer, "d%d%dy\0",player,opponent);
			write(c_fd, buffer, BUFDIM);
			sprintf(buffer, "s%d", opponent);
			
		}
		if (chance >= 85 && chance < 100) {
			sprintf(buffer, "i%d\0", opponent);
			stato[player] = 'i';
			stato[opponent] = 'f';
		}
		write(s_fd, buffer, BUFDIM);
		
	}
	else {
		player = buffer[1];
		stato[player] = 'a';
	}
	close(c_fd);

}



int main(int argc, char* argv[]) {
	int serverSocket, client, len;
	struct sockaddr_in serverAddr, clientAddr;
	char buffer[BUFDIM];
	len = sizeof(client);
	pthread_t player;

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_aton("0.0.0.0", &(serverAddr.sin_addr));
	memset(&(serverAddr.sin_zero), '\0', 8);

	bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	listen(serverSocket, 12);

	client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
	int i = 0;
	int j = 0;
	while (i < 5 || j < 5) {
		read(client, buffer, BUFDIM);
		if (buffer[0] == 'A') {
			squadre[buffer[1]] = 'A';
		}
		if (buffer[0] == 'B') {
			squadre[buffer[1]] = 'B';
		}
		stato[buffer[1]] = 'a';
	}

	while (1) {
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		pthread_create(&player, NULL, service, (void*)&client);
		pthread_join(player, NULL);
	}
	close(client);
	
	return 0;
}
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

#define PORT 8041
#define BUFDIM 1024
#define REFEREEPORT 8088 

void* service(void *arg){
	char buffer[BUFDIM];
    int s_fd, player, opponent, tempoI, tempoP, client_fd, id;
	struct sockaddr_in client_addr;
	s_fd = *(int*)arg;

	struct hostent* hent;
	hent = gethostbyname("gateway");
	char ip[40];
	inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15);

	read(s_fd, buffer, BUFDIM);
	player = buffer[0];
	opponent = buffer[1];

	time_t t;
	srand((unsigned)time(&t));

	tempoI = rand() % 15;
	while(tempoI <= 5){
		tempoI = rand() % 15;
	}
	tempoP = (tempoI/2);

	//formato messaggio infortunio: IXXXPXXX\0
	sprintf(buffer, "I%dP%d\0", tempoI, tempoP);
	write(s_fd, buffer, BUFDIM);

	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(REFEREEPORT);
	inet_aton(ip, &client_addr.sin_addr);
    if (connect(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr))) {
		printf("connect() failed to %s:%d\n", ip, REFEREEPORT);
	}
    
	sprintf(buffer, "i%d%d\0", player, opponent);
	write(client_fd, buffer, BUFDIM);

}
 
int main(int argc, char* argv[]) {
	int serverSocket, client, len, id;
	struct sockaddr_in serverAddr, clientAddr;
	char buffer[BUFDIM];
	len = sizeof(client);
	pthread_t player;
	char squadre[10];

	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	struct hostent* hent;
	hent = gethostbyname(hostname);
	char ip[40];
	inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15);

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_aton(ip, &(serverAddr.sin_addr));
	memset(&(serverAddr.sin_zero), '\0', 8);

    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	listen(serverSocket, 12);

	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &serverAddr.sin_addr, buf, sizeof(buf));

	printf("Accepting as %s:%d...\n", buf, PORT);
	client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
	int i = 0, j = 0;
	while (i < 5%j < 5){
		read(client, buffer, BUFDIM);
		if(buffer[0] == 'A'){
			squadre[buffer[1]] = 'A';
		}
		if(buffer[0] == 'B'){
			squadre[buffer[1]] = 'B';
		}
	}
    
	while(1){
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		pthread_create(&player, NULL, service, (void*)&client);
        pthread_join(player, NULL);
	}

	return 0;
}
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

#define PORT 8077
#define BUFDIM 1024
#define REFEREEPORT 8088

volatile short stop = -1;

void* service(void *arg){
	char buffer[BUFDIM];
    int s_fd, player, chance, client_fd;
	struct sockaddr_in client_addr;
	s_fd = *(int*)arg;

	struct hostent* hent;
	hent = gethostbyname("gateway");
	if (hent == NULL) {
		perror("gethostbyname");
		pthread_exit(NULL); // Exit the thread if gethostbyname fails
	}
	char ip[40];
	inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15);

	read(s_fd, buffer, BUFDIM);
	if (strcmp(buffer, "partita terminata\0") == 0) {
		stop = 0;
		pthread_exit(NULL);
	}
	printf("service: from player buffer = %s\n",buffer);
	player = buffer[0] - '0';


	chance = rand() % 100;
	if(chance < 50){
		//fallito
		//  t%d(r)\0
        snprintf(buffer, BUFDIM, "t%df\0", player);
	}else{
		//goal
		//  t%d(r)\0
        snprintf(buffer, BUFDIM, "t%dy\0", player);
	}

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(REFEREEPORT);
	inet_aton(ip, &client_addr.sin_addr);
    if (connect(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr))) {
		printf("connect() failed to %s:%d\n", ip, REFEREEPORT);
	}

	write(client_fd, buffer, BUFDIM);
	printf("service: to referee buffer = %s\n", buffer);

	close(client_fd);

}

int main(int argc, char* argv[]) {
	time_t t;
	srand((unsigned)time(&t));

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
	if (hent == NULL) {
		perror("gethostbyname");
		exit(1); // Exit the thread if gethostbyname fails
	}
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

	int i = 0, j = 0;

	while (i < 5 || j < 5){
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		read(client, buffer, BUFDIM);
		printf("main: creazione squadre: %c%c\n", buffer[0], buffer[1]);
		id = buffer[1] - '0';
		if(buffer[0] == 'A'){
			squadre[id] = 'A';
			i++;
		}
		if(buffer[0] == 'B'){
			squadre[id] = 'B';
			j++;
		}
	}

	while(stop == -1){
		printf("main: accepting player...\n");
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		printf("main: player accepted!\n");
		pthread_create(&player, NULL, service, (void*)&client);
		printf("main: player stopped\n");
        pthread_join(player, NULL);
	}

	return 0;
}

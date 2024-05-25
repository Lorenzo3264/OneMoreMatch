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
#define QUEUE 360

volatile char stop = -1;

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

void* service(void *arg){
	char buffer[BUFDIM];
    int s_fd, player, opponent, tempoI, tempoP, client_fd, id;
	struct sockaddr_in client_addr;
	s_fd = *(int*)arg;

	char ip[40];
	resolve_hostname("gateway", ip, sizeof(ip));

	read(s_fd, buffer, BUFDIM);
	if (strcmp(buffer, "partita terminata\0") == 0) {
		stop = 0;
		pthread_exit(NULL);
	}
	printf("service: from player buffer = %s\n", buffer);
	player = buffer[0] - '0';
	opponent = buffer[1] - '0';



	tempoI = rand() % 15;
	while(tempoI <= 5){
		tempoI = rand() % 15;
	}
	tempoP = (tempoI/2);

	//formato messaggio infortunio: IXXXPXXX\0
	snprintf(buffer, BUFDIM, "I%dP%d\0", tempoI, tempoP);
	write(s_fd, buffer, BUFDIM);
	printf("service: to player buffer = %s\n", buffer);

	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(REFEREEPORT);
	inet_aton(ip, &client_addr.sin_addr);
    if (connect(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr))) {
		printf("connect() failed to %s:%d\n", ip, REFEREEPORT);
	}

	snprintf(buffer, BUFDIM, "i%d%d\0", player, opponent);
	write(client_fd, buffer, BUFDIM);
	printf("service: to referee buffer = %s\n");

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
	char ip[40];
	resolve_hostname(hostname, ip, sizeof(ip));

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_aton(ip, &(serverAddr.sin_addr));
	memset(&(serverAddr.sin_zero), '\0', 8);

    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	listen(serverSocket, QUEUE);

	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &serverAddr.sin_addr, buf, sizeof(buf));

	printf("Accepting as %s:%d...\n", buf, PORT);

	int i = 0, j = 0;

	while (i < 5 || j < 5){
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		read(client, buffer, BUFDIM);
		printf("main: creazione squadre: %c%c\n", buffer[0], buffer[1]);
		id = buffer[0] - '0';
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
		printf("main: waiting for player...\n");
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		printf("main: player found!\n");
		pthread_create(&player, NULL, service, (void*)&client);
	}

	close(client);

	return 0;
}

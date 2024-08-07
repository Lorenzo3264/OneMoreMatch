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
#define QUEUE 2048

volatile short stop = -1;
pthread_mutex_t synchro;

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
	int s_fd = *(int*)arg;

	time_t t;
	srand(((unsigned)time(&t))*getpid());

	pthread_mutex_lock(&synchro);
	char buffer[BUFDIM];
    int player, chance, client_fd;
	struct sockaddr_in client_addr;
	char ip[40];
	char buf[BUFDIM];

	resolve_hostname("gateway", ip, sizeof(ip));
	recv(s_fd, buffer, BUFDIM, 0);
	if (buffer[0] == 't') {
		stop = 0;
		snprintf(buf, BUFDIM, "ack\0");
		send(s_fd, buf, BUFDIM, 0);
		exit(1);
	}
	
	printf("service: from player buffer = %s\n",buffer);
	player = buffer[0] - '0';

	if (player < 0 || player > 9) {
		printf("service: wrong buffer %s\n", buffer);

		snprintf(buf, BUFDIM, "err\0");
		send(s_fd, buf, BUFDIM, 0);
		pthread_mutex_unlock(&synchro);
		pthread_exit(NULL);
	}

	chance = rand() % 100;
	if(chance < 65){
		//fallito
        snprintf(buffer, BUFDIM, "t%df\0", player);
	}else{
		//goal
        snprintf(buffer, BUFDIM, "t%dy\0", player);
	}
	send(s_fd, buffer, BUFDIM, 0);
	printf("service: to player buffer = %s\n", buffer);
	pthread_mutex_unlock(&synchro);
}

int main(int argc, char* argv[]) {
	time_t t;

	srand((unsigned)time(&t));
	pthread_mutex_init(&synchro,NULL);

    int serverSocket, client, len, id;
	struct sockaddr_in serverAddr, clientAddr;
	char buffer[BUFDIM];
	len = sizeof(client);
	pthread_t player;

	char hostname[1023] = { '\0' };
	gethostname(hostname, 1023);
	char ip[40];
	resolve_hostname(hostname, ip, sizeof(ip));
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

	printf("Accepting as %s:%d...\n", buf, PORT);

	
	while(stop == -1){
		printf("main: accepting player...\n");
		client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
		printf("main: player accepted!\n");
		if (fork() == 0) service((void*)&client), exit(0);
		//pthread_create(&player, NULL, service, (void*)&client);
	}
	printf("main: closing...\n");
	sleep(1);

	return 0;
}

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

void* service(void *arg){
	char buffer[BUFDIM];
    int s_fd, player, chance, client_fd, id;
	struct sockaddr_in client_addr;
	s_fd = *(int*)arg;

	read(s_fd, buffer, BUFDIM);
	player = buffer[0];

	time_t t;
	srand((unsigned)time(&t));
	chance = rand() % 100;
	if(chance < 50){
		//fallito
		//  t%d(r)\0
        sprintf(buffer, "t%df\0", id);
	}else{
		//goal
		//  t%d(r)\0
        sprintf(buffer, "t%dy\0", id);
	}

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(REFEREEPORT);
	inet_aton("127.0.0.1", &client_addr.sin_addr);
    if (connect(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr))) {
		perror("connect() failed\n");
	}
    
	write(client_fd, buffer, BUFDIM);
	
}

int main(int argc, char* argv[]) {
    int serverSocket, client, len, id;
	struct sockaddr_in serverAddr, clientAddr;
	char buffer[BUFDIM];
	len = sizeof(client);
	pthread_t player;
	char squadre[10];

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	inet_aton("0.0.0.0", &(serverAddr.sin_addr));
	memset(&(serverAddr.sin_zero), '\0', 8);

    bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	listen(serverSocket, 12);

	printf("Accepting...\n");
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
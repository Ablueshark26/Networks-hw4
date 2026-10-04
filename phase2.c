#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PORT "43024"   // Hosting port
#define BACKLOG 10    // How many addresses we store

int main(int argc, char *argv[])
{
	if(argc == 3 && strcmp(argv[1], "host") == 0) { //checks to see three args and if 2nd is host
		char *port = argv[2]; //second has port num
		printf("this is host listening on %s\n", port);
	}
	else if (argc == 4 && strcmp (argv[1], "connect") == 0) { //checks to see if there's 4 arguments 
		char *host = argv[2];
		char *port = argv[3];
		printf("Connection will be made to host %s on port %s\n", host, port);
	}
	else{ //mistakes
		fprintf(stderr, "improper entry");
		return 1;
	}
	return 0;
}

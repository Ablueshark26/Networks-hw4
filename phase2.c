#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

//use port 43024
//#define ADDR "127.0.0.1"   // loop back to this machine
#define ADDR "127.0.0.1"
#define BACKLOG 3    // How many addresses we store
int listening(char *port){
	struct sockaddr_in addr;
	int yes = 1;
	//same as connector
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); //IPv4 and TCP
	if(sockfd == -1) {                                                                  
		perror("socket");                                                           
		return -1;
	} 
	//used in phase 1 allows quick reuse
	setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes, sizeof yes);
	
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY); //confusing, but replaces inet_pton takes its own address
	
	if(bind(sockfd, (struct sockaddr *)&addr, sizeof addr) == -1) {
		perror("bind");
		close(sockfd);
		return -1;
	}

	if(listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		close(sockfd);
		return -1;
	}
	return sockfd;
}

	
int connecting(char *port)
{
	//different from first phase forgot we had preset addresses to use meaining we can use sockaddr_in
	struct sockaddr_in addr;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); //IPv4 and TCP
	if(sockfd == -1) {
		perror("socket");
		return -1;
	}
	
	//addr in has different fields need to manually convert port
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port)); //atoi gives number, hton gives bytes
	
	//form for converting address to sin_addr
	if(inet_pton(AF_INET, ADDR, &addr.sin_addr) != 1) {
		fprintf(stderr, "bad address: %s\n", ADDR);
		close(sockfd);
		return -1;
	}
	//uses the connect command to link to socket
	if(connect(sockfd, (struct sockaddr *)&addr, sizeof addr) == -1) {
		perror("cannot connnect");
		close(sockfd);
		return -1;
	}
	return sockfd;
}




int main(int argc, char *argv[])
{

//HOSTING 
	if(argc == 3 && strcmp(argv[1], "host") == 0) { // checks to see three args and if 3rd is host
		char *port = argv[2]; //second has port num
		int listener = listening(port);
		printf("Host listening on %s\n", port);
		
		//look at phase one
		struct sockaddr_in other_addr;
		socklen_t addr_size = sizeof other_addr;
		int new_fd = accept(listener, (struct sockaddr *)&other_addr, &addr_size);
		if(new_fd == -1)
		{
			perror("new fd invalid");
			return 2;
		}

		//
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &other_addr.sin_addr, ip, sizeof ip);
        	printf("Connection from %s (fd %d)\n", ip, new_fd);
		
		//similar char buf to how we've seen 
		char buf[100];
        	int numbytes = recv(new_fd, buf, sizeof buf - 1, 0);
        	if (numbytes > 0) {
            		buf[numbytes] = '\0';
            		printf("Received: %s", buf);
        	}

 		//reciever from phase 1
        	char *reply = "This is host from port: %s\n", port;
        	if (send(new_fd, reply, strlen(reply), 0) == -1) {
            		perror("send");
        	}
 		
		//close out the sockets made 
	        close(new_fd);
	        close(listener);
	
}


//CONNECTING 	
	else if (argc == 4 && strcmp (argv[1], "connect") == 0) { //checks to see if there's 4 args
		
		char *port = argv[3];
		printf("Connection will be made to host %s on port %s\n", ADDR, port);
		int sockfd = connecting(port);
		if (sockfd == -1) {
    			fprintf(stderr, "Could not connect.\n");
    			return 2;
		}
		printf("Connection established %d\n", sockfd);
		
		//Similar message sender from phase1
		char *msg = "Hey! this is on port %s\n", port; //tries to send message on connect
        	if (send(sockfd, msg, strlen(msg), 0) == -1) {
            		perror("send");
        	}
 		//gets message from host
                char buf[100];
                int numbytes = recv(sockfd, buf, sizeof buf - 1, 0);
                if (numbytes > 0) {
                        buf[numbytes] = '\0';
                        printf("Host: %s", buf);
                }
 
        	close(sockfd);
	}

        else{ //mistakes
                fprintf(stderr, "improper call");
                return 1;
        }
        return 0;
}


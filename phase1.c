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

int main(void)
{
    struct addrinfo hints, *servinfo;
    int sockfd, new_fd;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags    = AI_PASSIVE;   // This address

    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        fprintf(stderr, "getaddrinfo failed\n");  
        return 1;  //fills servinfo using the established hints
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
                    servinfo->ai_protocol); //makes a socket
    if (sockfd == -1) {
        perror("socket");
        return 1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes); //hold address allowing restart

    //Attach to port using bind
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("bind");
        return 1;
    }
    freeaddrinfo(servinfo);           // done with this

    //starts listening
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        return 1;
    }
    printf("Listening on port %s ... waiting for a connection.\n", PORT);

    //waits for calls
    struct sockaddr_in their_addr;
    socklen_t addr_size = sizeof their_addr; //will store the size socklen_t is an int

    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1){ //waits at the accept phase
        perror("accept");
        return 1;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &their_addr.sin_addr, ip, sizeof ip); //from book. converts ip to readable
    printf("Someone connected from %s! (their line is fd %d)\n", ip, new_fd);


    char buf[100]; //can take 100 char
    int numbytes = recv(new_fd, buf, sizeof buf -1,0);

    if(numbytes == -1){
	    perror("recv");
    }
    else if(numbytes == 0){
	    printf("NOTHING SENT\n");
    }
    else{
	    buf[numbytes] = '\0';
	    printf("Recieved: %s\n",buf);
    }
    
    char *message = "RECIEVED\n";
    if(send(new_fd, message, strlen(message), 0) == -1){
	    perror("cannot send\n");
    }
    else{
	    printf("message sent\n");
    }

    close(new_fd);
    close(sockfd);
    return 0;
}


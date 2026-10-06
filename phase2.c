#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h> //used for tracking time and file size
#include <time.h> //return type for time
#include <stdint.h> //usage for special integer 32 bit
#include <pthread.h> //thread

//use port 43000-43100
#define ADDR "127.0.0.1"
#define BACKLOG 3    // How many pending addresses we store
#define SEND_FILE "blockchain.txt"
#define MAX_SIZE 10000000 //10mb max

//start threading
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;
time_t saved_mtime; //last save
int running = 1;  //check when threads are running



//needed for making file
int init_file(const char *path)
{
	FILE *fp = fopen(path, "a"); //"a" will create or open
	if(fp == NULL) {
		perror("init_file");
		return -1;
	}
	fclose(fp);
	return 0;
}
//will put a line of data on the file
int add_block(const char *path, const char *data)
{
	FILE *fp = fopen(path, "a"); //assuming it exists will write to end
	if(fp == NULL) {
		perror("fail in add_block");
		return -1;
	}
	fprintf(fp, "%s\n", data);
	fclose(fp);
	return 0;
}
 
//will get the size using stat object
long file_size(const char *path)
{
	struct stat st;
	if(stat(path, &st) == -1) {
		return -1;
	}
	return (long)st.st_size;
}
 
//gets time modiefied
time_t file_mtime(const char *path)
{
	struct stat st;
	if(stat(path, &st) == -1) {
		return -1;
	}
	return st.st_mtime;
}
 
//reads file puts in buffer to return.  does not free
char *read_file(const char *path, long *len)
{
	long size = file_size(path);
	if(size < 0) {
		return NULL;
	}
 
	FILE *fp = fopen(path, "rb"); //read only in binary
	if(fp == NULL) {
		perror("read_file");
		return NULL;
	}
 
	char *buf = malloc(size + 1); //add one for end character
	if(buf == NULL) {
		fclose(fp);
		return NULL;
	}
 
	size_t need = fread(buf, 1, size, fp); //reading and putting in buf
	fclose(fp);
	buf[need] = '\0'; //end character to buf
	*len = (long)need; //writes to len 
	return buf;
}
//makes temp write file and replaces original when done. saves us from crashes
int write_file(const char *path, const char *buf, long len)
{
        char tmp[256];
        snprintf(tmp, sizeof tmp, "%s.tmp", path); //makes temp blockchain

        FILE *fp = fopen(tmp, "wb"); //w erases whatever was there NOT a
        if(fp == NULL) {
                perror("write_file");
                return -1;
        }

        if(fwrite(buf, 1, len, fp) != (size_t)len) { //will write len items and returns amount written
                perror("fwrite");
                fclose(fp);
                remove(tmp);
                return -1;
        }
        fclose(fp);

        if(rename(tmp, path) == -1) { //swap as normal blockchain everything works!
                perror("rename");
                return -1;
        }
        return 0;
}
//will send until it matches the len of file
int sendall(int fd, const char *buf, long len)
{
	long total = 0; //keep track of bytes sent
	while(total < len) {
		ssize_t n = send(fd, buf + total, len - total, 0);  //counts size sent
		if(n == -1) {
			perror("sendall");
			return -1;
		}
		total += n;
	}
	return 0;
}

//recieve looks the same jsut checks zero 
int recvall(int fd, char *buf, long len)
{
	long total = 0;
	while(total < len) {
		ssize_t n = recv(fd, buf + total, len - total, 0);
		if(n == -1) {
			perror("recvall");
			return -1;
		}
		if(n == 0) { //0 means they closed the connection
			fprintf(stderr, "recvall: connection closed early\n");
			return -1;
		}
		total += n;
	}
	return 0;
}

//send data  
int send_data(int fd, const char *data, long len)
{
	uint32_t netlen = htonl((uint32_t)len); //size in network byte order, like the port
	if(sendall(fd, (char *)&netlen, sizeof netlen) == -1) {
		return -1;
	}
	return sendall(fd, data, len);
}

//sends our blockchain.txt as: 4byte
int send_file(int fd)
{
	long len;
	pthread_mutex_lock(&file_lock); //file lock
	char *data = read_file(SEND_FILE, &len);
	pthread_mutex_unlock(&file_lock);
	if(data == NULL) {
		return -1;
	}
 
	int rv = send_data(fd, data, len); //the slow network part happens OUTSIDE the lock
	free(data); //freeing from read_file
	return rv;
}
 

//receives a file sent adn  saves it as our blockchain.txt

int recv_file(int fd)
{
	uint32_t netlen;
	if(recvall(fd, (char *)&netlen, sizeof netlen) == -1) { //first the size
		return -1;
	}
	long len = ntohl(netlen);
	if(len > MAX_SIZE) { //test size
		fprintf(stderr, "recv_file: size %ld too big\n", len);
		return -1;
	}
 
	char *data = malloc(len + 1);
	if(data == NULL) {
		return -1;
	}
 
	if(recvall(fd, data, len) == -1) {
		free(data);
		return -1;
	}
 
	pthread_mutex_lock(&file_lock);
	int rv = write_file(SEND_FILE, data, len);
	saved_mtime = file_mtime(SEND_FILE); //write to main
	pthread_mutex_unlock(&file_lock); //releases after writing
	free(data); //free data from write
	return rv;
}
void *receiver(void *arg)
{
	int fd = *(int *)arg;
	while(1) {
		if(recv_file(fd) == -1) { //error, or the other peer hung up
			break;
		}
		printf("Received updated %s (%ld bytes)\n", SEND_FILE, file_size(SEND_FILE));
	}
	printf("Peer disconnected.\n");
 
	pthread_mutex_lock(&file_lock);
	running = 0; //tells the monitor to stop
	pthread_mutex_unlock(&file_lock);
	return NULL;
}
 
//thread: checks the file about once a second and sends it if it changed locally
void *monitor(void *arg)
{
	int fd = *(int *)arg;
	while(1) {
		sleep(1);
 
		char *data = NULL;
		long len = 0;
		pthread_mutex_lock(&file_lock);
		if(!running) {
			pthread_mutex_unlock(&file_lock);
			break;
		}
		time_t now = file_mtime(SEND_FILE);
		if(now != saved_mtime) { //somebody edited the file
			saved_mtime = now;
			data = read_file(SEND_FILE, &len);
		}
		pthread_mutex_unlock(&file_lock);
 
		if(data != NULL) { //send from other
			printf("Local change detected, sending %s (%ld bytes)\n", SEND_FILE, len);
			int rv = send_data(fd, data, len);
			free(data);
			if(rv == -1) {
				fprintf(stderr, "Could not send update.\n");
				break;
			}
		}
	}
 
	pthread_mutex_lock(&file_lock);
	running = 0;
	pthread_mutex_unlock(&file_lock);
	shutdown(fd, SHUT_RDWR); //wakes the receiver if it is stuck waiting in recv()
	return NULL;
}
 
//both modes end up here once the first file has been exchanged
void run_peer(int fd)
{
	pthread_mutex_lock(&file_lock);
	saved_mtime = file_mtime(SEND_FILE); //anything up to now is not a new change
	pthread_mutex_unlock(&file_lock);
 
	pthread_t recv_tid, mon_tid;
	if(pthread_create(&recv_tid, NULL, receiver, &fd) != 0) {
		fprintf(stderr, "could not start receiver thread\n");
		close(fd);
		return;
	}
	if(pthread_create(&mon_tid, NULL, monitor, &fd) != 0) {
		fprintf(stderr, "could not start monitor thread\n");
		shutdown(fd, SHUT_RDWR);
		pthread_join(recv_tid, NULL);
		close(fd);
		return;
	}
 
	printf("Peer is running. Edit %s to send an update, Ctrl-C to quit.\n", SEND_FILE);
	pthread_join(recv_tid, NULL); //wait here until the connection ends
	pthread_join(mon_tid, NULL);
	close(fd);
}


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
	if(init_file(SEND_FILE) == -1) {
		return 1;
	}

//HOSTING 
	if(argc == 3 && strcmp(argv[1], "host") == 0) { // checks to see three args and if 3rd is host
		char *port = argv[2]; //second has port num
		int listener = listening(port);
		if(listener == -1)
		{
			fprintf(stderr, "listen failed");
			return 2;
		}
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
		
		//new part host will send the file b4 connector speaks
		printf("Sending %s (%ld bytes)\n", SEND_FILE, file_size(SEND_FILE));
		if(send_file(new_fd) == -1) {
			fprintf(stderr, "Could not send file.\n");
			return 2;
		}
		printf("File sent \n");
 		
		//close out the sockets made
	        close(listener);
		run_peer(new_fd); //defined earlier will keep sync
	
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
		//tries to get file from host 
		if(recv_file(sockfd) == -1) {
			fprintf(stderr, "Could not receive file.\n");
			return 2;
		} 
        	printf("Received %s (%ld bytes)\n", SEND_FILE, file_size(SEND_FILE));
 
		run_peer(sockfd); //keeping socket open 
	}

        else{ //mistakes
                fprintf(stderr, "improper call\n");
                return 1;
        }
        return 0;
}


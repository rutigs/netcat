#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
/*
	This is the common functions that work for both versions ncP and ncTh
*/

/*
	Checks the command line arguments given and sets the appropriate flags for controlling the program
		-> Exits the program if the inputs are invalid
		-> Sets various arguments like hostname, portnum, maximum connections and other optional ones
*/
void check_args(int argc, char **argv)
{
	// check for at least hostname, portnum given
	if(argc < 3) {
		usage(argv[0]); 
		exit(1);
	}
	
	// We are assuming these 2 are the last 2 arguments
	hostname = argv[argc - 2];
	portnum = strtol(argv[argc - 1], NULL, 0);

	// set flags for command line arguments
	int i;
	for(i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "-k")) {
			staylistenflag = 1;
			continue;
		}

		if(!strcmp(argv[i], "-l")) {
			listenflag = 1;
			continue;
		}

		if(!strcmp(argv[i], "-v")) {
			verboseflag = 1;
			continue;
		} 
		
		if(!strcmp(argv[i], "-w")) {
			timeoutflag = 1;
			if(i+1 < argc - 2) { // argc - 2 because we assume the last 2 elts are domain port#
				timeout = strtol(argv[i+1], NULL, 0);
			} else {
				printf("You must provide a valid timeout with the -w flag.\n");	
				exit(1);
			}
			continue;
		} 
		
		if(!strcmp(argv[i], "-p")) {
			sourceportflag = 1;
			if(i+1 < argc)
				sourceport = strtol(argv[i+1], NULL, 0);
			else
				printf("You must provide a source port number with the -p flag.\n");
			continue;
		} 
		
		if(!strcmp(argv[i], "-r")) {
			multipleflag = 1;
		}
	}


	if((sourceportflag == 1) && (sourceportflag == listenflag)) {
		printf("You cannot use -l with -p\n");
		exit(1);
	}

	if(staylistenflag == 1 && staylistenflag != listenflag) {
		while(argv[i] != NULL) {
			printf("argv[%i]: %s\n", i, argv[i]);
			i++;
		}
		printf("-k must be used in conjunction with -l\n");
		exit(1);
	}
	
	if(multipleflag) {
		max_conns = 10;
	} else {
		max_conns = 1;
	}
}

/* 	CLIENT

	Creates a socket file descriptor based on the given information
	Set's various things like timeout and outgoing based on flags
*/
int create_client_socket()
{
	int sockfd;
	struct addrinfo hints, *servinfo, *res, *p;
	int status;
	char s[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	char port_str[16];
	char source_port_str[16];
	
	// Convert portnum integers to strings
	if(sourceportflag)
		sprintf(source_port_str, "%d", sourceport);	
	
	sprintf(port_str, "%d", portnum);
	
	// get the address info and store it in servinfo
	if ((status = getaddrinfo(hostname, port_str, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(1);
	}
	
	// iterate through the result(s) of servinfo to create a valid socket file descriptor
	for(res = servinfo; res != NULL; res = res->ai_next) {
		if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		
		// bind the outgoing port if the flag is set
		if (sourceportflag) {
			memset(&hints, 0, sizeof hints);
			hints.ai_family = AF_INET; // this could be UNSPEC but we are only worrying about IPv4
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

			getaddrinfo(NULL, source_port_str, &hints, &p);
			if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				perror("client: bind -p");
				continue;
			}
		}
		
		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		
		// set the timeout value if the flag is set
		if(timeoutflag) {
			struct timeval tv;
			tv.tv_sec = timeout;
			tv.tv_usec = 0;
			if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval))) {
				printf("setting socket timeout failed");				
				perror("setsockopt");
				exit(1);
			}
		}
		
		break;
	}
	
	if (res == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		exit(1);
	}
	
	inet_ntop(res->ai_family, get_in_addr((struct sockaddr *)res->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);
	
	// You should free the linkedlist of results after you've got a valid one
	freeaddrinfo(servinfo);

	return sockfd;
}
 
// Helper function for casting the address properly
void *get_in_addr(struct sockaddr *sa)
{
    return &(((struct sockaddr_in*)sa)->sin_addr);
}

/* 	SERVER

	Creates the socket that will listen for new connections
	Binds the socket to given port number
*/
int create_server_socket()
{
	int sockfd;
	int status;
	struct addrinfo hints, *servinfo, *res;
	int yes, optval = 1;
	
	memset(&hints, 0, sizeof(hints)); // make sure the struct is empty
	hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
	
	char port_str[16];
	sprintf(port_str, "%d", portnum);
	
	if ((status = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
		//return;	
	}
	
	for(res = servinfo; res != NULL; res = res->ai_next) {
		if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval)) {
			perror("setsockopt");
			exit(1);
		}
		
		if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		
		break;
	}
	freeaddrinfo(servinfo); // free the linked-list	

	if (res == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}
	
	return sockfd;
}

/*	SERVER
	
	Tells the given socket to listen for incoming connection requests
	Sets the backlog based on the flags
*/
void server_listen(int sockfd)
{	
	int backlog = 1;
	if(staylistenflag || multipleflag) {
		backlog = 20;
	}
	if (listen(sockfd, backlog) == -1) {
		perror("listen");
		return;
	}
}

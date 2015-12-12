#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>

/*
	Struct for passing arguments to a thread
	used for keeping track of threads in the array sock_threads
	and the associated threads file descriptor.
*/
typedef struct {
	int id;
	int fd;
} thread_args;

int staylistenflag, listenflag, sourceportflag, verboseflag, timeoutflag, multipleflag, sourceport, conn_count, thread_count = 0;
int timeout = -1;
char *hostname; // = argv[argc - 2];
int portnum, max_conns, serv_sock, client_sock;
int fd_array[50] = { -1 };
pthread_mutex_t lock;
pthread_t sock_threads[50] = { -1 };
thread_args ta[50];
int made_a_connection = 0;
pthread_t server_listen_thread;
pthread_t client_recv_thread;

// BOTH

// sends the data over the socket
void send_over_socket(int sockfd, char user_input[1024], int size) 
{
	int numbytes;
	numbytes = write(sockfd, user_input, size);
	if(numbytes < 0)
		perror("send");
}

/* 	SERVER

	cancels the thread with the corresponding id,
 	closes the corresponding file descriptor belonging to that thread
 	decrements the connection count
*/
void close_thread_and_sock(int inc_fd, int id)
{
	int i;

	pthread_mutex_lock(&lock);
	pthread_cancel(sock_threads[id]);
	for(i=0; i < thread_count; i++) {
		if(fd_array[i] == inc_fd) {
			fd_array[i] = -1;
		}
	}
	if(verboseflag)
		printf("thread %i, socket %i has been closed\n", inc_fd);
	close(inc_fd);
	conn_count--;
	pthread_mutex_unlock(&lock);
}

/*  SERVER

	Method given to the threads made from accept() calls
	Reads the data over that socket and passes it to be written to the other file descriptors
*/
void *recv_client_data(void * args) 
{
	// cast the arguments to get the fd and the id
	thread_args *ta = (thread_args *) args;
	int inc_fd = ta->fd;
	int id = ta->id;
	int bytes_recv;
	char buf[1024];
	int i;
	
	while(1) {
		bytes_recv = read(inc_fd, buf, 1024);
		if(bytes_recv == 0 || bytes_recv == -1) {
			if(verboseflag)
				perror("recv");
			break;
		}

		pthread_mutex_lock(&lock);
		write(1, buf, bytes_recv); // print data coming from clients to stdout	
		for(i=0; i < thread_count; i++) {
			if(fd_array[i] != inc_fd && fd_array[i] != -1)
				send_over_socket(fd_array[i], buf, bytes_recv);
		}
		pthread_mutex_unlock(&lock);
	}
	
	close_thread_and_sock(inc_fd, id);
}

/*  SERVER

	Method used by the listening thread to accept connections
	Each successful accept() creates a new thread to read() data from by passing the previous method
*/
void *accept_conns(void * args)
{
	socklen_t sin_size;
	int inc_fd, bytes_recv;
	struct sockaddr_storage their_addr;
	char s[INET_ADDRSTRLEN];
	int i;
	
	while(1) {  // main accept() loop
        if(verboseflag)
			printf("connection count: %i, max connections %i\n", conn_count, max_conns);
		if(staylistenflag && conn_count == max_conns) {
			continue;
		}

		if(!staylistenflag) {
			if(multipleflag) {
				if(made_a_connection == 1 && conn_count == 0) {
					break;
				} else if (conn_count == max_conns) {
					continue;
				}
			} else if (made_a_connection == 1) {
				break;
			}
		} else {
			if(conn_count == max_conns) {
				continue;
			}
		}

		sin_size = sizeof their_addr;

		if(verboseflag)
			printf("Waiting for a connection...\n");
        inc_fd = accept(serv_sock, (struct sockaddr *)&their_addr, &sin_size);
		if (inc_fd == -1) {
			// if(verboseflag)
	        perror("accept");
            continue;
        }

	    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);
		made_a_connection = 1;

		pthread_mutex_lock(&lock);
		fd_array[thread_count] = inc_fd;
		ta[thread_count].id = thread_count;
		ta[thread_count].fd = inc_fd;
		pthread_create(&sock_threads[thread_count], NULL, recv_client_data, (void *) &ta[thread_count]);
		thread_count++;
		conn_count++;
		pthread_mutex_unlock(&lock);
    }

	if(verboseflag)
		printf("no longer accepting new connections\n");
	pthread_cancel(server_listen_thread);
}

/*  SERVER

	Creates the socket that is going to listen
	Tells the server socket to listen
	Passes the server socket file descriptor to the listening thread that will accept() connections
	Reads from stdin and sends the information to file descriptors currently connected to the server
*/
void create_server()
{
	int num_bytes;
	
	serv_sock = create_server_socket();
	server_listen(serv_sock);
	
	pthread_mutex_init(&lock, NULL);
	
	pthread_create(&server_listen_thread, NULL, accept_conns, NULL);
	
	char user_input[1024];
	int i;
	if (verboseflag)
		printf("reading stdin\n");

	while(1) {
		if(staylistenflag == 0) {
			if(multipleflag) {
				if(made_a_connection == 1 && conn_count == 0) {
					break;
				} else if (conn_count == max_conns) {
					continue;
				}
			} else if (made_a_connection == 1) {
				break;
			}
		} else {
			if(conn_count == max_conns) {
				continue;
			}
		}

		num_bytes = read(0, user_input, 1024);

		if(num_bytes > 0) {	

			if(made_a_connection == 1 && conn_count == 0)
				break;

			pthread_mutex_lock(&lock);
			for(i = 0; i < thread_count; i++) {
				if(verboseflag)			
					printf("attempting to send over sock %i\n", fd_array[i]);
				
				if(fd_array[i] != -1) {
					send_over_socket(fd_array[i], user_input, num_bytes);
				}
			}
			pthread_mutex_unlock(&lock);
		}
	}
	pthread_mutex_destroy(&lock);
}
/*  CLIENT

	Method running on the client thread reading from the server it connected to
	Prints the read() calls to stdout
*/
void *recv_from_server()
{
	int num_bytes;
	char buf[1024];

	while(1) {
		if((num_bytes = read(client_sock, buf, 1024)) == -1) {
			if(verboseflag)		
				perror("read");
			exit(1);
		}
		if(num_bytes == 0) {
			//exit(0);
			break;
		}
		write(0, buf, num_bytes);
	}
	pthread_cancel(client_recv_thread);
}

/* 	CLIENT
	
	Creates the socket that will connect() to server
	Tells the socket to connect() to the server
	Passes the connected socket to a thread to do the read()ing
	Read() from stdin and send to server
*/
void setup_client()
{
	int num_bytes;
	char user_input[1024];
	
	client_sock = create_client_socket();
	
	pthread_create(&client_recv_thread, NULL, recv_from_server, NULL);
	
	if(timeoutflag) {
		struct timeval tv;
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		if (setsockopt(0, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval))) {
			printf("setting socket timeout failed");				
			perror("setsockopt");
			exit(1);
		}
	}

	while(1) {
		
		num_bytes = read(0, user_input, 1024);
		if(num_bytes == -1) {
			perror("read");
			exit(1);
		}
		if(num_bytes == 0) {
			continue;
		}
		if(!staylistenflag) {
			if(made_a_connection && conn_count == 0) {
				break;
			}
		}

		send_over_socket(client_sock, user_input, num_bytes);
	}
	pthread_exit(NULL);
}

int main(int argc, char **argv) 
{
	check_args(argc, argv);
	int i;

	if(listenflag) {
		create_server();
	} else {
		setup_client();
	}
	
	if(listenflag) {
		for(i=0; i < thread_count; i++) 
			pthread_join(sock_threads[i], NULL);
	}

	return 0;
}

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "commonProto.h"
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <errno.h>

int staylistenflag, listenflag, sourceportflag, verboseflag, timeoutflag, multipleflag, sourceport = 0;
int timeout = -1;
char *hostname; // = argv[argc - 2];
int portnum, max_conns, serv_sock, client_sock;
int curr_conns[10] = { -1 };
int conn_count = 0;
struct pollfd fds[12];
int made_a_connection = 0;

/* SERVER

	Puts stdin as the first file descriptor in fds
	Gets a server socket and tells it to listen
	Puts the server socket fd as the 2nd item in fds
	poll()s
*/
void create_server()
{
	int status, i;
	int size = 0;
	int nfds = 2; // 0 is stdin 1 is the listening socket
	int new_fd;
	socklen_t sin_size;
	struct sockaddr_storage their_addr;
	int close_conn = 0; 
	int run_server = 1; // 1 means keep it runnning 
	int compress_array = 0;
	int k, j;
	char s[INET_ADDRSTRLEN];
	char buf[1024];
	
	memset(fds, 0 , sizeof(fds));
	
	fds[0].fd = 0; // stdin
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	
	serv_sock = create_server_socket();
	server_listen(serv_sock);
	
	fds[1].fd = serv_sock;
	fds[1].events = POLLIN;
	
	while(run_server) {
		if(verboseflag)
			printf("waiting on poll()...\n");
		
		status = poll(fds, nfds, -1);
		
		if(status < 0) {
			perror("poll() failed\n");
			break;
		}
		
		if(status == 0) {
			printf("poll() timed out.\n");
			break;
		}
		
		if(!staylistenflag) {
			if(made_a_connection == 1 && conn_count == 0) {
				break;
			}
		}

		size = nfds;
		// loop through each descriptor and do stuff to them
		for(i=0; i < size; i++) {
			if(fds[i].revents == 0)
				continue;
			
			if(fds[i].revents != POLLIN) {
				printf("poll() error: revents = %d\n", fds[i].revents);
				break;
			}
			
			// there is incoming connections to accept
			if(fds[i].fd == serv_sock) {
				if(conn_count < max_conns) {
					new_fd = accept(serv_sock, (struct sockaddr *)&their_addr, &sin_size);
					if (new_fd == -1) {
						perror("accept");
						run_server = 0;
						continue;
					}
	
					inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
					printf("server: got connection from %s\n", s);
					made_a_connection = 1;
					fds[nfds].fd = new_fd;
					fds[nfds].events = POLLIN;
					nfds++;
					conn_count ++;
				}
			} else {
				do {
					if(verboseflag)
						printf("trying to read from this fd: %i\n", fds[i].fd);
					
					close_conn = 0;
					status = read(fds[i].fd, buf, sizeof(buf));
					
					if(status < 0) {
						perror("read");
						close_conn = 1;
						break;
					}
					
					if(status == 0) {
						//printf("connection closed \n");
						break;
					}
					
					// send the data to whomever it needs to go to
					for(k=0; k < nfds; k++) {
						if(verboseflag)
							printf("trying to write to fd %i\n", fds[k].fd);

						if(fds[i].fd == fds[k].fd) { // dont write to itself
							continue;
						}
						if(k == 0) { // can't write to stdin/ stdout is 1
							status = write(1, buf, status);
						} else if(k == 1){ // can't write to the listening socket
							continue;
						} else {
							status = write(fds[k].fd, buf, status);
						}
						
					}
					break;
				} while (1);
				
				if(close_conn) {
					close(fds[i].fd);
					fds[i].fd = -1;
					conn_count--;
					compress_array = 1;
					close_conn = 0;
				}
			}
		}
		
		if(compress_array) {
			compress_array = 0;
			for(i = 0; i < nfds; i++) {
				if(fds[i].fd == -1) {
					for(j = i; j < nfds; j++) {
						fds[j].fd = fds[j+1].fd;
					}
					i--;
					nfds--;
				}
			}
		}

		if(conn_count == 0 && made_a_connection && staylistenflag == 0) {
			run_server = 0;
		}
	}
}

/*	CLIENT

	Puts stdin as the first file descriptor in fds
	Creates the client socket and connects its to the host
	Puts teh client socket file descriptor into fds
	poll()s
*/	
void setup_client()
{
	int status, i;
	int nfds = 2; // 0 is stdin
	int new_fd;
	socklen_t sin_size;
	struct sockaddr_storage their_addr;
	int close_conn = 0; 
	int stay_connected = 1; // 1 means keep it runnning 
	int compress_array = 0;
	int k, j;
	char s[INET_ADDRSTRLEN];
	char buf[1024];
	
	memset(fds, 0 , sizeof(fds));
	
	fds[0].fd = 0; // stdin
	fds[0].events = POLLIN;
	//fds[0].revents = 0;
	
	client_sock = create_client_socket();
	fds[1].fd = client_sock;
	fds[1].events = POLLIN;

	while(stay_connected) {
		if(verboseflag)
			printf("waiting on poll()...\n");
		
		if(timeoutflag) {
			status = poll(fds, nfds, timeout);
		} else {
			status = poll(fds, nfds, -1);
		}

		if(status < 0) {
			perror("poll() failed\n");
			break;
		}
		
		if(status == 0) {
			printf("poll() timed out.\n");
			break;
		}

		// loop through each descriptor and do stuff to them
		for(i=0; i < 2; i++) {
			if(fds[i].revents == 0)
				continue;
			
			if(fds[i].revents != POLLIN) {
				printf("poll() error: revents = %d\n", fds[i].revents);
				break;
			}
			
			// there is incoming connections to accept
			if(fds[i].fd == 0) {
				if(verboseflag)
					printf("trying to read from this fd: %i\n", fds[i].fd);

				status = read(fds[i].fd, buf, sizeof(buf));
				if(status < 0) {
					perror("read");
					continue;
				}

				if(status == 0) {
					exit(0);
				}

				status = write(fds[i+1].fd, buf, status); 
			} else {
				if(verboseflag)
					printf("trying to read from this fd: %i\n", fds[i].fd);

				status = read(fds[i].fd, buf, sizeof(buf));
				if(status < 0) {
					perror("read");
					close_conn = 1;
					continue;
				}

				if(status == 0) {
					close_conn = 1;
					continue;
				}

				status = write(0, buf, status); 

				if(close_conn) {
					close(fds[i].fd);
					fds[i].fd = -1;
					conn_count--;
					compress_array = 1;
					close_conn = 0;
				}
			}
		}
		
		if(compress_array) {
			compress_array = 0;
			for(i = 0; i < nfds; i++) {
				if(fds[i].fd == -1) {
					for(j = i; j < nfds; j++) {
						fds[j].fd = fds[j+1].fd;
					}
					i--;
					nfds--;
				}
			}
		}

		if(conn_count == 0 && made_a_connection && staylistenflag == 0) {
			stay_connected = 0;
		}
	}
}

int main(int argc, char **argv) 
{
	check_args(argc, argv);

	if(listenflag) {
		create_server();
	} else {
		setup_client();
	}

	return 0;
}

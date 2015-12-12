




#ifndef COMMONPROTO
#define COMMONPROTO

extern int staylistenflag, listenflag, sourceportflag, verboseflag, timeoutflag, multipleflag;
extern int sourceport;
extern int timeout;
extern char *hostname; 
extern int portnum;
extern int max_conns;
extern int serv_sock;
extern int client_sock;
extern int conn_count;
extern int made_a_connection;
extern int curr_conns[10];

void usage(char *);
void check_args(int argc, char **argv); 
void *get_in_addr(struct sockaddr *sa);
int create_server_socket();
void server_listen(int sockfd);
int create_client_socket();
#endif

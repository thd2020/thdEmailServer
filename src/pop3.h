#ifndef POP3_H
#define POP3_H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <regex.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <mysql/mysql.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PPORT			"110"
#define BACKLOG_MAX		(10)
#define LOG_BUF_SIZE	1024
#define BUF_SIZE		4096
#define SHORTBUF_SIZE	128
#define STREQU(a,b)		(strcmp(a, b) == 0)

/**Sockets file descriptors*/
struct psfd_ll {
	int 	sfd;
	struct psfd_ll* next;
};

/**Function prototypes*/ 
int start_pop3(char* pname);
void init_pop3_socket();
void* handle_pop3(void* thread_arg);
void* pget_in_addr(struct sockaddr* sa);

#endif /*POP3_H*/
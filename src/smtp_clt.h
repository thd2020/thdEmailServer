#ifndef SMTP_CLT_H
#define SMTP_CLT_H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT			"2525"
#define DOMAIN			"thd2020.site"
#define BACKLOG_MAX		(10)
#define LOG_BUF_SIZE	1024
#define BUF_SIZE		4096
#define STREQU(a,b)		(strcmp(a, b) == 0)

/**Sockets file descriptors*/
struct sfd_ll {
	int 	sfd;
	struct sfd_ll* next;
};

/**Function prototypes*/ 
int start_smtp_clt(int argc, char** argv);
void init_listen_socket();
void* handle_clt_smtp(void* thread_arg);
void* get_in_addr(struct sockaddr* sa);

#endif
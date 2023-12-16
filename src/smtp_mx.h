#ifndef SMTP_MX_H
#define SMTP_MX_H

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

#define PORT			"2525"
#define BACKLOG_MAX		(10)
#define LOG_BUF_SIZE	1024
#define BUF_SIZE		4096
#define SHORTBUF_SIZE	128
#define STREQU(a,b)		(strcmp(a, b) == 0)

/**Function prototypes*/ 
int start_smtp_mx(char* pname);
int handle_mx(int sm_id);
int copy(char* src_path, char* des_path);
#endif /*SMTP_MX_H*/
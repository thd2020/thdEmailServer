#include "pop3.h"
#include "utils.h"

/**Overall server sfd_state*/
struct {
	struct psfd_ll* 	sockfds;
	int 		sockfd_max;
	char*		domain;
	pthread_t 	thread; /*Latest spawned thread*/
} pstate;

int start_pop3(char* pname){
    pstate.domain = DOMAIN; /*domain name for this mail server*/
    struct sockaddr_storage clt_addr; /*client address*/
    struct psfd_ll* p; /*listening sockets link list*/
    fd_set listen_socks; /*listening sockets fdset*/
    char ipaddr_buf[INET6_ADDRSTRLEN]; /*buffer for ipv6 address*/
    char* syslog_buf = malloc(LOG_BUF_SIZE); /*buffer for log*/

    /**init system log*/
    sprintf(syslog_buf, "%s", pname);
    openlog(syslog_buf, LOG_PERROR|LOG_PID, LOG_USER);
    /**open listening sockets for clients*/
    init_pop3_socket();
	init_mysql_con();
    while (1){
		socklen_t clt_addr_size;
        /**init listen_sock fdset for select*/
        FD_ZERO(&listen_socks);
        for (p = pstate.sockfds; p != NULL; p = p->next){
            FD_SET(p->sfd, &listen_socks);
        }
        /**select readable listen_socks*/
        select(pstate.sockfd_max+1, &listen_socks, NULL, NULL, NULL);
        for (p = pstate.sockfds; p != NULL; p = p->next){
            if (FD_ISSET(p->sfd, &listen_socks)){
                /**create connection socket*/
                int conn_sock = accept(p->sfd, (struct sockaddr*)&clt_addr, &clt_addr_size);
                if (conn_sock == -1){
                    perror(&errno);
					syslog(LOG_ERR, "Accepting client connection failed");
                    continue;
                }
                /**log clients information*/
                void* client_ip = pget_in_addr((struct sockaddr*)&clt_addr);
                inet_ntop(clt_addr.ss_family, client_ip, ipaddr_buf, sizeof(ipaddr_buf));
                syslog(LOG_DEBUG, "Connection from %s", ipaddr_buf);
                /**Pack the connection socket into dynamic mem
                to be passed to thread; it will free this when done.*/
                int* thread_arg = malloc(sizeof(int));
                *thread_arg = conn_sock;
                /**Spawn new thread to handle SMTP exchange*/
                pthread_create(&(pstate.thread), NULL, handle_pop3, thread_arg);
            }
        }
    }
    return 0;
}

/**
 * initiate listening sockets
*/
void init_pop3_socket(){
	int listen_sock; /*listening soocket fd*/
	struct addrinfo template; /*address template*/
    struct addrinfo* hostinfo,* p; /*addresses for localhost(ipv4, ipv6) and a pointer*/
	int com=0, yes=0;

    /**Set up the template indicating all of localhost's sockets*/
	memset(&template, 0, sizeof(template));
	template.ai_family = AF_UNSPEC;
	template.ai_socktype = SOCK_STREAM;
	template.ai_flags = AI_PASSIVE;
    /**get the addrinfo from the pop3 PORT*/
	if (getaddrinfo(NULL, PPORT, &template, &hostinfo) != 0){
		syslog(LOG_ERR, "Failed to get host addr info");
		exit(EXIT_FAILURE);
	}
    /**initiate server pstate's listening sockets*/
	pstate.sockfds = NULL;
	pstate.sockfd_max = 0;
	for (p=hostinfo; p!=NULL; p=p->ai_next){
		void* addr; /*sock addr*/
		char ipstr[INET6_ADDRSTRLEN]; /*ip*/

        /**get host ip*/
		if (p->ai_family == AF_INET) {
			addr = &((struct sockaddr_in*)p->ai_addr)->sin_addr; 
		} else {
			addr = &((struct sockaddr_in6*)p->ai_addr)->sin6_addr; 
		}
		inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
        /**set up listening socket*/
		listen_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listen_sock == -1) {
			syslog(LOG_NOTICE, "Failed to create IPv%d socket", \
                (p->ai_family == AF_INET) ? 4 : 6 );
			continue;
		}
		setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, 1, sizeof(int));
        /**bind listening socket*/
		if (bind(listen_sock, p->ai_addr, p->ai_addrlen) == -1){
			perror(&errno);
			close(listen_sock);
			syslog(LOG_NOTICE, "Failed to bind to IPv%d socket", \
				(p->ai_family == AF_INET) ? 4 : 6 );
			continue;
		}
        /**start listening*/
		if (listen(listen_sock, BACKLOG_MAX) == -1) {
			perror(&errno);
			syslog(LOG_NOTICE, "Failed to listen to IPv%d socket", \
				(p->ai_family == AF_INET) ? 4 : 6 );
			exit(EXIT_FAILURE);
		}
		/**Update highest fd value for select*/
		(listen_sock > pstate.sockfd_max) ? (pstate.sockfd_max = listen_sock) : 1;
		/**Then new listening socket to the pstate sfd list*/
		struct psfd_ll* new_listen_sock = malloc(sizeof(struct psfd_ll));
		new_listen_sock->sfd = listen_sock;
		new_listen_sock->next = pstate.sockfds;
		pstate.sockfds = new_listen_sock;
		if (pstate.sockfds == NULL){
			syslog(LOG_ERR, "Completely failed to bind to any sockets");
			exit(EXIT_FAILURE);
		}
		freeaddrinfo(hostinfo);
		return;
    }
} 

/**
 * handle client's smtp request
*/
void* handle_pop3(void* thread_arg){
	char* smtp_host = (char*)malloc(30);
	sprintf(smtp_host, "smtp.%s", DOMAIN);
    int sockfd = *(int*)thread_arg; /*connection socket fd*/
    char buffer[BUF_SIZE], bufferout[BUF_SIZE];
	buffer[BUF_SIZE-1] = '\0';
	int buffer_offset = 0;
	int ubuf_offset = 0;
    int inmessage = 0;
	char* ubuf = (char*)malloc(SHORTBUF_SIZE);
	char* test_user = (char*)malloc(SHORTBUF_SIZE);
	int user_id = 0;
	char* username = (char*)malloc(SHORTBUF_SIZE);
	char* userpass_base64 = (char*)malloc(SHORTBUF_SIZE);
	int rm_id = 0; /*current receiving mail id*/
	char* rcpt = (char*)malloc(SHORTBUF_SIZE); /*receipient email address*/
	char* data_path = (char*)malloc(150); /*where to store sending mail*/
	char* query = (char*)malloc(BUF_SIZE);
	time_t cur = malloc(sizeof(time_t));
	FILE* data = NULL; /*data file pointer*/
	int data_lines = 0; /*line counter*/
	int mailcount = 0;
	int mailsize = 0;
	int valid = 0;
	char** maildrop = malloc(128);
	int dropcount = 0; 
	MYSQL_RES* res = NULL;
    
    /**starting system log*/
    syslog(LOG_DEBUG, "Starting thread for socket #%d", sockfd);
    free(thread_arg);
    /**send welcome*/
    sprintf(bufferout, "+OK POP3 server ready %s\r\n", pstate.domain);
	printf("%s", bufferout);
	send(sockfd, bufferout, strlen(bufferout), 0);
    while (1){
        struct timeval tv;
        tv.tv_sec = 120;
        tv.tv_usec = 0;
        fd_set conn_socks;
        int buffer_left = BUF_SIZE - buffer_offset - 1;
		int ubuf_left = SHORTBUF_SIZE - ubuf_offset - 1;
        char* eol;
		char* ueol;

        FD_ZERO(&conn_socks);
        FD_SET(sockfd, &conn_socks);
        /**if time elasped by tv, then send something*/
		receive:
        select(sockfd+1, &conn_socks, NULL, NULL, &tv);
        if (!FD_ISSET(sockfd, &conn_socks)){
			syslog(LOG_DEBUG, "%d: Socket timed out", sockfd);
			break;
		}
        /**happens when command line length surpress BUF_SIZE*/
        if (buffer_left == 0){
			syslog(LOG_DEBUG, "%d: Command line too long", sockfd);
			sprintf(bufferout, "500 Too long\r\n");
			printf("S%d: %s", sockfd, bufferout);
			send(sockfd, bufferout, strlen(bufferout), 0);
			buffer_offset = 0;
			continue;
		}
        /**receive command from client*/
        int cmd_len = recv(sockfd, buffer + buffer_offset, buffer_left, 0);
        if (cmd_len == 0){
			syslog(LOG_DEBUG, "%d: Remote host closed socket", sockfd);
			break;
		}
        if (cmd_len == -1){
			syslog(LOG_DEBUG, "%d: Error on socket", sockfd);
			break;
		}
        buffer_offset += cmd_len;
        /**process only single line each turn*/
        process_line:
        eol = strstr(buffer, "\r\n");
		if (eol == NULL){
			goto receive;
		}
        eol[0] = '\0';
        if (!inmessage){
            printf("C%d: %s\n", sockfd, buffer);
			/**Replace all lower case letters so verbs are all caps*/
			for (int i=0; i<4; i++) {
				if (islower(buffer[i])) {
					buffer[i] += 'A' - 'a';
				}
			}
			/** Respond to each verb accordingly.*/
			if (strstr(buffer, "USER")-buffer==0){
				sscanf(buffer, "USER %s", test_user);
				sprintf(query, "SELECT `user_id`, `username`, `password` FROM `users` WHERE `username` = '%s'", test_user);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto fallback;
				}
				res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto fallback;
				}
				MYSQL_ROW row;
				if ((row = mysql_fetch_row(res)) != NULL){
					user_id = atoi(row[0]);
					username = row[1];
					userpass_base64 = row[2];
					sprintf(bufferout, "+OK Please enter a password\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				} else {
					syslog(LOG_WARNING, "no such user %s", test_user);
					goto fallback;
				}
			} else if (strstr(buffer, "PASS")-buffer==0){
				if (user_id == 0){
					sprintf(bufferout, "Need USER first!\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
				sscanf(buffer, "PASS %[0-9a-fA-F]", ubuf);
				ubuf = base64_encode(ubuf);
				if (!STREQU(ubuf, userpass_base64)){
					syslog(LOG_WARNING, "userpass incorrect or not match");
					goto fallback;
				}
				else if (STREQU(ubuf, userpass_base64)){
					valid = 1;
					sprintf(bufferout, "+OK valid login\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
			} else if (strstr(buffer, "STAT")-buffer==0){ /*Diplay Mails Status*/
				if (valid != 1){
					sprintf(bufferout, "-ERR Need Auth First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sprintf(query, "SELECT COUNT(*), SUM(`size`) FROM `rc_mails` WHERE `user_id`=%d AND `processed`=0", user_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto post_process;
				}
				res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto post_process;
				}
				MYSQL_ROW row;
				if ((row = mysql_fetch_row(res)) != NULL){
					mailcount = atoi(row[0]);
					mailsize = atoi(row[1]);
				} else {
					syslog(LOG_WARNING, "cannot fetch row");
					goto post_process;
				}
				sprintf(bufferout, "+OK %d messages [%d byte(s)]\r\n", mailcount, mailsize);
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "LIST")){ /*List Mails*/
				if (valid != 1){
					sprintf(bufferout, "-ERR Need Auth First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sprintf(query, "SELECT COUNT(*), SUM(`size`) FROM `rc_mails` WHERE `user_id`=%d AND `processed`=0", user_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto post_process;
				}
				res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto post_process;
				}
				MYSQL_ROW row;
				if ((row = mysql_fetch_row(res)) != NULL){
					mailcount = atoi(row[0]);
					mailsize = atoi(row[1]);
				} else {
					syslog(LOG_WARNING, "cannot fetch row");
					goto post_process;
				}
				sprintf(bufferout, "+OK %d messages [%d byte(s)]\r\n", mailcount, mailsize);
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				sprintf(query, "SELECT `rm_id`, `size` FROM `rc_mails` WHERE `user_id`=%d AND `processed`=0", user_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto post_process;
				}
				res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto post_process;
				}
				for (int i=0; i<mysql_num_rows(res); i++){
					MYSQL_ROW row;
					if ((row = mysql_fetch_row(res)) != NULL){
						sprintf(bufferout, "%s %d\r\n", row[0], row[1]);
						printf("S%d: %s", sockfd, bufferout);
						send(sockfd, bufferout, strlen(bufferout), 0);
					} else {
						syslog(LOG_WARNING, "cannot fetch row %d", i);
					}
				}
			} else if (strstr(buffer, "RETR")-buffer==0){ /*Retrieve a Mail*/
				if (valid != 1){
					sprintf(bufferout, "-ERR Need Auth First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sscanf(buffer, "RETR %d", &rm_id);
				sprintf(query, "SELECT `data_path`, `size` FROM `rc_mails` WHERE `rm_id`=%d", rm_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto post_process;
				}
				res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto post_process;
				}
				MYSQL_ROW row;
				if ((row = mysql_fetch_row(res)) != NULL){
					data_path = row[0];
				} else {
					syslog(LOG_WARNING, "cannot fetch row");
					goto post_process;
				}
				FILE* data = fopen(data_path, "r");
				char* buff = malloc(1024);
				int len = 0;
				sprintf(bufferout, "+OK %d octets follow", atoi(row[1]));
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				while(len = fgets(buff, 1024, data))
				{
					sprintf(bufferout, buff);
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
			} else if (strstr(buffer, "TOP")-buffer==0){ /*Fetch top n line of a Mail*/
				if (valid != 1){
					sprintf(bufferout, "-ERR Need Auth First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				int top = 1;
				sscanf(buffer, "RETR %d %d", &rm_id, &top);
				sprintf(query, "SELECT `data_path`, `size` FROM `rc_mails` WHERE `rm_id`=%d", rm_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto post_process;
				}
				res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto post_process;
				}
				MYSQL_ROW row;
				if ((row = mysql_fetch_row(res)) != NULL){
					data_path = row[0];
				} else {
					syslog(LOG_WARNING, "cannot fetch row");
					goto post_process;
				}
				FILE* data = fopen(data_path, "r");
				char buff[1024];
				int len = 0;
				sprintf(bufferout, "+OK");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				int i = 0;
				while(len = fgets(buff, 1024, data))
				{
					sprintf(bufferout, buff);
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					i++;
					if (i >= top)
						break;
				}
			} else if (strstr(buffer, "DELE")-buffer==0){ /*Delete a mail*/
				if (valid != 1){
					sprintf(bufferout, "-ERR Need Auth First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sscanf(buffer, "DELE %d", &rm_id);
				sprintf(query, "SELECT `data_path` FROM `rc_mails` WHERE `rm_id`=%d", rm_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto post_process;
				}
				res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto post_process;
				}
				MYSQL_ROW row;
				if ((row = mysql_fetch_row(res)) != NULL){
					data_path = row[0];
				} else {
					syslog(LOG_WARNING, "cannot fetch row");
					goto post_process;
				}
				maildrop[dropcount] = data_path;
				dropcount++;
				if (remove(data_path)){
					syslog(LOG_WARNING, "cannot remove %s", data_path);
					goto post_process;
				}
				sprintf(query, "DELETE FROM `rc_mails` WHERE `rm_id`=%d", rm_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto post_process;
				}
				sprintf(bufferout, "+OK message deleted");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (strstr(buffer, "UIDL")-buffer==0){ 
				if (valid != 1){
					sprintf(bufferout, "-ERR Need Auth First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				int input_id = 0;
				sscanf(buffer, "UIDL %d", &input_id);
				sprintf(bufferout, "+OK %d %d\r\n", &input_id, &input_id);
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (strstr(buffer, "RSET")-buffer==0){ 
				if (valid != 1){
					sprintf(bufferout, "-ERR Need Auth First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sprintf(bufferout, "+OK\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "NOOP")){ /*Do nothing*/
				sprintf(bufferout, "250 +OK noop\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "QUIT")){ /*Close the connection*/
				sprintf(bufferout, "221 +OK\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				break;
			} else { /*The verb used hasn't been implemented.*/
				sprintf(bufferout, "502 Command Not Implemented\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			}
		} else {
			fallback:
			user_id = 0;
			bzero(username, strlen(username));
			bzero(userpass_base64, strlen(userpass_base64));
			sprintf(bufferout, "-ERR Something went wrong, try again\r\n");
			printf("S%d: %s", sockfd, bufferout);
			send(sockfd, bufferout, strlen(bufferout), 0);
        }
		post_process:
        /**Shift the rest of the buffer to the front*/
		memmove(buffer, eol+2, BUF_SIZE - (eol + 2 - buffer));
		buffer_offset -= (eol - buffer) + 2;
		/**Do we already have additional lines to process? If so,
		commit a horrid sin and goto the line processing section again.*/
		if (strstr(buffer, "\r\n")) 
			goto process_line;
	}
	/**All done. Clean up everything and exit.*/
	close(sockfd);
	pthread_exit(NULL);
}

/**
 * Extract the address from sockaddr depending on which family of socket it is
 * */
void* pget_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
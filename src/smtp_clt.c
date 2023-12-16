#include "smtp_clt.h"
#include "utils.h"

/**Overall server sfd_state*/
struct {
	struct sfd_ll* 	sockfds;
	int 		sockfd_max;
	char*		domain;
	pthread_t 	thread; /*Latest spawned thread*/
} state;

int start_smtp_clt(char* pname){
    state.domain = DOMAIN; /*domain name for this mail server*/
    struct sockaddr_storage clt_addr; /*client address*/
    struct sfd_ll* p; /*listening sockets link list*/
    fd_set listen_socks; /*listening sockets fdset*/
    char ipaddr_buf[INET6_ADDRSTRLEN]; /*buffer for ipv6 address*/
    char* syslog_buf = malloc(LOG_BUF_SIZE); /*buffer for log*/

    /**init system log*/
    sprintf(syslog_buf, "%s", pname);
    openlog(syslog_buf, LOG_PERROR|LOG_PID, LOG_USER);
    /**open listening sockets for clients*/
    init_listen_socket();
	init_mysql_con();
    while (1){
		socklen_t clt_addr_size;
        /**init listen_sock fdset for select*/
        FD_ZERO(&listen_socks);
        for (p = state.sockfds; p != NULL; p = p->next){
            FD_SET(p->sfd, &listen_socks);
        }
        /**select readable listen_socks*/
        select(state.sockfd_max+1, &listen_socks, NULL, NULL, NULL);
        for (p = state.sockfds; p != NULL; p = p->next){
            if (FD_ISSET(p->sfd, &listen_socks)){
                /**create connection socket*/
                int conn_sock = accept(p->sfd, (struct sockaddr*)&clt_addr, &clt_addr_size);
                if (conn_sock == -1){
                    perror(&errno);
					syslog(LOG_ERR, "Accepting client connection failed");
                    continue;
                }
                /**log clients information*/
                void* client_ip = get_in_addr((struct sockaddr*)&clt_addr);
                inet_ntop(clt_addr.ss_family, client_ip, ipaddr_buf, sizeof(ipaddr_buf));
                syslog(LOG_DEBUG, "Connection from %s", ipaddr_buf);
                /**Pack the connection socket into dynamic mem
                to be passed to thread; it will free this when done.*/
                int* thread_arg = malloc(sizeof(int));
                *thread_arg = conn_sock;
                /**Spawn new thread to handle SMTP exchange*/
                pthread_create(&(state.thread), NULL, handle_clt_smtp, thread_arg);
            }
        }
    }
    return 0;
}

/**
 * initiate listening sockets
*/
void init_listen_socket(){
	int listen_sock; /*listening soocket fd*/
	struct addrinfo template; /*address template*/
    struct addrinfo* hostinfo,* p; /*addresses for localhost(ipv4, ipv6) and a pointer*/
	int com=0, yes=0;

    /**Set up the template indicating all of localhost's sockets*/
	memset(&template, 0, sizeof(template));
	template.ai_family = AF_UNSPEC;
	template.ai_socktype = SOCK_STREAM;
	template.ai_flags = AI_PASSIVE;
    /**get the addrinfo from the smtp PORT*/
	if (getaddrinfo(NULL, PORT, &template, &hostinfo) != 0){
		syslog(LOG_ERR, "Failed to get host addr info");
		exit(EXIT_FAILURE);
	}
    /**initiate server state's listening sockets*/
	state.sockfds = NULL;
	state.sockfd_max = 0;
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
		(listen_sock > state.sockfd_max) ? (state.sockfd_max = listen_sock) : 1;
		/**Then new listening socket to the state sfd list*/
		struct sfd_ll* new_listen_sock = malloc(sizeof(struct sfd_ll));
		new_listen_sock->sfd = listen_sock;
		new_listen_sock->next = state.sockfds;
		state.sockfds = new_listen_sock;
		if (state.sockfds == NULL){
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
void* handle_clt_smtp(void* thread_arg){
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
	int sm_id = 0; /*current sending mail id*/
	char* rcpt = (char*)malloc(SHORTBUF_SIZE); /*receipient email address*/
	char* data_path = (char*)malloc(150); /*where to store sending mail*/
	char* query = (char*)malloc(BUF_SIZE);
	time_t cur = malloc(sizeof(time_t));
	FILE* data = NULL; /*data file pointer*/
	int data_lines = 0; /*line counter*/
    
    /**starting system log*/
    syslog(LOG_DEBUG, "Starting thread for socket #%d", sockfd);
    free(thread_arg);
    /**send welcome*/
    sprintf(bufferout, "220 %s SMTP CCSMTP thdEmail Test Server\r\n", state.domain);
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
			if (strstr(buffer, "HELO")-buffer==0 || strstr(buffer, "EHLO")-buffer==0){
				char* format = malloc(50);
				char* ehlo_format = malloc(50);
				sprintf(format, "HELO %s", smtp_host);
				sprintf(ehlo_format, "EHLO %s", smtp_host);
				if (strstr(buffer, format)-buffer!=0 && strstr(buffer, ehlo_format)-buffer!=0){
					sprintf(bufferout, "Wrong host!\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
				else if (strstr(buffer, format)-buffer==0 || strstr(buffer, ehlo_format)-buffer==0){
					sprintf(bufferout, "250 Ok\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
			} else if (STREQU(buffer, "AUTH LOGIN")){
				sprintf(bufferout, "334 VXNlcm5hbWU6\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				memset(ubuf, 0, sizeof(ubuf));
				ubuf_offset = 0;
				recv_user:
				cmd_len = recv(sockfd, ubuf + ubuf_offset, ubuf_left, 0);
				if (cmd_len <= 0){
					syslog(LOG_WARNING, "receive username failed");
					goto fallback;
				}
				ubuf_offset += cmd_len;
				ueol = strstr(ubuf, "\r\n");
				if (ueol == NULL){
					goto recv_user;
				}
				ubuf[strlen(ubuf) - 1] = '\0';
				ubuf[strlen(ubuf) - 1] = '\0';
				test_user = (char*)base64_decode(ubuf);
				sprintf(query, "SELECT `user_id`, `username`, `password` FROM `users` WHERE `username` = '%s'", test_user);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					goto fallback;
				}
				MYSQL_RES* res = mysql_store_result(con);
				if (res == NULL){
					syslog(LOG_WARNING, "store mysql result failed");
					goto fallback;
				}
				MYSQL_ROW row;
				if ((row = mysql_fetch_row(res)) != NULL){
					user_id = atoi(row[0]);
					username = row[1];
					userpass_base64 = row[2];
				} else {
					syslog(LOG_WARNING, "no such user %s", test_user);
					goto fallback;
				}
				sprintf(bufferout, "334 UGFzc3dvcmQ6\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				bzero(ubuf, strlen(ubuf));
				ubuf_offset = 0;
				recv_pass:
				cmd_len = recv(sockfd, ubuf + ubuf_offset, ubuf_left, 0);
				if (cmd_len <= 0){
					syslog(LOG_WARNING, "receive userpass failed");
					goto fallback;
				}
				ubuf_offset += cmd_len;
				ueol = strstr(ubuf, "\r\n");
				if (ueol == NULL){
					goto recv_pass;
				}
				ubuf[strlen(ubuf) - 1] = '\0';
				ubuf[strlen(ubuf) - 1] = '\0';
				if (!STREQU(ubuf, userpass_base64)){
					syslog(LOG_WARNING, "userpass incorrect or not match");
					goto fallback;
				}
				else if (STREQU(ubuf, userpass_base64)){
					sprintf(bufferout, "Authentication successful\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
				else {
					fallback:
					user_id = 0;
					bzero(username, strlen(username));
					bzero(userpass_base64, strlen(userpass_base64));
					sprintf(bufferout, "Something went wrong, try again\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
			} else if (strstr(buffer, "MAIL FROM")-buffer==0){ /*New mail from*/
				if (user_id == 0){
					sprintf(bufferout, "Need Authentication First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
				else if (user_id != 0){
					char* format = malloc(50);
					sprintf(format, "MAIL FROM:<%s@%s>", username, DOMAIN);
					if (strstr(buffer, format)-buffer!=0){
						sprintf(bufferout, "Username not match\r\n");
						printf("S%d: %s", sockfd, bufferout);
						send(sockfd, bufferout, strlen(bufferout), 0);
						goto post_process;
					}
					cur = time(NULL);
					sprintf(query, "INSERT INTO `sent_mails` (`user_id`, `time`) VALUES (%d, FROM_UNIXTIME(%d))", user_id, cur);
					if (mysql_query(con, query)){
						syslog(LOG_WARNING, "parsing sql: %s failed", query);
						sprintf(bufferout, "sql insert error\r\n");
						printf("S%d: %s", sockfd, bufferout);
						send(sockfd, bufferout, strlen(bufferout), 0);
						goto post_process;
					}
					sprintf(query, "SELECT `sm_id` FROM `sent_mails` WHERE UNIX_TIMESTAMP(`time`)=%d", cur);
					if (mysql_query(con, query)){
						syslog(LOG_WARNING, "parsing sql: %s failed", query);
						sprintf(bufferout, "sql select error\r\n");
						printf("S%d: %s", sockfd, bufferout);
						send(sockfd, bufferout, strlen(bufferout), 0);
						goto post_process;
					}
					MYSQL_RES* res = mysql_store_result(con);
					if (res == NULL){
						syslog(LOG_WARNING, "store mysql result failed");
						goto post_process;
					}
					MYSQL_ROW row;
					if ((row = mysql_fetch_row(res)) != NULL){
						sm_id = atoi(row[0]);
					}
					sprintf(bufferout, "250 Mail OK\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
				}
			} else if (strstr(buffer, "RCPT TO")-buffer==0){ /*Mail addressed to...*/
				if (user_id == 0){
					sprintf(bufferout, "Need Authentication First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				if (sm_id == 0){
					sprintf(bufferout, "Need MAIL FROM First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				regex_t format;
				regcomp(&format, "^RCPT TO:<\\w+([-+.]\\w+)*@\\w+([-.]\\w+)*.\\w+([-.]\\w+)*>*$", REG_EXTENDED);
				if (regexec(&format, buffer, 0, NULL, 0)){
					sprintf(bufferout, "RCPT format not match\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sscanf(buffer, "RCPT TO:<%50[^>]>", rcpt);
				struct tm* tf = gmtime(&cur);
				char* date = malloc(30);
				sprintf(date, "%d-%02d-%02d", tf->tm_year+1900, tf->tm_mon+1, tf->tm_mday);
				sprintf(data_path, "%s/%s/sentmails/%s_%s_%d.bin", BASE_PATH, username, rcpt, date, sm_id);
				FILE* temp = fopen(data_path, "w");
				fclose(temp);
				sprintf(query, "UPDATE `sent_mails` SET `rcpt`='%s', `data_path`='%s' WHERE `sm_id`=%d", rcpt, data_path, sm_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					sprintf(bufferout, "sql update error\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sprintf(bufferout, "250 Mail OK\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "DATA")){ /*Message contents...*/
				if (user_id == 0){
					sprintf(bufferout, "Need Authentication First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				if (sm_id == 0){
					sprintf(bufferout, "Need MAIL FROM First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				if (rcpt == 0){
					sprintf(bufferout, "Need RCPT TO First\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
				}
				sprintf(bufferout, "354 End data with <CR><LF>.<CR><LF>\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				inmessage = 1;
				data = fopen(data_path, "w");
			} else if (STREQU(buffer, "RSET")){ /*Reset the connection*/
				if (sm_id != 0){
					sprintf(query, "DELETE FROM `sent_mails` WHERE `sm_id`=%d", sm_id);
					if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
					sprintf(bufferout, "sql update error\r\n");
					printf("S%d: %s", sockfd, bufferout);
					send(sockfd, bufferout, strlen(bufferout), 0);
					goto post_process;
					}
				}
				user_id = 0;
				sm_id = 0;
				data_lines = 0;
				bzero(username, strlen(username));
				bzero(userpass_base64, strlen(userpass_base64));
				bzero(rcpt, strlen(rcpt));
				struct stat st = {0};
				if (stat(data_path, &st) != -1){
					remove(data_path);
				}
				bzero(data_path, strlen(data_path));
				sprintf(bufferout, "250 Ok reset\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "NOOP")){ /*Do nothing*/
				sprintf(bufferout, "250 Ok noop\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "QUIT")){ /*Close the connection*/
				sprintf(bufferout, "221 Ok\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				break;
			} else { /*The verb used hasn't been implemented.*/
				sprintf(bufferout, "502 Command Not Implemented\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			}
		} else { /*We are inside the message after a DATA verb.*/
			data_lines++;
			fputs(buffer, data);
			fputc('\n', data);
			printf("C%d: %s\n", sockfd, buffer);
			if (data_lines==3 && strstr(buffer, "Subject:")-buffer==0){
				char* subject = malloc(50);
				sscanf(buffer, "Subject: %s", subject);
				sprintf(query, "UPDATE `sent_mails` SET `title`='%s' WHERE `sm_id`=%d", subject, sm_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
				}
			}
			else if (data_lines==3){
				char* subject = malloc(50);
				sprintf(query, "UPDATE `sent_mails` SET `title`='%s' WHERE `sm_id`=%d", buffer, sm_id);
				if (mysql_query(con, query)){
					syslog(LOG_WARNING, "parsing sql: %s failed", query);
				}
			}
			if (STREQU(buffer, ".")) { // A single "." signifies the end
				fclose(data);
				sprintf(bufferout, "250 Mail Ok queued as\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				inmessage = 0;
			}
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
void* get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
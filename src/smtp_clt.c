#include "smtp_clt.h"

/**Overall server sfd_state*/
struct {
	struct sfd_ll* 	sockfds;
	int 		sockfd_max;
	char*		domain;
	pthread_t 	thread; /*Latest spawned thread*/
} state;

int start_smtp_clt(int argc, char** argv, MYSQL* con){
    state.domain = DOMAIN; /*domain name for this mail server*/
    struct sockaddr_storage clt_addr; /*client address*/
    struct sfd_ll* p; /*listening sockets link list*/
    fd_set listen_socks; /*listening sockets fdset*/
    char ipaddr_buf[INET6_ADDRSTRLEN]; /*buffer for ipv6 address*/
    char* syslog_buf = malloc(LOG_BUF_SIZE); /*buffer for log*/

    /**init system log*/
    sprintf(syslog_buf, "%s", argv[0]);
    openlog(syslog_buf, LOG_PERROR|LOG_PID, LOG_USER);
    /**open listening sockets for clients*/
    init_listen_socket();
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
			close(listen_sock);
			syslog(LOG_NOTICE, "Failed to bind to IPv%d socket", \
				(p->ai_family == AF_INET) ? 4 : 6 );
			continue;
		}
        /**start listening*/
		if (listen(listen_sock, BACKLOG_MAX) == -1) {
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
    int sockfd = *(int*)thread_arg; /*connection socket fd*/
    char buffer[BUF_SIZE], bufferout[BUF_SIZE];
	buffer[BUF_SIZE-1] = '\0';
	int buffer_offset = 0;
    int inmessage = 0;
    
    /**starting system log*/
    syslog(LOG_DEBUG, "Starting thread for socket #%d", sockfd);
    free(thread_arg);
    /**send welcome*/
    sprintf(bufferout, "220 %s SMTP CCSMTP\r\n", state.domain);
	printf("%s", bufferout);
	send(sockfd, bufferout, strlen(bufferout), 0);
    while (1){
        struct timeval tv;
        tv.tv_sec = 120;
        tv.tv_usec = 0;
        fd_set conn_socks;
        int buffer_left = BUF_SIZE - buffer_offset - 1;
        char* eol;
		char* username_base64;
		char* username;
		char* userpass_base64;
		int user_id = NULL;

        FD_ZERO(&conn_socks);
        FD_SET(sockfd, &conn_socks);
        /**if time elasped by tv, then send something*/
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
			syslog(LOG_DEBUG, "%d: Haven't found EOL yet", sockfd);
			continue;
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
			// Null-terminate the verb for strcmp
			buffer[4] = '\0';
			/** Respond to each verb accordingly.
			You should replace these with more meaningful
			actions than simply printing everything.
			*/
			if (STREQU(buffer, "HELO")){ /*Initial greeting*/
				sprintf(bufferout, "250 Ok\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "AUTH LOGIN")){
				auth:
				sprintf(bufferout, "334 VXNlcm5hbWU6\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				if (cmd_len = recv(sockfd, username_base64, buffer_left, 0) == -1){
					syslog(LOG_WARNING, "receive username failed");
					goto auth;
				}
				username = base64_decode(username_base64);
				if (mysql_query(con, "SELECT user_id FROM users WHERE user_name"))
			} else if (STREQU(buffer, "MAIL FROM")){ /*New mail from*/
				sprintf(bufferout, user_id == NULL?"Need Authentication First\r\n":"250 Ok\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "RCPT TO")){ /*Mail addressed to...*/
				sprintf(bufferout, user_id == NULL?"Need Authentication First":"250 Ok recipient\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
			} else if (STREQU(buffer, "DATA")){ /*Message contents...*/
				sprintf(bufferout, user_id == NULL?"Need Authentication First\r\n":"354 Continue\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				inmessage = 1;
			} else if (STREQU(buffer, "RSET")){ /*Reset the connection*/
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
			printf("C%d: %s\n", sockfd, buffer);
			if (STREQU(buffer, ".")) { // A single "." signifies the end
				sprintf(bufferout, "250 Ok\r\n");
				printf("S%d: %s", sockfd, bufferout);
				send(sockfd, bufferout, strlen(bufferout), 0);
				inmessage = 0;
			}
        }
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
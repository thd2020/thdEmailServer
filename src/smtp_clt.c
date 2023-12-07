#include "smtp_clt.h"

int main(int argc, char** argv){
    int rc, i, j;
    char strbuf[INET6_ADDRSTRLEN];
    char* syslog_buf = malloc(LOG_BUF_SIZE);
    struct sockaddr_storage clt_addr;
    state.domain = DOMAIN;
    fd_set socks;
    int_ll* p;

    /**init system log*/
    sprintf(syslog_buf, "%s", argv[0]);
    openlog(syslog_buf, LOG_PERROR|LOG_PID, LOG_USER);

    /**open listening socket for clients*/
    init_listen_socket();

    while(1){
        /**init sock link for select*/
        FD_ZERO(&socks);
        for(p = state.sockfds; p != NULL; p = p->next){
            FD_SET(p->sfd, &socks);
        } 
        /**select readable socks*/
        select(state.sockfd_max+1, &socks, NULL, NULL, NULL);
        for(p = state.sockfds; p != NULL && FD_ISSET(p->sfd, &socks); p = p->next){
            int conn_sock = accept(p->sfd, (struct sockaddr*)&clt_addr, sizeof(clt_addr));
            if (conn_sock == -1) {
					syslog(LOG_ERR, "Accepting client connection failed");
					continue;
			}
            /**Log clients information*/
            void *client_ip = get_in_addr(\
                    (struct sockaddr *)&client_addr);
            inet_ntop(client_addr.ss_family, \
                    client_ip, strbuf, sizeof(strbuf));
            syslog(LOG_DEBUG, "Connection from %s", strbuf);

        // Pack the socket file descriptor into dynamic mem
        // to be passed to thread; it will free this when done.
        int * thread_arg = (int*) malloc(sizeof(int));
        *thread_arg = new_sock;

        // Spawn new thread to handle SMTP exchange
        pthread_create(&(state.thread), NULL, \
                handle_smtp, thread_arg);
    }

    }
}





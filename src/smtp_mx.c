#include "smtp_mx.h"
#include "utils.h"

/**Overall server sfd_state*/
struct {
	struct sfd_ll* 	sockfds;
	int 		sockfd_max;
	char*		domain;
	pthread_t 	thread; /*Latest spawned thread*/
} mstate;

int start_smtp_mx(char* pname){
	mstate.domain = DOMAIN; /*domain name for this mail server*/
	char* query = (char*)malloc(SHORTBUF_SIZE);
	int sent_mails[SHORTBUF_SIZE];

	init_mysql_con();
	sprintf(query, "SELECT `sm_id` FROM `sent_mails` WHERE `processed`=0");
	if (mysql_query(con, query)){
		syslog(LOG_WARNING, "parsing sql: %s failed", query);
		return -1;
	}
	MYSQL_RES* res = mysql_store_result(con);
	if (res == NULL){
		syslog(LOG_WARNING, "store mysql result failed");
		return -1;
	}
	for (int i=0; i<mysql_num_rows(res); i++){
		MYSQL_ROW row;
		if ((row = mysql_fetch_row(res)) != NULL){
			sent_mails[i] = atoi(row[0]);
			handle_mx(sent_mails[i]);
		} else {
			syslog(LOG_WARNING, "cannot fetch row %d", i);
			return -1;
		}
	}
}

int handle_mx(int sm_id){
	int user_id = 0;
	int ruser_id = 0;
	int rm_id = 0;
	char* from = (char*)malloc(SHORTBUF_SIZE);
	char* rcpt = (char*)malloc(SHORTBUF_SIZE);
	char* data_path = (char*)malloc(150);
	char* rcpt_user = (char*)malloc(30); /*接收者用户名*/
	char* rcpt_domain = (char*)malloc(30); /*接收者所在域*/
	char* rcpt_data_path  = (char*)malloc(SHORTBUF_SIZE); /*接收者邮件路径*/
	char* title = (char*)malloc(150);
	time_t cur = malloc(sizeof(time_t));
	char* query = (char*)malloc(BUF_SIZE);

	sprintf(query, "SELECT sm.`user_id`, sm.`rcpt`, sm.`data_path`, sm.`title`, u.`username` FROM `sent_mails` sm, `users` u WHERE `sm_id`=%d", sm_id);
	if (mysql_query(con, query)){
		syslog(LOG_WARNING, "parsing sql: %s failed", query);
		return -1;
	}
	MYSQL_RES* res = mysql_store_result(con);
	if (res == NULL){
		syslog(LOG_WARNING, "store mysql result failed");
		return -1;
	}
	MYSQL_ROW row;
	if ((row = mysql_fetch_row(res)) != NULL){
		user_id = atoi(row[0]);
		rcpt = row[1];
		data_path = row[2];
		title = row[3];
		from = row[4];
	}
	else{
		syslog(LOG_WARNING, "cannot fetch row for %s", sm_id);
		return -1;
	}
	if (rcpt == NULL){
		syslog(LOG_WARNING, "no rcpt set for sm_id %d", sm_id);
		return -1;
	}
	sscanf(rcpt, "%50[^@]@%s", rcpt_user, rcpt_domain);
	if (STREQU(rcpt_domain, DOMAIN)){ /*local email*/
		sprintf(query, "SELECT `user_id`, `path` FROM `users` WHERE `username`='%s'", rcpt_user);
		if (mysql_query(con, query)){
		syslog(LOG_WARNING, "parsing sql: %s failed", query);
		return -1;
		}
		MYSQL_RES* res = mysql_store_result(con);
		if (res == NULL){
			syslog(LOG_WARNING, "store mysql result failed");
			return -1;
		}
		MYSQL_ROW row;
		if ((row = mysql_fetch_row(res)) != NULL){
			ruser_id = atoi(row[0]);
			cur = time(NULL);
			sprintf(query, "INSERT INTO `rc_mails` (`user_id`, `time`, `title`, `from`) VALUES (%d, FROM_UNIXTIME(%d), '%s', '%s@%s')", ruser_id, cur, title, from, DOMAIN);
			if (mysql_query(con, query)){
				syslog(LOG_WARNING, "parsing sql: %s failed", query);
				return -1;
			}
			sprintf(query, "SELECT `rm_id` FROM `rc_mails` WHERE UNIX_TIMESTAMP(`time`)=%d", cur);
			if (mysql_query(con, query)){
				syslog(LOG_WARNING, "parsing sql: %s failed", query);
				return -1;
			}
			MYSQL_RES* res = mysql_store_result(con);
			if (res == NULL){
				syslog(LOG_WARNING, "store mysql result failed");
				return -1;
			}
			MYSQL_ROW row;
			if ((row = mysql_fetch_row(res)) != NULL){
				rm_id = atoi(row[0]);
			}
			struct tm* tf = gmtime(&cur);
			char* date = malloc(30);
			sprintf(date, "%d-%02d-%02d", tf->tm_year+1900, tf->tm_mon+1, tf->tm_mday);
			sprintf(rcpt_data_path, "%s/%s/rcmails/%s_%s_%d.bin", BASE_PATH, rcpt_user, from, date, rm_id);
			if (copy(data_path, rcpt_data_path)){
				syslog(LOG_WARNING, "copy mail failed");
				perror(&errno);
			}
			sprintf(query, "UPDATE `rc_mails` SET `data_path`='%s' WHERE `rm_id`=%d", rcpt_data_path, rm_id);
			if (mysql_query(con, query)){
				syslog(LOG_WARNING, "parsing sql: %s failed", query);
				return -1;
			}
			sprintf(query, "UPDATE `sent_mails` SET `processed`=1 WHERE `sm_id`=%d", sm_id);
			if (mysql_query(con, query)){
				syslog(LOG_WARNING, "parsing sql: %s failed", query);
				return -1;
			}
			printf("sending mail from %s@%s to %s success\n", from, DOMAIN, rcpt);
		}
		else {
			syslog(LOG_WARNING, "no such user %s", rcpt_user);
			return -1;
		}
	}
}

int copy(char* src_path, char* des_path){
	FILE *in,*out;
	char buff[1024];
	int len = 0;
 
	in = fopen(src_path,"r+");
	out = fopen(des_path,"w+");
	while(len = fread(buff,1,sizeof(buff),in))
	{
		fwrite(buff,1,len,out);
	}
	return 0;
}
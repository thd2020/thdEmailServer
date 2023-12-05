#ifndef SENDMAIL_H
#define SENDMAIL_H

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

#define SMTP_SERVER "smtp.thd2020.site"
#define SMTP_PORT 587
typedef struct{
  char *from = "\"User Name\" <username@gmail.com>";
  char *to = "\"Recipient Name\" <recipient@gmail.com>";
  char *subject = "Hello, world!";
  char *body = "This is a test email sent from a simple SMTP client.";
} MailContent;

int send_command(int sock, const char *cmd, const char *arg);
int sendMail();

#endif
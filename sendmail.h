#ifndef SENDMAIL_H
#define SENDMAIL_H

#define SMTP_SERVER "smtp.thd2020.site"
#define SMTP_PORT 587
typedef struct{
  char *from = "\"User Name\" <username@gmail.com>";
  char *to = "\"Recipient Name\" <recipient@gmail.com>";
  char *subject = "Hello, world!";
  char *body = "This is a test email sent from a simple SMTP client.";
} MailContent
#endif

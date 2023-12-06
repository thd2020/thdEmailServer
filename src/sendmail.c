#include "sendmail.h"

#define SMTP_SERVER "smtp.gmail.com"
#define SMTP_PORT 587

int send_command(int sock, const char *cmd, const char *arg){
    char buffer[1024] = {0};
    sprintf(buffer, "%s%s\r\n", cmd, arg);  // Construct the command
    if(send(sock, buffer, strlen(buffer), 0) < 0){
        return -1;
    }
    return 0;
}

int sendMail(){
    // 对邮件内容进行设置
    const char *from = "\"User Name\" <username@gmail.com>";
    const char *to = "\"Recipient Name\" <recipient@gmail.com>";
    const char *subject = "Hello, world!";
    const char *body = "This is a test email sent from a simple SMTP client.";

    // 建立到服务器的连接
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(SMTP_SERVER);
    server.sin_family = AF_INET;
    server.sin_port = htons(SMTP_PORT);

    // Connect to the server
    if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0){
        perror("connect failed");
        return 1;
    }

    // Interact with the server
    send_command(sock, "HELO ", "myhostname");  // Say hello to the server
    send_command(sock, "MAIL FROM:", from);  // Send MAIL FROM command
    send_command(sock, "RCPT TO:", to);  // Send RCPT TO command
    send_command(sock, "DATA", "");  // Send DATA command
    // Send multiple lines of text
    send(sock, "Subject: ", 9, 0);
    send(sock, subject, strlen(subject), 0);
    send(sock, "\r\n\r\n", 4, 0);
    send(sock, body, strlen(body), 0);
    send(sock, "\r\n.\r\n", 5, 0);
    send_command(sock, "QUIT", "");  // Send QUIT command

    // 关闭套接字
    close(sock);

    return 0;
}

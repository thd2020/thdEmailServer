#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>  
#include <string.h>  
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mysql/mysql.h>

/**mysql username*/
#define mysql_user "ubuntu"
/**mysql password*/
#define mysql_pass "635108"
/**基础路径**/
#define BASE_PATH "/var/thdEmail/users"
/**用户名长度**/
#define USERNAME_LENGTH 30

/**BASE64编码函数**/
unsigned char* base64_encode(unsigned char* str);
/**BASE64解码函数**/
unsigned char* base64_decode(unsigned char* code);
/**初始化数据库链接**/
int init_mysql_con();
/**用户注册**/
int register_user(char* username, char* userpass); 

#endif /* UTILS_H */
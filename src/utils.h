#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>  
#include <string.h>  
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**基础路径**/
#define BASE_PATH "/mailserver/users"
/**用户名长度**/
#define USERNAME_LENGTH 30

/**BASE64编码函数**/
unsigned char *base64_encode(unsigned char *str);
/**BASE64解码函数**/
unsigned char *base64_decode(unsigned char *code);
/**用户注册**/
int register_user(char *username); 

#endif /* REGISTER_H */
#include "utils.h"

/******************
*****BASE64编码****
******************/
unsigned char* base64_encode(unsigned char* str)  
{  
    long len;  
    long str_len;  /**源字符串和目标字符串长度都设为长整型**/
    unsigned char *res;  
    int i,j;  
    // The Base64 Alphabet
    unsigned char *base64_table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";  
  
    /**初始化目标串**/
    str_len = strlen(str);  
    if(str_len % 3 == 0)  
        len = str_len/3 * 4;  /**编码后大小增加1/3**/
    else  
        len = (str_len/3 + 1) * 4;  /**最后一字节补全**/
    res = malloc(sizeof(unsigned char) * len + 1);  /**为目标串分配内存，包括尾字符**/
    res[len] = '\0';  
  
    /**以3个8位字符为一组进行编码**/
    for(i=0, j=0; i<len-2; i+=4, j+=3)
    {  
        res[i] = base64_table[str[j]>>2]; /**取出第一个字符的前6位并找出对应的结果字符**/
        res[i+1] = base64_table[(str[j]&0x3)<<4 | (str[j+1]>>4)]; /**将第一个字符的后2位与第二个字符的前4位进行组合并找到对应的结果字符**/
        res[i+2] = base64_table[(str[j+1]&0xf)<<2 | (str[j+2]>>6)]; //将第二个字符的后4位与第三个字符的前2位组合并找出对应的结果字符  
        res[i+3] = base64_table[str[j+2]&0x3f]; //取出第三个字符的后6位并找出结果字符  
    }  
  
    switch(str_len % 3)  
    {  
        case 1:  
            res[i-2] = '=';  
            res[i-1] = '=';  
            break;  
        case 2:  
            res[i-1] = '=';  
            break;  
    }
    return res;  
} 


/******************
******用户注册******
******************/
int register_user(char *username) {
  /**检查用户名是否为空**/
  if (strlen(username) == 0 || strlen(username) > USERNAME_LENGTH){
    printf("Invalid username!\n");
    return -1;
  }
  /**构建邮箱文件夹路径**/
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", BASE_PATH, username);
  /**创建目录**/
  struct stat st = {0};
  /**文件夹存在性检查**/
  if (stat(path, &st) != -1){
    printf("This user already exists!\n");
    return -1;
  }
  /**尝试创建文件夹**/
  if (mkdir(path, 0700) != 0){
    perror("Error in mkdir");
    return -1;
  }
  printf("User registered and mailbox created successfully!\n");
  return 0;
}

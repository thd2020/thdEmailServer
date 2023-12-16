#include "utils.h"

MYSQL* con = NULL;

/**
 * BASE64编码函数
*/
unsigned char* base64_encode(unsigned char* str)  {  
    /**变量声明*/
    int i, j;
    long len;  /*目标编码长度*/
    long str_len;  /*原始字串长度*/
    unsigned char* res = malloc(BUFSIZ);  /*目标BASE64编码*/
    unsigned char* base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; /* The Base64 Alphabet*/
  
    /**初始化目标编码*/
    str_len = strlen(str);  
    if(str_len % 3 == 0)  
        len = str_len/3*4;  /**编码后大小增加1/3*/
    else  
        len = (str_len/3+1)*4;  /**不足3字节则补全成4字节*/
    res = malloc(sizeof(unsigned char)*len + 1); 
    res[len] = '\0';  /**为目标串分配内存，包括尾字符*/
  
    /**以3个8位字符为一组进行编码*/
    for(i=0, j=0; j<str_len; i+=4, j+=3)
    {  
        res[i] = base64_table[str[j]>>2];
        res[i+1] = (j+1<str_len)?base64_table[(str[j]&0x3)<<4 | (str[j+1]>>4)] : base64_table[(str[j]&0x3)<<4]; 
        res[i+2] = (j+2<str_len)?base64_table[(str[j+1]&0xf)<<2 | (str[j+2]>>6)] : base64_table[(str[j+1]&0xf)<<2]; 
        res[i+3] = (j+2<str_len)?base64_table[str[j+2]&0x3f] : '='; 
    }

    /**依照余数填充等号*/
    switch(str_len % 3)  
    {  
        case 1:  
            res[i-2]='=';  
            res[i-1]='=';  
            break;  
        case 2:  
            res[i-1]='=';  
            break;  
    }  
  
    return res;  
} 

/**
 * BASE64解码函数
 */
unsigned char *base64_decode(unsigned char *code)  {  
    /**变量声明*/
    /*根据base64表，以字符值为数组下标找到对应的十进制数据*/  
    int table[] = {
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,62,0,0,0,
        63,52,53,54,55,56,57,58,
        59,60,61,0,0,0,0,0,0,0,0,
        1,2,3,4,5,6,7,8,9,10,11,12,
        13,14,15,16,17,18,19,20,21,
        22,23,24,25,0,0,0,0,0,0,26,
        27,28,29,30,31,32,33,34,35,
        36,37,38,39,40,41,42,43,44,
        45,46,47,48,49,50,51
    };  
    long len;  /*源码长度*/
    long str_len; /*解码长度*/ 
    unsigned char* res = malloc(BUFSIZ);  /*解码*/
    
    /**根据'='数量初始化解码*/  
    len = strlen(code);  
    if(strstr(code, "=="))  
        str_len = len/4*3-2;  
    else if(strstr(code, "="))  
        str_len = len/4*3-1;  
    else  
        str_len = len/4*3;  
    res=malloc(sizeof(unsigned char)*str_len + 1);  
    res[str_len]='\0';  
  
    /**以4个字符为一组进行解码*/
    for(int i=0, j=0; i<len-2; i+=4, j+=3)  
    {  
        res[j] = ((unsigned char)table[code[i]])<<2 | (((unsigned char)table[code[i+1]])>>4); //取出第一个字符对应base64表的十进制数的前6位与第二个字符对应base64表的十进制数的后2位进行组合  
        res[j+1] = (((unsigned char)table[code[i+1]])<<4) | (((unsigned char)table[code[i+2]])>>2); //取出第二个字符对应base64表的十进制数的后4位与第三个字符对应bas464表的十进制数的后4位进行组合  
        res[j+2] = (((unsigned char)table[code[i+2]])<<6) | ((unsigned char)table[code[i+3]]); //取出第三个字符对应base64表的十进制数的后2位与第4个字符进行组合  
    }  
    return res;  
}

/**
 * 初始化数据库链接
*/
int init_mysql_con(){
    if (con != NULL)
        return 1;
    con = mysql_init(NULL);
	/**connect to mysql*/
	if (!mysql_real_connect(con, "localhost", mysql_user, mysql_pass, "smtp_server", 0, NULL, 0)){
		perror("mysql connection failed");
		mysql_close(con);
		exit(EXIT_FAILURE);
	}
    return 0;
}

/**
 * 用户注册
*/
int register_user(char* username, char* userpass){
    char* query = (char*)malloc(BUFSIZ);
    char* path = malloc(128);
    char* sdpath = malloc(128);
    char* rcpath = malloc(128);
    init_mysql_con();

    /**检查用户名是否为空**/
    if (strlen(username) == 0 || strlen(username) > USERNAME_LEN){
        printf("Invalid username!\n");
        return -1;
    }
    /**构建邮箱文件夹路径**/
    sprintf(path, "%s/%s", BASE_PATH, username);
    sprintf(sdpath, "%s/%s/sentmails", BASE_PATH, username);
    sprintf(rcpath, "%s/%s/rcmails", BASE_PATH, username);
    struct stat st = {0};
    if (stat(path, &st) != -1){
        printf("This user already exists!\n");
        return -1;
    }
    sprintf(query, "INSERT INTO `users` (`username`, `password`, `path`) VALUES ('%s', '%s', '%s')", username, (char*)base64_encode(userpass), path);
    if (mysql_query(con, query)){
        syslog(LOG_WARNING, "parsing sql: %s failed", query);
        return -1;
    }
    /**创建目录**/
    if (stat("/var/thdEmail", &st) == -1){
        mkdir("/var/thdEmail", S_IRWXG);
    }
    if (stat(BASE_PATH, &st) == -1){
        mkdir(BASE_PATH, S_IRWXG);
    }
    /**尝试创建文件夹**/
    if (mkdir(path, S_IRWXU) != 0){
        printf("Error in mkdir %s", path);
        syslog(LOG_WARNING, "Error in mkdir %s", path);
        return -1;
    }
    if (mkdir(sdpath, S_IRWXU) != 0){
        printf("Error in mkdir %s", path);
        syslog(LOG_WARNING, "Error in mkdir %s", path);
        return -1;
    }
    if (mkdir(rcpath, S_IRWXU) != 0){
        printf("Error in mkdir %s", path);
        syslog(LOG_WARNING, "Error in mkdir %s", path);
        return -1;
    }
    printf("User registered and mailbox created successfully!\n");
    return 0;
}

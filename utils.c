#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
＃include "utils.h"
  
int register_user(char *username) {
  // 检查用户名是否为空
  if (strlen(username) == 0 || strlen(username) > USERNAME_LENGTH){
    printf("Invalid username!\n");
    return -1;
  }
  
  // 构建邮箱文件夹路径
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", BASE_PATH, username);

  // 创建目录
  struct stat st = {0};

  // 文件夹存在性检查
  if (stat(path, &st) != -1){
    printf("This user already exists!\n");
    return -1;
  }

  // 尝试创建文件夹
  if (mkdir(path, 0700) != 0){
    perror("Error in mkdir");
    return -1;
  }

  printf("User registered and mailbox created successfully!\n");
  return 0;
}

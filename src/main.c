#include "utils.h"
#include <stdio.h>

int main(char** argv, int argc){
    unsigned char* origin = malloc(sizeof(unsigned char));
    while(1){
        printf("please input string to convert: \n");
        gets(origin);
        unsigned char* encode = base64_encode(origin);
        printf("your base64 code is:\n %s\n", encode);
    }
    return(1);
}
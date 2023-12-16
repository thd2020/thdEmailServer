#include "utils.h"
#include "smtp_clt.h"
#include "smtp_mx.h"

#define MAXBUFF 100

int main(int argc, char** argv){
	if (!strcmp(argv[1], "--create-user")){
        register_user(argv[2], argv[3]);
    }
    if (!strcmp(argv[1],  "--start-smtp-clt") || !strcmp(argv[1],  "-sc")){
        start_smtp_clt(argv[0]);
    }
    if (!strcmp(argv[1],  "--start-smtp-mx") || !strcmp(argv[1],  "-sm")){
        start_smtp_mx(argv[0]);
    }
    if (!strcmp(argv[1], "--base64-test") || !strcmp(argv[1],  "-bt")){
        unsigned char* origin = malloc(MAXBUFF * sizeof(unsigned char));
        int* option = malloc(sizeof(int));
        bool quit = false;

        while(1){
            if (quit == true){
                printf("Bye\n");
                break;
            }
            printf("0 - base64 encoding\n1 - base64 decoding\n2 - exit\n");
            scanf("%d", option);
            int c;
            do{
                c = getchar();
            }while(c != EOF && c != '\n');
            switch(*option){
                case 0:
                    printf("please input string to encode: \n");
                    fgets(origin, MAXBUFF, stdin);
                    origin[strlen(origin) - 1] = '\0';
                    unsigned char* encode = base64_encode(origin);
                    printf("your base64 code is:\n%s\n", encode);
                    continue;
                case 1:
                    printf("please input code to decode: \n");
                    fgets(origin, MAXBUFF, stdin);
                    origin[strlen(origin) - 1] = '\0';
                    unsigned char* decode = base64_decode(origin);
                    printf("your original string is:\n%s\n", decode);
                    continue;
                case 2:
                    quit = true;
                    break;
            }
        }
    }
    return 0;
}
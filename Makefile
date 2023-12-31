CC = gcc -g -Wall -lpthread 
SOURCE = src/utils.c obj/smtp_clt.c src/smtp_mx.c src/pop3.c src/main.c
INC =  src/utils.h
OBJS = obj/utils.o obj/smtp_clt.o obj/smtp_mx.o obj/pop3.o obj/main.o
EXEC = bin/thdEmail

${EXEC} : ${OBJS}
	${CC} -o ${EXEC} ${OBJS} -L/usr/lib/aarch64-linux-gnu -lmysqlclient -lzstd -lssl -lcrypto -lresolv -lm
obj/utils.o : src/utils.c src/utils.h
	${CC} -c src/utils.c -o obj/utils.o
obj/smtp_clt.o : src/smtp_clt.c src/smtp_clt.h
	${CC} -c src/smtp_clt.c -o obj/smtp_clt.o
obj/smtp_mx.o : src/smtp_mx.c src/smtp_mx.h
	${CC} -c src/smtp_mx.c -o obj/smtp_mx.o
obj/pop3.o : src/pop3.c src/pop3.h
	${CC} -c src/pop3.c -o obj/pop3.o
obj/main.o : src/main.c src/utils.h src/smtp_clt.h src/smtp_mx.h src/pop3.h
	${CC} -c src/main.c -o obj/main.o

clean:
	rm -rf ${EXEC} ${OBJS} 
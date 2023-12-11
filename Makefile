CC = gcc -g -Wall -lpthread
SOURCE = src/utils.c obj/smtp_clt.c src/main.c
INC =  src/utils.h
OBJS = obj/utils.o obj/smtp_clt.o obj/main.o
EXEC = bin/smtp_clt

${EXEC} : ${OBJS}
	${CC} -o ${EXEC} ${OBJS}
obj/utils.o : src/utils.c src/utils.h
	${CC} -c src/utils.c -o obj/utils.o
obj/smtp_clt.o : src/smtp_clt.c src/smtp_clt.h
	${CC} -c src/smtp_clt.c -o obj/smtp_clt.o
obj/main.o : src/main.c src/utils.h src/smtp_clt.h
	${CC} -c src/main.c -o obj/main.o

clean:
	rm -rf ${EXEC} ${OBJS} 
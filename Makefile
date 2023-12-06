CC = gcc -g -Wall
SOURCE = src/utils.c src/main.c
INC =  src/utils.h
OBJS = obj/utils.o obj/main.o
EXEC = bin/base64_coding

${EXEC} : ${OBJS}
	${CC} -o ${EXEC} ${OBJS}
obj/utils.o : src/utils.c src/utils.h
	${CC} -c src/utils.c -o obj/utils.o
obj/main.o : src/main.c src/utils.h
	${CC} -c src/main.c -o obj/main.o

clean:
	rm -rf ${EXEC} ${OBJS} 
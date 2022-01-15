CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall

error: error.c error.h
	${CC} -c error.c -o error.o

master: master.c info.h
	${CC} -c ${CFLAGS} master.c -o master.o
	${CC} master.o error.o -o master.out

node: node.c info.h
	${CC} -c ${CFLAGS} node.c -o node.o
	${CC} node.o error.o -o node.out

user: user.c info.h
	${CC} -c ${CFLAGS} user.c -o user.o
	${CC} user.o error.o -o user.out

rm:
	rm *.o
	rm *.out
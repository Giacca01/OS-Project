CC=gcc
CFLAGS=-std=c89 -pedantic-errors -Wall
ESSENTIALS=-DESSENTIALS_PRINTS=0

all: error.o master.o node.o user.o
allDebug: error masterDebug nodeDebug userDebug
allEssentialsPrints: error masterEssentialsPrints nodeEssentialsPrints userEssentialsPrints

error.o: error.c error.h
	${CC} -c ${CFLAGS} error.c -o error.o

master.o: master.c info.h
	${CC} -c ${CFLAGS} master.c -o master.o
	${CC} master.o error.o -o master.out

masterEssentialsPrints: master.c info.h
	${CC} -c ${CFLAGS} ${ESSENTIALS} master.c -o master.o
	${CC} master.o error.o -o master.out

masterDebug: master.c info.h
	gcc -g -o0  master.c error.o -o master.out

node.o: node.c info.h
	${CC} -c ${CFLAGS} node.c -o node.o
	${CC} node.o error.o -o node.out

nodeEssentialsPrints: node.c info.h
	${CC} -c ${CFLAGS} ${ESSENTIALS} node.c -o node.o
	${CC} node.o error.o -o node.out

nodeDebug: node.c info.h
	gcc -g -o0  node.c error.o -o node.out

user.o: user.c info.h
	${CC} -c ${CFLAGS} user.c -o user.o
	${CC} user.o error.o -o user.out

userEssentialsPrints: user.c info.h
	${CC} -c ${CFLAGS} ${ESSENTIALS} user.c -o user.o
	${CC} user.o error.o -o user.out

userDebug: user.c info.h
	gcc -g -o0  user.c error.o -o user.out

# used to remove compiled files #
rm:
	rm -f *.o
	rm -f *.out

run: all
	./master.out

runDebug: allDebug
	./master.out

runEssentialsPrints: allEssentialsPrints
	./master.out
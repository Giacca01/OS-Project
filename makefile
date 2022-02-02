CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall

all: rm error master node user
allDebug: rm error masterDebug nodeDebug userDebug

error: error.c error.h
	${CC} -c error.c -o error.o

master: master.c info.h
	${CC} -c ${CFLAGS} master.c -o master.o
	${CC} master.o error.o -o master.out

masterDebug: master.c info.h
	gcc -g -o0  master.c error.o -o master.out

node: node.c info.h
	${CC} -c ${CFLAGS} node.c -o node.o
	${CC} node.o error.o -o node.out

nodeDebug: node.c info.h
	gcc -g -o0  node.c error.o -o node.out

user: user.c info.h
	${CC} -c ${CFLAGS} user.c -o user.o
	${CC} user.o error.o -o user.out

userDebug: user.c info.h
	gcc -g -o0  user.c error.o -o user.out

rm:
	rm -f *.o
	rm -f *.out
	rm -f node_creation_report.txt
	rm -f master_msgrcv_content.txt
	rm -f IPC_remover/IPC_resources.txt
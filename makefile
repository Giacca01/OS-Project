CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall

# mettere una regola che compili tutto
master: master.c info.h
	${CC} ${CFLAGS}

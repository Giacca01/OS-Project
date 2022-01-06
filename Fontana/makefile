CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall

# SISTEMARE
# mettere una regola che compili tutto
user: user.c info.h
	${CC} -c error.c -o error.o
	${CC} ${CFLAGS} user.c error.o -o user
	./user
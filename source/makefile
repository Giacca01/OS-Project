CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall

# SISTEMARE
# mettere una regola che compili tutto
master: master.c info.h
	cp Error/error.o ./error.o
	${CC} -c ${CFLAGS} master.c -o master.o
	${CC} master.o error.o

rm:
	rm *.o
	rm *.out
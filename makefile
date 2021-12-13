CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall


error: error.c error.h
	${CC} ${CFLAGS} 
# ???
# mettere una regola che compili tutto
master: master.c info.h
	${CC} ${CFLAGS}

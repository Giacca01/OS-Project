CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall

error: error.h
	gcc -c error.c -o error.o
node: node.c node.h
	gcc node.c error.o -o node

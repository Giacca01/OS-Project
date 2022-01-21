/*
    Error Module Header
*/
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void unsafeErrorPrint(char *msg, int line);
void safeErrorPrint(char *msg, int line);
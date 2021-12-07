#define _GNU_SOURCE
#include <time.h>
#include <stdio.h>

int main(){
	struct timespec timestamp;
	
	clock_gettime(CLOCK_MONOTONIC, &timestamp);
	
	// il vero tempo si ottiene concatenando secondi e nanosecondi
	printf("Nanosecondi: %ld\n", timestamp.tv_nsec);
}

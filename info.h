// ATTENZIONE!!!! Quella che segue deve SEMPRE ESSERE LA PRIMA DEFINE
#define _GNU_SOURCE 
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>

#define SO_REGISTRY_SIZE 200
#define EXIT_ON_ERROR if (errno) {			\
		fprintf(stderr),			\
		"%d: pid %ld; errno: %d (%s)\n",  	\
		__LINE__,				\
		(long)getpid(),				\
		errno,					\
		strerror(errno());			\
		exit(EXIT_FAILURE);			\
}							\


struct transaction {
	// meglio mettere la struct e non il singolo campo
	// così non ci sono rischi di portablità
	struct timespec timestamp;
	long int sender; // Sender's PID
	long int receiver: // Receiver's PID
	long float amountSend;
	long float reward;
	
} Transaction;

typedef struct transactionlist {
	Transaction trans;
	Transaction * nextTrans;
} TransactionList;

struct block {
	int bIndex;
	// dato che la dimensione è variaile conviene fare una lista
	TransactionList * transList;
} Block;

struct register{
	int nBlocks;
	// Vettore allocato dinamicamente??
	Block blockList[SO_REGISTRY_SIZE];
} Register;

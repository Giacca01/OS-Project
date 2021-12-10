// ATTENZIONE!!!! Quella che segue deve SEMPRE ESSERE LA PRIMA DEFINE
#define _GNU_SOURCE 
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

#define SO_REGISTRY_SIZE 200
// Right now it's not reentrant, we should modify it
#define EXIT_ON_ERROR if (errno) {			\
		fprintf(stderr),			\
		"%d: pid %ld; errno: %d (%s)\n",  	\
		__LINE__,				\
		(long)getpid(),				\
		errno,					\
		strerror(errno());			\
		exit(EXIT_FAILURE);			\
}							\

// sviluppare meglio: come affrontare il caso in cui SO_REGISTRY_SIZE % 3 != 0
#define REG_PARTITION_SIZE (SO_REGISTRY_SIZE / 3) // Modificato
#define REWARD_TRANSACTION -1 // Modificato
#define INIT_TRANSACTION -1 // Modificato
#define REG_PARTITION_COUNT 3 // Modificato

typedef enum{TERMINATED = 0, ACTIVE} States; // Modificato

// Nella documentazione fare disegno di sta roba
typedef struct {
	// meglio mettere la struct e non il singolo campo
	// così non ci sono rischi di portablità
	struct timespec timestamp;
	long int sender; // Sender's PID
	long int receiver; // Receiver's PID
	float amountSend; // double???
	float reward;
	
} Transaction;

typedef struct {
	// puntatore per evitare troppe copie di struct per valore
	Transaction * trans; // Modificato
	Transaction * nextTrans;
} TransactionList;

typedef struct {
	int bIndex;
	// dato che la dimensione è variaile conviene fare una lista
	TransactionList * transList;
} Block;

typedef struct{
	int nBlocks;
	// Vettore allocato dinamicamente?? Potrebbe semplificare il caso in cui
	// SO_REGISTRY_SIZE % 3 != 0
	// vettore di puntatori, altrimenti si fanno troppe copie di struct per valore
	Block * blockList[REG_PARTITION_SIZE]; // modficato
} Register;

typedef struct procList {
	long int procId;
	States procState; // Modificato
	procList * next;
} ProcessesList;

typedef struct tplist {
	long int procId;
	int msgId;
	tplist * next;
} TransactionPoolList;

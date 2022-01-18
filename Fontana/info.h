/* ATTENZIONE!!!! Quella che segue deve SEMPRE ESSERE LA PRIMA DEFINE*/
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
/*#include <sys/ipc.h> VEDERE SE SERVA*/
#include "error.h"
#define SEMFILEPATH "../semfile.txt"
#define FAIRSTARTSEED 1
#define WRPARTSEED 2
#define RDPARTSEED 3
#define USERLISTSEED 4
#define PARTMUTEXSEED 5

#define SHMFILEPATH "../shmfile.txt"
#define REGPARTONESEED 1
#define REGPARTTWOSEED 2
#define REGPARTTHREESEED 3
#define USERSLISTSEED 4
#define NODESLISTSEED 5
#define NOREADERSONESEED 6
#define NOREADERSTWOSEED 7
#define NOREADERSTHREESEED 8
#define NOUSRSEGRDERSSEED 9

#define MSGFILEPATH "../msgfile.txt"
#define GLOBALMSGSEED 1
/* il seed è il pid del proprietario
i figli lo preleveranno dalla lista dei nodi*/

#define SO_REGISTRY_SIZE 200
/*Right now it's not reentrant, we should modify it*/
#define EXIT_ON_ERROR                        \
    if (errno)                               \
    {                                        \
        fprintf(stderr),                     \
            "%d: pid %ld; errno: %d (%s)\n", \
            __LINE__,                        \
            (long)getpid(),                  \
            errno,                           \
            strerror(errno());               \
        exit(EXIT_FAILURE);                  \
    }

#define FTOK_TEST_ERROR(key) \
    if (key == -1){          \
        unsafeErrorPrint("Node: ftok failed during semaphores creation. Error: ");\
        return FALSE; \
    }

#define SEM_TEST_ERROR(id) \
    if (id == -1) {         \
        unsafeErrorPrint("Node: semget failed during semaphore creation. Error: ");\
        return FALSE;\
    }

#define SHM_TEST_ERROR(id) \
    if (id == -1){          \
        unsafeErrorPrint("Nde: shmget failed during shared memory segment creation. Error: ");\
        return FALSE;\
    }

#define MSG_TEST_ERROR(id)                                                                 \
    if (id == -1){                                                                          \
        unsafeErrorPrint("Master: msgget failed during messages queue creation. Error: ");\
        return FALSE;\
    }

#define TEST_MALLOC_ERROR(ptr)                                        \
    if (ptr == NULL) {                                               \
        unsafeErrorPrint("User: failed to allocate memory. Error: "); \
        return FALSE;                                                   \
    }

#define TEST_SHMAT_ERROR(ptr)                                        \
    if (ptr == NULL)                                                                   \
    {                                                                                 \
        unsafeErrorPrint("User: failed to attach to shared memory segment. Error: "); \
        return FALSE;                                                 \
    }

/* sviluppare meglio: come affrontare il caso in cui SO_REGISTRY_SIZE % 3 != 0*/
#define REG_PARTITION_SIZE (SO_REGISTRY_SIZE / 3) 
#define REWARD_TRANSACTION -1                     
#define INIT_TRANSACTION -1                       
#define REG_PARTITION_COUNT 3                     
#define SO_BLOCK_SIZE 10                          /* Modificato 10/12/2021*/
                          /* Modificato 10/12/2021*/
#define CONF_MAX_LINE_SIZE 128 /* Configuration file's line maximum bytes length*/
#define CONF_MAX_LINE_NO 14 /* Configuration file's maximum lines count*/

typedef enum
{
    TERMINATED = 0,
    ACTIVE
} States; 

/* Nella documentazione fare disegno di sta roba*/
typedef struct
{
    /* meglio mettere la struct e non il singolo campo
    così non ci sono rischi di portablità*/
    struct timespec timestamp;
    long int sender;    /* Sender's PID*/
    long int receiver; /* Receiver's PID*/
    float amountSend;
    float reward;

} Transaction;

/* Each block is a list of transactions*/
typedef struct /* Modificato 10/12/2021*/
{
    int bIndex;
    /* SO_BLOCK_SIZE è da leggere da file???*/
    Transaction transList[SO_BLOCK_SIZE];
} Block;

/* Serve per memorizzare le transazioni, NON i puntatori ai segmenti*/
/* Each register is a group of blocks*/
typedef struct /* Modificato 10/12/2021*/
{
    int nBlocks;
    /* Vettore allocato dinamicamente?? Potrebbe semplificare il caso in cui
     SO_REGISTRY_SIZE % 3 != 0
     NOPE, non si potrebbe condividere*/
    Block blockList[REG_PARTITION_SIZE]; /* modficato*/
} Register;

typedef struct /* Modificato 10/12/2021*/
{
    long int procId;
    States procState;  /* dA ignorare se il processo è un nodo*/
} ProcListElem;

/* Il singolo elemento della lista degli amici è una coppia
 -(PID del processo a cui è riferita, lista di pid e stato degli amici)*/
/*
typedef struct { // Modificato 10/12/2021
    long int ownerId;
    ProcListElem friends;
} FriendsList;*/

typedef struct /* Modificato 10/12/2021*/
{
    long int procId; /* è necessario??? NO, a nessuno serve l'ID del nodo*/
    int msgQId;
} TPElement;

/*
    NEWNODE: message sent to master from node 
            to request the creation of a new node to serve a transaction
    
    NEWFRIEND: message sent to node from master to order the latter
                to add a new process to its friends
    FAILEDTRANS: message sent to user from node to inform it that
    the attached transaction has failed (this is used in case
    the receiver was a terminated user)

    FRIENDINIT: massage sent to user from master to initialize its friends list
    
    TRANSTPFULL: message sent to user (or node) to node to inform it that a transaction
    must be served either by requesting the creation of new node or by dispatching it to a friend
*/
typedef enum
{
    NEWNODE = 0,
    NEWFRIEND,
    FAILEDTRANS,
    FRIENDINIT,
    TRANSTPFULL
} GlobalMsgContent;

/* attenzione!!!! Per friends va fatta una memcopy
 non si può allocare staticamente perchè la sua dimensione è letta a runtime */
typedef struct
{
    long int mType; /* Pid of the receiver, taken with getppid (children) or from dedicated arrays (parent) */
    GlobalMsgContent msgContent;
    /* ProcListElem * friends;  /* garbage if msgcontent == NEWNODE || msgcontent == FAILEDTRANS */
    /*
     * Abbiamo rimosso la definizione precedente in quanto friends non può essere allocato staticamente,
     * quindi era necessario definirlo dinamicamente prima di inviare il messaggio sulla coda; questo
     * però comporta che quando il ricevitore (processo diverso da quello che ha mandato il messaggio) 
     * prova ad accedere a tale array, si verifica un errore di segmentazione perché il processo cerca
     * di accedere ad una zona di memoria che non è la sua ma è di un altro processo. Per questo motivo,
     * dopo aver valutato attentamente le diverse opzioni per risolvere questo problema, si è deciso di non 
     * mandare la lista di nodi amici tramite un singolo messaggio ma di mandarla tramite più messaggi 
     * sulla coda globale e starà quindi al nodo destinatario leggere i messaggi dalla coda e creare la 
     * sua lista di nodi amici.
     */
    ProcListElem friend;        /* garbage if msgContent == NEWNODE || msgContent == FAILEDTRANS || msgContent == TRANSTPFULL  */
    Transaction transaction;    /* garbage if msgContent == NEWFRIEND || msgContent == FRIENDINIT */
    long hops;                  /* garbage if msgContent == NEWNODE || msgContent == NEWFRIEND || msgContent == FRIENDINIT || msgContent == FAILEDTRANS */
} MsgGlobalQueue;

typedef enum
{
    FALSE = 0, 
    TRUE
} boolean;

/*
 * Fields:
 * mType = pid of node which the transaction is sent
 * transaction = transaction sent to the node
 */
typedef struct
{
    long int mType; /* pid of node - not that important, the transaction pool is private to the node */
    Transaction transaction;
} MsgTP;

typedef struct
{
    long mtype; /* type of message */
    pid_t pid;  /* user-define message */
} msgbuff;
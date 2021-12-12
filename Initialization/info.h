// ATTENZIONE!!!! Quella che segue deve SEMPRE ESSERE LA PRIMA DEFINE
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define SO_REGISTRY_SIZE 200
// Right now it's not reentrant, we should modify it
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

// sviluppare meglio: come affrontare il caso in cui SO_REGISTRY_SIZE % 3 != 0
#define REG_PARTITION_SIZE (SO_REGISTRY_SIZE / 3)
#define REWARD_TRANSACTION -1
#define INIT_TRANSACTION -1
#define REG_PARTITION_COUNT 3
#define SO_BLOCK_SIZE 10 // Modificato 10/12/2021
//#define SO_USER_NUM 10   // Modificato 10/12/2021
//#define SO_NODES_NUM 7   // Modificato 10/12/2021

typedef enum
{
    TERMINATED = 0,
    ACTIVE
} States;

// Nella documentazione fare disegno di sta roba
typedef struct
{
    // meglio mettere la struct e non il singolo campo
    // così non ci sono rischi di portablità
    struct timespec timestamp;
    long int sender;   // Sender's PID
    long int receiver; // Receiver's PID
    float amountSend;
    float reward;

} Transaction;

// Each block is a list of transactions
typedef struct // Modificato 10/12/2021
{
    int bIndex;
    // SO_BLOCK_SIZE è da leggere da file???
    Transaction transList[SO_BLOCK_SIZE];
} Block;

// Serve per memorizzare le transazioni, NON i puntatori ai segmenti
// Each register is a group of blocks
typedef struct // Modificato 10/12/2021
{
    int nBlocks;
    // Vettore allocato dinamicamente?? Potrebbe semplificare il caso in cui
    // SO_REGISTRY_SIZE % 3 != 0
    // NOPE, non si potrebbe condividere
    Block blockList[REG_PARTITION_SIZE]; // modficato
} Register;

typedef struct // Modificato 10/12/2021
{
    long int procId;
    States procState; // dA ignorare se il processo è un nodo
} ProcListElem;

// Il singolo elemento della lista degli amici è una coppia
// -(PID del processo a cui è riferita, lista di pid e stato degli amici)
/*
typedef struct { // Modificato 10/12/2021
    long int ownerId;
    ProcListElem friends;
} FriendsList;*/

typedef struct // Modificato 10/12/2021
{
    long int procId; // è necessario??? NO, a nessuno serve l'ID del nodo
    int msgQId;
} TPElement;

/*
    NEWNODE: message sent to master from node 
            to request the creation of a new node to serve a transaction
    
    NEWFRIEND: message sent to node from master to order the latter
                to add a new process to its friends
*/
typedef enum
{
    NEWNODE = 0,
    NEWFRIEND = 1
} GlobalMsgContent;

typedef struct
{
    long int mType; // Pid of the receiver, taken with getppid (children) or from dedicated arrays (parent)
    GlobalMsgContent msgContent;
    ProcListElem newFriend;  // garbage if msgcontent == NEWNODE
    Transaction transaction; // garbage if msgContent == NEWFRIEND
} MsgGlobalQueue;
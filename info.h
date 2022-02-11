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
#include "error.h"

#define SO_REGISTRY_SIZE 10000                                                                  /* Maximum number of blocks in the master */
#define SO_BLOCK_SIZE 20                                                                        /* Number of transactions contained in a block */
#define MAX_ADDITIONAL_NODES 100                                                                /* Maximum number of additional nodes created on request */
#define REG_PARTITION_COUNT 3                                                                   /* Number of partitions into which the master is divided */
#define CONF_MAX_LINE_SIZE 128                                                                  /* Configuration file's line maximum bytes length*/
#define CONF_MAX_LINE_NO 14                                                                     /* Configuration file's maximum lines count*/
#define REG_PARTITION_SIZE ((SO_REGISTRY_SIZE + REG_PARTITION_COUNT - 1) / REG_PARTITION_COUNT) /* Size of master single partition */
#define MASTERPERMITS 0600                                                                      /* Permits for master */

#define IPCREMOVERFILEPATH "IPC_remover/IPC_resources.txt"

/* Constant for creating keys with ftok for semaphores */
#define SEMFILEPATH "semfile.txt"
#define FAIRSTARTSEED 1
#define WRPARTSEED 2
#define RDPARTSEED 3
#define USERLISTSEED 4
#define PARTMUTEXSEED 5
#define NOALLTIMESNODESSEMSEED 6

/* Constant for creating keys with ftok for shared memory segments */
#define SHMFILEPATH "shmfile.txt"
#define REGPARTONESEED 6
#define REGPARTTWOSEED 7
#define REGPARTTHREESEED 8
#define USERSLISTSEED 9
#define NODESLISTSEED 10
#define NOREADERSONESEED 11
#define NOREADERSTWOSEED 12
#define NOREADERSTHREESEED 13
#define NOUSRSEGRDERSSEED 14
#define NONODESEGRDERSSEED 15
#define NOALLTIMESNODESSEED 16

/* Constant for creating keys with ftok for messages queue */
#define MSGFILEPATH "msgfile.txt"
#define PROC_QUEUE_SEED 16
#define NODE_CREATION_QUEUE_SEED 17
#define TRANS_QUEUE_SEED 18

/*** Macros to detect errors ***/
/**
 * @brief If ftok returns -1 it returns error.
 */
#define FTOK_TEST_ERROR(key, msg)        \
    if (key == -1)                       \
    {                                    \
        unsafeErrorPrint(msg, __LINE__); \
        return FALSE;                    \
    }

/**
 * @brief If semget returns -1 it returns error.
 */
#define SEM_TEST_ERROR(id, msg)          \
    if (id == -1)                        \
    {                                    \
        unsafeErrorPrint(msg, __LINE__); \
        return FALSE;                    \
    }

/**
 * @brief If semctl returns -1 it returns error.
 */
#define SEMCTL_TEST_ERROR(id, msg)       \
    if (id == -1)                        \
    {                                    \
        unsafeErrorPrint(msg, __LINE__); \
        return FALSE;                    \
    }

/**
 * @brief If shmget returns -1 it returns error.
 */
#define SHM_TEST_ERROR(id, msg)          \
    if (id == -1)                        \
    {                                    \
        unsafeErrorPrint(msg, __LINE__); \
        return FALSE;                    \
    }

/**
 * @brief If msgget returns -1 it returns error.
 */
#define MSG_TEST_ERROR(id, msg)          \
    if (id == -1)                        \
    {                                    \
        unsafeErrorPrint(msg, __LINE__); \
        return FALSE;                    \
    }

/**
 * @brief If malloc returns NULL it returns error.
 */
#define TEST_MALLOC_ERROR(ptr, msg)      \
    if (ptr == NULL)                     \
    {                                    \
        unsafeErrorPrint(msg, __LINE__); \
        return FALSE;                    \
    }

/**
 * @brief If shmat returns NULL it returns error.
 */
#define TEST_SHMAT_ERROR(ptr, msg)       \
    if (ptr == NULL)                     \
    {                                    \
        unsafeErrorPrint(msg, __LINE__); \
        return FALSE;                    \
    }

/**
 * @brief if reading the configuration parameters from the environment
 *  variables causes an error, we report it.
 */
#define TEST_ERROR_PARAM                                                                       \
    if (errno)                                                                                 \
    {                                                                                          \
        unsafeErrorPrint("Master: failed to read configuration parameter. Error: ", __LINE__); \
        return FALSE;                                                                          \
    }
/***************************/

/*** Constants and macros define to perform, if desired, ***/
/***     only the printing of essential information      ***/
/*
 * Constant used in NOT_ESSENTIAL_PRINT, if during compiling its value it's specified,
 * we use it in the macro (1 = print all, 0 = only essential prints). Otherwise we
 * define it here.
 */
#ifndef ESSENTIALS_PRINTS
#define ESSENTIALS_PRINTS 1
#endif

/* Macro used to hide not essentials prints during execution for a cleaner output */
#define NOT_ESSENTIAL_PRINT(instruction) \
    if (ESSENTIALS_PRINTS)               \
    {                                    \
        instruction                      \
    }
/*************************************************************/

/*** Definition of the types used in the project ***/
/*
 * By using this new datatype we're able
 * to distinguish between a "normal" node, that
 * must wait for the simulation to start, and
 * an "additional one", that doesn't have to, because
 * it's created when the simulation has already started
 */
typedef enum
{
    NORMAL = 0,
    ADDITIONAL = 1
} NodeType;

/* It allows us to know the status of the processes: whether active or terminated */
typedef enum
{
    TERMINATED = 0,
    ACTIVE
} States;

/* Definition of the transaction, consisting of all the required fields */
typedef struct
{
    struct timespec timestamp;
    long int sender;   /* Sender's PID*/
    long int receiver; /* Receiver's PID*/
    float amountSend;  /* Amount of money sent */
    float reward;      /* Money paid by the sender to the node that processes the transaction */

} Transaction;

/* Definition of a block which is a list of transactions*/
typedef struct /* Modificato 10/12/2021*/
{
    int bIndex;
    Transaction transList[SO_BLOCK_SIZE]; /* Transactions list */
} Block;

/* Definition of the register that is a group of blocks */
/* It is for storing transactions, NOT pointers to segments */
typedef struct
{
    int nBlocks;
    Block blockList[REG_PARTITION_SIZE]; /* Blocks list */
} Register;

/*
 * It allows us to keep track of all the processes
 * that are part of the simulation
 */
typedef struct
{
    long int procId;  /* Process' id */
    States procState; /* Process' state */
} ProcListElem;

/*
 * It allows us to keep track of the
 * transaction pools of the nodes
 */
typedef struct
{
    long int procId; /* Node's id */
    int msgQId;      /* Id of the message queue that performs the transaction pool function */
} TPElement;

/*
    It allows us to define the type of message sent on global message queues:

    -NEWNODE: message sent to master from node
        to request the creation of a new node to serve a transaction

    -NEWFRIEND: message sent to node from master to order the latter
        to add a new process to its friends

    -FAILEDTRANS: message sent to user from node to inform it that
        the attached transaction has failed (this is used in case
        the receiver was a terminated user)

    -FRIENDINIT: massage sent to user from master to initialize its friends list

    -TRANSTPFULL: message sent to user (or node) to node to inform it that a transaction
        must be served either by requesting the creation of new node or by dispatching it to a friend

    -TERMINATEDUSER: message sent from user when it terminates its execution

    -TERMINATEDNODE: message sent from node when it terminates its execution
*/
typedef enum
{
    NEWNODE = 0,
    NEWFRIEND,
    FAILEDTRANS,
    FRIENDINIT,
    TRANSTPFULL,
    TERMINATEDUSER,
    TERMINATEDNODE
} GlobalMsgContent;

/*
 * Structure of the message sent on the message
 * queue used for the creation of new nodes or friends
 */
typedef struct
{
    long int mtype;              /* pid of node which the transaction is sent */
    GlobalMsgContent msgContent; /* NEWNODE or NEWFRIEND*/
    Transaction transaction;
    pid_t procPid;
} NodeCreationQueue;

/*
 * Structure of the message sent on the global
 * queue for terminated users and friends initialization
 */
typedef struct
{
    long int mtype;              /* pid of node which the transaction is sent */
    GlobalMsgContent msgContent; /* FRIENDINIT or TERMINATEDUSER or TERMINATEDNODE*/
    pid_t procPid;
} ProcQueue;

/*
 * Structure of the message sent on the global
 * queue for spare and failed transactions
 */
typedef struct
{
    long int mtype;              /* pid of node which the transaction is sent */
    GlobalMsgContent msgContent; /* FAILEDTRANS or TRANSTPFULL*/
    Transaction transaction;
    long hops;
} TransQueue;

/*
 * Structure of messages sent on the message
 * queue which acts as a transaction pool
 */
typedef struct msgtp
{
    long int mtype;          /* pid of node which the transaction is sent */
    Transaction transaction; /* transaction sent to the node */
} MsgTP;

/* Definition of boolean type */
typedef enum
{
    FALSE = 0,
    TRUE
} boolean;

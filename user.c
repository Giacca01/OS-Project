#include "user.h"

/*** GLOBAL VARIABLES FOR IPC ***/
#pragma region GLOBAL VARIABLES FOR IPC
/* Poiter to the array that contains the ids of the shared memory segments of the register's partitions.
 * regPartsIds[0]: id of the first partition segment
 * regPartsIds[1]: id of the second partition segment
 * regPartsIds[2]: id of the third partition segment
 */
int regPartsIds[REG_PARTITION_COUNT] = {-1, -1, -1};

/* Pointer to the array that contains the pointers to the the register's partitions.
 * regPtrs[0]: pointer to the first partition segment
 * regPtrs[1]: pointer to the second partition segment
 * regPtrs[2]: pointer to the third partition segment
 */
Register *regPtrs[REG_PARTITION_COUNT] = {NULL, NULL, NULL};

/* Id of the shared memory segment that contains the users list */
int usersListId = -1;

/* Pointer to the users list */
ProcListElem *usersList = NULL;

/* Id of the shared memory segment that contains the nodes list */
int nodesListId = -1;

/* Pointer to the nodes list */
ProcListElem *nodesList = NULL;

/* Id of the global message queue where users, nodes and master communicate */
int procQueue = -1;
int transQueue = -1;

/* Id of the set that contains the three semaphores used to write on the register's partitions */
int fairStartSem = -1;

/* Id of the set that contains the three semaphores used to write on the register's partitions */
int wrPartSem = -1;

/* Id of the set that contains the three semaphores used to read from the register's partitions */
int rdPartSem = -1;

/* Id of the set that contains the three sempagores used to access the number of readers
 * variables of the registers partitions in mutual exclusion
 */
int mutexPartSem = -1;

/* Pointer to the array containing the ids of the shared memory segments where the variables used to syncronize
 * readers and writers access to register's partition are stored.
 * noReadersPartitions[0]: id of first partition's shared variable
 * noReadersPartitions[1]: id of second partition's shared variable
 * noReadersPartitions[2]: id of third partition's shared variable
 */
int noReadersPartitions[REG_PARTITION_COUNT] = {-1, -1, -1};

/* Pointer to the array containing the variables used to syncronize readers and writers access to register's partition.
 * noReadersPartitionsPtrs[0]: pointer to the first partition's shared variable
 * noReadersPartitionsPtrs[1]: pointer to the second partition's shared variable
 * noReadersPartitionsPtrs[2]: pointer to the third partition's shared variable
 */
int *noReadersPartitionsPtrs[REG_PARTITION_COUNT] = {NULL, NULL, NULL};

/* Id of the set that contains the semaphores (mutex = 0, read = 1, write = 2) used to read and write users list */
int userListSem = -1;

/* Id of the shared memory segment that contains the variable used to syncronize readers and writers access to users list */
int noUserSegReaders = -1;

/* Pointer to the variable that counts the number of readers, used to syncronize readers and writers access to users list */
int *noUserSegReadersPtr = NULL;

/* Id of the set that contains the semaphores (mutex = 0, read = 1, write = 2) used to read and write nodes list */
int nodeListSem = -1;
/*
 * Serve una variabile per contare il lettori perchè per estrarre un nodo a cui mandare la
 * transazione da processare bisogna leggere la lista dei nodi.
 */

/* Id of the shared memory segment that contains the variable used to syncronize readers and writers access to nodes list */
int noNodeSegReaders = -1;

/* Pointer to the variable that counts the number of readers, used to syncronize readers and writers access to nodes list */
int *noNodeSegReadersPtr = NULL;

/* Id of the mutex semaphore used to read/write the number of all times node processes' shared variabile */
int noAllTimesNodesSem = -1;

/* Id of the shared memory segment that contains the variable used to count the number of all times node processes */
int noAllTimesNodes = -1;

/* Pointer to the variable that counts the number of all times node processes */
long *noAllTimesNodesPtr = NULL;

#pragma endregion
/*** END GLOBAL VARIABLES FOR IPC ***/

/*** GLOBAL VARIABLES ***/
#pragma region GLOBAL VARIABLES
/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
/* Better to use long: the values could be very large */
long SO_USERS_NUM;           /* Number of user processes */
long SO_NODES_NUM;           /* Number of node processes */
long SO_REWARD;              /* Percentage of node's reward of the transaction */
long SO_MIN_TRANS_GEN_NSEC;  /* Min time for wait for transaction's processing */
long SO_MAX_TRANS_GEN_NSEC;  /* Max time for wait for transaction's processing */
long SO_RETRY;               /* Attempts to send a transaction before termination of user */
long SO_TP_SIZE;             /* Size of Transaction Pool of node processes */
long SO_MIN_TRANS_PROC_NSEC; /* Min time for transactions' block processing */
long SO_MAX_TRANS_PROC_NSEC; /* Max time for transactions' block processing */
long SO_BUDGET_INIT;         /* Initial budget of user processes */
long SO_SIM_SEC;             /* Duration of the simulation*/
long SO_FRIENDS_NUM;         /* Number of friends*/
long SO_HOPS;                /* Attempts to insert a transaction in a node's TP before elimination */
/*******************************************************/
/*******************************************************/

/*
 *    List that contains all the transactions sent by a process.
 *    We use it to keep track of the transactions sent by a process
 *    the haven't been written on the register yet.
 *    Since it could be very large it should be done
 *    orderly insertion and dichotomous search
 */
TransList *transactionsSent = NULL; /* it must be global, otherwise I cannot use it in the function for generating the transaction */
int num_failure = 0;                /* it must be global, otherwise I cannot use it in the function for generating the transaction */
struct timespec now;
long my_pid; /* pid of current user */
#pragma endregion
/*** END GLOBAL VARIABLES ***/

/*** FUNCTIONS PROTOTYPES DECLARATION ***/
#pragma region FUNCTIONS PROTOTYPES DECLARATION
/**
 * @brief Function that reads from environment variables the parameters necessary for the execution.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean readParams();

/**
 * @brief Function that initializes the IPC facilities for the user.
 * @return Returns TRUE if successfull, FALSE in case an error occurres.
 */
boolean initializeFacilities();

/**
 * @brief Function that computes the budget of the user counting from the register
 * and removing the amount of the sent but not processed transaction (the one not in register)
 * @return returns the actual balance as a double
 */
double computeBalance();

/**
 * @brief Function that removes the passed transaction from the list of sent transactions.
 * @param tList a pointer to the list of sent transactions to modify
 * @param t a pointer to the transaction to remove from list
 */
TransList *removeTransaction(TransList *, Transaction *);

/**
 * @brief Function that computes the budget of the user, generates a transaction,
 * sends it to a randomly chosen node and then simulates the wait for processing
 * the transaction.
 * @param sig the type of event that triggered the handler, 0 if not called on event
 */
void transactionGeneration(int);

/**
 * @brief Function that ends the execution of the user; this can happen in three different ways,
 * rappresented by the values that the parameter might assume.
 * @param sig the parameters value are: 0 -> only end of execution; -1 -> end of execution and deallocation (called from error);
 * SIGUSR1 -> end of execution and deallocation (called by signal from master)
 */
void endOfExecution(int);

/**
 * @brief Function that deallocates the IPC facilities for the user.
 */
void deallocateIPCFacilities();

/**
 * @brief Function that increases by one the number of failure counter of the user while attempting
 * to create a transaction and if the counter is equal to SO_RETRY, the simulation must terminate.
 */
void userFailure();

/**
 * @brief Function that adds the transaction passed as second argument to the list of sent transactions
 * passed as first argument.
 * @param transSent a pointer to the list of sent transactions
 * @param t a pointer to the transaction to add to the list
 * @return returns a pointer to the new head of the list of sent transactions
 */
TransList *addTransaction(TransList *, Transaction *);

/**
 * @brief Function that deallocates the list of sent transactions.
 * @param transSent a pointer to the list of sent transactions to deallocate
 */
void freeTransList(TransList *);

/**
 * @brief Function that extracts randomly a receiver for the transaction which is not the same user
 * that generated the transaction, whose pid is the argument passed to the function.
 * @param pid is the pid of the current user, the return value must be different from this pid
 * @return Returns the pid of the selected user in the usersList shared array, -1 if the function generates an error.
 */
pid_t extractReceiver(pid_t);

/**
 * @brief Function that extracts randomly a node which to send the generated transaction.
 * @return Returns the pid of the selected user in the usersList shared array, -1 if the function generates an error.
 */
pid_t extractNode();

/**
 * @brief Function that catches any segmentation fault error during execution and
 * avoids brutal termination.
 *
 * @param sig signal that fired the handler
 */
void segmentationFaultHandler(int);
#pragma endregion
/*** END FUNCTIONS PROTOTYPES DECLARATION ***/

/*** MAIN FUNCTION ***/
int main(int argc, char *argv[], char *envp[])
{
    struct sigaction actEndOfExec;
    struct sigaction actGenTrans;
    struct sigaction actSegFaultHandler;
    sigset_t mask;
    TransQueue msgCheckFailedTrans;
    char *printMsg;
    struct sembuf op;
    char * aus;

    /* initializing print string message */
    printMsg = (char *)calloc(200, sizeof(char));
    my_pid = (long)getpid();

    if (readParams())
    {
        if (initializeFacilities())
        {
            if (sigfillset(&mask) == -1)
            {
                snprintf(printMsg, 199, "[USER %5ld]: failed to initialize signal mask. Error: ", my_pid);
                unsafeErrorPrint(printMsg, __LINE__);
                printMsg[0] = 0; /* resetting string's content */
                endOfExecution(-1);
            }
            else
            {
                actEndOfExec.sa_handler = endOfExecution;
                actEndOfExec.sa_mask = mask;
                if (sigaction(SIGUSR1, &actEndOfExec, NULL) == -1)
                {
                    snprintf(printMsg, 199, "[USER %5ld]: failed to set up end of simulation handler. Error: ", my_pid);
                    unsafeErrorPrint(printMsg, __LINE__);
                    printMsg[0] = 0; /* resetting string's content */
                    endOfExecution(-1);
                }
                else
                {
                    actGenTrans.sa_handler = transactionGeneration;
                    actGenTrans.sa_mask = mask;
                    if (sigaction(SIGUSR2, &actGenTrans, NULL) == -1)
                    {
                        snprintf(printMsg, 199, "[USER %5ld]: failed to set up transaction generation handler. Error: ", my_pid);
                        unsafeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0; /* resetting string's content */
                        endOfExecution(-1);
                    }
                    else
                    {
                        actSegFaultHandler.sa_handler = segmentationFaultHandler;
                        actSegFaultHandler.sa_mask = mask;
                        if (sigaction(SIGSEGV, &actSegFaultHandler, NULL) == -1)
                        {
                            snprintf(printMsg, 199, "[USER %5ld]: failed to set up segmentation fault handler. Error: ", my_pid);
                            unsafeErrorPrint(printMsg, __LINE__);
                            printMsg[0] = 0; /* resetting string's content */
                            endOfExecution(-1);
                        }
                        else
                        {
                            printf("[USER %5ld]: waiting for simulation to start....\n", my_pid);
                            op.sem_op = 0;
                            op.sem_num = 0;
                            op.sem_flg = 0;
                            if (semop(fairStartSem, &op, 1) == -1)
                            {
                                snprintf(aus, 300, "[USER %5ld]: failed to wait for zero on start semaphore. Error: ", my_pid);
                                unsafeErrorPrint(aus, __LINE__);
                                endOfExecution(-1);
                            }
                            else
                            {
                                printf("[USER %5ld]: starting lifecycle...\n", my_pid);

                                /* User's lifecycle */
                                while (TRUE)
                                {
                                    printf("[USER %5ld]: checking if there are failed transactions...\n", my_pid);
                                    /* check on global queue if a sent transaction failed */
                                    if (msgrcv(transQueue, &msgCheckFailedTrans, sizeof(msgCheckFailedTrans) - sizeof(long), my_pid, IPC_NOWAIT) != -1)
                                    {

                                        /* got a message for this user from global queue */
                                        if (msgCheckFailedTrans.msgContent == FAILEDTRANS)
                                        {
                                            printf("[USER %5ld]: failed transaction found. Removing it from list...\n", my_pid);
                                            /* the transaction failed, so we remove it from the list of sent transactions */
                                            transactionsSent = removeTransaction(transactionsSent, &(msgCheckFailedTrans.transaction));
                                        }
                                        else
                                        {
                                            printf("[USER %5ld]: no failed transactions found.\n", my_pid);
                                            /* the message wasn't the one we were looking for, reinserting it on the global queue */
                                            if (msgsnd(transQueue, &msgCheckFailedTrans, sizeof(msgCheckFailedTrans) - sizeof(long), 0) == -1)
                                            {
                                                snprintf(printMsg, 199, "[USER %5ld]: failed to reinsert the message read from global queue while checking for failed transactions. Error: ", my_pid);
                                                unsafeErrorPrint(printMsg, __LINE__);
                                                printMsg[0] = 0; /* resetting string's content */
                                            }
                                        }
                                    }
                                    else if (errno != 0 && errno != ENOMSG)
                                    {
                                        snprintf(printMsg, 199, "[USER %5ld]: failed to check for failed transaction messages on global queue. Error: ", my_pid);
                                        unsafeErrorPrint(printMsg, __LINE__);
                                        printMsg[0] = 0; /* resetting string's content */
                                    }

                                    /* generate a transaction */
                                    transactionGeneration(0);

                                    sleep(1);
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            endOfExecution(-1);
        }
    }
    else
    {
        endOfExecution(0);
    }

    /* freeing print string message */
    if (printMsg != NULL)
        free(printMsg);

    return 0;
}
/*** END MAIN FUNCTION ***/

/*** FUNCTIONS IMPLEMENTATIONS ***/
#pragma region FUNCTIONS IMPLEMENTATIONS
/**
 * @brief Function that reads from environment variables the parameters necessary for the execution.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean readParams()
{
    /*
     * strtol allows us to check if this has occurred
     * an error (atol instead does not set errno and there is no way
     * to distinguish between the zero legitimate result and the error)
     */
    SO_USERS_NUM = strtol(getenv("SO_USERS_NUM"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_NODES_NUM = strtol(getenv("SO_NODES_NUM"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_REWARD = strtol(getenv("SO_REWARD"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_MIN_TRANS_GEN_NSEC = strtol(getenv("SO_MIN_TRANS_GEN_NSEC"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_MAX_TRANS_GEN_NSEC = strtol(getenv("SO_MAX_TRANS_GEN_NSEC"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_RETRY = strtol(getenv("SO_RETRY"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_TP_SIZE = strtol(getenv("SO_TP_SIZE"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_MIN_TRANS_PROC_NSEC = strtol(getenv("SO_MIN_TRANS_PROC_NSEC"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_MAX_TRANS_PROC_NSEC = strtol(getenv("SO_MAX_TRANS_PROC_NSEC"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_BUDGET_INIT = strtol(getenv("SO_BUDGET_INIT"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_SIM_SEC = strtol(getenv("SO_SIM_SEC"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_FRIENDS_NUM = strtol(getenv("SO_FRIENDS_NUM"), NULL, 10);
    TEST_ERROR_PARAM;

    SO_HOPS = strtol(getenv("SO_HOPS"), NULL, 10);
    TEST_ERROR_PARAM;

    return TRUE;
}

/**
 * @brief Function that initializes the IPC facilities for the user.
 * @return Returns TRUE if successfull, FALSE in case an error occurres.
 */
boolean initializeFacilities()
{
    key_t key;
    /* Initialization of semaphores*/
    key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during fair start semaphore creation. Error: ")
    fairStartSem = semget(key, 1, 0600);
    SEM_TEST_ERROR(fairStartSem, "[USER]: semget failed during fair start semaphore creation. Error: ");

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during partitions writing semaphores creation. Error: ");
    wrPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(wrPartSem, "[USER]: semget failed during partitions writing semaphores creation. Error: ");

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during partitions reading semaphores creation. Error: ");
    rdPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(rdPartSem, "[USER]: semget failed during partitions reading semaphores creation. Error: ");

    key = ftok(SEMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during user list semaphore creation. Error: ");
    userListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(userListSem, "[USER]: semget failed during user list semaphore creation. Error: ");

    key = ftok(SEMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during nodes list semaphore creation. Error: ");
    nodeListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(nodeListSem, "[USER]: semget failed during nodes list semaphore creation. Error: ");

    key = ftok(SEMFILEPATH, PARTMUTEXSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during partitions mutex semaphores creation. Error: ");
    mutexPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(mutexPartSem, "[USER]: semget failed during partitions mutex semaphores creation. Error: ");

    key = ftok(SEMFILEPATH, NOALLTIMESNODESSEMSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during number of all times nodes' shared variable's semaphore creation. Error: ")
    noAllTimesNodesSem = semget(key, 1, 0600);
    SEM_TEST_ERROR(noAllTimesNodesSem, "[USER]: semget failed during number of all times nodes' shared variable's semaphore creation. Error: ");

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    /* Creates the global queue*/
    key = ftok(MSGFILEPATH, PROC_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during processes global queue creation. Error: ");
    procQueue = msgget(key, 0600);
    MSG_TEST_ERROR(procQueue, "[USER]: msgget failed during processes global queue creation. Error: ");

    key = ftok(MSGFILEPATH, TRANS_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during transactions global queue creation. Error: ");
    transQueue = msgget(key, 0600);
    MSG_TEST_ERROR(transQueue, "[USER]: msgget failed during transactions global queue creation. Error: ");
    /********************************************************/
    /********************************************************/

    /*****  Initialization of shared memory segments    *****/
    /********************************************************/
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during register parition one creation. Error: ");
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[0], "[USER]: shmget failed during partition one creation. Error: ");

    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during register parition two creation. Error: ");
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[1], "[USER]: shmget failed during partition two creation. Error: ");

    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during register parition three creation. Error: ");
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[2], "[USER]: shmget failed during partition three creation. Error: ");

    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[0], "[USER]: failed to attach to partition one's memory segment. Error: ");
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[1], "[USER]: failed to attach to partition two's memory segment. Error: ");
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[2], "[USER]: failed to attach to partition three's memory segment. Error: ");

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during users list creation. Error: ");
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(usersListId, "[USER]: shmget failed during users list creation. Error: ");
    usersList = (ProcListElem *)shmat(usersListId, NULL, SHM_RDONLY);
    TEST_SHMAT_ERROR(usersList, "[USER]: failed to attach to users list's memory segment. Error: ")

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during nodes list creation. Error: ");
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(nodesListId, "[USER]: shmget failed during nodes list creation. Error: ");
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, SHM_RDONLY);
    TEST_SHMAT_ERROR(nodesList, "[USER]: failed to attach to nodes list's memory segment. Error: ");

    /* Segment hooking for shared variables */
    key = ftok(SHMFILEPATH, NOREADERSONESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during parition one's shared variable creation. Error: ");
    noReadersPartitions[0] = shmget(key, sizeof(int), 0600);
    SHM_TEST_ERROR(nodesListId, "[USER]: shmget failed during parition one's shared variable creation. Error: ");
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, 0);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[0], "[USER]: failed to attach to parition one's shared variable segment. Error: ");

    key = ftok(SHMFILEPATH, NOREADERSTWOSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during parition two's shared variable creation. Error: ");
    noReadersPartitions[1] = shmget(key, sizeof(int), 0600);
    SHM_TEST_ERROR(noReadersPartitions[1], "[USER]: shmget failed during parition two's shared variable creation. Error: ")
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[1], "[USER]: failed to attach to parition rwo's shared variable segment. Error: ");

    key = ftok(SHMFILEPATH, NOREADERSTHREESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during parition three's shared variable creation. Error: ");
    noReadersPartitions[2] = shmget(key, sizeof(int), 0600);
    SHM_TEST_ERROR(noReadersPartitions[2], "[USER]: shmget failed during parition three's shared variable creation. Error: ")
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[2], "[USER]: failed to attach to parition three's shared variable segment. Error: ");

    key = ftok(SHMFILEPATH, NOUSRSEGRDERSSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during users list's shared variable creation. Error: ");
    noUserSegReaders = shmget(key, sizeof(int), 0600);
    SHM_TEST_ERROR(noUserSegReaders, "[USER]: shmget failed during users list's shared variable creation. Error: ")
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noUserSegReadersPtr, "[USER]: failed to attach to users list's shared variable segment. Error: ");

    key = ftok(SHMFILEPATH, NONODESEGRDERSSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during nodes list's shared variable creation. Error: ");
    noNodeSegReaders = shmget(key, sizeof(int), 0600);
    SHM_TEST_ERROR(noNodeSegReaders, "[USER]: shmget failed during nodes list's shared variable creation. Error: ")
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noNodeSegReadersPtr, "[USER]: failed to attach to nodes list's shared variable segment. Error: ");

    key = ftok(SHMFILEPATH, NOALLTIMESNODESSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during number of all times nodes' shared variable creation. Error: ");
    noAllTimesNodes = shmget(key, sizeof(long), 0600);
    SHM_TEST_ERROR(noAllTimesNodes, "[USER]: shmget failed during number of all times nodes' shared variable creation. Error: ")
    noAllTimesNodesPtr = (long *)shmat(noAllTimesNodes, NULL, 0);
    TEST_SHMAT_ERROR(noAllTimesNodesPtr, "[USER]: failed to attach to number of all times nodes' shared variable segment. Error: ");

    return TRUE;
}

/**
 * @brief Function that computes the budget of the user counting from the register
 * and removing the amount of the sent but not processed transaction (the one not in register)
 * @param transSent it rappresents a pointer to the gloabl list of sent transactions, we must not
 * use the global one otherwise we lose the real pointer
 * @return returns the actual balance as a double
 */
double computeBalance()
{
    double balance = 0;
    int i, j, k, l, msg_length;
    Register *ptr;
    TransList *transSent;
    struct sembuf op;
    boolean errBeforeComputing = FALSE, errAfterComputing = FALSE;
    char *aus;
    aus = (char *)calloc(200, sizeof(char));

    balance = SO_BUDGET_INIT;

    /*
     * Equal solution to the problem of writer-readers
     * freely inspired by that of Professor Gunetti
     * We made this choice because it is not possible to predict
     * if there will be multiple read or write cycles and to avoid
     * the starvation of writers or readers.
     */
    msg_length = snprintf(aus, 199, "[USER %5ld]: computing balance...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    for (i = 0; i < REG_PARTITION_COUNT && !errBeforeComputing && !errAfterComputing; i++)
    {
        op.sem_num = i;
        op.sem_op = -1;
        op.sem_flg = 0;

        /*
         * In case of an error the idea is to return 0
         * so that the user does not send any transactions
         * Furthermore, the cycle is interrupted in the event of a check
         * an error with semaphores (before or after computation)
         * and terminate the function
         */
        if (semop(rdPartSem, &op, 1) == -1)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to reserve register partition reading semaphore. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
            errBeforeComputing = TRUE;
        }
        else
        {
            if (semop(mutexPartSem, &op, 1) == -1)
            {
                snprintf(aus, 199, "[USER %5ld]: failed to reserve register partition mutex semaphore. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0; /* resetting string's content */
                errBeforeComputing = TRUE;
            }
            else
            {
                (*noReadersPartitionsPtrs[i])++;
                if ((*noReadersPartitionsPtrs[i]) == 1)
                {
                    if (semop(wrPartSem, &op, 1) == -1)
                    {
                        snprintf(aus, 199, "[USER %5ld]: failed to reserve register partition writing semaphore. Error: ", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0; /* resetting string's content */
                        errBeforeComputing = TRUE;
                    }
                }

                op.sem_num = i;
                op.sem_op = 1;
                op.sem_flg = 0;

                if (semop(mutexPartSem, &op, 1) == -1)
                {
                    snprintf(aus, 199, "[USER %5ld]: failed to release register partition mutex semaphore. Error: ", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0; /* resetting string's content */
                    errBeforeComputing = TRUE;
                }
                else
                {
                    if (semop(rdPartSem, &op, 1) == -1)
                    {
                        snprintf(aus, 199, "[USER %5ld]: failed to release register partition reading semaphore. Error: ", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0; /* resetting string's content */
                        errBeforeComputing = TRUE;
                    }
                    else
                    {
                        /* if we arrive here and errBeforeComputing is true, an error occurred only reserving wrPartSem */
                        if (errBeforeComputing)
                            break; /* we stop the cycle and end balance computation */

                        ptr = regPtrs[i];

                        /* The while led to the generation of segmentation faults */
                        for (l = 0; l < REG_PARTITION_COUNT; l++)
                        {
                            for (j = 0; j < ptr->nBlocks; j++)
                            {
                                for (k = 0; k < SO_BLOCK_SIZE; k++)
                                {
                                    if (ptr->blockList[j].transList[k].receiver == my_pid)
                                    {
                                        balance += ptr->blockList[j].transList[k].amountSend;
                                    }
                                    else if (ptr->blockList[j].transList[k].sender == my_pid)
                                    {
                                        /*
                                         * We remove the transactions already present in the master
                                         * from the list of those sent
                                         */
                                        balance -= ((ptr->blockList[j].transList[k].amountSend) + (ptr->blockList[j].transList[k].reward));
                                        transactionsSent = removeTransaction(transactionsSent, &(ptr->blockList[j].transList[k]));
                                    }
                                }
                            }
                        }

                        op.sem_num = i;
                        op.sem_op = -1;
                        op.sem_flg = 0;

                        if (semop(mutexPartSem, &op, 1) == -1)
                        {
                            balance = 0;
                            snprintf(aus, 199, "[USER %5ld]: failed to reserve register partition mutex semaphore. Error: ", my_pid);
                            safeErrorPrint(aus, __LINE__);
                            aus[0] = 0; /* resetting string's content */
                            userFailure();
                            errAfterComputing = TRUE;
                        }
                        else
                        {
                            (*noReadersPartitionsPtrs[i])--;
                            if ((*noReadersPartitionsPtrs[i]) == 0)
                            {
                                op.sem_num = i;
                                op.sem_op = 1;
                                op.sem_flg = 0;

                                if (semop(wrPartSem, &op, 1) == -1)
                                {
                                    balance = 0;
                                    snprintf(aus, 199, "[USER %5ld]: failed to release register partition writing semaphore. Error: ", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0; /* resetting string's content */
                                    errAfterComputing = TRUE;
                                }
                            }

                            op.sem_num = i;
                            op.sem_op = 1;
                            op.sem_flg = 0;

                            if (semop(mutexPartSem, &op, 1) == -1)
                            {
                                balance = 0;
                                snprintf(aus, 199, "[USER %5ld]: failed to release register partition mutex semaphore. Error: ", my_pid);
                                safeErrorPrint(aus, __LINE__);
                                aus[0] = 0; /* resetting string's content */
                                userFailure();
                                errAfterComputing = TRUE;
                            }
                            else
                            {
                                /* if we arrive here and errAfterComputing is true, an error occurred only releasing wrPartSem */
                                if (errAfterComputing)
                                {
                                    userFailure(); /* I put this here and not in the then branch of the wrPartSem if because if
                                                    * an error occurs in both wrPartSem and mutexPartSem number
                                                    * of failures would be decremented twice */
                                    break;         /* we stop the cycle and end balance computation */
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (errBeforeComputing)
    {
        /* an error occurred with wrPartSem or rdPartSem or mutexPartSem before balance computing */
        userFailure();
        balance = 0;
    }
    else if (!errAfterComputing)
    {
        /*
         * Precondition: Transactions have been deleted from transSent
         * already registered in the master
         */
        transSent = transactionsSent;
        while (transSent != NULL)
        {
            balance -= ((transSent->currTrans.amountSend) + (transSent->currTrans.reward));
            transSent = transSent->nextTrans;
        }
    }

    /* print the user's balance */
    msg_length = snprintf(aus, 199, "[USER %5ld]: current balance is %4.1f\n", my_pid, balance);
    write(STDOUT_FILENO, aus, msg_length);

    if (aus != NULL)
        free(aus);

    return balance;
}

/**
 * @brief Function that removes the passed transaction from the list of sent transactions.
 * @param tList a pointer to the list of sent transactions to modify
 * @param t a pointer to the transaction to remove from list
 */
TransList *removeTransaction(TransList *tList, Transaction *t)
{
    TransList *headPointer = tList;
    TransList *prev = NULL;
    boolean done = FALSE;

    while (tList != NULL && !done)
    {
        if (tList->currTrans.timestamp.tv_nsec == t->timestamp.tv_nsec &&
            tList->currTrans.sender == t->sender &&
            tList->currTrans.receiver == t->receiver)
        {
            if (prev != NULL)
            {
                /*
                 * We are in the middle or at the end of the list. We need to remove the current transaction,
                 * stored in tList. First we need to save the transaction reference that
                 * the current happens, so we set the next of the previous transaction with the
                 * next of the current transaction (in the case of the end of the list the latter will be NULL)
                 */
                prev->nextTrans = tList->nextTrans;
                free(tList); /* if we are still in the loop, we are sure that tList will not be NULL */
            }
            else
            {
                /*
                 * prev is equal to NULL, so we are at the first loop and the transaction to be removed from
                 * list is the one at the top.
                 */
                if (tList->nextTrans == NULL)
                {
                    /*
                     * Case in which the list contains only one element
                     * (so tList-> nextTrans is already NULL)
                     */
                    headPointer = NULL;
                    free(tList); /* if we are still in the loop, we are sure that tList will not be NULL */
                }
                else
                {
                    /*
                     * Case where the transaction to be removed is the top of the list,
                     * we need to return the new pointer to the top of the list (i.e. the
                     * subsequent transaction).
                     */
                    headPointer = headPointer->nextTrans;
                    free(tList); /* if we are still in the loop, we are sure that tList will not be NULL */
                }
            }

            done = TRUE;
        }
        else
        {
            prev = tList;
            tList = tList->nextTrans;
        }
    }

    return headPointer;
}

/**
 * @brief Function that ends the execution of the user; this can happen in three different ways,
 * rappresented by the values that the parameter might assume.
 * @param sig the parameters value are: 0 -> only end of execution; -1 -> end of execution and deallocation (called from error);
 * SIGUSR1 -> end of execution and deallocation (called by signal from master)
 */
void endOfExecution(int sig)
{
    int exitCode = EXIT_FAILURE;
    char *aus = NULL;
    ProcQueue msgOnGQueue;

    aus = (char *)calloc(200, sizeof(char));

    computeBalance();

    deallocateIPCFacilities();

    if (sig == SIGUSR1)
    {
        exitCode = EXIT_SUCCESS;
        /*
         * If the method is called in response to receiving the SIGUSR1 signal, then it was
         * the master to request the termination of the user, so we set as state of
         * termination EXIT_SUCCESS; otherwise, the execution ends in bankruptcy.
         * It is also not necessary to signal termination to the master because it was he who asked to terminate
         */
    }
    else
    {
        /* notify master that user process terminated before expected */
        msgOnGQueue.mtype = getppid();
        msgOnGQueue.msgContent = TERMINATEDUSER;
        msgOnGQueue.procPid = (pid_t)my_pid;
        if (msgsnd(procQueue, &msgOnGQueue, sizeof(msgOnGQueue) - sizeof(long), IPC_NOWAIT) == -1)
        {
            if (errno == EAGAIN)
                snprintf(aus, 199, "[USER %5ld]: failed to inform master of my termination (global queue was full). Error: ", my_pid);
            else
                snprintf(aus, 199, "[USER %5ld]: failed to inform master of my termination. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
        }
    }

    if (aus)
        free(aus);
    exit(exitCode);
}

/**
 * @brief Function that deallocates the IPC facilities for the user.
 */
void deallocateIPCFacilities()
{
    int i = 0, msg_length;
    char *aus = NULL;

    aus = (char *)calloc(200, sizeof(char));

    msg_length = snprintf(aus, 199, "[USER %5ld]: detaching from register's partitions...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(regPtrs[i]) == -1)
        {
            if (errno != 0 && errno != EINVAL)
            {
                snprintf(aus, 199, "[USER %5ld]: failed to detach from register's partition. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0; /* resetting string's content */
            }
        }
    }

    msg_length = snprintf(aus, 199, "[USER %5ld]: detaching from users list...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    if (usersList != NULL && shmdt(usersList) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to detach from users list. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
        }
    }

    msg_length = snprintf(aus, 199, "[USER %5ld]: detaching from nodes list...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    if (nodesList != NULL && shmdt(nodesList) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to detach from nodes list. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
        }
    }

    msg_length = snprintf(aus, 199, "[USER %5ld]: detaching from partitions' number of readers shared variable...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(noReadersPartitionsPtrs[i]) == -1)
        {
            if (errno != 0 && errno != EINVAL)
            {
                snprintf(aus, 199, "[USER %5ld]: failed to detach from partitions' number of readers shared variable. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0; /* resetting string's content */
            }
        }
    }

    msg_length = snprintf(aus, 199, "[USER %5ld]: detaching from users list's number of readers shared variable...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    if (noUserSegReadersPtr != NULL && shmdt(noUserSegReadersPtr) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to detach from users list's number of readers shared variable. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
        }
    }

    msg_length = snprintf(aus, 199, "[USER %5ld]: detaching from nodes list's number of readers shared variable...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    if (noNodeSegReadersPtr != NULL && shmdt(noNodeSegReadersPtr) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to detach from nodes list's number of readers shared variable. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
        }
    }

    msg_length = snprintf(aus, 199, "[USER %5ld]: detaching from number of all times nodes' shared variable...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0; /* resetting string's content */

    if (noAllTimesNodesPtr != NULL && shmdt(noAllTimesNodesPtr) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to detach from number of all times nodes' shared variable. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
        }
    }

    msg_length = snprintf(aus, 199, "[USER %5ld]: cleanup operations completed. Process is about to end its execution...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);

    /* freeing the list of sent transactions */
    if (transactionsSent != NULL)
        freeTransList(transactionsSent);

    if (aus != NULL)
        free(aus);
}

/**
 * @brief Function that computes the budget of the user, generates a transaction,
 * sends it to a randomly chosen node and then simulates the wait for processing
 * the transaction.
 * @param sig the type of event that triggered the handler, 0 if not called on event
 */
void transactionGeneration(int sig)
{
    int bilancio, queueId, msg_length;
    Transaction new_trans;
    MsgTP msg_to_node;
    key_t key;
    pid_t receiver_node, receiver_user;
    struct timespec request, remaining, randTime;
    TransQueue msgOnGQueue;
    char *aus;

    aus = (char *)calloc(200, sizeof(char));

    bilancio = computeBalance(); /* computing balance */

    if (sig == 0)
    {
        msg_length = snprintf(aus, 199, "[USER %5ld]: generating a new transaction...\n", my_pid);
        write(STDOUT_FILENO, aus, msg_length);
        aus[0] = 0; /* resetting string's content */
    }
    else
    {
        msg_length = snprintf(aus, 199, "[USER %5ld]: generating a new transaction on event request...\n", my_pid);
        write(STDOUT_FILENO, aus, msg_length);
        aus[0] = 0; /* resetting string's content */
    }

    if (bilancio > 2)
    {
        /* It must be global */
        num_failure = 0; /* I managed to send the transaction, I reset the counter of consecutive bankruptcies */

        /* Extracts the receiving user randomly */
        receiver_user = extractReceiver((pid_t)my_pid);
        if (receiver_user == -1)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to extract user receiver. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
            userFailure();
        }
        else
        {
            /* getting nanoseconds to generate a random amount */
            clock_gettime(CLOCK_REALTIME, &randTime);

            /* Generating transaction */
            new_trans.sender = my_pid;
            new_trans.receiver = receiver_user;
            new_trans.amountSend = (randTime.tv_nsec % bilancio) + 2;    /* calculation of the budget between 2 and the budget (so it only does it whole) */
            new_trans.reward = (new_trans.amountSend / 100) * SO_REWARD; /* if we suppose that SO_REWARD is a value (percentage) expressed between 1 and 100 */
            if (new_trans.reward < 1)
                new_trans.reward = 1;

            new_trans.amountSend -= new_trans.reward; /* update amount sent to receiver removing node's reward */

            clock_gettime(CLOCK_REALTIME, &new_trans.timestamp); /* get timestamp for transaction */

            /* extracting node which to send the transaction */
            receiver_node = extractNode();
            if (receiver_node == -1)
            {
                snprintf(aus, 199, "[USER %5ld]: failed to extract node which to send transaction on TP. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0; /* resetting string's content */
                userFailure();
            }
            else
            {
                /* preparing message to send on node's queue */
                msg_to_node.mtype = receiver_node;
                msg_to_node.transaction = new_trans;

                /* generating key to retrieve node's queue */
                key = ftok(MSGFILEPATH, receiver_node);
                if (key == -1)
                {
                    snprintf(aus, 199, "[USER %5ld]: ftok failed during node's queue retrieving. Error: ", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0; /* resetting string's content */
                    userFailure();
                }
                else
                {
                    /* retrieving the message queue connection */
                    queueId = msgget(key, 0600);
                    if (queueId == -1)
                    {
                        snprintf(aus, 199, "[USER %5ld]: failed to connect to node's transaction pool. Error: ", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0; /* resetting string's content */
                        userFailure();
                    }
                    else
                    {
                        /* Inserting new transaction on list of transaction sent */
                        transactionsSent = addTransaction(transactionsSent, &new_trans);

                        /* sending the transaction to node */
                        msg_length = snprintf(aus, 199, "[USER %5ld]: sending the created transaction to the node...\n", my_pid);
                        write(STDOUT_FILENO, aus, msg_length);
                        aus[0] = 0; /* resetting string's content */

                        if (msgsnd(queueId, &msg_to_node, sizeof(Transaction), IPC_NOWAIT) == -1)
                        {
                            if (errno == EAGAIN)
                            {
                                /* TP of Selected Node was full, we need to send the message on the global queue */
                                msg_length = snprintf(aus, 199, "[USER %5ld]: transaction pool of selected node was full. Sending transaction on global queue...\n", my_pid);
                                write(STDOUT_FILENO, aus, msg_length);
                                aus[0] = 0; /* resetting string's content */

                                msgOnGQueue.mtype = receiver_node;
                                msgOnGQueue.msgContent = TRANSTPFULL;
                                msgOnGQueue.transaction = new_trans;
                                msgOnGQueue.hops = SO_HOPS;
                                if (msgsnd(transQueue, &msgOnGQueue, sizeof(msgOnGQueue) - sizeof(long), 0) == -1)
                                {
                                    snprintf(aus, 199, "[USER %5ld]: failed to send transaction on global queue. Error: ", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0; /* resetting string's content */
                                    userFailure();
                                }
                            }
                            else
                            {
                                if (sig == 0)
                                {
                                    snprintf(aus, 199, "[USER %5ld]: failed to send transaction to node. Error: ", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0; /* resetting string's content */
                                }
                                else
                                {
                                    snprintf(aus, 199, "[USER %5ld]: failed to send transaction generated on event to node. Error: ", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0; /* resetting string's content */
                                }

                                userFailure();
                            }
                        }
                        else
                        {
                            if (sig == 0)
                            {
                                msg_length = snprintf(aus, 199, "[USER %5ld]: transaction correctly sent to node.\n", my_pid);
                                write(STDOUT_FILENO, aus, msg_length);
                                aus[0] = 0; /* resetting string's content */
                            }
                            else
                            {
                                msg_length = snprintf(aus, 199, "[USER %5ld]: transaction generated on event correctly sent to node.\n", my_pid);
                                write(STDOUT_FILENO, aus, msg_length);
                                aus[0] = 0; /* resetting string's content */
                            }

                            /* Wait a random time in between SO_MIN_TRANS_GEN_NSEC and SO_MAX_TRANS_GEN_NSEC */
                            request.tv_sec = 0;
                            request.tv_nsec = (rand() % (SO_MAX_TRANS_GEN_NSEC + 1 - SO_MIN_TRANS_GEN_NSEC)) + SO_MIN_TRANS_GEN_NSEC;

                            /*
                             * Adjusting wait time, if number of nanoseconds is greater or equal to 1 second (10^9 nanoseconds)
                             * we increase the number of seconds.
                             */
                            while (request.tv_nsec >= 1000000000)
                            {
                                request.tv_sec++;
                                request.tv_nsec -= 1000000000;
                            }

                            msg_length = snprintf(aus, 199, "[USER %5ld]: processing the transaction...\n", my_pid);
                            write(STDOUT_FILENO, aus, msg_length);
                            aus[0] = 0; /* resetting string's content */

                            if (nanosleep(&request, &remaining) == -1)
                            {
                                snprintf(aus, 199, "[USER %5ld]: failed to simulate wait for processing the transaction. Error: ", my_pid);
                                safeErrorPrint(aus, __LINE__);
                                aus[0] = 0; /* resetting string's content */
                            }

                            /*
                             * Here in case of failure it is not necessary to increase the counter of the number of failures because
                             * we created the transaction anyway
                             */
                        }
                    }
                }
            }
        }
    }
    else
    {
        msg_length = snprintf(aus, 199, "[USER %5ld]: not enough money to make a transaction...\n", my_pid);
        write(STDOUT_FILENO, aus, msg_length);
        userFailure();
    }

    if (aus)
        free(aus);
}

/**
 * @brief Function that increases by one the number of failure counter of the user while attempting
 * to create a transaction and if the counter is equal to SO_RETRY, the simulation must terminate.
 */
void userFailure()
{
    int msg_length;
    char *aus;

    aus = (char *)calloc(200, sizeof(char));

    msg_length = snprintf(aus, 199, "[USER %5ld]: failed to create a transaction, increasing number of failure counter.\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    if (aus != NULL)
        free(aus);

    num_failure++; /* I increase the consecutive number of times I can't send a transaction */
    if (num_failure == SO_RETRY)
    {
        /* I have failed to send the transaction for SO_RETRY times, I have to finish */
        endOfExecution(-1);
    }
}

/**
 * @brief Function that adds the transaction passed as second argument to the list of sent transactions
 * passed as first argument.
 * @param transSent a pointer to the list of sent transactions
 * @param t a pointer to the transaction to add to the list
 * @return returns a pointer to the new head of the list of sent transactions
 */
TransList *addTransaction(TransList *transSent, Transaction *t)
{
    TransList *new_el = NULL;
    char *aus;

    aus = (char *)calloc(200, sizeof(char));

    if (t == NULL)
    {
        snprintf(aus, 199, "[USER %5ld]: error: transaction passed to function addTransaction is a NULL pointer.", my_pid);
        safeErrorPrint(aus, __LINE__);
        return NULL;
    }

    /* insertion of new transaction to list */
    new_el = (TransList *)malloc(sizeof(TransList));
    memcpy(&(new_el->currTrans), t, sizeof(Transaction));
    new_el->nextTrans = transSent;
    transSent = new_el;

    if (aus != NULL)
        free(aus);

    return transSent;
}

/**
 * @brief Function that deallocates the list of sent transactions.
 * @param transSent a pointer to the list of sent transactions to deallocate
 */
void freeTransList(TransList *transSent)
{
    if (transSent == NULL)
        return;

    freeTransList(transSent->nextTrans);

    if (transSent != NULL)
        free(transSent);
}

/**
 * @brief Function that extracts randomly a receiver for the transaction which is not the same user
 * that generated the transaction, whose pid is the argument passed to the function.
 * @param pid is the pid of the current user, the return value must be different from this pid
 * @return Returns the pid of the selected user in the usersList shared array, -1 if the function generates an error.
 */
pid_t extractReceiver(pid_t pid)
{
    int n = -1;
    struct sembuf sops;
    pid_t pid_to_return = -1;
    boolean errBeforeExtraction = FALSE;
    char *aus;

    aus = (char *)calloc(200, sizeof(char));

    sops.sem_flg = 0;

    sops.sem_num = 0;
    sops.sem_op = -1;
    if (semop(userListSem, &sops, 1) != -1)
    {
        (*noUserSegReadersPtr)++;
        if ((*noUserSegReadersPtr) == 1)
        {
            sops.sem_num = 2;
            sops.sem_op = -1;
            if (semop(userListSem, &sops, 1) == -1)
            {
                snprintf(aus, 199, "[USER %5ld]: failed to reserve write usersList semaphore. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0; /* resetting string's content */
                errBeforeExtraction = TRUE;
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if (semop(userListSem, &sops, 1) != -1)
        {
            if (errBeforeExtraction)
                return (pid_t)-1;

            do
            {
                /* It is used for the seed of generation */
                clock_gettime(CLOCK_REALTIME, &now);
                n = now.tv_nsec % SO_USERS_NUM;
            } while (pid == usersList[n].procId && usersList[n].procState != ACTIVE);
            /* It loops until the randomly extracted pid is the same as that of the user himself and until the selected user is active */

            pid_to_return = usersList[n].procId; /* save user pid so as to return it */

            sops.sem_num = 0;
            sops.sem_op = -1;
            if (semop(userListSem, &sops, 1) != -1)
            {
                (*noUserSegReadersPtr)--;
                if ((*noUserSegReadersPtr) == 0)
                {
                    sops.sem_num = 2;
                    sops.sem_op = 1;
                    if (semop(userListSem, &sops, 1) == -1)
                    {
                        snprintf(aus, 199, "[USER %5ld]: failed to release write usersList semaphore. Error: ", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0; /* resetting string's content */
                        pid_to_return = (pid_t)-1;
                    }
                }

                sops.sem_num = 0;
                sops.sem_op = 1;
                if (semop(userListSem, &sops, 1) != -1)
                {
                    return pid_to_return;
                }
                else
                {
                    snprintf(aus, 199, "[USER %5ld]: failed to release mutex usersList semaphore. Error: ", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0; /* resetting string's content */
                }
            }
            else
            {
                snprintf(aus, 199, "[USER %5ld]: failed to reserve mutex usersList semaphore. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0; /* resetting string's content */
            }
        }
        else
        {
            snprintf(aus, 199, "[USER %5ld]: failed to release mutex usersList semaphore. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
        }
    }
    else
    {
        snprintf(aus, 199, "[USER %5ld]: failed to reserve mutex usersList semaphore. Error: ", my_pid);
        safeErrorPrint(aus, __LINE__);
        aus[0] = 0; /* resetting string's content */
    }

    if (aus != NULL)
        free(aus);

    return (pid_t)-1;
}

/**
 * @brief Function that extracts randomly a node which to send the generated transaction.
 * @return Returns the pid of the selected user in the usersList shared array, -1 if the function generates an error.
 */
pid_t extractNode()
{
    int n = -1;
    struct sembuf sops;
    pid_t pid_to_return = -1;
    boolean errBeforeExtraction = FALSE;
    char *aus;
    long numNodes = 0;

    aus = (char *)calloc(200, sizeof(char));
    sops.sem_flg = 0;

    /* entering critical section for number of all times nodes' shared variable */
    sops.sem_op = -1;
    sops.sem_num = 0;
    if (semop(noAllTimesNodesSem, &sops, 1) == -1)
    {
        snprintf(aus, 199, "[USER %5ld]: failed to reserve number of all times nodes' semaphore. Error: ", my_pid);
        safeErrorPrint(aus, __LINE__);
        aus[0] = 0; /* resetting string's content */
    }
    else
    {
        /* saving number of all times node processes */
        numNodes = (*noAllTimesNodesPtr);

        /* exiting critical section for number of all times nodes' shared variable */
        sops.sem_op = 1;
        sops.sem_num = 0;
        if (semop(noAllTimesNodesSem, &sops, 1) == -1)
        {
            snprintf(aus, 199, "[USER %5ld]: failed to release number of all times nodes' semaphore. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0; /* resetting string's content */
        }
        else
        {
            sops.sem_num = 0;
            sops.sem_op = -1;
            if (semop(nodeListSem, &sops, 1) != -1)
            {
                (*noNodeSegReadersPtr)++;
                if ((*noNodeSegReadersPtr) == 1)
                {
                    sops.sem_num = 2;
                    sops.sem_op = -1;
                    if (semop(nodeListSem, &sops, 1) == -1)
                    {
                        snprintf(aus, 199, "[USER %5ld]: failed to reserve write nodesList semaphore. Error: ", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0; /* resetting string's content */
                        errBeforeExtraction = TRUE;
                    }
                }

                sops.sem_num = 0;
                sops.sem_op = 1;
                if (semop(nodeListSem, &sops, 1) != -1)
                {
                    if (errBeforeExtraction)
                        return (pid_t)-1;

                    do
                    {
                        clock_gettime(CLOCK_REALTIME, &now);
                        n = now.tv_nsec % numNodes;
                    } while (nodesList[n].procState != ACTIVE);
                    /* it loops until the randomly chosen node is active */

                    pid_to_return = nodesList[n].procId;

                    sops.sem_num = 0;
                    sops.sem_op = -1;
                    if (semop(nodeListSem, &sops, 1) != -1)
                    {
                        (*noNodeSegReadersPtr)--;
                        if ((*noNodeSegReadersPtr) == 0)
                        {
                            sops.sem_num = 2;
                            sops.sem_op = 1;
                            if (semop(nodeListSem, &sops, 1) == -1)
                            {
                                snprintf(aus, 199, "[USER %5ld]: failed to release write nodesList semaphore. Error: ", my_pid);
                                safeErrorPrint(aus, __LINE__);
                                aus[0] = 0; /* resetting string's content */
                                pid_to_return = (pid_t)-1;
                            }
                        }

                        sops.sem_num = 0;
                        sops.sem_op = 1;
                        if (semop(nodeListSem, &sops, 1) != -1)
                        {
                            return pid_to_return;
                        }
                        else
                        {
                            snprintf(aus, 199, "[USER %5ld]: failed to release mutex nodesList semaphore. Error: ", my_pid);
                            safeErrorPrint(aus, __LINE__);
                            aus[0] = 0; /* resetting string's content */
                        }
                    }
                    else
                    {
                        snprintf(aus, 199, "[USER %5ld]: failed to reserve mutex nodesList semaphore. Error: ", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0; /* resetting string's content */
                    }
                }
                else
                {
                    snprintf(aus, 199, "[USER %5ld]: failed to release mutex nodesList semaphore. Error: ", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0; /* resetting string's content */
                }
            }
            else
            {
                snprintf(aus, 199, "[USER %5ld]: failed to reserve mutex nodesList semaphore. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0; /* resetting string's content */
            }
        }
    }

    if (aus != NULL)
        free(aus);

    return (pid_t)-1;
}

/**
 * @brief Function that catches any segmentation fault error during execution and
 * avoids brutal termination.
 *
 * @param sig signal that fired the handler
 */
void segmentationFaultHandler(int sig)
{
    char *aus = NULL;
    int msg_length;

    aus = (char *)calloc(200, sizeof(char));

    msg_length = snprintf(aus, 199, "[USER %5ld]: a segmentation fault error happened. Terminating...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    if (aus != NULL)
        free(aus);

    if (sig == SIGSEGV)
        endOfExecution(-1);
    else
        exit(EXIT_FAILURE);
}
#pragma endregion
/*** END FUNCTIONS IMPLEMENTATIONS ***/

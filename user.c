#include "user.h"

/*** GLOBAL VARIABLES FOR IPC ***/
#pragma region GLOBAL VARIABLES FOR IPC
/* Poiter to the array that contains the ids of the shared memory segments of the register's partitions.
 * regPartsIds[0]: id of the first partition segment
 * regPartsIds[1]: id of the second partition segment
 * regPartsIds[2]: id of the third partition segment
 */
int *regPartsIds = NULL;

/* Pointer to the array that contains the pointers to the the register's partitions.
 * regPtrs[0]: pointer to the first partition segment
 * regPtrs[1]: pointer to the second partition segment
 * regPtrs[2]: pointer to the third partition segment
 */
Register **regPtrs = NULL;

/* Id of the shared memory segment that contains the users list */
int usersListId = -1;

/* Pointer to the users list */
ProcListElem *usersList = NULL;

/* Id of the shared memory segment that contains the nodes list */
int nodesListId = -1;

/* Pointer to the nodes list */
ProcListElem *nodesList = NULL;

/* Id of the global message queue where users, nodes and master communicate */
int globalQueueId = -1;
/*
    Serve perchè su di essa potrebbe arrivare
    la notifica del fallimento di una transazione nel caso in cui
    il master non possa aggiornare la lista utenti perchè
    un utente la sta già leggendo, vedendo ancora attivo
    un utente in realtà terminato (vale la pena fare tutto ciò??)
*/

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
int *noReadersPartitions = NULL;

/* Pointer to the array containing the variables used to syncronize readers and writers access to register's partition.
 * noReadersPartitionsPtrs[0]: pointer to the first partition's shared variable
 * noReadersPartitionsPtrs[1]: pointer to the second partition's shared variable
 * noReadersPartitionsPtrs[2]: pointer to the third partition's shared variable
 */
int **noReadersPartitionsPtrs = NULL;

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

#pragma endregion
/*** END GLOBAL VARIABLES FOR IPC ***/

/*** GLOBAL VARIABLES ***/
#pragma region GLOBAL VARIABLES
/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
/*
    Meglio usare long: i valori potrebbero essere molto grandi
*/
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
    List that contains all the transactions sent by a process.
    We use it to keep track of the transactions sent by a process
    the haven't been written on the register yet.
    Dato che potrebbe essere molto grande bisognerebbe fare
    inserimento ordinato e ricerca dicotomica
*/
TransList *transactionsSent = NULL; /* deve essere globale, altrimenti non posso utilizzarla nella funzione per la generazione della transazione */
int num_failure = 0;                /* deve essere globale, altrimenti non posso utilizzarla nella funzione per la generazione della transazione */
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
 * @brief Function that allocates memory for the array variables.
 * @return Returns TRUE if successfull, FALSE in case an error occurres.
 */
boolean allocateMemory();

/**
 * @brief Function that initializes the IPC facilities for the user.
 * @return Returns TRUE if successfull, FALSE in case an error occurres.
 */
boolean initializeFacilities();

/**
 * @brief Function that computes the budget of the user counting from the register
 * and removing the amount of the sent but not processed transaction (the one not in register)
 * @param transSent it rappresents a pointer to the gloabl list of sent transactions, we must not
 * use the global one otherwise we lose the real pointer
 * @return returns the actual balance as a double
 */
double computeBalance(TransList *);

/**
 * @brief Function that removes the passed transaction from the list of sent transactions.
 * @param tList a pointer to the list of sent transactions to modify
 * @param t a pointer to the transaction to remove from list
 */
TransList * removeTransaction(TransList *, Transaction *);

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
    /*
        List that contains all the transactions sent by a process.
        We use it to keep track of the transactions sent by a process
        the haven't been written on the register yet.
        Dato che potrebbe essere molto grande bisognerebbe fare
        inserimento ordinato e ricerca dicotomica
    */
    /*
        CORREGGERE: ALLOCARLO
    */
    /*TransList *transactionsSent = NULL;*/ /* spostato globalmente */
    struct sigaction actEndOfExec;
    struct sigaction actGenTrans;
    struct sigaction actSegFaultHandler;
    sigset_t mask;
    MsgGlobalQueue msgCheckFailedTrans;
    char * printMsg;

    /* initializing print string message */
    printMsg = (char*)calloc(200, sizeof(char));
    my_pid = (long)getpid();

    if (readParams())
    {
        if (allocateMemory())
        {
            if (initializeFacilities())
            {
                if (sigfillset(&mask) == -1)
                {
                    sprintf(printMsg, "[USER %5ld]: failed to initialize signal mask. Error", my_pid);
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
                        sprintf(printMsg, "[USER %5ld]: failed to set up end of simulation handler. Error", my_pid);
                        unsafeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0;/* resetting string's content */
                        endOfExecution(-1);
                    }
                    else
                    {
                        actGenTrans.sa_handler = transactionGeneration;
                        actGenTrans.sa_mask = mask;
                        if (sigaction(SIGUSR2, &actGenTrans, NULL) == -1)
                        {
                            sprintf(printMsg, "[USER %5ld]: failed to set up transaction generation handler. Error", my_pid);
                            unsafeErrorPrint(printMsg, __LINE__);
                            printMsg[0] = 0;/* resetting string's content */
                            endOfExecution(-1);
                        }
                        else
                        {
                            actSegFaultHandler.sa_handler = segmentationFaultHandler;
                            actSegFaultHandler.sa_mask = mask;
                            if (sigaction(SIGSEGV, &actSegFaultHandler, NULL) == -1)
                            {
                                sprintf(printMsg, "[USER %5ld]: failed to set up segmentation fault handler. Error", my_pid);
                                unsafeErrorPrint(printMsg, __LINE__);
                                printMsg[0] = 0;/* resetting string's content */
                                endOfExecution(-1);
                            }
                            else
                            {
                                printf("[USER %5ld]: starting lifecycle...\n", my_pid);

                                /*
                                    User's lifecycle
                                */
                                while (TRUE)
                                {
                                    printf("[USER %5ld]: checking if there are failed transactions...\n", my_pid);
                                    /* check on global queue if a sent transaction failed */
                                    if (msgrcv(globalQueueId, &msgCheckFailedTrans, sizeof(msgCheckFailedTrans) - sizeof(long), my_pid, IPC_NOWAIT) != -1)
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
                                            if (msgsnd(globalQueueId, &msgCheckFailedTrans, sizeof(msgCheckFailedTrans) - sizeof(long), 0) == -1)
                                            {
                                                sprintf(printMsg, "[USER %5ld]: failed to reinsert the message read from global queue while checking for failed transactions. Error", my_pid);
                                                unsafeErrorPrint(printMsg, __LINE__);
                                                printMsg[0] = 0;/* resetting string's content */
                                            }
                                        }
                                    }
                                    else if (errno != ENOMSG)
                                    {
                                        sprintf(printMsg, "[USER %5ld]: failed to check for failed transaction messages on global queue. Error", my_pid);
                                        unsafeErrorPrint(printMsg, __LINE__);
                                        printMsg[0] = 0;/* resetting string's content */
                                    }
                                    /* else errno == ENOMSG, so no transaction has failed */

                                    /* generate a transaction */
                                    transactionGeneration(0);

                                    sleep(1);
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
        strtol ci consente di verificare se si sia verificato
        un error (atol invece non setta errno e non c'è modo
        di distinguere tra lo zero risultato legittimo e l'errore)
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
 * @brief Function that allocates memory for the array variables.
 * @return Returns TRUE if successfull, FALSE in case an error occurres.
 */
boolean allocateMemory()
{
    regPtrs = (Register **)calloc(REG_PARTITION_COUNT, sizeof(Register *));
    TEST_MALLOC_ERROR(regPtrs, "[USER]: failed to allocate register paritions' pointers array. Error");

    regPartsIds = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(regPartsIds, "[USER]: failed to allocate register paritions' ids array. Error");

    /*
        noReadersPartitions e noReadersPartitionsPtrs vanno allocati
        perchè sono vettori, quindi dobbiamo allocare un'area di memoria
        abbastanza grande da contenere REG_PARTITION_COUNT interi/puntatori ad interi
    */
    noReadersPartitions = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(noReadersPartitions, "[USER]: failed to allocate registers partitions' shared variables ids. Error");

    /*
    Non allochiamo il noReadersPartitionsPtrs[i] perchè esso dovrà
    contenere un puntatore alla shared memory
    */
    noReadersPartitionsPtrs = (int **)calloc(REG_PARTITION_COUNT, sizeof(int *));
    TEST_MALLOC_ERROR(noReadersPartitionsPtrs, "[USER]: failed to allocate registers partitions' shared variables pointers. Error");

    return TRUE;
}

/**
 * @brief Function that initializes the IPC facilities for the user.
 * @return Returns TRUE if successfull, FALSE in case an error occurres.
 */
boolean initializeFacilities()
{
    /* Initialization of semaphores*/
    /*
        CORREGGERE: sostituire i numeri con costanti simboliche
        testare errori shmat
    */
    key_t key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during fair start semaphore creation. Error")
    fairStartSem = semget(key, 1, 0600);
    SEM_TEST_ERROR(fairStartSem, "[USER]: semget failed during fair start semaphore creation. Error");

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during partitions writing semaphores creation. Error");
    wrPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(wrPartSem, "[USER]: semget failed during partitions writing semaphores creation. Error");

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during partitions reading semaphores creation. Error");
    rdPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(rdPartSem, "[USER]: semget failed during partitions reading semaphores creation. Error");

    key = ftok(SEMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during user list semaphore creation. Error");
    userListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(userListSem, "[USER]: semget failed during user list semaphore creation. Error");

    key = ftok(SEMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during nodes list semaphore creation. Error");
    nodeListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(nodeListSem, "[USER]: semget failed during nodes list semaphore creation. Error");

    key = ftok(SEMFILEPATH, PARTMUTEXSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during partitions mutex semaphores creation. Error");
    mutexPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(mutexPartSem, "[USER]: semget failed during partitions mutex semaphores creation. Error");

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    /* Creates the global queue*/
    key = ftok(MSGFILEPATH, GLOBALMSGSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during global queue creation. Error");
    globalQueueId = msgget(key, 0600);
    MSG_TEST_ERROR(globalQueueId, "[USER]: msgget failed during global queue creation. Error");
    /********************************************************/
    /********************************************************/

    /*****  Initialization of shared memory segments    *****/
    /********************************************************/
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during register parition one creation. Error");
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[0], "[USER]: shmget failed during partition one creation. Error");

    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during register parition two creation. Error");
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[1], "[USER]: shmget failed during partition two creation. Error");

    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during register parition three creation. Error");
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[2], "[USER]: shmget failed during partition three creation. Error");

    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    /*SHMAT_TEST_ERROR(regPtrs[0], "User");*/
    TEST_SHMAT_ERROR(regPtrs[0], "[USER]: failed to attach to partition one's memory segment. Error");
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    /*SHMAT_TEST_ERROR(regPtrs[1], "User");*/
    TEST_SHMAT_ERROR(regPtrs[1], "[USER]: failed to attach to partition two's memory segment. Error");
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    /*SHMAT_TEST_ERROR(regPtrs[2], "User");*/
    TEST_SHMAT_ERROR(regPtrs[2], "[USER]: failed to attach to partition three's memory segment. Error");

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during users list creation. Error");
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(usersListId, "[USER]: shmget failed during users list creation. Error");
    usersList = (ProcListElem *)shmat(usersListId, NULL, SHM_RDONLY);
    /*SHMAT_TEST_ERROR(usersList, "User");*/
    TEST_SHMAT_ERROR(usersList, "[USER]: failed to attach to users list's memory segment. Error")

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during nodes list creation. Error");
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(nodesListId, "[USER]: shmget failed during nodes list creation. Error");
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, SHM_RDONLY);
    /*SHMAT_TEST_ERROR(nodesList, "User");*/
    TEST_SHMAT_ERROR(nodesList, "[USER]: failed to attach to nodes list's memory segment. Error");

    /* Aggancio segmenti per variabili condivise*/
    key = ftok(SHMFILEPATH, NOREADERSONESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during parition one's shared variable creation. Error");
    noReadersPartitions[0] = shmget(key, sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(nodesListId, "[USER]: shmget failed during parition one's shared variable creation. Error");
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, 0);
    /*SHMAT_TEST_ERROR(noReadersPartitionsPtrs[0], "User");*/
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[0], "[USER]: failed to attach to parition one's shared variable segment. Error");

    key = ftok(SHMFILEPATH, NOREADERSTWOSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during parition two's shared variable creation. Error");
    noReadersPartitions[1] = shmget(key, sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noReadersPartitions[1], "[USER]: shmget failed during parition two's shared variable creation. Error")
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    /*SHMAT_TEST_ERROR(noReadersPartitionsPtrs[1], "User");*/
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[1], "[USER]: failed to attach to parition rwo's shared variable segment. Error");

    key = ftok(SHMFILEPATH, NOREADERSTHREESEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during parition three's shared variable creation. Error");
    noReadersPartitions[2] = shmget(key, sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noReadersPartitions[2], "[USER]: shmget failed during parition three's shared variable creation. Error")
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    /*SHMAT_TEST_ERROR(noReadersPartitionsPtrs[2], "User");*/
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[2], "[USER]: failed to attach to parition three's shared variable segment. Error");

    key = ftok(SHMFILEPATH, NOUSRSEGRDERSSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during users list's shared variable creation. Error");
    noUserSegReaders = shmget(key, sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noUserSegReaders, "[USER]: shmget failed during users list's shared variable creation. Error")
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    /*SHMAT_TEST_ERROR(noUserSegReadersPtr, "User");*/
    TEST_SHMAT_ERROR(noUserSegReadersPtr, "[USER]: failed to attach to users list's shared variable segment. Error");

    key = ftok(SHMFILEPATH, NONODESEGRDERSSEED);
    FTOK_TEST_ERROR(key, "[USER]: ftok failed during nodes list's shared variable creation. Error");
    noNodeSegReaders = shmget(key, sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noNodeSegReaders, "[USER]: shmget failed during nodes list's shared variable creation. Error")
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    /*SHMAT_TEST_ERROR(noNodeSegReadersPtr, "User");*/
    TEST_SHMAT_ERROR(noNodeSegReadersPtr, "[USER]: failed to attach to nodes list's shared variable segment. Error");

    return TRUE;
}

/**
 * @brief Function that computes the budget of the user counting from the register
 * and removing the amount of the sent but not processed transaction (the one not in register)
 * @param transSent it rappresents a pointer to the gloabl list of sent transactions, we must not
 * use the global one otherwise we lose the real pointer
 * @return returns the actual balance as a double
 */
double computeBalance(TransList *transSent)
{
    double balance = 0;
    int i, j, k, l, msg_length;
    Register *ptr;
    struct sembuf op;
    boolean errBeforeComputing = FALSE, errAfterComputing = FALSE;
    char * aus;
    aus = (char *)calloc(200, sizeof(char));

    balance = SO_BUDGET_INIT;

    /*
        Soluzione equa al problema dei lettori scrittori
        liberamente ispirata a quella del Professor Gunetti
        Abbiamo fatto questa scelta perchè non è possibile prevedere
        se vi saranno più cicli di lettura o di scrittura e per evitare
        la starvation degli scrittori o dei lettori.
    */
    
    msg_length = sprintf(aus, "[USER %5ld]: computing balance...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0;/* resetting string's content */

    for (i = 0; i < REG_PARTITION_COUNT && !errBeforeComputing && !errAfterComputing; i++)
    {
        op.sem_num = i;
        op.sem_op = -1;
        op.sem_flg = 0;

        /*
            In caso di errore l'idea è quella di restituire 0
            in modo che l'utente non invii alcuna transazione
            Inoltre si interrompe il ciclo in caso di verifichi
            un errore con i semafori (prima o dopo la computazione)
            e si termina la funzione
        */
        if (semop(rdPartSem, &op, 1) == -1)
        {
            sprintf(aus, "[USER %5ld]: failed to reserve register partition reading semaphore. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
            errBeforeComputing = TRUE;
        }
        else
        {
            if (semop(mutexPartSem, &op, 1) == -1)
            {
                sprintf(aus, "[USER %5ld]: failed to reserve register partition mutex semaphore. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
                errBeforeComputing = TRUE;
            }
            else
            {
                *(noReadersPartitionsPtrs[i])++;
                if (*(noReadersPartitionsPtrs[i]) == 1)
                {
                    if (semop(wrPartSem, &op, 1) == -1)
                    {
                        sprintf(aus, "[USER %5ld]: failed to reserve register partition writing semaphore. Error", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0;/* resetting string's content */
                        errBeforeComputing = TRUE;
                    }
                }

                op.sem_num = i;
                op.sem_op = 1;
                op.sem_flg = 0;

                if (semop(mutexPartSem, &op, 1) == -1)
                {
                    sprintf(aus, "[USER %5ld]: failed to release register partition mutex semaphore. Error", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0;/* resetting string's content */
                    errBeforeComputing = TRUE;
                }
                else
                {
                    if (semop(rdPartSem, &op, 1) == -1)
                    {
                        sprintf(aus, "[USER %5ld]: failed to release register partition reading semaphore. Error", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0;/* resetting string's content */
                        errBeforeComputing = TRUE;
                    }
                    else
                    {
                        /* if we arrive here and errBeforeComputing is true, an error occurred only reserving wrPartSem */
                        if (errBeforeComputing)
                            break; /* we stop the cycle and end balance computation */

                        ptr = regPtrs[i];
                        
                        /* Il while portava alla generazione di segmentation fault */
                        for (l = 0; l < REG_PARTITION_COUNT; l++, ptr++)
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
                                            Togliamo le transazioni già presenti nel master
                                            dalla lista di quelle inviate
                                        */
                                        balance -= (ptr->blockList[j].transList[k].amountSend) +
                                                   (ptr->blockList[j].transList[k].reward);
                                        transactionsSent = removeTransaction(transSent, &(ptr->blockList[j].transList[k]));
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
                            sprintf(aus, "[USER %5ld]: failed to reserve register partition mutex semaphore. Error", my_pid);
                            safeErrorPrint(aus, __LINE__);
                            aus[0] = 0;/* resetting string's content */
                            userFailure();
                            errAfterComputing = TRUE;
                        }
                        else
                        {
                            *(noReadersPartitionsPtrs[i])--;
                            if (*(noReadersPartitionsPtrs[i]) == 0)
                            {
                                op.sem_num = i;
                                op.sem_op = 1;
                                op.sem_flg = 0;

                                if (semop(wrPartSem, &op, 1) == -1)
                                {
                                    balance = 0;
                                    sprintf(aus, "[USER %5ld]: failed to release register partition writing semaphore. Error", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0;/* resetting string's content */
                                    errAfterComputing = TRUE;
                                }
                            }

                            op.sem_num = i;
                            op.sem_op = 1;
                            op.sem_flg = 0;

                            if (semop(mutexPartSem, &op, 1) == -1)
                            {
                                balance = 0;
                                sprintf(aus, "[USER %5ld]: failed to release register partition mutex semaphore. Error", my_pid);
                                safeErrorPrint(aus, __LINE__);
                                aus[0] = 0;/* resetting string's content */
                                userFailure();
                                errAfterComputing = TRUE;
                            }
                            else
                            {
                                /* if we arrive here and errAfterComputing is true, an error occurred only releasing wrPartSem */
                                if (errAfterComputing)
                                {
                                    userFailure(); /* metto questo qui e non nel ramo then dell'if di wrPartSem perché se
                                                    * si verifica un errore sia in wrPartSem che in mutexPartSem il numero
                                                    * di fallimenti verrebbe decrementato due volte */
                                    break;         /* we stop the cycle and end balance computation */
                                }

                                /* l'ho spostato qui perché va fatto solo se tutto viene eseguito correttamente */
                                /*
                                    Precondizione: da transSent sono state eliminate le transazioni
                                    già registrate nel master
                                */
                                while (transSent != NULL)
                                {
                                    balance -= (transSent->currTrans.amountSend) +
                                               (transSent->currTrans.reward);
                                    transSent = transSent->nextTrans;
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

    /* print the user's balance */
    msg_length = sprintf(aus, "[USER %5ld]: current balance is %4.1f\n", my_pid, balance);
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
TransList * removeTransaction(TransList *tList, Transaction *t)
{
    TransList *headPointer = tList;
    TransList *prev = NULL;
    boolean done = FALSE;

    while (tList != NULL && !done)
    {
        /*
            CORREGGERE: i tempi si possono confrontare direttamente???
        */
        if (tList->currTrans.timestamp.tv_nsec == t->timestamp.tv_nsec &&
            tList->currTrans.sender == t->sender &&
            tList->currTrans.receiver == t->receiver)
        {
            if (prev != NULL)
            {
                prev->nextTrans = tList->nextTrans;
                /*
                    Non bisognerebbe fare anche la free quando rimuoviamo un elemento?
                */
                if (tList != NULL)
                    free(tList);
            }
            else
            {
                if(tList->nextTrans == NULL) 
                {
                    /*
                        Caso in cui la lista contiene un solo elemento
                        (quindi tList->nextTrans è gia NULL)
                    */
                    /*tList->currTrans = NULL;*/
                    /* ora currTrans non è più un puntatore, quindi non può essere impostato a NULL;
                    soluzione: impostiamo la testa della lista a NULL */

                    /*
                        Non bisognerebbe fare anche la free quando rimuoviamo un elemento?
                    */
                    if (tList != NULL)
                        free(tList);
                    headPointer = NULL;
                }
                else
                {
                    /*
                        Caso in cui la transazione da rimuovere è la cima della lista,
                        dobbiamo restituire il nuovo puntatore
                    */
                    headPointer = tList->nextTrans;
                    if (tList != NULL)
                        free(tList);
                }
            }

            done = TRUE;
        }
        else
        {
            prev = tList;
            tList = tList->nextTrans;
            /*
                Non bisognerebbe fare anche la free quando rimuoviamo un elemento?
            */
            /*free(prev);*/
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
    char * aus;
    MsgGlobalQueue msgOnGQueue;
    
    aus = (char *)calloc(200, sizeof(char));

    deallocateIPCFacilities();

    if (sig == SIGUSR1)
    {
        exitCode = EXIT_SUCCESS;
        /*
         * se il metodo viene chiamato in risposta alla ricezione del segnale SIGUSR1, allora è stato
         * il master a richiedere la terminazione dello user, quindi impostiamo come stato di
         * terminazione EXIT_SUCCESS; in caso contrario l'esecuzione termina con un fallimento.
         */
    }
    else
    {
        /* notify master that user process terminated before expected */
        msgOnGQueue.mtype = getppid();
        msgOnGQueue.msgContent = TERMINATEDUSER;
        msgOnGQueue.terminatedPid = (pid_t)my_pid;
        if (msgsnd(globalQueueId, &msgOnGQueue, sizeof(msgOnGQueue) - sizeof(long), IPC_NOWAIT) == -1)
        {
            if(errno == EAGAIN)
                sprintf(aus, "[USER %5ld]: failed to inform master of my termination (global queue was full). Error", my_pid);
            else
                sprintf(aus, "[USER %5ld]: failed to inform master of my termination. Error", my_pid);
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
    /*
     * Cose da eliminare:
     *  - collegamento alla memoria condivisa
     *  - i semafori li dealloca il master
     *  - bisogna chiudere le write/read end della globalQueue? No, lo fa il master!
     */
    int i = 0, msg_length;
    char * aus = NULL;

    aus = (char *)calloc(200, sizeof(char));

    /*
        CORREGGERE CON NUOVO MECCANISMO DI RILEVAZIONE ERRORI
    */

    msg_length = sprintf(aus, "[USER %5ld]: detaching from register's partitions...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0;/* resetting string's content */

    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(regPtrs[i]) == -1)
        {
            if (errno != EINVAL)
            {
                /*
                    Implementare un meccanismo di retry??
                    Contando che non è un errore così frequente si potrebbe anche ignorare...
                    Non vale la pena, possiamo limitarci a proseguire la deallocazione
                    riducendo al minimo il memory leak
                */
                sprintf(aus, "[USER %5ld]: failed to detach from register's partition. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
            }
        }
    }
    if (regPtrs != NULL)
        free(regPtrs);

    if (regPartsIds != NULL)
        free(regPartsIds);

    msg_length = sprintf(aus, "[USER %5ld]: detaching from users list...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0;/* resetting string's content */

    if (shmdt(usersList) == -1)
    {
        if (errno != EINVAL)
        {
            sprintf(aus, "[USER %5ld]: failed to detach from users list. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
        }
    }

    msg_length = sprintf(aus, "[USER %5ld]: detaching from nodes list...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0;/* resetting string's content */

    if (shmdt(nodesList) == -1)
    {
        if (errno != EINVAL)
        {
            sprintf(aus, "[USER %5ld]: failed to detach from nodes list. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
        }
    }

    msg_length = sprintf(aus, "[USER %5ld]: detaching from partitions' number of readers shared variable...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0;/* resetting string's content */

    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(noReadersPartitionsPtrs[i]) == -1)
        {
            if (errno != EINVAL)
            {
                sprintf(aus, "[USER %5ld]: failed to detach from partitions' number of readers shared variable. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
            }
        }
    }
    if (noReadersPartitions != NULL)
        free(noReadersPartitions);

    if (noReadersPartitionsPtrs != NULL)
        free(noReadersPartitionsPtrs);

    msg_length = sprintf(aus, "[USER %5ld]: detaching from users list's number of readers shared variable...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0;/* resetting string's content */

    if (shmdt(noUserSegReadersPtr) == -1)
    {
        if (errno != EINVAL)
        {
            sprintf(aus, "[USER %5ld]: failed to detach from users list's number of readers shared variable. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
        }
    }

    msg_length = sprintf(aus, "[USER %5ld]: detaching from nodes list's number of readers shared variable...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    aus[0] = 0;/* resetting string's content */

    if (shmdt(noNodeSegReadersPtr) == -1)
    {
        if (errno != EINVAL)
        {
            sprintf(aus, "[USER %5ld]: failed to detach from nodes list's number of readers shared variable. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
        }
    }

    msg_length = sprintf(aus, "[USER %5ld]: cleanup operations completed. Process is about to end its execution...\n", my_pid);
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
    MsgGlobalQueue msgOnGQueue;
    char * aus;

    aus = (char *)calloc(200, sizeof(char));

    bilancio = computeBalance(transactionsSent); /* calcolo del bilancio */

    if (sig == 0)
    {
        msg_length = sprintf(aus, "[USER %5ld]: generating a new transaction...\n", my_pid);
        write(STDOUT_FILENO, aus, msg_length);
        aus[0] = 0; /* resetting string's content */
    }
    else
    {
        msg_length = sprintf(aus, "[USER %5ld]: generating a new transaction on event request...\n", my_pid);
        write(STDOUT_FILENO, aus, msg_length);
        aus[0] = 0; /* resetting string's content */
    }

    if (bilancio > 2)
    {
        /* deve essere globale */
        num_failure = 0; /* sono riuscito a mandare la transazione, azzero il counter dei fallimento consecutivi */

        /* Extracts the receiving user randomly */
        receiver_user = extractReceiver((pid_t)my_pid);
        if (receiver_user == -1)
        {
            sprintf(aus, "[USER %5ld]: failed to extract user receiver. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
            userFailure();
        }
        else
        {
            /*Controllo tipo transazione era qui*/

            /* getting nanoseconds to generate a random amount */
            clock_gettime(CLOCK_REALTIME, &randTime);

            /* Generating transaction */
            new_trans.sender = my_pid;
            new_trans.receiver = receiver_user;
            new_trans.amountSend = (randTime.tv_nsec % bilancio) + 2;    /* calcolo del budget fra 2 e il budget (così lo fa solo intero) */
            /*new_trans.reward = new_trans.amountSend*SO_REWARD;*/       /* se supponiamo che SO_REWARD sia un valore (percentuale) espresso tra 0 e 1 */
            new_trans.reward = (new_trans.amountSend / 100) * SO_REWARD; /* se supponiamo che SO_REWARD sia un valore (percentuale) espresso tra 1 e 100 */
            if (new_trans.reward < 1)
                new_trans.reward = 1;
            
            new_trans.amountSend -= new_trans.reward;                    /* update amount sent to receiver removing node's reward */

            clock_gettime(CLOCK_REALTIME, &new_trans.timestamp); /* get timestamp for transaction */

            /* extracting node which to send the transaction */
            receiver_node = extractNode();
            if (receiver_node == -1)
            {
                sprintf(aus, "[USER %5ld]: failed to extract node which to send transaction on TP. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
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
                    sprintf(aus, "[USER %5ld]: ftok failed during node's queue retrieving. Error", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0;/* resetting string's content */
                    userFailure();
                }
                else
                {
                    /* retrieving the message queue connection */
                    queueId = msgget(key, 0600);
                    if (queueId == -1)
                    {
                        sprintf(aus, "[USER %5ld]: failed to connect to node's transaction pool. Error", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0; /* resetting string's content */
                        userFailure();
                    }
                    else
                    {
                        /* Inserting new transaction on list of transaction sent */
                        transactionsSent = addTransaction(transactionsSent, &new_trans);

                        /* sending the transaction to node */
                        msg_length = sprintf(aus, "[USER %5ld]: sending the created transaction to the node...\n", my_pid);
                        write(STDOUT_FILENO, aus, msg_length);
                        aus[0] = 0;/* resetting string's content */

                        if (msgsnd(queueId, &msg_to_node, sizeof(Transaction), IPC_NOWAIT) == -1)
                        {
                            if (errno == EAGAIN)
                            {
                                /* TP of Selected Node was full, we need to send the message on the global queue */
                                msg_length = sprintf(aus, "[USER %5ld]: transaction pool of selected node was full. Sending transaction on global queue...\n", my_pid);
                                write(STDOUT_FILENO, aus, msg_length);
                                aus[0] = 0;/* resetting string's content */

                                msgOnGQueue.mtype = receiver_node;
                                msgOnGQueue.msgContent = TRANSTPFULL;
                                msgOnGQueue.transaction = new_trans;
                                msgOnGQueue.hops = 0;
                                if (msgsnd(globalQueueId, &msgOnGQueue, sizeof(msgOnGQueue) - sizeof(long), 0) == -1)
                                {
                                    sprintf(aus, "[USER %5ld]: failed to send transaction on global queue. Error", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0;/* resetting string's content */
                                    userFailure();
                                }
                            }
                            else
                            {
                                if (sig == 0)
                                {
                                    sprintf(aus, "[USER %5ld]: failed to send transaction to node. Error", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0;/* resetting string's content */
                                }
                                else
                                {
                                    sprintf(aus, "[USER %5ld]: failed to send transaction generated on event to node. Error", my_pid);
                                    safeErrorPrint(aus, __LINE__);
                                    aus[0] = 0;/* resetting string's content */
                                }

                                userFailure();
                            }
                        }
                        else
                        {
                            if (sig == 0)
                            {
                                msg_length = sprintf(aus, "[USER %5ld]: transaction correctly sent to node.\n", my_pid);
                                write(STDOUT_FILENO, aus, msg_length);
                                aus[0] = 0;/* resetting string's content */
                            }
                            else
                            {
                                msg_length = sprintf(aus, "[USER %5ld]: transaction generated on event correctly sent to node.\n", my_pid);
                                write(STDOUT_FILENO, aus, msg_length);
                                aus[0] = 0;/* resetting string's content */
                            }

                            /* Wait a random time in between SO_MIN_TRANS_GEN_NSEC and SO_MAX_TRANS_GEN_NSEC */
                            request.tv_sec = 0;
                            request.tv_nsec = (rand() % (SO_MAX_TRANS_GEN_NSEC+1-SO_MIN_TRANS_GEN_NSEC)) + SO_MIN_TRANS_GEN_NSEC;

                            /* 
                             * Adjusting wait time, if number of nanoseconds is greater or equal to 1 second (10^9 nanoseconds)
                             * we increase the number of seconds.
                             */
                            while(request.tv_nsec >= 1000000000)
                            {
                                request.tv_sec++;
                                request.tv_nsec -= 1000000000;
                            }
                            
                            msg_length = sprintf(aus, "[USER %5ld]: processing the transaction...\n", my_pid);
                            write(STDOUT_FILENO, aus, msg_length);
                            aus[0] = 0;/* resetting string's content */
                            
                            if (nanosleep(&request, &remaining) == -1)
                            {
                                sprintf(aus, "[USER %5ld]: failed to simulate wait for processing the transaction. Error", my_pid);
                                safeErrorPrint(aus, __LINE__);
                                aus[0] = 0;/* resetting string's content */
                            }
                            
                            /* 
                                qui in caso di fallimento non serve incrementare il counter del numero di fallimenti perché 
                                la transazione l'abbiamo comunque creata
                            */
                        }
                    }
                }
            }
        }
    }
    else
    {
        msg_length = sprintf(aus, "[USER %5ld]: not enough money to make a transaction...\n", my_pid);
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
    char * aus;

    aus = (char *)calloc(200, sizeof(char));

    msg_length = sprintf(aus, "[USER %5ld]: failed to create a transaction, increasing number of failure counter.\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    if (aus != NULL)
        free(aus);

    num_failure++; /* incremento il numero consecutivo di volte che non riesco a mandare una transazione */
    if (num_failure == SO_RETRY)
    {
        /* non sono riuscito a mandare la transazione per SO_RETRY volte, devo terminare */
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
    char * aus;

    aus = (char *)calloc(200, sizeof(char));

    if (t == NULL)
    {
        sprintf(aus, "[USER %5ld]: error: transaction passed to function addTransaction is a NULL pointer.", my_pid);
        safeErrorPrint(aus, __LINE__);
        return NULL;
    }

    /* insertion of new transaction to list */
    new_el = (TransList *)malloc(sizeof(TransList));
    new_el->currTrans = *t;
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
    char * aus;

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
                sprintf(aus, "[USER %5ld]: failed to reserve write usersList semaphore. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                errBeforeExtraction = TRUE;
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if (semop(userListSem, &sops, 1) != -1)
        {
            if(errBeforeExtraction)
                return (pid_t)-1;

            do
            {
                /*
                    Serve per il seme di generazione
                */
                clock_gettime(CLOCK_REALTIME, &now);
                n = now.tv_nsec % SO_USERS_NUM;
            } while (pid == usersList[n].procId && usersList[n].procState != ACTIVE);
            /* cicla finché il pid estratto casualmente è uguale a quello dell'utente stesso e finché l'utente scelto non è attivo */

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
                        sprintf(aus, "[USER %5ld]: failed to release write usersList semaphore. Error", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0;/* resetting string's content */
                        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
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
                    sprintf(aus, "[USER %5ld]: failed to release mutex usersList semaphore. Error", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0;/* resetting string's content */
                    /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                }
            }
            else
            {
                sprintf(aus, "[USER %5ld]: failed to reserve mutex usersList semaphore. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
            }
        }
        else
        {
            sprintf(aus, "[USER %5ld]: failed to release mutex usersList semaphore. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
            /* restituiamo -1 e contiamo come fallimento di invio di transazione */
        }
    }
    else
    {
        sprintf(aus, "[USER %5ld]: failed to reserve mutex usersList semaphore. Error", my_pid);
        safeErrorPrint(aus, __LINE__);
        aus[0] = 0;/* resetting string's content */
        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
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
    char * aus;

    aus = (char *)calloc(200, sizeof(char));
    sops.sem_flg = 0;

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
                sprintf(aus, "[USER %5ld]: failed to reserve write nodesList semaphore. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                errBeforeExtraction = TRUE;
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if (semop(nodeListSem, &sops, 1) != -1)
        {
            if(errBeforeExtraction)
                return (pid_t)-1;

            do
            {
                clock_gettime(CLOCK_REALTIME, &now);
                n = now.tv_nsec % SO_NODES_NUM;
            } while (nodesList[n].procState != ACTIVE);
            /* cicla finché il nodo scelto casualmente non è attivo */

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
                        sprintf(aus, "[USER %5ld]: failed to release write nodesList semaphore. Error", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0;/* resetting string's content */
                        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
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
                    sprintf(aus, "[USER %5ld]: failed to release mutex nodesList semaphore. Error", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0;/* resetting string's content */
                    /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                }
            }
            else
            {
                sprintf(aus, "[USER %5ld]: failed to reserve mutex nodesList semaphore. Error", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;/* resetting string's content */
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
            }
        }
        else
        {
            sprintf(aus, "[USER %5ld]: failed to release mutex nodesList semaphore. Error", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;/* resetting string's content */
            /* restituiamo -1 e contiamo come fallimento di invio di transazione */
        }
    }
    else
    {
        sprintf(aus, "[USER %5ld]: failed to reserve mutex nodesList semaphore. Error", my_pid);
        safeErrorPrint(aus, __LINE__);
        aus[0] = 0;/* resetting string's content */
        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
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
    char * aus = NULL;
    int msg_length;
    
    aus = (char*)calloc(200, sizeof(char));

    msg_length = sprintf(aus, "[USER %ld]: a segmentation fault error happened. Terminating...\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);
    if (aus != NULL)
        free(aus);

    endOfExecution(-1);
}
#pragma endregion
/*** END FUNCTIONS IMPLEMENTATIONS ***/

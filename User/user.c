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
void removeTransaction(TransList *, Transaction *);

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
 * @param sig the parameters value are: 0 -> only end of execution; 1 -> end of execution and deallocation (called from error);
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
TransList * addTransaction(TransList *, Transaction *);

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
    long transCount = 0;
    struct sigaction actEndOfExec;
    struct sigaction actGenTrans;
    sigset_t mask;
    key_t key;
    int tId = -1;
    int trans_gen_time;
    int balance = 100;
    float amount = 0.0;
    float reward = 0.0;
    pid_t receiver_node;
    int receiver_node_index;
    pid_t receiver_user;
    int receiver_user_index;
    Transaction new_tran;
    MsgTP msgT;
    MsgGlobalQueue msgCheckFailedTrans;
    struct timespec remaining, request;

    if (readParams())
    {
        if (allocateMemory())
        {
            if (initializeFacilities())
            {
                if(sigfillset(&mask) == -1)
                {
                    unsafeErrorPrint("User: failed to initialize signal mask. Error: ");
                    endOfExecution(1);
                }
                else
                {
                    actEndOfExec.sa_handler = endOfExecution;
                    actEndOfExec.sa_mask = mask;
                    if(sigaction(SIGUSR1, &actEndOfExec, NULL) == -1)
                    {
                        unsafeErrorPrint("User: failed to set up end of simulation handler. Error: ");
                        endOfExecution(1);
                    }
                    else
                    {
                        actGenTrans.sa_handler = transactionGeneration;
                        actGenTrans.sa_mask = mask;
                        if(sigaction(SIGUSR2, &actGenTrans, NULL) == -1)
                        {
                            unsafeErrorPrint("User: failed to set up transaction generation handler. Error: ");
                            endOfExecution(1);
                        }
                        else
                        {
                            printf("User %5d: starting lifecycle...\n", getpid());
                            
                            /*
                                User's lifecycle
                            */
                            while(TRUE)
                            {
                                /* check on global queue if a sent transaction failed */
                                if(msgrcv(globalQueueId, &msgCheckFailedTrans, sizeof(msgCheckFailedTrans)-sizeof(long), getpid(), IPC_NOWAIT) != -1)
                                {
                                    /* got a message for this user from global queue */
                                    if(msgCheckFailedTrans.msgContent == FAILEDTRANS)
                                    {
                                        /* the transaction failed, so we remove it from the list of sent transactions */
                                        removeTransaction(transactionsSent, &(msgCheckFailedTrans.transaction));
                                    }
                                    else
                                    {
                                        /* the message wasn't the one we were looking for, reinserting it on the global queue */
                                        if(msgsnd(globalQueueId, &msgCheckFailedTrans, sizeof(msgCheckFailedTrans)-sizeof(long), 0) == -1)
                                            unsafeErrorPrint("User: failed to reinsert the message read from global queue while checking for failed transactions. Error: ");
                                    }
                                }
                                else if(errno != ENOMSG)
                                    unsafeErrorPrint("User: failed to check for failed transaction messages on global queue. Error: ");
                                /* else errno == ENOMSG, so no transaction has failed */
                                
                                /* generate a transaction */
                                transactionGeneration(0);
                            }
                        }
                    }
                }
            }
            else
            {
                endOfExecution(1);
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
    TEST_MALLOC_ERROR(regPtrs);

    regPartsIds = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(regPartsIds);

    /*
        noReadersPartitions e noReadersPartitionsPtrs vanno allocati
        perchè sono vettori, quindi dobbiamo allocare un'area di memoria
        abbastanza grande da contenere REG_PARTITION_COUNT interi/puntatori ad interi
    */
    noReadersPartitions = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(noReadersPartitions);

    /*
    Non allochiamo il noReadersPartitionsPtrs[i] perchè esso dovrà
    contenere un puntatore alla shared memory
    */
    noReadersPartitionsPtrs = (int **)calloc(REG_PARTITION_COUNT, sizeof(int *));
    TEST_MALLOC_ERROR(noReadersPartitionsPtrs);

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
    FTOK_TEST_ERROR(key);
    fairStartSem = semget(key, 1, 0600);
    SEM_TEST_ERROR(fairStartSem);

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key);
    wrPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(wrPartSem);

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key);
    rdPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(rdPartSem);

    key = ftok(SEMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key);
    userListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(userListSem);

    key = ftok(SEMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key);
    nodeListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(nodeListSem);

    key = ftok(SEMFILEPATH, PARTMUTEXSEED);
    FTOK_TEST_ERROR(key);
    mutexPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(mutexPartSem);

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    /* Creates the global queue*/
    key = ftok(MSGFILEPATH, GLOBALMSGSEED);
    FTOK_TEST_ERROR(key);
    globalQueueId = msgget(key, 0600);
    MSG_TEST_ERROR(globalQueueId);
    /********************************************************/
    /********************************************************/

    /*****  Initialization of shared memory segments    *****/
    /********************************************************/
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[0]);

    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[1]);

    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[2]);

    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    SHMAT_TEST_ERROR(regPtrs[0], "User");
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    SHMAT_TEST_ERROR(regPtrs[1], "User");
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    SHMAT_TEST_ERROR(regPtrs[2], "User");

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key);
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(usersListId);
    usersList = (ProcListElem *)shmat(usersListId, NULL, SHM_RDONLY);
    SHMAT_TEST_ERROR(usersList, "User");

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key);
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(nodesListId);
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, SHM_RDONLY);
    SHMAT_TEST_ERROR(nodesList, "User");

    /* Aggancio segmenti per variabili condivise*/
    key = ftok(SHMFILEPATH, NOREADERSONESEED);
    FTOK_TEST_ERROR(key);
    noReadersPartitions[0] = shmget(key, sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(nodesListId);
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, 0);
    SHMAT_TEST_ERROR(noReadersPartitionsPtrs[0], "User");

    key = ftok(SHMFILEPATH, NOREADERSTWOSEED);
    FTOK_TEST_ERROR(key);
    noReadersPartitions[1] = shmget(key, sizeof(SO_USERS_NUM), 0600);
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    SHMAT_TEST_ERROR(noReadersPartitionsPtrs[1], "User");

    key = ftok(SHMFILEPATH, NOREADERSTHREESEED);
    FTOK_TEST_ERROR(key);
    noReadersPartitions[2] = shmget(key, sizeof(SO_USERS_NUM), 0600);
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    SHMAT_TEST_ERROR(noReadersPartitionsPtrs[2], "User");

    key = ftok(SHMFILEPATH, NOUSRSEGRDERSSEED);
    FTOK_TEST_ERROR(key);
    noUserSegReaders = shmget(key, sizeof(SO_USERS_NUM), 0600);
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    SHMAT_TEST_ERROR(noUserSegReadersPtr, "User");

    key = ftok(SHMFILEPATH, NONODESEGRDERSSEED);
    FTOK_TEST_ERROR(key);
    noNodeSegReaders = shmget(key, sizeof(SO_USERS_NUM), 0600);
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    SHMAT_TEST_ERROR(noNodeSegReadersPtr, "User");

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
    int i;
    Register *ptr;
    int j;
    int k;
    pid_t procPid = getpid();
    struct sembuf op;
    boolean errBeforeComputing = FALSE, errAfterComputing = FALSE;

    /*
        Soluzione equa al problema dei lettori scrittori
        liberamente ispirata a quella del Professor Gunetti
        Abbiamo fatto questa scelta perchè non è possibile prevedere
        se vi saranno più cicli di lettura o di scrittura e per evitare 
        la starvation degli scrittori o dei lettori.
    */
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
            safeErrorPrint("Node: failed to reserve register partition reading semaphore. Error: ");
            errBeforeComputing = TRUE;
        }
        else 
        {
            if (semop(mutexPartSem, &op, 1) == -1)
            {
                safeErrorPrint("Node: failed to reserve register partition mutex semaphore. Error: ");
                errBeforeComputing = TRUE;
            }
            else 
            {
                *(noReadersPartitionsPtrs[i])++;
                if (*(noReadersPartitionsPtrs[i]) == 1)
                {
                    if (semop(wrPartSem, &op, 1) == -1)
                    {
                        safeErrorPrint("Node: failed to reserve register partition writing semaphore. Error:");
                        errBeforeComputing = TRUE;
                    }
                }

                op.sem_num = i;
                op.sem_op = 1;
                op.sem_flg = 0;

                if (semop(mutexPartSem, &op, 1) == -1)
                {
                    safeErrorPrint("Node: failed to release register partition mutex semaphore. Error: ");
                    errBeforeComputing = TRUE;
                }
                else 
                {
                    if (semop(rdPartSem, &op, 1) == -1)
                    {
                        safeErrorPrint("Node: failed to release register partition reading semaphore. Error: ");
                        errBeforeComputing = TRUE;
                    }
                    else 
                    {
                        /* if we arrive here and errBeforeComputing is true, an error occurred only reserving wrPartSem */
                        if(errBeforeComputing)
                            break; /* we stop the cycle and end balance computation */

                        ptr = regPtrs[i];
                        balance = SO_BUDGET_INIT;
                        while (ptr != NULL)
                        {
                            for (j = 0; j < ptr->nBlocks; j++)
                            {
                                for (k = 0; k < SO_BLOCK_SIZE; k++)
                                {
                                    if (ptr->blockList[j].transList[k].receiver == procPid)
                                    {
                                        balance += ptr->blockList[j].transList[k].amountSend;
                                    }
                                    else if (ptr->blockList[j].transList[k].sender == procPid)
                                    {
                                        /*
                                            Togliamo le transazioni già presenti nel master
                                            dalla lista di quelle inviate
                                        */
                                        balance -= (ptr->blockList[j].transList[k].amountSend) + 
                                                    (ptr->blockList[j].transList[k].reward);
                                        removeTransaction(transSent, &(ptr->blockList[j].transList[k]));
                                    }
                                }
                            }
                            ptr++;
                        }

                        op.sem_num = i;
                        op.sem_op = -1;
                        op.sem_flg = 0;

                        if (semop(mutexPartSem, &op, 1) == -1)
                        {
                            balance = 0;
                            safeErrorPrint("Node: failed to reserve register partition mutex semaphore. Error: ");
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
                                    safeErrorPrint("Node: failed to release register partition writing semaphore. Error: ");
                                    errAfterComputing = TRUE;
                                }
                            }

                            op.sem_num = i;
                            op.sem_op = 1;
                            op.sem_flg = 0;

                            if (semop(mutexPartSem, &op, 1) == -1)
                            {
                                balance = 0;
                                safeErrorPrint("Node: failed to release register partition mutex semaphore. Error: ");
                                userFailure();
                                errAfterComputing = TRUE;
                            }
                            else
                            {
                                /* if we arrive here and errAfterComputing is true, an error occurred only releasing wrPartSem */
                                if(errAfterComputing)
                                {
                                    userFailure(); /* metto questo qui e non nel ramo then dell'if di wrPartSem perché se 
                                                    * si verifica un errore sia in wrPartSem che in mutexPartSem il numero 
                                                    * di fallimenti verrebbe decrementato due volte */
                                    break; /* we stop the cycle and end balance computation */
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
                                    transSent++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if(errBeforeComputing)
    {
        /* an error occurred with wrPartSem or rdPartSem or mutexPartSem before balance computing */
        userFailure();
        balance = 0;
    }

    return balance;
}

/**
 * @brief Function that removes the passed transaction from the list of sent transactions.
 * @param tList a pointer to the list of sent transactions to modify
 * @param t a pointer to the transaction to remove from list
 */
void removeTransaction(TransList *tList, Transaction *t)
{
    TransList *prev = NULL;
    TransList *aus = NULL;
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
                free(tList);
            }
            else
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
                free(tList);
                tList = NULL;
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
            free(prev);
        }
    }
}

/**
 * @brief Function that ends the execution of the user; this can happen in three different ways,
 * rappresented by the values that the parameter might assume.
 * @param sig the parameters value are: 0 -> only end of execution; 1 -> end of execution and deallocation (called from error);
 * SIGUSR1 -> end of execution and deallocation (called by signal from master)
 */
void endOfExecution(int sig)
{
    int exitCode = EXIT_FAILURE;

    deallocateIPCFacilities();
        
    if(sig == SIGUSR1)
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
        kill(getppid(), SIGCHLD);
    }
    
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
    int i = 0;

    /*
        CORREGGERE CON NUOVO MECCANISMO DI RILEVAZIONE ERRORI
    */

    write(STDOUT_FILENO, 
        "User: detaching from register's partitions...\n",
        strlen("User: detaching from register's partitions...\n"));
    
    for(i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if(shmdt(regPtrs[i]) == -1)
        {
            if (errno != EINVAL) {
                /*
                    Implementare un meccanismo di retry??
                    Contando che non è un errore così frequente si potrebbe anche ignorare...
                    Non vale la pena, possiamo limitarci a proseguire la deallocazione
                    riducendo al minimo il memory leak
                */
                safeErrorPrint("User: failed to detach from register's partition. Error: ");
            }
        }
    }
    if(regPtrs != NULL)
        free(regPtrs);
    
    if(regPartsIds != NULL)
        free(regPartsIds);

    write(STDOUT_FILENO, 
        "User: detaching from users list...\n",
        strlen("User: detaching from users list...\n"));

    if(shmdt(usersList) == -1)
    {
        if (errno != EINVAL)
            safeErrorPrint("User: failed to detach from users list. Error: ");
    }

    write(STDOUT_FILENO, 
        "User: detaching from nodes list...\n",
        strlen("User: detaching from nodes list...\n"));

    if(shmdt(nodesList) == -1)
    {
        if (errno != EINVAL)
            safeErrorPrint("User: failed to detach from nodes list. Error: ");
    }

    write(STDOUT_FILENO, 
        "User: detaching from partitions' number of readers shared variable...\n",
        strlen("User: detaching from partitions' number of readers shared variable...\n"));

    for(i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if(shmdt(noReadersPartitionsPtrs[i]) == -1)
        {
            if(errno != EINVAL)
                safeErrorPrint("User: failed to detach from partitions' number of readers shared variable. Error: ");
        }
    }
    if(noReadersPartitions != NULL)
        free(noReadersPartitions);
    
    if(noReadersPartitionsPtrs != NULL)
        free(noReadersPartitionsPtrs);

    write(STDOUT_FILENO, 
        "User: detaching from users list's number of readers shared variable...\n",
        strlen("User: detaching from users list's number of readers shared variable...\n"));
    
    if(shmdt(noUserSegReadersPtr) == -1)
    {
        if(errno != EINVAL)
            safeErrorPrint("User: failed to detach from users list's number of readers shared variable. Error: ");
    }

    write(STDOUT_FILENO, 
        "User: detaching from nodes list's number of readers shared variable...\n",
        strlen("User: detaching from nodes list's number of readers shared variable...\n"));
    
    if(shmdt(noNodeSegReadersPtr) == -1)
    {
        if(errno != EINVAL)
            safeErrorPrint("User: failed to detach from nodes list's number of readers shared variable. Error: ");
    }

    write(STDOUT_FILENO, 
        "User: cleanup operations completed. Process is about to end its execution...\n",
        strlen("User: cleanup operations completed. Process is about to end its execution...\n"));
    
    /* freeing the list of sent transactions */
    freeTransList(transactionsSent);
}

/**
 * @brief Function that computes the budget of the user, generates a transaction, 
 * sends it to a randomly chosen node and then simulates the wait for processing
 * the transaction.
 * @param sig the type of event that triggered the handler, 0 if not called on event
 */
void transactionGeneration(int sig)
{
    int bilancio, queueId;
    Transaction new_trans;
    MsgTP msg_to_node;
    key_t key;
    pid_t receiver_node, receiver_user;
    struct timespec request, remaining;
    MsgGlobalQueue msgOnGQueue;
    struct sembuf sops;
    
    sops.sem_flg = 0;

    bilancio = computeBudget(transactionsSent); /* calcolo del bilancio */
    srand(getpid());

    if(bilancio > 2)
    {
        /* deve essere globale */
        num_failure = 0; /* sono riuscito a mandare la transazione, azzero il counter dei fallimento consecutivi */

        /* Extracts the receiving user randomly */
        receiver_user = extractReceiver(getpid());
        if(receiver_user == -1)
        {
            safeErrorPrint("User: failed to extract user receiver. Error: ");
            userFailure();
        }
        else 
        {
            if(sig == 0)
                write(STDOUT_FILENO, 
                    "User: generating a new transaction...\n", 
                    strlen("User: generating a new transaction...\n"));
            else
                write(STDOUT_FILENO, 
                    "User: generating a new transaction on event request...\n",
                    strlen("User: generating a new transaction on event request...\n"));

            /* Generating transaction */
            new_trans.sender = getpid();
            new_trans.receiver = receiver_user;
            new_trans.amountSend = (rand()%bilancio)+2; /* calcolo del budget fra 2 e il budget (così lo fa solo intero) */
            new_trans.reward = new_trans.amountSend*SO_REWARD; /* se supponiamo che SO_REWARD sia un valore (percentuale) espresso tra 0 e 1 */
            /*new_trans.reward = (new_trans.amountSend/100)*SO_REWARD; /* se supponiamo che SO_REWARD sia un valore (percentuale) espresso tra 1 e 100 */
            if(new_trans.reward < 1)
                new_trans.reward = 1;

            clock_gettime(CLOCK_REALTIME, &new_trans.timestamp); /* get timestamp for transaction */

            /* extracting node which to send the transaction */
            receiver_node = extractNode();
            if(receiver_node == -1)
            {
                safeErrorPrint("User: failed to extract node which to send transaction on TP. Error: ");
                userFailure();
            }
            else
            {
                /* preparing message to send on node's queue */
                msg_to_node.mType = receiver_node;
                msg_to_node.transaction = new_trans;

                /* generating key to retrieve node's queue */
                key = ftok(MSGFILEPATH, receiver_node);
                if(key == -1)
                {
                    safeErrorPrint("User: ftok failed during node's queue retrieving. Error: ");
                    userFailure();
                }
                else
                {
                    /* retrieving the message queue connection */
                    queueId = msgget(key, 0600);
                    if(queueId == -1)
                    {
                        safeErrorPrint("User: failed to connect to node's transaction pool. Error: ");
                        userFailure();
                    }
                    else
                    {
                        /* Inserting new transaction on list of transaction sent */
                        transactionsSent = addTransaction(transactionsSent, &new_trans);
                        
                        /* sending the transaction to node */
                        write(STDOUT_FILENO, 
                            "User: sending the created transaction to the node...\n",
                            strlen("User: sending the created transaction to the node...\n"));

                        if(msgsnd(queueId, &msg_to_node, sizeof(Transaction), IPC_NOWAIT) == -1)
                        {
                            if(errno == EAGAIN)
                            {
                                /* TP of Selected Node was full, we need to send the message on the global queue */
                                write(STDOUT_FILENO, 
                                    "User: transaction pool of selected node was full. Sending transaction on global queue...\n",
                                    strlen("User: transaction pool of selected node was full. Sending transaction on global queue...\n"));
                                
                                msgOnGQueue.mType = receiver_node;
                                msgOnGQueue.msgContent = TRANSTPFULL;
                                msgOnGQueue.transaction = new_trans;
                                msgOnGQueue.hops = 0;
                                if(msgsnd(globalQueueId, &msgOnGQueue, sizeof(msgOnGQueue)-sizeof(long), 0) == -1)
                                {
                                    safeErrorPrint("User: failed to send transaction on global queue. Error: ");
                                    userFailure();
                                }
                            }
                            else
                            {
                                if(sig == 0)
                                    safeErrorPrint("User: failed to send transaction to node. Error: ");
                                else
                                    safeErrorPrint("User: failed to send transaction generated on event to node. Error: ");
                                
                                userFailure();
                            }
                        }
                        else
                        {
                            if(sig == 0)
                                write(STDOUT_FILENO, 
                                    "User: transaction correctly sent to node.\n",
                                    strlen("User: transaction correctly sent to node.\n"));
                            else
                                write(STDOUT_FILENO, 
                                    "User: transaction generated on event correctly sent to node.\n",
                                    strlen("User: transaction generated on event correctly sent to node.\n"));
                            
                            /* Wait a random time in between SO_MIN_TRANS_GEN_NSEC and SO_MAX_TRANS_GEN_NSEC */
                            request.tv_sec = 0;
                            request.tv_nsec = (rand() % SO_MAX_TRANS_GEN_NSEC) + SO_MIN_TRANS_GEN_NSEC;
                            
                            write(STDOUT_FILENO, 
                                "User: processing the transaction...\n",
                                strlen("User: processing the transaction...\n"));
                            
                            if (nanosleep(&request, &remaining) == -1)
                                safeErrorPrint("User: failed to simulate wait for processing the transaction. Error: ");
                            
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
        write(STDOUT_FILENO, 
            "User: not enough money to make a transaction...\n",
            strlen("User: not enough money to make a transaction...\n"));
        
        userFailure();
    }
}

/**
 * @brief Function that increases by one the number of failure counter of the user while attempting
 * to create a transaction and if the counter is equal to SO_RETRY, the simulation must terminate.
 */
void userFailure()
{
    write(STDOUT_FILENO, 
            "User: failed to create a transaction, increasing number of failure counter.\n",
            strlen("User: failed to create a transaction, increasing number of failure counter.\n"));
        
    num_failure++; /* incremento il numero consecutivo di volte che non riesco a mandare una transazione */
    if(num_failure == SO_RETRY)
    {
        /* non sono riuscito a mandare la transazione per SO_RETRY volte, devo terminare */
        endOfExecution(1);
    }
}

/**
 * @brief Function that adds the transaction passed as second argument to the list of sent transactions
 * passed as first argument.
 * @param transSent a pointer to the list of sent transactions
 * @param t a pointer to the transaction to add to the list
 * @return returns a pointer to the new head of the list of sent transactions
 */
TransList * addTransaction(TransList * transSent, Transaction *t)
{
    if(t == NULL) {
        write(STDERR_FILENO, 
            "User: transaction passed to function is a NULL pointer.\n",
            strlen("User: transaction passed to function is a NULL pointer.\n"));
        return NULL;
    }

    /* insertion of new transaction to list */
    TransList * new_el = (TransList*) malloc(sizeof(TransList));
    new_el->currTrans = *t;
    new_el->nextTrans = transSent;
    transSent = new_el;
    
    return transSent;
}

/**
 * @brief Function that deallocates the list of sent transactions.
 * @param transSent a pointer to the list of sent transactions to deallocate
 */
void freeTransList(TransList * transSent)
{
    if(transSent == NULL)
        return;

    freeTransList(transSent->nextTrans);
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
                safeErrorPrint("User: failed to reserve write usersList semaphore. Error: ");
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                return (pid_t)-1;
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if (semop(userListSem, &sops, 1) != -1)
        {
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
                        safeErrorPrint("User: failed to release write usersList semaphore. Error: ");
                        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                        return (pid_t)-1;
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
                    safeErrorPrint("User: failed to release mutex usersList semaphore. Error: ");
                    /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                }
            }
            else
            {
                safeErrorPrint("User: failed to reserve mutex usersList semaphore. Error: ");
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
            }
        }
        else
        {
            safeErrorPrint("User: failed to release mutex usersList semaphore. Error: ");
            /* restituiamo -1 e contiamo come fallimento di invio di transazione */
        }
    }
    else
    {
        safeErrorPrint("User: failed to reserve mutex usersList semaphore. Error: ");
        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
    }

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
    sops.sem_flg = 0;

    sops.sem_num = 0; 
    sops.sem_op = -1;
    if(semop(nodeListSem, &sops, 1) != -1)
    {
        (*noNodeSegReadersPtr)++;
        if((*noNodeSegReadersPtr) == 1)
        {
            sops.sem_num = 2;
            sops.sem_op = -1;
            if(semop(nodeListSem, &sops, 1) == -1)
            {
                safeErrorPrint("User: failed to reserve write nodesList semaphore. Error: ");
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                return (pid_t)-1;
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if(semop(nodeListSem, &sops, 1) != -1)
        {
            do
            {
                clock_gettime(CLOCK_REALTIME, &now);
                n = now.tv_nsec % SO_NODES_NUM;
            } while (nodesList[n].procState != ACTIVE);
            /* cicla finché il nodo scelto casualmente non è attivo */
            
            pid_to_return = nodesList[n].procId;

            sops.sem_num = 0;
            sops.sem_op = -1;
            if(semop(nodeListSem, &sops, 1) != -1)
            {
                (*noNodeSegReadersPtr)--;
                if((*noNodeSegReadersPtr) == 0)
                {
                    sops.sem_num = 2;
                    sops.sem_op = 1;
                    if(semop(nodeListSem, &sops, 1) == -1)
                    {
                        safeErrorPrint("User: failed to release write nodesList semaphore. Error: ");
                        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                        return (pid_t)-1;
                    }
                }

                sops.sem_num = 0;
                sops.sem_op = 1;
                if(semop(nodeListSem, &sops, 1) != -1)
                {
                    return pid_to_return;
                }
                else
                {
                    safeErrorPrint("User: failed to release mutex nodesList semaphore. Error: ");
                    /* restituiamo -1 e contiamo come fallimento di invio di transazione */
                }
            }
            else
            {
                safeErrorPrint("User: failed to reserve mutex nodesList semaphore. Error: ");
                /* restituiamo -1 e contiamo come fallimento di invio di transazione */
            }
        }
        else
        {
            safeErrorPrint("User: failed to release mutex nodesList semaphore. Error: ");
            /* restituiamo -1 e contiamo come fallimento di invio di transazione */
        }
    }
    else
    {
        safeErrorPrint("User: failed to reserve mutex nodesList semaphore. Error: ");
        /* restituiamo -1 e contiamo come fallimento di invio di transazione */
    }

    return (pid_t)-1;
}
#pragma endregion
/*** END FUNCTIONS IMPLEMENTATIONS ***/
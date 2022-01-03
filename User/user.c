#include "user.h"

Register **regPtrs = NULL;
int *regPartsIds = NULL;

int usersListId = -1;
ProcListElem *usersList = NULL;

int nodesListId = -1;
ProcListElem *nodesList = NULL;

/*
    Serve perchè su di essa potrebbe arrivare
    la notifica del fallimento di una transazione nel caso in cui
    il master non possa aggiornare la lista utenti perchè
    un utente la sta già leggendo, vedendo ancora attivo
    un utente in realtà terminato (vale la pena fare tutto ciò??)
*/
/*** Global variables for IPC ***/
int globalQueueId = -1;

int fairStartSem = -1; /* Id of the set that contais the three semaphores*/
                       /* used to write on the register's partitions*/
int wrPartSem = -1;    /* Id of the set that contais the three semaphores*/
                       /* used to write on the register's partitions*/
int rdPartSem = -1;    /* Id of the set that contais the three semaphores*/
                       /* used to read from the register's partitions*/
int mutexPartSem = -1; /* id of the set that contains the three sempagores used to
                        to access the number of readers variables of the registers partitions
                        in mutual exclusion*/

int *noReadersPartitions = NULL;      /* Pointer to the array contains the ids of the shared memory segments
                                // where the variables used to syncronize
                                 // readers and writes access to register's partition are stored
// noReadersPartitions[0]: id of first partition's shared variable
// noReadersPartitions[1]: id of second partition's shared variable
// noReadersPartitions[2]: id of third partition's shared variable*/
int **noReadersPartitionsPtrs = NULL; /* Pointer to the array contains the variables used to syncronize
                                  // readers and writes access to register's partition
// noReadersPartitionsPtrs[0]: pointer to the first partition's shared variable
// noReadersPartitionsPtrs[1]: pointer to the second partition's shared variable
// noReadersPartitionsPtrs[2]: pointer to the third partition's shared variable*/
int userListSem = -1;                 /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                    // to read and write users list*/
int noUserSegReaders = -1;            /* id of the shared memory segment that contains the variable used to syncronize
                           // readers and writes access to users list*/
int *noUserSegReadersPtr = NULL;

/*
    Non serve una variabile per contare il lettori perchè nessuno può leggere o scrivere
*/
int nodeListSem = -1; /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                      // to read and write nodes list*/
/*** End Global variables for IPC ***/

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
/*
    Meglio usare long: i valori potrebbero essere molto grandi
*/
long SO_USERS_NUM, /* Number of user processes NOn è "statico" ???*/
    SO_NODES_NUM,  /* Number of node processes ?? NOn è "statico" ???*/
    SO_REWARD,
    SO_MIN_TRANS_GEN_NSEC,
    SO_MAX_TRANS_GEN_NSEC,
    SO_RETRY,
    SO_TP_SIZE,
    SO_MIN_TRANS_PROC_NSEC,
    SO_MAX_TRANS_PROC_NSEC,
    SO_BUDGET_INIT,
    SO_SIM_SEC,     /* Duration of the simulation*/
    SO_FRIENDS_NUM, /* Number of friends*/
    SO_HOPS;
/*******************************************************/
/*******************************************************/

boolean readParams();
boolean allocateMemory();
boolean initializeFacilities();
double computeBalance(TransList *);
void removeTransaction(TransList *, Transaction *);

int main()
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
    TransList *transactionsSent = NULL;
    long transCount = 0;
    if (readParams())
    {
        if (allocateMemory())
        {
            if (initializeFacilities())
            {
                /*
                    User's lifecycle
                */
                while (TRUE)
                {
                }
            }
            else
            {
                /*
                    Fine esecuzione + deallocazione
                */
            }
        }
        else
        {
            /*
            Fine esecuzione
        */
        }
    }
    else
    {
        /*
            Fine esecuzione
        */
    }

    return 0;
}

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

boolean allocateMemory()
{
    /* CORREGGERE USANDO CALLOC E FARE SEGNALAZIONE ERRORI
        (in caso di errore deallocare quanto già allocato e terminare la simulazione ??)
     */
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

    noReadersPartitions[1] = shmget(ftok(SHMFILEPATH, NOREADERSTWOSEED), sizeof(SO_USERS_NUM), 0600);
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    SHMAT_TEST_ERROR(noReadersPartitionsPtrs[1], "User");

    noReadersPartitions[2] = shmget(ftok(SHMFILEPATH, NOREADERSTHREESEED), sizeof(SO_USERS_NUM), 0600);
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    SHMAT_TEST_ERROR(noReadersPartitionsPtrs[2], "User");

    noUserSegReaders = shmget(ftok(SHMFILEPATH, NOUSRSEGRDERSSEED), sizeof(SO_USERS_NUM), 0600);
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    SHMAT_TEST_ERROR(noUserSegReadersPtr, "User");

    return TRUE;
}

double computeBalance(TransList *transSent)
{
    double balance = SO_BUDGET_INIT;
    int i;
    Register *ptr;
    int j;
    int k;
    pid_t procPid = getpid();

    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        ptr = regPtrs[i];
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
                        balance -= ptr->blockList[j].transList[k].amountSend + ptr->blockList[j].transList[k].reward;
                        removeTransaction(transSent, &(ptr->blockList[j].transList[k]));
                    }
                }
            }
            ptr++;
        }

        /*
            Precondizione: da transSent sono state eliminate le transazioni
            già registrate nel master
        */
        while (transSent != NULL)
        {
            balance -= transSent->currTrans->amountSend + transSent->currTrans->reward;
            transSent++;
        }
    }

    return balance;
}

void removeTransaction(TransList *tList, Transaction *t)
{
    TransList *prev = NULL;
    boolean done = FALSE;

    while (tList != NULL && !done)
    {
        /*
            CORREGGERE: i tempi si possono confrontare direttamente???
        */
        if (tList->currTrans->timestamp.tv_nsec == t->timestamp.tv_nsec &&
            tList->currTrans->sender == t->sender &&
            tList->currTrans->receiver == t->receiver)
        {
            if (prev != NULL)
            {
                prev->nextTrans = tList->nextTrans;
            }
            else
            {
                /*
                    Caso in cui la lista contiene un solo elemento
                    (quindi tList->nextTrans è gia NULL)
                */
                tList->currTrans = NULL;
            }

            done = TRUE;
        }
        else
        {
            prev = tList;
            tList = tList->nextTrans;
        }
    }
}
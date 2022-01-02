#include "node.h"

/*****        Global structures        *****/
/*******************************************/
Register **regPtrs = NULL;
int *regPartsIds = NULL;

int usersListId = -1;
ProcListElem *usersList = NULL;

int nodesListId = -1;
ProcListElem *nodesList = NULL;

TPElement *tpList = NULL;

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

/* Si dovrebbe fare due vettori*/
int *noReadersPartitions = NULL;      /* Pointer to the array contains the ids of the shared memory segments
                                // where the variables used to syncronize
                                 // readers and writes access to register's partition are stored
// noReadersPartitions[0]: id of first partition's shared variable
// noReadersPartitions[1]: id of second partition's shared variable
// noReadersPartitions[2]: id of third partition's shared variable*/
int **noReadersPartitionsPtrs = NULL; /* Pointer to the array contains the variables used to syncronize
                                  // readers and writes access to register's partition
// noReadersPartitions[0]: pointer to the first partition's shared variable
// noReadersPartitions[1]: pointer to the second partition's shared variable
// noReadersPartitions[2]: pointer to the third partition's shared variable*/
int userListSem = -1;                 /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                    // to read and write users list*/
int noUserSegReaders = -1;            /* id of the shared memory segment that contains the variable used to syncronize
                           // readers and writes access to users list*/
int *noUserSegReadersPtr = NULL;

int nodeListSem = -1; /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                      // to read and write nodes list*/

/*
    ATTENZIONE AL TIPO DI DATO!!!
*/
int noTerminated = 0; /* NUmber of processes that terminated before end of simulation*/

void freeGlobalVariables();
/******************************************/

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
int SO_USERS_NUM, /* Number of user processes NOn è "statico" ???*/
    SO_NODES_NUM, /* Number of node processes ?? NOn è "statico" ???*/
    SO_REWARD,
    SO_MIN_TRANS_GEN_NSEC,
    SO_MAX_TRANS_GEN_NSEC,
    SO_RETRY,
    SO_TP_SIZE,
    SO_MIN_TRANS_PROC_NSEC,
    SO_MAX_TRANS_PROC_NSEC,
    SO_BUDGET_INIT,
    SO_SIM_SEC,     /* Duration of the simulation*/
    SO_FRIENDS_NUM; /* Number of friends*/
/*******************************************************/
/*******************************************************/

/***** Function that assigns the values ​​of the environment *****/
/***** variables to the global variables defined above     *****/
/***************************************************************/
void assignEnvironmentVariables()
{
    SO_USERS_NUM = atoi(getenv("SO_USERS_NUM"));
    SO_NODES_NUM = atoi(getenv("SO_NODES_NUM"));
    SO_REWARD = atoi(getenv("SO_REWARD"));
    SO_MIN_TRANS_GEN_NSEC = atoi(getenv("SO_MIN_TRANS_GEN_NSEC"));
    SO_MAX_TRANS_GEN_NSEC = atoi(getenv("SO_MAX_TRANS_GEN_NSEC"));
    SO_RETRY = atoi(getenv("SO_RETRY"));
    SO_TP_SIZE = atoi(getenv("SO_TP_SIZE"));
    SO_MIN_TRANS_PROC_NSEC = atoi(getenv("SO_MIN_TRANS_PROC_NSEC"));
    SO_MAX_TRANS_PROC_NSEC = atoi(getenv("SO_MAX_TRANS_PROC_NSEC"));
    SO_BUDGET_INIT = atoi(getenv("SO_BUDGET_INIT"));
    SO_SIM_SEC = atoi(getenv("SO_SIM_SEC"));
    SO_FRIENDS_NUM = atoi(getenv("SO_FRIENDS_NUM"));
}
/***************************************************************/
/***************************************************************/

int readConfigParameters()
{
    char *filename = "params.txt";
    FILE *fp = fopen(filename, "r");
    /* Reading line by line, max 128 bytes*/
    const unsigned MAX_LENGTH = 128;
    /* Array that will contain the lines read from the file
    // each "row" of the "matrix" will contain a different file line*/
    char line[14][128];
    /* Counter of the number of lines in the file*/
    int k = 0;
    char *aus = NULL;
    int exitCode = 0;
    int i = 0;

    /* Handles any error in opening the file*/
    if (fp == NULL)
    {
        sprintf(aus, "Error: could not open file %s", filename);
        unsafeErrorPrint(aus);
        exitCode = -1;
    }
    else
    {
        /* Inserts the lines read from the file into the array*/
        while (fgets(line[k], MAX_LENGTH, fp))
            k++;

        /* It inserts the parameters read into environment variables*/
        for (i = 0; i < k; i++)
            putenv(line[i]);

        /* Assigns the values ​​of the environment
        // variables to the global variables defined above*/
        assignEnvironmentVariables();

        /* Close the file*/
        fclose(fp);
    }

    return exitCode;
}

/****   Function that creates the ipc structures used in the project    *****/
/****************************************************************************/
boolean createIPCFacilties()
{
    boolean ret = FALSE;

    /* CORREGGERE USANDO CALLOC E FARE SEGNALAZIONE ERRORI
     */
    regPtrs = (Register **)malloc(REG_PARTITION_COUNT * sizeof(Register *));
    regPartsIds = (int *)malloc(REG_PARTITION_COUNT * sizeof(int));

    tpList = (TPElement *)malloc(SO_NODES_NUM * sizeof(TPElement));

    noReadersPartitions = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    if (noReadersPartitions == NULL)
        unsafeErrorPrint("Master: failed to allocate shared variables' ids array. ");
    else
    {
        noReadersPartitionsPtrs = (int **)calloc(REG_PARTITION_COUNT, sizeof(int *));
        if (noReadersPartitionsPtrs == NULL)
            unsafeErrorPrint("Master: failed to allocate shared variables' array. ");
        else
        {
            ret = TRUE;
        }
    }

    return ret;
}
/****************************************************************************/
/****************************************************************************/

/*****  Function that initialize the ipc structures used in the project *****/
/****************************************************************************/
void initializeIPCFacilities()
{
    /* Initialization of semaphores*/
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

    key = ftok(SEMFILEPATH, NODELISTSEED);
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
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key);
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(usersListId);
    usersList = (ProcListElem *)shmat(usersListId, NULL, 0);

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key);
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(nodesListId);
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, 0);

    /* Aggiungere segmenti per variabili condivise*/
    noReadersPartitions[0] = shmget(ftok(SHMFILEPATH, NOREADERSONESEED), sizeof(SO_USERS_NUM), 0600);
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, 0);
    /**(noReadersPartitionsPtrs[0]) = 0; */

    noReadersPartitions[1] = shmget(ftok(SHMFILEPATH, NOREADERSTWOSEED), sizeof(SO_USERS_NUM), 0600);
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    /**(noReadersPartitionsPtrs[1]) = 0;*/

    noReadersPartitions[2] = shmget(ftok(SHMFILEPATH, NOREADERSTHREESEED), sizeof(SO_USERS_NUM), 0600);
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    /**(noReadersPartitionsPtrs[2]) = 0;*/

    noUserSegReaders = shmget(ftok(SHMFILEPATH, NOUSRSEGRDERSSEED), sizeof(SO_USERS_NUM), 0600);
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    /**noUserSegReadersPtr = 0;*/
    /********************************************************/
    /********************************************************/
}
/****************************************************************************/
/****************************************************************************/

int main()
{
    int exitCode = EXIT_FAILURE;
    struct msgbuff mybuf;
    struct sembuf sops[3];
    int num_bytes = 0;
    long rcv_type;
    pid_t *friends_node;
    int contMex = 0;
    int i;

    /*Set common semaphore options*/
    sops[0].sem_num = 0;
    sops[0].sem_flg = 0;
    sops[1].sem_num = 2;
    sops[1].sem_flg = 0;
    sops[2].sem_num = 2;
    sops[2].sem_flg = 0;

    /* Assigns the values ​​of the environment variables to the global variables */
    readConfigParameters();

    /* Allocate the array that will contain friends pid */
    friends_node = calloc(SO_NODES_NUM, sizeof(pid_t));
    printf("Node %d: hooking up of IPC facilitites...\n", getpid());

    /* Hooks IPC Resources */
    if (createIPCFacilties() == TRUE)
    {
        initializeIPCFacilities();

        /* Receives all friends pid from global message queue and stores them in the array */
        while (contMex < SO_FRIENDS_NUM)
        {
            rcv_type = getpid();
            num_bytes = msgrcv(globalQueueId, &mybuf, sizeof(pid_t), rcv_type, 0);
            friends_node[contMex] = mybuf.pid;
            contMex++;
        }

        /*
            PROVVISORIO, CORREGGERE
        */
        for (i = 0; i < SO_FRIENDS_NUM; i++)
        {
            printf("Nodo %d -> Amico: %d\n", getpid(), friends_node[i]);
        }

        /* Wait all processes are ready to start the simulation */
        printf("Node %ld is waiting for simulation to start....\n", (long)getpid());
        sops[0].sem_op = 0;
        sops[0].sem_num = 0;
        sops[0].sem_flg = 0;
        semop(fairStartSem, &sops[0], 1);

        printf("Sono il nodo %d -> Eseguo!\n", getpid());
        printf("Node done %d!\n", getpid());
    }
    else
    {
        freeGlobalVariables();
    }
    exit(EXIT_SUCCESS);
}

void freeGlobalVariables()
{
    int i;

    free(regPartsIds);
    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(regPtrs[i]) == -1)
        {
            fprintf(stderr, "%s: %d. Errore in shmdt #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    
    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(noReadersPartitionsPtrs[i]) == -1)
        {
            fprintf(stderr, "%s: %d. Errore in shmdt #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    free(noReadersPartitions);

    if (shmdt(usersList) == -1)
    {
        fprintf(stderr, "%s: %d. Errore in shmdt #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (shmdt(nodesList) == -1)
    {
        fprintf(stderr, "%s: %d. Errore in shmdt #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    free(tpList);
}
/*
    Cose da fare:
        Test: Testare persistenza associazione: 
            Su Linux funziona (purtroppo :D) senza bisogno
            di riagganciare l'handler
            PROVARE SU ALTRI SO
        Refactoring
        Handler CTRL + C
        Handler per graceful termination
        REG_PARTITION_SIZE: OK
        Correzione stampe in handler
        Modifica procedure d'errore in modo che __LINE__ sia indicativo
        e modifica macro che stampa l'errore
        Sistemare kill: Ok
        Vedere perchè ci siano più processi master: A quanto pare non ci sono
        Fare in modo che kill non segnali il master
*/
#define _GNU_SOURCE
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "info.h"

#define NO_ATTEMPS 3 /* Maximum number of attemps to terminate the simulation*/

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

/**********  Function prototypes  *****************/
void endOfSimulation(int);
void printBudget();
void deallocateFacilities(int *);
void freeGlobalVariables();
/**************************************************/

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
int noEffective = 0; /* Holds the effective number of child processes: must be implemented when a new node is created */
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

/*** Functions declaration ***/
void endOfSimulation(int);
void printBudget();
void deallocateFacilities(int *);
/*****************************/

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

struct timespec now;
int *extractedFriendsIndex;
extern char ** environ;

/***** Function that reads the file containing the configuration    *****/
/***** parameters to save them as environment variables             *****/
/************************************************************************/
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
/************************************************************************/
/************************************************************************/

/****   Function that creates the ipc structures used in the project    *****/
/****************************************************************************/
boolean createIPCFacilties()
{
    boolean ret = FALSE;

    /* CORREGGERE USANDO CALLOC E FARE SEGNALAZIONE ERRORI
     */
    regPtrs = (Register **)malloc(REG_PARTITION_COUNT * sizeof(Register *));
    /*
        È sbagliato: con questo ciclo allochiamo già le partizioni
        sullo heap del processo master, mentre noi vogliamo
        allocarle nel segmento di memoria condiviso, avvinchè siano accessibili
        da più processi.
        È quindi sufficiente la prima malloc, con la quale allochiamo lo spazio
        per tre puntatori a dati di tipo register, che poi  valorizzeremo
        con i risultati di shat
    for (int i = 0; i < REG_PARTITION_COUNT; i++)
        regPtrs[i] = (Register *)malloc(REG_PARTITION_SIZE * sizeof(Register));*/
    regPartsIds = (int *)malloc(REG_PARTITION_COUNT * sizeof(int));

    /*CORREGGERE: NON SERVE LA MALLOC, perchè usiamo un segmento di memoria condivisa*/
    /*usersList = (ProcListElem *)malloc(SO_USERS_NUM * sizeof(ProcListElem));*/

    /*CORREGGERE: NON SERVE LA MALLOC, perchè usiamo un segmento di memoria condivisa*/
    /*nodesList = (ProcListElem *)malloc(SO_NODES_NUM * sizeof(ProcListElem));*/

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
            /* we need to do this to reserve some space where to store
            the beginning address of each segment*/
            /*
                Sbagliato anche questo per gli stessi motivi di cui sopra
            */
            /*
            for (j = 0; j < REG_PARTITION_COUNT && ret; j++){
                noReadersPartitionsPtrs[j] = malloc(sizeof(int *));
                if (noReadersPartitionsPtrs[j] == NULL){
                    safeErrorPrint("Master: failed to allocate shared variables' pointers array. ");
                    ret = FALSE;
                }
            }*/
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
    /*
        Sostituire la macro attuale con un meccanismo che deallochi
        le risorse IPC in caso di errore
    */
    union semun arg;
    unsigned short aux[REG_PARTITION_COUNT] = {1, 1, 1};
    /* Initialization of semaphores*/
    key_t key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key);
    /*
        CORREGGERE AGGIUNGENDO METTERE IPC_CREAT ED IPC_EXCL!!! per non correre
        il rischio di legere dati sporchi
    */
    fairStartSem = semget(key, 1, IPC_CREAT | 0600);
    SEM_TEST_ERROR(fairStartSem);

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key);
    wrPartSem = semget(key, 3, IPC_CREAT | 0600);
    SEM_TEST_ERROR(wrPartSem);

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key);
    rdPartSem = semget(key, 3, IPC_CREAT | 0600);
    SEM_TEST_ERROR(rdPartSem);

    key = ftok(SEMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key);
    userListSem = semget(key, 3, IPC_CREAT | 0600);
    SEM_TEST_ERROR(userListSem);

    key = ftok(SEMFILEPATH, NODELISTSEED);
    FTOK_TEST_ERROR(key);
    nodeListSem = semget(key, 3, IPC_CREAT | 0600);
    SEM_TEST_ERROR(nodeListSem);

    key = ftok(SEMFILEPATH, PARTMUTEXSEED);
    FTOK_TEST_ERROR(key);
    mutexPartSem = semget(key, 3, IPC_CREAT | 0600);
    SEM_TEST_ERROR(mutexPartSem);

    arg.val = SO_USERS_NUM + SO_NODES_NUM; /*+1*/
    semctl(fairStartSem, 0, SETVAL, arg);

    arg.array = aux;
    semctl(rdPartSem, 0, SETALL, arg);

    aux[0] = SO_USERS_NUM + SO_NODES_NUM + 1;
    aux[1] = SO_USERS_NUM + SO_NODES_NUM + 1;
    aux[2] = SO_USERS_NUM + SO_NODES_NUM + 1;
    arg.array = aux;
    semctl(wrPartSem, 0, SETALL, arg);

    arg.val = 1;
    semctl(userListSem, 0, SETVAL, arg); /* mutex*/
    arg.val = 0;                         /*CORREGGERE mettendolo nel master, prima della sleep su fairStart*/
    semctl(userListSem, 1, SETVAL, arg); /* read*/
    arg.val = 1;
    semctl(userListSem, 2, SETVAL, arg); /* write*/

    arg.val = 1;
    semctl(nodeListSem, 0, SETVAL, arg); /* mutex*/
    arg.val = 0;                         /*CORREGGERE*/
    semctl(nodeListSem, 1, SETVAL, arg); /* read*/
    arg.val = 1;
    semctl(nodeListSem, 2, SETVAL, arg); /* write*/

    aux[0] = aux[1] = aux[2] = 1;
    arg.array = aux;
    semctl(mutexPartSem, 0, SETALL, arg);

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    /* Creates the global queue*/
    key = ftok(MSGFILEPATH, GLOBALMSGSEED);
    globalQueueId = msgget(key, IPC_CREAT | IPC_EXCL | 0600);
    MSG_TEST_ERROR(globalQueueId);
    /********************************************************/
    /********************************************************/

    /*****  Initialization of shared memory segments    *****/
    /********************************************************/
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(regPartsIds[0]);
    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(regPartsIds[1]);
    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(regPartsIds[2]);
    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    regPtrs[0]->nBlocks = 2;
    regPtrs[1]->nBlocks = 2;
    regPtrs[2]->nBlocks = 2;

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key);
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), IPC_CREAT | S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(usersListId);
    usersList = (ProcListElem *)shmat(usersListId, NULL, 0);

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key);
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), IPC_CREAT | S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(nodesListId);
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, 0);

    /* Aggiungere segmenti per variabili condivise*/
    noReadersPartitions[0] = shmget(ftok(SHMFILEPATH, NOREADERSONESEED), sizeof(SO_USERS_NUM), IPC_CREAT | S_IRUSR | S_IWUSR);
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, 0);
    *(noReadersPartitionsPtrs[0]) = 0;

    noReadersPartitions[1] = shmget(ftok(SHMFILEPATH, NOREADERSTWOSEED), sizeof(SO_USERS_NUM), IPC_CREAT | S_IRUSR | S_IWUSR);
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    *(noReadersPartitionsPtrs[1]) = 0;

    noReadersPartitions[2] = shmget(ftok(SHMFILEPATH, NOREADERSTHREESEED), sizeof(SO_USERS_NUM), IPC_CREAT | S_IRUSR | S_IWUSR);
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    *(noReadersPartitionsPtrs[2]) = 0;

    noUserSegReaders = shmget(ftok(SHMFILEPATH, NOUSRSEGRDERSSEED), sizeof(SO_USERS_NUM), IPC_CREAT | S_IRUSR | S_IWUSR);
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    *noUserSegReadersPtr = 0;
    /********************************************************/
    /********************************************************/
}
/****************************************************************************/
/****************************************************************************/

/*****  Momentary functions created for testing purposes  *****/
/**************************************************************/
void busy_cpu(unsigned long loops)
{
    int i;
    double my_var = 0.25;

    while (1)
    {
        my_var += 0.5;
    }

    /*
        for (i = 0; i < loops; i++)
    {

        //my_var = my_var > 1 ? my_var - 1 : my_var;
    }
    */
}

void do_stuff(int t)
{
    if (t == 1)
        printf("Hi, I'm a user, my pid is %d\n", getpid());
    else
        printf("Hi, I'm a node, my pid is %d\n", getpid());
    /*srand(time(0));*/

    regPtrs[0]->nBlocks = REG_PARTITION_SIZE;
    regPtrs[1]->nBlocks = REG_PARTITION_SIZE;
    regPtrs[2]->nBlocks = REG_PARTITION_SIZE;
    /*busy_cpu(rand() % 1000000000);*/
}
/**************************************************************/
/**************************************************************/

void estrai(int k)
{
    int x, p;
    int count;
    int i = 0;

    int n;

    for (count = 0; count < SO_FRIENDS_NUM; count++)
    {
        do
        {
            clock_gettime(CLOCK_REALTIME, &now);
            n = now.tv_nsec % SO_NODES_NUM;
        } while (k == n);
        extractedFriendsIndex[count] = n;
    }

    while (i < SO_FRIENDS_NUM)
    {
        int r;
        do
        {
            clock_gettime(CLOCK_REALTIME, &now);
            r = now.tv_nsec % SO_NODES_NUM;
        } while (r == k);

        for (x = 0; x < i; x++)
        {
            if (extractedFriendsIndex[x] == r)
            {
                break;
            }
        }
        if (x == i)
        {
            extractedFriendsIndex[i++] = r;
        }
    }
}

void tmpHandler(int sig);

int main(int argc, char *argv[])
{
    pid_t child_pid;
    int status;
    struct sembuf sops[3];
    struct msgbuff mybuf;
    sigset_t set;
    struct sigaction act;
    int fullRegister = TRUE;
    int exitCode = EXIT_FAILURE;
    key_t key;
    int i = 0, j = 0;

    char *argVec[] = {NULL};
    char *envVec[] = {NULL};

    /* Set common semaphore options*/
    sops[0].sem_num = 0;
    sops[0].sem_flg = 0;
    sops[1].sem_num = 2;
    sops[1].sem_flg = 0;
    sops[2].sem_num = 2;
    sops[2].sem_flg = 0;

    extractedFriendsIndex = (int *)malloc(SO_FRIENDS_NUM * sizeof(int));

    printf("PID MASTER: %ld\n", (long)getpid());
    printf("Master: setting up simulation timer...\n");
    /* No previous alarms were set, so it must return 0*/
    /*
        CORREGGERE, va letta da file
    */
    /* SO_SIM_SEC = 200; */
    printf("Master simulation lasts %d seconnds\n", SO_SIM_SEC);
    if (alarm(SO_SIM_SEC) != 0)
        unsafeErrorPrint("Master: failed to set simulation timer. ");
    else
    {
        printf("Master: simulation alarm initilized successfully.\n");
        printf("Master: setting up signal mask...\n");
        if (sigfillset(&set) == -1)
            unsafeErrorPrint("Master: failed to initialize signals mask. Error: ");
        else
        {
            /* We block all the signals during the execution of the handler*/
            act.sa_handler = endOfSimulation;
            act.sa_mask = set;
            printf("Master: signal mask initialized successfully.\n");

            printf("Master: setting end of timer disposition...\n");
            if (sigaction(SIGALRM, &act, NULL) == -1)
                unsafeErrorPrint("Master: failed to set end of timer disposition. Error: ");
            else
            {
                printf("Master: End of timer disposition initialized successfully.\n");
                printf("Master: setting end of simulation disposition...\n");
                if (sigaction(SIGUSR1, &act, NULL) == -1)
                    unsafeErrorPrint("Master: failed to set end of simulation disposition. Error: ");
                else
                {
                    /* Read configuration parameters from
                    // file and save them as environment variables*/
                    printf("Master: reading configuration parameters...\n");
                    if (readConfigParameters() == -1)
                        exit(EXIT_FAILURE);
                    else
                        printf("Master: configuration parameters read successfully!!!\n");

                    noEffective = SO_NODES_NUM + SO_USERS_NUM;
                    noTerminated = 0;
                    /*****  Creates and initialize the IPC Facilities   *****/
                    /********************************************************/
                    printf("Master: creating IPC facilitites...\n");
                    if (createIPCFacilties() == TRUE)
                    {
                        printf("Master: initializating IPC facilitites...\n");
                        /*
                            Fare una funzione che ritorna un valore come createIPCFacilties
                            in modo da poter eliminare le facilities IPC in caso di errore
                        */
                        initializeIPCFacilities();
                        /********************************************************/
                        /********************************************************/

                        /*****  Creates SO_USERS_NUM children   *****/
                        /********************************************/
                        printf("Master: forking user processes...\n");
                        for (i = 0; i < SO_USERS_NUM; i++)
                        {
                            printf("Master: user number %d\n", i);
                            /*
                                CORREGGERE: manca l'error handling
                                e tutte queste semop son ogiuste??
                            */
                            switch (child_pid = fork())
                            {
                            case -1:
                                /*Handle error*/
                                unsafeErrorPrint("Master: fork failed. Error: ");
                                exit(EXIT_FAILURE);
                            case 0:
                                /*
                                // The process tells the father that it is ready to run
                                // and that it waits for all processes to be ready*/
                                printf("User of PID %ld starts its execution....\n", (long)getpid());
                                /*
                                    For test's sake
                                */
                                signal(SIGALRM, SIG_IGN);
                                signal(SIGUSR1, tmpHandler);
                                /*
                                sops[0].sem_op = -1;
                                semop(fairStartSem, &sops[0], 1);*/

                                printf("User %d is waiting for simulation to start....\n", i);
                                sops[0].sem_op = 0;
                                sops[0].sem_num = 0;
                                sops[0].sem_flg = 0;
                                semop(fairStartSem, &sops[0], 1);

                                /* Temporary part to get the process to do something*/
                                do_stuff(1);
                                printf("User done! PID:%d\n", getpid());
                                /*exit(i);*/
                                busy_cpu(1);
                                break;

                            default:
                                sops[0].sem_num = 0;
                                sops[0].sem_op = -1;
                                sops[0].sem_flg = IPC_NOWAIT;
                                semop(fairStartSem, &sops[0], 1);

                                /* Save users processes pid and state into usersList*/
                                sops[1].sem_op = -1;
                                sops[1].sem_num = 2;
                                semop(userListSem, &sops[1], 1);

                                usersList[i].procId = child_pid;
                                usersList[i].procState = ACTIVE;

                                sops[1].sem_op = 1;
                                sops[1].sem_num = 2;
                                semop(userListSem, &sops[1], 1);

                                break;
                            }
                        }
                        /********************************************/
                        /********************************************/

                        printf("Master: forking nodes processes...\n");
                        /*****  Creates SO_NODES_NUM children   *****/
                        /********************************************/
                        for (i = 0; i < SO_NODES_NUM; i++)
                        {
                            printf("Master: node number %d\n", i);
                            switch (child_pid = fork())
                            {
                            case -1:
                                /* Handle error*/
                                unsafeErrorPrint("Master: fork failed. Error: ");
                                exit(EXIT_FAILURE);
                            case 0:
                                /*
                                // The process tells the father that it is ready to run
                                // and that it waits for all processes to be ready*/
                                printf("Node of PID %ld starts its execution....\n", (long)getpid());
                                /*sops[0].sem_op = -1;
                                semop(fairStartSem, &sops[0], 1);*/
                                /*
                                    Provvisorio
                                */
                                /*
                                signal(SIGALRM, SIG_IGN);
                                signal(SIGUSR1, tmpHandler);*/

                                /* Save users processes pid and state into usersList*/
                                sops[1].sem_op = -1;
                                sops[1].sem_num = 2;
                                semop(nodeListSem, &sops[1], 1);

                                nodesList[i].procId = getpid();
                                nodesList[i].procState = ACTIVE;

                                /*Perchè c'era 2???*/
                                sops[1].sem_op = 1;
                                sops[1].sem_num = 2;
                                semop(nodeListSem, &sops[1], 1);

                                /* Initialize messages queue for transactions pools*/
                                tpList[i].procId = (long)getpid();
                                key = ftok(MSGFILEPATH, getpid());
                                FTOK_TEST_ERROR(key);
                                tpList[i].msgQId = msgget(key, IPC_CREAT | IPC_EXCL | 0600);
                                MSG_TEST_ERROR(tpList[i].msgQId);

                                /*
                                    In teoria possiamo passare l'ambiente del padre senza problemi
                                */
                                if (execle("./node", "node", NULL, environ) == -1)
                                    safe("Master: failed to load node's code. Error: ");

                                /* Temporary part to get the process to do something*/
                                /*
                                do_stuff(2);
                                printf("Node done! PID:%d\n", getpid());
                                /*exit(i);*/
                               /* busy_cpu(1);*/
                                /*exit(i);*/
                                break;

                            default:
                                sops[0].sem_num = 0;
                                sops[0].sem_op = -1;
                                sops[0].sem_flg = IPC_NOWAIT;
                                semop(fairStartSem, &sops[0], 1);

                                /* Save users processes pid and state into usersList*/
                                sops[1].sem_op = -1;
                                sops[1].sem_num = 2;
                                semop(nodeListSem, &sops[1], 1);

                                nodesList[i].procId = getpid();
                                nodesList[i].procState = ACTIVE;

                                sops[1].sem_op = 1;
                                sops[1].sem_num = 2;
                                semop(nodeListSem, &sops[1], 1);

                                break;
                            }
                        }
                        /********************************************/
                        /********************************************/

                        /* The father also waits for all the children
                        // to be ready to continue the execution*/
                        for (i = 0; i < SO_NODES_NUM; i++)
                        {
                            estrai(i);
                            mybuf.mtype = nodesList[i].procId;
                            for (j = 0; j < SO_FRIENDS_NUM; j++)
                            {
                                mybuf.pid = nodesList[extractedFriendsIndex[j]].procId;
                                msgsnd(globalQueueId, &mybuf, sizeof(pid_t), 0);
                                /* printf("Message %d[%d] delivered...\n", snd_kids[i], j); */
                            }
                        }

                        /*sops[0].sem_op = -1;
                    semop(fairStartSem, &sops[0], 1);*/
                        sops[0].sem_op = 0;
                        sops[0].sem_num = 0;
                        sops[0].sem_flg = 0;
                        semop(fairStartSem, &sops[0], 1);

                        /* master lifecycle*/
                        printf("Master: starting lifecycle...\n");
                        /*sleep(10);*/ /*CORREGGERE*/
                        while (1 && child_pid)
                        {
                            /* check if register is full: in that case it must
                         signal itself ? No
                        this should be inserted in the master lifecycle*/
                            printf("Master: checking if register's partitions are full...\n");
                            fullRegister = TRUE;
                            for (i = 0; i < REG_PARTITION_COUNT && fullRegister; i++)
                            {
                                printf("Master: number of blocks is %d\n", regPtrs[i]->nBlocks);
                                printf("Master: Max size %d\n", REG_PARTITION_SIZE);
                                /*sleep(5);*/
                                if (regPtrs[i]->nBlocks < REG_PARTITION_SIZE)
                                    fullRegister = FALSE;
                            }

                            if (fullRegister)
                            {
                                /* it contains an exit call
                             so no need to set exit code*/
                                printf("Master: all register's partitions are full. Terminating simulation...\n");
                                endOfSimulation(SIGUSR1);
                            }
                            printf("Master: register's partitions are not full. Starting a new cycle...\n");
                        }
                    }
                    else
                    {
                        freeGlobalVariables();
                    }
                }
            }
        }
    }

    /* POSTCONDIZIONE: all'esecuzione di questa system call
        l'handler di fine simulazione è già stato eseguito*/
    exit(exitCode);
}

void tmpHandler(int sig)
{
    printf("ONE LAST KEKW OF PID %ld\n", (long)getpid());
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

void freeGlobalVariables()
{
}

void endOfSimulation(int sig)
{ /* IT MUST BE REENTRANT!!!!
    // Notify children
    // sends termination signal to all the processes
    // that are part of the master's group (in this case we
    // reach every children with just one system call).
    // how to check if everyone was signaled (it returns true even if
    // only one signal was sent)*/
    char *terminationMessage = (char *)calloc(100, sizeof(char));
    int ret = -1;
    char *aus = (char *)calloc(100, sizeof(char));
    int i = 0;
    /*
    // Contiene una exit, perchè potrebbe essere ivocato in maniera
    // asincrona durante l'esecuzione del ciclo di vita
    // in tal caso l'esecuzione dovrebbe terminare dopo l'esecuzione
    // dell'handler senza eseguire il codice rimanente del ciclo di vita
    // cosa che potrebbe anche causare degli errori
    // per il riferimento a zone di memoria non più allocate
    // in case of error we don't stop the whole procedure
    // but we signal it by setting the exit code to EXIT_FAILURE*/
    int exitCode = EXIT_SUCCESS;
    boolean done = FALSE;

    /*
	// viene inviato anche al master stesso ? Sì
	// come assicurarsi che venga inviato a tutti?
	// fallisce se non viene inviato a nessuno
	// ma inviato != consegnato???*/
    /*
        Aggiornare tenendo conto del fatto che gli utenti potrebbero già essere terminati:
        in tal caso il meccanismo di retry è inutile
        Bisogna fare solo la wait senza mandare il segnale
        In ogni caso, se non si riescono a terminare i processi dopo n tentativi deallocare comunque le facilities
    */
    printf("Master: PID %ld\nMaster: parent PID %ld\n", (long int)getpid(), (long)getppid());
    if (terminationMessage == NULL || aus == NULL)
        safeErrorPrint("Master: failed to alloacate memory. Error: ");
    else
    {
        write(STDOUT_FILENO,
              "Master: trying to terminate simulation...\n",
              strlen("Master: trying to terminate simulation...\n"));
        /* error check*/
        fflush(stdout);
        if (noTerminated < noEffective) {
            /*
                There are still active children that need
                to be notified the end of simulation
            */
            for (i = 0; i < NO_ATTEMPS && !done; i++){
                if (kill(0, SIGUSR1) == -1){
                    safeErrorPrint("Master: failed to signal children for end of simulation. Error: ");
                } else {
                    write(STDOUT_FILENO, 
                        "Master: end of simulation notified successfully to children.\n", 
                        strlen("Master: end of simulation notified successfully to children.\n")
                    );
                    done = TRUE;
                }
            }
            
        } else
            done = TRUE;
        /*
			// wait for children
			// dovremmo aspettare solo la ricezione del segnale di terminazione????
			// mettere nell'handler
			// dovremmo usare waitpid e teastare che i figli siano
			// terminati correttamente ? Sarebbe complicato
			// meglio inserire nel figlio un meccanismo che tenta più volte la stampa
			// in caso di errore
			// in teoria questo si sblocca solo dopo la terminazione di tutti i figli
			// quindi ha senso fare così*/
        if (done) {
            write(STDOUT_FILENO,
                "Master: waiting for children to terminate...\n",
                strlen("Master: waiting for children to terminate...\n"));
            /*
                Conviene fare comunque la wait anche se tutti sono già terminati
                in modo che non ci siano zombies
            */
            while (wait(NULL) != -1);
            if (errno == ECHILD)
            {
                /*
                // print report: we use the write system call: slower, but async-signal-safe
                // Termination message composition
                // we use only one system call for performances' sake.
                // termination reason*/
                write(STDOUT_FILENO,
                      "Master: simulation terminated successfully. Printing report...\n",
                      strlen("Master: simulation terminated successfully. Printing report...\n"));

                /* Users and nodes budgets*/
                /*printBudget();*/

                /*Per la stampa degli errori non si può usare perror, perchè non è elencata* tra la funzioni signal
                in teoria non si può usare nemmno sprintf*/

                /* processes terminated before end of simulation*/
                /*printf("Processes terminated before end of simulation: %d\n", noTerminated);*/
                ret = sprintf(terminationMessage, "Processes terminated before end of simulation: %d\n", noTerminated);

                /* Blocks in register*/
                ret = sprintf(aus, "There are %d blocks in the register.\n",
                              regPtrs[0]->nBlocks + regPtrs[1]->nBlocks + regPtrs[2]->nBlocks);
                strcat(terminationMessage, aus);

                if (sig == SIGALRM)
                    strcat(terminationMessage, "Termination reason: end of simulation.\n");
                else
                    strcat(terminationMessage, "Termination reason: register is full\n");

                /* Writes termination message on standard output*/
                write(STDOUT_FILENO, terminationMessage, strlen(terminationMessage));
                write(STDOUT_FILENO,
                      "Master: report printed successfully. Deallocating IPC facilities...\n",
                      strlen("Master: report printed successfully. Deallocating IPC facilities...\n"));
                /* deallocate facilities*/
                deallocateFacilities(&exitCode);
                done = TRUE;
            }
            else
            {
                safeErrorPrint("Master: an error occurred while waiting for children. Description: ");
            }
            write(STDOUT_FILENO, "Master: simulation terminated successfully!!!\n", strlen("Master: simulation terminated successfully!!!\n"));
        } else {
            deallocateFacilities(&exitCode);
            exitCode = EXIT_FAILURE;
            write(STDOUT_FILENO, 
                "Master: failed to terminate children. IPC facilties will be deallocated anyway.\n", 
                strlen("Master: failed to terminate children. IPC facilties will be deallocated anyway.\n")
            );
        }
    }
    /* Releasing local variables' memory*/
    free(terminationMessage);
    /*
                CORREGGERE: perchè la free va in errore ??
                (Forse è per strcat)
            */
    /*free(aus);*/
    exit(exitCode);
}

void printBudget()
{ /* Vedere se si possa sostituire la write con printf*/
    Register *reg = NULL;
    long int pid = -2;        /* -1 è usato per rappresentare la transazione di reward e di init*/
    ProcListElem *usr = NULL; /* non sono da deallocare, altrimenti cancelli la lista di processi*/
    ProcListElem *node = NULL;
    float balance = 0; /* leggendo la transazione di inizializzazione (i.e. la prima indirizzata la processo)
                        verrà inizializzata a SO_BUDGET_INIT*/
    char *balanceString = NULL;
    int ret = 0;
    int i = 0;
    int j = 0;
    int k = 0;
    Transaction *tList = NULL;
    TPElement *tpPtr = NULL;
    struct msqid_ds *buf = NULL;
    int noRemainingMsg = 0;
    Transaction msg;
    int qId = -1;

    /* Compute balance for users*/
    ret = write(STDOUT_FILENO,
                "Master prints users' balances...\n",
                strlen("Master prints users' balances...\n"));

    if (ret == -1)
        safeErrorPrint("Master: failed to write operation's description.");
    else
    {
        usr = usersList;
        /* è un algoritmo di complessità elevata
        // vedere se sia possibile ridurla
        // for each user...*/
        while (usr != NULL)
        {
            pid = (long)(usr->procId);
            balance = 0;
            /* we scan all the register's partitions...*/
            for (i = 0; i < REG_PARTITION_COUNT; i++)
            {
                reg = regPtrs[i];
                /* we scan every block in the partition
                // and every transaction in it to compute the balance*/
                for (j = 0; j < REG_PARTITION_SIZE; j++)
                {
                    /* necessary in order not to lose
                    // the transaction list pointer*/
                    tList = (reg->blockList[i]).transList;
                    for (k = 0; k < SO_BLOCK_SIZE; k++)
                    {
                        if (tList[k].sender == pid)
                            balance -= tList[k].amountSend;
                        else if (tList[k].receiver == pid)
                            balance += tList[k].amountSend;
                    }
                }
            }

            ret = sprintf(balanceString, "The balance of the user of PID %ld is: %f", pid, balance);
            if (ret <= 0)
            {
                if (write(STDOUT_FILENO, balanceString, strlen(balanceString)) == -1)
                    write(STDERR_FILENO, "Master: failed to write balance message for user.", strlen("Master: failed to write balance message for user."));
            }
            usr++;
        }
    }

    ret = write(STDOUT_FILENO, "Master prints nodes' balances...\n", strlen("Master prints nodes' balances...\n"));
    if (ret == -1)
        write(STDERR_FILENO, "Master: failed to write operation's description.", strlen("Master: failed to write operation's description."));
    else
    {
        node = nodesList;
        while (node != NULL)
        {
            pid = (long)(node->procId);

            /* Se perdessimo il puntatore non riusciremmo più a deallocare tpList*/
            ret = sprintf(balanceString, "Master: printing remaining transactions of node of PID %ld\n", pid);
            write(STDOUT_FILENO, balanceString, ret);
            tpPtr = tpList;
            while (tpPtr != NULL)
            {
                if (tpPtr->procId == pid)
                {
                    /* fare macro per permessi*/
                    qId = msgget(tpPtr->procId, 0600);
                    if (ret == -1)
                    {
                        sprintf(balanceString,
                                "Master: failed to open transaction pool of node of PID %ld\n",
                                pid);
                        safeErrorPrint(balanceString);
                    }
                    else
                    {
                        if (msgctl(qId, IPC_STAT, buf) == -1)
                        {
                            sprintf(balanceString,
                                    "Master: failed to retrive remaining transactio of node of PID %ld\n",
                                    pid);
                            safeErrorPrint(balanceString);
                        }
                        else
                        {
                            noRemainingMsg = buf->msg_qnum;
                            if (noRemainingMsg > 0)
                            {
                                for (k = 0; k < noRemainingMsg; k++)
                                {
                                    /* reads the first message from the transaction pool
                                    // the last parameter is not  that necessary, given
                                    // that we know there's still at least one message
                                    // and no one is reading but the master*/
                                    if (msgrcv(qId, &msg, sizeof(msg), 0, IPC_NOWAIT) == -1)
                                    {
                                        sprintf(balanceString,
                                                "Master: failed to read transaction number %d of PID %ld\n",
                                                (k + 1), pid);
                                    }
                                    else
                                    {
                                        ret = sprintf(balanceString,
                                                      "Transaction number %d:\n\tTimestamp: %ld\nSender: %ld\nReceiver: %ld\nAmount sent:%f\nReward:%f\n",
                                                      (k + 1), msg.timestamp.tv_nsec, msg.sender,
                                                      msg.receiver, msg.amountSend, msg.reward);
                                        write(STDOUT_FILENO, balanceString, ret);
                                    }
                                }
                            }
                            else
                            {
                                write(STDOUT_FILENO,
                                      "Transaction pool is empty.\n",
                                      strlen("Transaction pool is empty.\n"));
                            }
                        }
                    }
                }

                tpPtr++;
            }

            ret = sprintf(balanceString, "Master: printing budget of node of PID %ld\n", pid);
            write(STDOUT_FILENO, balanceString, ret);
            balance = 0;
            for (i = 0; i < REG_PARTITION_COUNT; i++)
            {
                reg = regPtrs[i];
                /* note that we start from one because there's no
                // initialization blocks for nodes*/
                for (j = 1; j < REG_PARTITION_SIZE; j++)
                {
                    tList = (reg->blockList[i]).transList;
                    for (k = 0; k < SO_BLOCK_SIZE; k++)
                    {
                        /*
                        // non c'è il rischio di contare più volte le transazioni
                        /// perchè cerchiamo solo quella di reward e l'implementazione
                        // del nodo garantisce che c'è ne sia una sola per blocco
                        // serve testare*/
                        if (tList[k].sender == REWARD_TRANSACTION && tList[k].receiver == pid)
                            balance += tList[k].amountSend;
                        tList++;
                    }
                }
            }
            ret = sprintf(balanceString, "The balance of the node of PID %ld is: %f", pid, balance);
            if (ret <= 0)
            {
                if (write(STDOUT_FILENO, balanceString, strlen(balanceString)) == -1)
                    write(STDERR_FILENO, "Master: failed to write balance message for node.", strlen("Master: failed to write balance message for node."));
            }
            node++;
        }
    }

    /* Deallocate memory*/
    free(reg);
    free(usr);
    free(node);
    free(balanceString);
    free(tList);
    free(tpPtr);
    free(buf);
}

void deallocateFacilities(int *exitCode)
{
    /*
    // Ovviamente i processi figli dovranno scollegarsi
    // dai segmenti e chiudere i loro riferimenti alla coda
    // ed ai semafori prima di terminare

    // Precondizione: tutti i processi figli si sono scollegati dai segmenti di memoria
    // In generale, tutti i figli hanno chiuso i loro riferimenti alle facilities IPC
    // ne siamo certi perchè questa procedura viene richiamata solo dopo aver atteso la terminazione di ogni figlio

    // Cosa fare se una delle system call usate per la deallocazione fallisce?*/
    /*
        L'idea è quella di implementare l'eliminazione di ogni facility in maniera indipendente
        dalle altre (i.e. l'eliminazione della n + 1 viene effettuata anche se quella dell'n-esima è fallita)
        ma di non implementare un meccanismo tale per cui si tenta ripetutamente di eliminare
        la facility n-esima qualora una delle system call coinvolte fallisca
    */

    /*
        TODO:
            -dellocate registers partition: OK
            -deallocate registers ids' array: OK
            -deallocate users and nodes lists (serve fare la malloc o basta deallocare il segmento???): Ok
            -deallocate friensd' shared memory segment: OK
            -deallocate transaction pools' message queues: OK
            -deallocate semaphores: Ok
            -free dynamically allocated memory: Ok
    */

    char *aus = (char *)calloc(100, sizeof(char));
    int msgLength = 0;
    int i = 0;
    TPElement * tmp;

    printf("**********Il deallocatore è %ld**********\n", (long)getpid());
    printf("**********Il padre del deallocatore è %ld**********\n", (long)getppid());
    fflush(stdout);
    /* Deallocating register's partitions*/
    write(STDOUT_FILENO,
          "Master: deallocating register's paritions...\n",
          strlen("Master: deallocating register's paritions...\n"));
    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(regPtrs[i]) == -1)
        {
            msgLength = sprintf(aus,
                                "Master: failed to detach from register's partition number %d.\n",
                                (i + 1));
            write(STDERR_FILENO, aus, msgLength); /* by doing this we avoid calling strlength*/
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            if (shmctl(regPartsIds[i], IPC_RMID, NULL) == -1)
            {
                msgLength = sprintf(aus,
                                    "Master: failed to remove register's partition number %d.",
                                    (i + 1));
                write(STDERR_FILENO, aus, msgLength);

                *exitCode = EXIT_FAILURE;
            }
            else
            {
                msgLength = sprintf(aus,
                                    "Master: register's partition number %d removed successfully.\n",
                                    (i + 1));
                write(STDOUT_FILENO, aus, msgLength);
            }
        }
    }
    free(regPtrs);
    free(regPartsIds);

    /* Users list deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating users' list segment...\n",
          strlen("Master: deallocating users' list segment...\n"));
    if (shmdt(usersList) == -1)
    {
        safeErrorPrint("Master: failed to detach from users' list segment. Error: ");
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        if (shmctl(usersListId, IPC_RMID, NULL) == -1)
        {
            safeErrorPrint("Master: failed to remove users' list segment. Error: ");
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: users' list memory segment successfully removed.\n",
                  strlen("Master: users list memory segment successfully removed.\n"));
        }
    }

    /* Nodes list deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating nodes' list segment...\n",
          strlen("Master: deallocating nodes' list segment...\n"));
    if (shmdt(nodesList) == -1)
    {
        safeErrorPrint("Master: failed to detach from nodes' list segment. Error: ");
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        if (shmctl(nodesListId, IPC_RMID, NULL) == -1)
        {
            safeErrorPrint("Master: failed to remove nodes' list segment. Error: ");
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: nodes' list memory segment successfully removed.\n",
                  strlen("Master: nodes list memory segment successfully removed.\n"));
            /*
                Non serve: abbiamo già deallocato il segmento di memoria condivisa
            */
            /*free(nodesList);*/
        }
    }

    /* Partitions' shared variable deallocation*/
    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(noReadersPartitionsPtrs[i]) == -1)
        {
            msgLength = sprintf(aus,
                                "Master: failed to detach from partition number %d shared variable segment.\n",
                                (i + 1));

            write(STDERR_FILENO, aus, msgLength); /* by doing this we avoid calling strlength*/
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            if (shmctl(noReadersPartitions[i], IPC_RMID, NULL) == -1)
            {
                msgLength = sprintf(aus,
                                    "Master: failed to remove partition number %d shared variable segment.",
                                    (i + 1));
                write(STDERR_FILENO, aus, msgLength);

                *exitCode = EXIT_FAILURE;
            }
            else
            {
                msgLength = sprintf(aus,
                                    "Master: register's partition number %d shared variable segment removed successfully.\n",
                                    (i + 1));
                write(STDOUT_FILENO, aus, msgLength);
            }
        }
    }
    free(noReadersPartitions);

    /* Transaction pools list deallocation*/
    /*
            Per il momento va in errore perchè non ci sono ancora le code
        */
    /*
    write(STDOUT_FILENO,
          "Master: deallocating transaction pools...\n",
          strlen("Master: deallocating transaction pools...\n"));
    tmp = tpList;
    for (i = 0; tpList + i != NULL; i++){
        printf("Id coda: %d\n", tpList[i].msgQId);
        printf("Id Processo: %ld\n", (long)tpList[i].procId);
        if (msgctl(tpList[i].msgQId, IPC_RMID, NULL) == -1)
        {
            msgLength = sprintf(aus,
                                "Master: failed to remove transaction pool of process %ld\n",
                                (long)tpList[i].procId);
            write(STDERR_FILENO, aus, msgLength);

            *exitCode = EXIT_FAILURE;
        }
        else
        {
            msgLength = sprintf(aus,
                                "Master: transaction pool of node of PID %ld successfully removed.\n",
                                (long)tpList[i].procId);
            write(STDOUT_FILENO, aus, msgLength);
        }
    }*/
    /*
    while (tmp != NULL)
    {
        printf("Id coda: %d\n", *(tmp).msgQId);
        printf("Id Processo: %ld\n", *tmp.procId);
        fflush(stdout);
        
        tmp++;
        
    }*/
    free(tpList);

    /* Deallocating process friends array*/
    /*free(processesFriends);*/
    /*Non serve più: ora è responsabilità del nodo
    deallocare il suo vettore di amici valorizzato
    con i messaggi inviati sulla coda globale
    */

    /* Global queue deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating global message queue...\n",
          strlen("Master: deallocating global message queue...\n"));
    if (msgctl(globalQueueId, IPC_RMID, NULL) == -1)
    {
        msgLength = sprintf(aus, "Master: failed to remove global message queue");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: global message queue successfully removed.\n",
              strlen("Master: global message queue successfully removed.\n"));
    }

    /* Writing Semaphores deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating writing semaphores...\n",
          strlen("Master: deallocating writing semaphores...\n"));
    if (semctl(wrPartSem, 0, IPC_RMID) == -1)
    {
        msgLength = sprintf(aus, "Master: failed to remove partions' writing semaphores");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: Writing Semaphores successfully removed.\n",
              strlen("Master: Writing Semaphores successfully removed.\n"));
    }

    /* Reading Semaphores deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating reading semaphores...\n",
          strlen("Master: deallocating reading semaphores...\n"));
    if (semctl(rdPartSem, 0, IPC_RMID) == -1)
    {
        msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: Reading Semaphores successfully removed.\n",
              strlen("Master: Reading Semaphores successfully removed.\n"));
    }

    /* Fair start semaphore deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating fair start semaphores...\n",
          strlen("Master: deallocating fair start semaphores...\n"));
    if (semctl(fairStartSem, 0, IPC_RMID) == -1)
    {
        msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: Fair start semaphore successfully removed.\n",
              strlen("Master: Fair start semaphore successfully removed.\n"));
    }

    /* Users' list semaphores deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating users' list semaphores...\n",
          strlen("Master: deallocating users' list semaphores...\n"));
    if (semctl(userListSem, 0, IPC_RMID) == -1)
    {
        sprintf(aus, "Master: failed to remove users' list semaphores\n");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: users' list semaphores successfully removed.\n",
              strlen("Master: users' list semaphores successfully removed.\n"));
    }

    /* Register's paritions mutex semaphores deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating register's paritions mutex semaphores...\n",
          strlen("Master: deallocating register's paritions mutex semaphores...\n"));
    if (semctl(mutexPartSem, 0, IPC_RMID) == -1)
    {
        sprintf(aus, "Master: failed to remove register's paritions mutex semaphores\n");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: register's paritions mutex semaphores successfully removed.\n",
              strlen("Master: register's paritions mutex semaphores successfully removed.\n"));
    }

    /*
        Correggere: per il momento l'eliminazione di questo semaforo fallisce
    */
    write(STDOUT_FILENO,
          "Master: deallocating user list's semaphores...\n",
          strlen("Master: deallocating user list's semaphores...\n"));
    if (semctl(userListSem, 0, IPC_RMID) == -1)
    {
        sprintf(aus, "Master: failed to remove user list's semaphores\n");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: user list's semaphores successfully removed.\n",
              strlen("Master: user list's semaphores successfully removed.\n"));
    }

    write(STDOUT_FILENO,
          "Master: deallocating user list's shared variable...\n",
          strlen("Master: deallocating user list's shared variable...\n"));
    if (shmdt(noUserSegReadersPtr) == -1)
    {
        safeErrorPrint("Master: failed to detach from user list's shared variable. Error: ");
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        if (shmctl(noUserSegReaders, IPC_RMID, NULL) == -1)
        {
            safeErrorPrint("Master: failed to remove user list's shared variable. Error: ");
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: user list's shared variable successfully removed.\n",
                  strlen("Master: user list's shared variable successfully removed.\n"));
        }
    }

    write(STDOUT_FILENO,
          "Master: deallocating nodes list's semaphores...\n",
          strlen("Master: deallocating nodes list's semaphores...\n"));
    if (semctl(nodeListSem, 0, IPC_RMID) == -1)
    {
        sprintf(aus, "Master: failed to remove nodes list's semaphores");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: nodes list's semaphores successfully removed.\n",
              strlen("Master: nodes list's semaphores successfully removed.\n"));
    }

    /* Releasing local variables' memory*/
    free(aus);
}
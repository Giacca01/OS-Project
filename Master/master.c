/*
    Cose da fare:
        Test
        Refactoring
        Handler CTRL + C
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

int fairStartSem = -1; // Id of the set that contais the three semaphores
                       // used to write on the register's partitions
int wrPartSem = -1;    // Id of the set that contais the three semaphores
                       // used to write on the register's partitions
int rdPartSem = -1;    // Id of the set that contais the three semaphores
                       // used to read from the register's partitions
int mutexPartSem = -1; // id of the set that contains the three sempahores used to
                       // to access the number of readers variables of the registers partitions
                       // in mutual exclusion

// Si dovrebbe fare due vettori

/* AGGIUNTO DA STEFANO */
int noReaders[3] = {-1, -1, -1}; /* ids of the shared memory segment that contains the variable used to syncronize
                                  * readers and writers access to first, second and third register's partition */
int* noReadersPtr[3] = {NULL, NULL, NULL};
/* END */

/*int noReadersPartOne = -1; // id of the shared memory segment that contains the variable used to syncronize
                           // readers and writes access to first register's partition
int *noReadersPartOnePtr = NULL;

int noReadersPartTwo = -1; // id of the shared memory segment that contains the variable used to syncronize
                                              // readers and writes access to second register's partition
int *noReadersPartTwoPtr = NULL;

int noReadersPartThree = -1; // id of the shared memory segment that contains the variable used to syncronize
                           // readers and writes access to third register's partition
int *noReadersPartThreePtr = NULL;*/

int userListSem = -1;      // Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                           // to read and write users list
int noUserSegReaders = -1; // id of the shared memory segment that contains the variable used to syncronize
                           // readers and writes access to users list
int *noUserSegReadersPtr = NULL;

int nodeListSem = -1; /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                         to read and write nodes list */

int noNodeSegReaders = -1; /* id of the shared memory segment that contains the variable used to syncronize
                              readers and writes access to nodes list */
int *noNodeSegReadersPtr = NULL;

int nodeListSem = -1; // Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                      // to read and write nodes list

int noTerminated = 0; // NUmber of processes that terminated before end of simulation
/******************************************/

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
// Long???
int SO_USERS_NUM,   // Number of user processes NOn è "statico" ???
    SO_NODES_NUM,   // Number of node processes ?? NOn è "statico" ???
    SO_SIM_SEC,     // Duration of the simulation
    SO_FRIENDS_NUM; // Number of friends
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
    SO_SIM_SEC = atoi(getenv("SO_SIM_SEC"));
    SO_FRIENDS_NUM = atoi(getenv("SO_FRIENDS_NUM"));
}
/***************************************************************/
/***************************************************************/

/***** Function that reads the file containing the configuration    *****/
/***** parameters to save them as environment variables             *****/
/************************************************************************/
int readConfigParameters()
{
    char *filename = "params.txt";
    FILE *fp = fopen(filename, "r");
    // Reading line by line, max 128 bytes
    const unsigned MAX_LENGTH = 128;
    // Array that will contain the lines read from the file
    // each "row" of the "matrix" will contain a different file line
    char line[14][MAX_LENGTH];
    // Counter of the number of lines in the file
    int k = 0;
    char *aus = NULL;
    int exitCode = 0;

    // Handles any error in opening the file
    if (fp == NULL)
    {
        aus = sprintf(aus, "Error: could not open file %s", filename);
        unsafeErrorPrint(aus);
        exitCode = -1;
    }
    else
    {
        // Inserts the lines read from the file into the array
        while (fgets(line[k], MAX_LENGTH, fp))
            k++;

        // It inserts the parameters read into environment variables
        for (int i = 0; i < k; i++)
            putenv(line[i]);

        // Assigns the values ​​of the environment
        // variables to the global variables defined above
        assignEnvironmentVariables();

        // Close the file
        fclose(fp);
    }

    return exitCode;
}
/************************************************************************/
/************************************************************************/

/****   Function that creates the ipc structures used in the project    *****/
/****************************************************************************/
void createIPCFacilties()
{
    // calloc???
    regPtrs = (Register **)malloc(REG_PARTITION_COUNT * sizeof(Register *));
    for (int i = 0; i < REG_PARTITION_COUNT; i++)
        regPtrs[i] = (Register *)malloc(REG_PARTITION_SIZE * sizeof(Register));
    regPartsIds = (int *)malloc(REG_PARTITION_COUNT * sizeof(int));

    usersList = (ProcListElem *)malloc(SO_USERS_NUM * sizeof(ProcListElem));

    nodesList = (ProcListElem *)malloc(SO_NODES_NUM * sizeof(ProcListElem));

    tpList = (TPElement *)malloc(SO_NODES_NUM * sizeof(TPElement));
}
/****************************************************************************/
/****************************************************************************/

/*****  Function that initialize the ipc structures used in the project *****/
/****************************************************************************/
void initializeIPCFacilities()
{
    union semun arg;
    unsigned short *aux = {1, 1, 1};
    // Initialization of semaphores
    key_t key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key);
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

    arg.val = SO_USERS_NUM + SO_NODES_NUM + 1;
    semctl(fairStartSem, 0, SETVAL, arg);

    arg.array = aux;
    semctl(rdPartSem, 0, SETALL, arg);

    aux[0] = aux[1] = aux[2] = SO_USERS_NUM + SO_NODES_NUM + 1;
    arg.array = aux;
    semctl(wrPartSem, 0, SETALL, arg);

    arg.val = 1;
    semctl(userListSem, 0, SETVAL, arg); // mutex
    arg.val = SO_USERS_NUM + SO_NODES_NUM + 1;
    semctl(userListSem, 1, SETVAL, arg); // read
    arg.val = 0;
    semctl(userListSem, 2, SETVAL, arg); // write

    arg.val = 1;
    semctl(nodeListSem, 0, SETVAL, arg); // mutex
    arg.val = SO_USERS_NUM + SO_NODES_NUM + 1;
    semctl(nodeListSem, 1, SETVAL, arg); // read
    arg.val = 0;
    semctl(nodeListSem, 2, SETVAL, arg); // write

    aux[0] = aux[1] = aux[2] = 1;
    arg.array = aux;
    semctl(mutexPartSem, 0, SETALL, arg);

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    // Creates the global queue
    globalQueueId = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL);
    MSG_TEST_ERROR(globalQueueId);
    /********************************************************/
    /********************************************************/

    /*****  Initialization of shared memory segments    *****/
    /********************************************************/
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(regPartsIds[0]);
    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(regPartsIds[1]);
    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key);
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(regPartsIds[2]);
    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key);
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(usersListId);
    usersList = (ProcListElem *)shmat(usersListId, NULL, 0);

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key);
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(nodesListId);
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, 0);

    // Aggiungere segmenti per variabili condivise
    key = ftok(SHMFILEPATH, NOREADERSONESEED);
    FTOK_TEST_ERROR(key);
    noReadersPartOne = shmget(key, sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(noReadersPartOne);
    noReadersPartOnePtr = (int *)shmat(noReadersPartOne, NULL, 0);
    *noReadersPartOnePtr = 0;

    key = ftok(SHMFILEPATH, NOREADERSTWOSEED);
    FTOK_TEST_ERROR(key);
    noReadersPartTwo = shmget(key, sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(noReadersPartTwo);
    noReadersPartTwoPtr = (int *)shmat(noReadersPartTwo, NULL, 0);
    *noReadersPartTwoPtr = 0;

    key = ftok(SHMFILEPATH, NOREADERSTHREESEED);
    FTOK_TEST_ERROR(key);
    noReadersPartThree = shmget(key, sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(noReadersPartThree);
    noReadersPartThreePtr = (int *)shmat(noReadersPartThree, NULL, 0);
    *noReadersPartThreePtr = 0;

    key = ftok(SHMFILEPATH, NOUSRSEGRDERSSEED);
    FTOK_TEST_ERROR(key);
    noUserSegReaders = shmget(key, sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
    SHM_TEST_ERROR(noUserSegReaders);
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    *noUserSegReadersPtr = 0;

	key = ftok(SHMFILEPATH, NONODESEGRDERSSEED);
    FTOK_TEST_ERROR(key);
	noNodeSegReaders = shmget(key, sizeof(SO_NODES_NUM), S_IRUSR | S_IWUSR);
	SHM_TEST_ERROR(noNodeSegReaders);
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    *noNodeSegReadersPtr = 0;

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

    for (i = 0; i < loops; i++)
    {
        my_var += 0.5;
        my_var = my_var > 1 ? my_var - 1 : my_var;
    }
}

void do_stuff(int t)
{
    if (t == 1)
        printf("Hi, I'm a user, my pid is %d\n", getpid());
    else
        printf("Hi, I'm a node, my pid is %d\n", getpid());
    srand(time(0));
    busy_cpu(rand() % 1000000000);
}
/**************************************************************/
/**************************************************************/

/**************** CAPIRE SE SPOSTARE IN INFO.H O SE LASCIARE QUI ****************/

/* struct that rappresents a process and its budget*/
typedef struct proc_budget {
	pid_t proc_pid;
	int budget;
	int p_type; /* type of node: 0 if user, 1 if node */
	struct proc_budget * next;
} proc_budget;

/* list of budgets for every user and node process */
typedef proc_budget* budgetlist;

/* function that frees the space allocated for budgetlist p */
void budgetlist_free(budgetlist p);

/* initialization of the budgetlist - array to maintain budgets read from ledger */
budgetlist bud_list = NULL;

/**************** CAPIRE SE SPOSTARE IN INFO.H O SE LASCIARE QUI ****************/


int main(int argc, char *argv[])
{
    pid_t child_pid;
    int status;
    struct sembuf sops[3];
    sigset_t set;
    struct sigaction act;
    int fullRegister = TRUE;
    int exitCode = EXIT_FAILURE;
	key_t key;
    int i = 0;

    /* elements for creation of budgetlist */
	budgetlist new_el;
	budgetlist el_list;

	/* definition of objects necessary for nanosleep */
	struct timespec onesec, tim;
	onesec.tv_sec=1;
	onesec.tv_nsec=0;
	
	/* definition of indexes for cycles */
	int i,j,k, ct_updates;
	/* da definire fuori da while per mantenerne i valori */
	int index_reg[REG_PARTITION_COUNT];
	for(i = 0; i < REG_PARTITION_COUNT; i++)
		index_reg[i] = i;

	/* da definire fuori da while per mantenerne i valori */
	int prev_read_nblock[REG_PARTITION_COUNT];
	for(i = 0; i < REG_PARTITION_COUNT; i++)
		prev_read_nblock[i] = 0; /* qui memorizzo il blocco a cui mi sono fermato allo scorso ciclo nella i-esima partizione */

	int ind_block; /* indice per scorrimento blocchi */
	int ind_tr_in_block = 0; /* indice per scorrimento transazioni in blocco */

    // Set common semaphore options
    sops[0].sem_num = 0;
    sops[0].sem_flg = 0;
    sops[1].sem_num = 2;
    sops[1].sem_flg = 0;
    sops[2].sem_num = 2;
    sops[2].sem_flg = 0;

    printf("Master: setting up simulation timer...\n");
    /* No previous alarms were set, so it must return 0*/
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
                    // Read configuration parameters from
                    // file and save them as environment variables
                    if (readConfigParameters() == -1)
                        exit(EXIT_FAILURE);

                    /*****  Creates and initialize the IPC Facilities   *****/
                    /********************************************************/
                    createIPCFacilties();

                    initializeIPCFacilities();
                    /********************************************************/
                    /********************************************************/

                    /*****  Creates SO_USERS_NUM children   *****/
                    /********************************************/
                    for (int i = 0; i < SO_USERS_NUM; i++)
                    {
                        switch (child_pid = fork())
                        {
                        case -1:
                            //Handle error
                            unsafeErrorPrint("Master: fork failed. Error: ");
                            exit(EXIT_FAILURE);
                        case 0:
                            // The process tells the father that it is ready to run
                            // and that it waits for all processes to be ready
                            sops[0].sem_op = -1;
                            semop(fairStartSem, &sops[0], 1);

                            // Save users processes pid and state into usersList
                            sops[1].sem_op = -1;
                            semop(userListSem, &sops[1], 1);

                            usersList[i].procId = getpid();
                            usersList[i].procState = ACTIVE;

                            sops[1].sem_op = 1;
                            semop(userListSem, &sops[1], 1);

                            sops[0].sem_op = 0;
                            semop(fairStartSem, &sops[0], 1);

                            // Temporary part to get the process to do something
                            do_stuff(1);
                            printf("User done! PID:%d\n", getpid());
                            exit(i);
                            break;

                        default:
                            break;
                        }
                    }
                    /********************************************/
                    /********************************************/

                    /*****  Creates SO_NODES_NUM children   *****/
                    /********************************************/
                    for (int i = 0; i < SO_NODES_NUM; i++)
                    {
                        switch (child_pid = fork())
                        {
                        case -1:
                            // Handle error
                            unsafeErrorPrint("Master: fork failed. Error: ");
                            exit(EXIT_FAILURE);
                        case 0:
                            // The process tells the father that it is ready to run
                            // and that it waits for all processes to be ready
                            sops[0].sem_op = -1;
                            semop(fairStartSem, &sops[0], 1);

                            // Save users processes pid and state into usersList
                            sops[2].sem_op = -1;
                            semop(nodeListSem, &sops[2], 1);

                            nodesList[i].procId = getpid();
                            nodesList[i].procState = ACTIVE;

                            sops[2].sem_op = 1;
                            semop(nodeListSem, &sops[2], 1);

                            // Initialize messages queue for transactions pools
                            tpList[i].procId = getpid();
                            key = ftok(MSGFILEPATH, getpid());
                            FTOK_TEST_ERROR(key);
                            tpList[i].msgQId = msgget(key, IPC_CREAT | IPC_EXCL);
                            MSG_TEST_ERROR(tpList[i].msgQId);

                            sops[0].sem_op = 0;
                            semop(fairStartSem, &sops[0], 1);

                            // Temporary part to get the process to do something
                            do_stuff(2);
                            printf("Node done! PID:%d\n", getpid());
                            exit(i);
                            break;

                        default:
                            break;
                        }
                    }
                    /********************************************/
                    /********************************************/

                    /************** INITIALIZATION OF BUDGETLIST **************/
                    /**********************************************************/

                    /* we enter the critical section for the noUserSegReadersPtr variabile */
                    sops.sem_num = 0;
                    sops.sem_op = -1;
                    semop(userListSem, &sops, 1);
                    *noUserSegReadersPtr++;
                    if(*noUserSegReadersPtr == 1)
                    {
                        sops.sem_num = 2;
                        sops.sem_op = -1; /* controllare se giusto!!! */
                        semop(userListSem, &sops, 1);
                        /* 
                         * se lo scrittore sta scrivendo, allora il primo lettore che entrerà in questo 
                         * ramo si addormenterà su questo semaforo.
                         * se lo scrittore non sta scrivendo, allora il primo lettore decrementerà di 1 il
                         * valore semaforico, in modo tale se lo scrittore vuole scrivere, si addormenterà 
                         * sul semaforo
                         */
                    }
                    /* we exit the critical section for the noUserSegReadersPtr variabile */
                    sops.sem_num = 0;
                    sops.sem_op = 1;
                    semop(userListSem, &sops, 1);

                    /* initializing budget for users processes */
                    for (i = 0; i < SO_USERS_NUM; i++) 
                    {
                        new_el = malloc(sizeof(*new_el));
                        /* DEVO ACCEDERVI IN MUTUA ESCLUSIONE */
                        new_el->proc_pid = usersList[i].procId;
                        new_el->budget = atoi(getenv("SO_BUDGET_INIT"));
                        new_el->p_type = 0;
                        new_el->next = bud_list;
                        bud_list = new_el;
                    }

                    /* initializing budget for nodes processes */
                    for (i = 0; i < SO_NODES_NUM; i++) 
                    {
                        new_el = malloc(sizeof(*new_el));
                        /* DEVO ACCEDERVI IN MUTUA ESCLUSIONE */
                        new_el->proc_pid = nodesList[i].procId;
                        new_el->budget = 0;
                        new_el->p_type = 1;
                        new_el->next = bud_list;
                        bud_list = new_el;
                    }

                    /* we enter the critical section for the noUserSegReadersPtr variabile */
                    sops.sem_num = 0;
                    sops.sem_op = -1;
                    semop(userListSem, &sops, 1);
                    *noUserSegReadersPtr--;
                    if(*noUserSegReadersPtr == 0)
                    {
                        sops.sem_num = 2;
                        sops.sem_op = 1; /* controllare se giusto!!! */
                        semop(userListSem, &sops, 1);
                        /* 
                         * se sono l'ultimo lettore e smetto di leggere, allora devo riportare a 0
                         * il valore semaforico in modo che se lo scrittore vuole scrivere possa farlo.
                         */
                    }
                    /* we exit the critical section for the noUserSegReadersPtr variabile */
                    sops.sem_num = 0;
                    sops.sem_op = 1;
                    semop(userListSem, &sops, 1);

                    // The father also waits for all the children
                    // to be ready to continue the execution

                    sops[0].sem_op = -1;
                    semop(fairStartSem, &sops[0], 1);
                    sops[0].sem_op = 0;
                    semop(fairStartSem, &sops[0], 1);

                    /* master lifecycle*/
                    while (1)
                    {
                        /* check if register is full: in that case it must
						 signal itself ? No
						this should be inserted in the master lifecycle*/
                        printf("Master: checking if register's partitions are full...\n");
                        fullRegister = TRUE;
                        for (i = 0; i < 3 && fullRegister; i++)
                        {
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

                        /* cycle that updates the budget list before printing it */
                        /* at every cycle we do the count of budgets in blocks of the i-th partition */
                        for(i = 0; i < REG_PARTITION_COUNT; i++)
                        {
                            /* setting options for getting access to i-th partition of register */
                            /*sops.sem_num = i; /* we want to get access to i-th partition */
                            /*sops.sem_op = -1; /* CHECK IF IT'S THE CORRECT VALUE */
                            /*semop(rdPartSem, &sops, 1);*/

                            /* NUOVO ACCESSO A SEMAFORO IN LETTURA */
                            /* we enter the critical section for the noReaders variabile of i-th partition */
                            sops.sem_num = i;
                            sops.sem_op = -1;
                            semop(rdPartSem, &sops, 1);
                            *noReadersPtr[i]++;
                            if(*noReadersPtr[i] == 1)
                            {
                                sops.sem_num = i;
                                sops.sem_op = -1; /* controllare se giusto!!! */
                                semop(wrPartSem, &sops, 1);
                                /* 
                                * se lo scrittore sta scrivendo, allora il primo lettore che entrerà in questo 
                                * ramo si addormenterà su questo semaforo.
                                * se lo scrittore non sta scrivendo, allora il primo lettore decrementerà di 1 il
                                * valore semaforico, in modo tale se lo scrittore vuole scrivere, si addormenterà 
                                * sul semaforo
                                */
                            }
                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                            sops.sem_num = i;
                            sops.sem_op = 1;
                            semop(rdPartSem, &sops, 1);

                            printf("Master: gained access to %d-th partition of register\n", i);

                            ind_block = prev_read_nblock[i]; /* inizializzo l'indice al blocco in cui mi ero fermato allo scorso ciclo */

                            /* ciclo di scorrimento dei blocchi della i-esima partizione */
                            while(ind_block < regPtrs[index_reg[i]]->nBlocks)
                            { /* CONTROLLARE SE GIUSTO O SE DEVO USARE REG_PARTITION_SIZE */
                                Block block = regPtrs[index_reg[i]]->blockList[ind_block]; /* restituisce il blocco di indice ind_block */
                                ind_tr_in_block = 0;
                                
                                /* scorro la lista di transizioni del blocco di indice ind_block */
                                while(ind_tr_in_block < SO_BLOCK_SIZE)
                                {
                                    Transaction trans = block.transList[ind_tr_in_block]; /* restituisce la transazione di indice ind_tr_in_block */
                                    
                                    ct_updates = 0; /* conta il numero di aggiornamenti di budget fatti per la transazione (totale 2, uno per sender e uno per receiver) */
                                    if(trans.sender == -1)
                                    {
                                        ct_updates++;
                                        /* 
                                        * se il sender è -1, rappresenta transazione di pagamento reward del nodo,
                                        * quindi non bisogna aggiornare il budget del sender, ma solo del receiver.
                                        */
                                    }
                                        
                                    for(el_list = bud_list; el_list != NULL; el_list = el_list->next)
                                    {
                                        /* guardo sender --> devo decrementare di amountSend il suo budget */
                                        /* se il sender è -1, non si entrerà mai nel ramo then (???) */
                                        if(trans.sender == el_list->proc_pid)
                                        {
                                            /* aggiorno il budget */
                                            el_list->budget -= trans.amountSend;
                                            ct_updates++;
                                        }

                                        /* guardo receiver --> devo incrementare di amountSend il suo budget */
                                        if(trans.receiver == el_list->proc_pid)
                                        {
                                            /* aggiorno il budget */
                                            el_list->budget += trans.amountSend;
                                            ct_updates++;
                                        }

                                        /* 
                                        * condizione di terminazione del ciclo, per velocizzare (non serve controllare il resto 
                                        * degli elementi della lista perché ho già aggiornato il budget di sender e receiver della corrente transazione) 
                                        */
                                        if(ct_updates == 2)
                                            break;
                                    }

                                    ind_tr_in_block++;
                                }

                                ind_block++;
                            }

                            prev_read_nblock[i] = ind_block; /* memorizzo il blocco a cui mi sono fermato */

                            /* setting options for releasing resource of i-th partition of register */
                            /*sops.sem_num = i; /* try to release semaphore for patition i */
                            /*sops.sem_op = 1; /* CHECK IF IT'S THE CORRECT VALUE */
                            /*semop(rdPartSem, &sops, 1);*/

                            /* NUOVO ACCESSO A SEMAFORO IN LETTURA */
                            /* we enter the critical section for the noReaders variabile of i-th partition */
                            sops.sem_num = i;
                            sops.sem_op = -1;
                            semop(rdPartSem, &sops, 1);
                            *noReadersPtr[i]--;
                            if(*noReadersPtr[i] == 0)
                            {
                                sops.sem_num = i;
                                sops.sem_op = 1; /* controllare se giusto!!! */
                                semop(wrPartSem, &sops, 1);
                                /* 
                                * se lo scrittore sta scrivendo, allora il primo lettore che entrerà in questo 
                                * ramo si addormenterà su questo semaforo.
                                * se lo scrittore non sta scrivendo, allora il primo lettore decrementerà di 1 il
                                * valore semaforico, in modo tale se lo scrittore vuole scrivere, si addormenterà 
                                * sul semaforo
                                */
                            }
                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                            sops.sem_num = i;
                            sops.sem_op = 1;
                            semop(rdPartSem, &sops, 1);
                        }

                        /* I've read all the data, now it's time to print them */
		  
                        /* print budget of every process with associated PID */
                        printf("Master: Budget of processes:\n");
                        for(el_list = bud_list; el_list != NULL; el_list = el_list->next)
                        {
                            if(el_list->p_type) /* Budget of user process */
                                printf("Master:  - USER PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
                            else /* Budget of node process */
                                printf("Master:  - NODE PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
                        }

                        /* 
                         * creation of a new node process if a transaction doesn't fit in 
                         * any transaction pool of existing node processes
                         */
                        MsgGlobalQueue msg_from_node;
                        int c_msg_read;
                        c_msg_read = 0;
                        int SO_TP_SIZE = atoi(getenv("SO_TP_SIZE"));
                        Transaction transanctions_read[SO_TP_SIZE]; /* array of transactions read from global queue */

                        /* messages reading cycle */
                        /* MSG_COPY solo LINUX, non va bene..... */
                        /* IN CASO DECIDIAMO CHE NON VADA BENE MSG_COPY, DEVO TOGLIERLO E SE NON È IL MESSAGGIO CHE VOGLIO LO DEVO RISCRIVERE SULLA CODA */
                        while(msgrcv(globalQueueId, &msg_from_node, sizeof(msg_from_node)-sizeof(long), getpid(), IPC_NOWAIT | MSG_COPY) != -1 && c_msg_read < SO_TP_SIZE)
                        {
                            /* come dimensione specifichiamo sizeof(msg_from_node)-sizeof(long) perché bisogna specificare la dimensione del testo, non dell'intera struttura */
                            /* come mType prendiamo i messaggi destinati al Master, cioè il suo pid (prende il primo messaggio con quel mType) */
                            /* prendiamo il messaggio con flag MSG_COPY perché altrimenti se non è di tipo NEWNODE lo elimineremmo */
                            
                            /* in questo caso cerchiamo i messaggi con msgContent NEWNODE */
                            if(msg_from_node.msgContent == NEWNODE)
                            {
                                /* 
                                * per aggiungere la transazione alla transaction pool, devo aggiungere un nuovo messaggio 
                                * alla msgqueue che sarebbe la tp del nuovo nodo 
                                * siccome prima di creare la TP del nuovo nodo devo accertarmi che ci sia un nuovo nodo da creare,
                                * creiamo una lista di TPElement di massimo SO_TP_SIZE transazioni e poi quando abbiamo creato la TP
                                * del nuovo nodo ci inseriamo i messaggi sopra. 
                                */
                                memcpy(&transanctions_read[c_msg_read], &msg_from_node.transaction, sizeof(msg_from_node.transaction));
                                /* DA TESTARE !!!!!! */
                                
                                c_msg_read++;

                                /* Removing the message that we have consumed from the global queue */
                                if(msgrcv(globalQueueId, &msg_from_node, sizeof(msg_from_node)-sizeof(long), getpid(), IPC_NOWAIT) == -1)
                                {
                                    unsafeErrorPrint("Master: failed to remove the transaction from the global queue. Error: ");
                                    exit(EXIT_FAILURE);
                                    /* This is necessary, otherwise the message won't be removed from queue and transaction processed two times (?) */
                                }
                            }
                        }

                        /* SHOULD CHECK IF ERRNO is ENOMSG, otherwise an error occurred */
                        if(errno == ENOMSG)
                        {
                            if(c_msg_read == 0)
                            {
                                printf("Master: no creation of new node needed\n");
                            }
                            else 
                            {
                                printf("Master: no more transactions to read from global queue. Starting creation of new node...\n");
                                
                                /******* CREATION OF NEW NODE PROCESS *******/
                                /********************************************/

                                int id_new_friends[SO_FRIENDS_NUM]; /* array to keep track of already chosen new friends */
                                int new; /* flag */
                                int index, tr_written;

                                /* setting every entry of array to -1 (it rappresents "not chosen") */
                                for(i = 0; i < SO_FRIENDS_NUM; i++)
                                    id_new_friends[i] = -1;
                                
                                switch(fork()) 
                                {
                                    case -1:
                                        /* Handle error */
                                        unsafeErrorPrint("Master: failed to fork the new node process. Error: ");
                                        exit(EXIT_FAILURE);
                                        /* Is this necessary ??? */
                                    case 0:
                                        /* NEW NODE */
                                        
                                        /* Adding new node to budgetlist */
                                        new_el = malloc(sizeof(*new_el));
                                        new_el->proc_pid = getpid();
                                        new_el->budget = 0;
                                        new_el->p_type = 1;
                                        new_el->next = bud_list;
                                        bud_list = new_el;
                                        
                                        srand(getpid()); /* we put it here so that for every new node we generate a different sequence */

                                        /* Creation of list of friends for new node */
                                        for(i = 0; i < SO_FRIENDS_NUM; i++)
                                        {
                                            if(i == 0)
                                            {
                                                /* first friend in array, no need to check if already chosen */
                                                index = rand()%SO_NODES_NUM; /* generate new index */
                                            } 
                                            else 
                                            {
                                                new = 0;
                                                /* choosing a new friend */
                                                while(!new)
                                                {
                                                    index = rand()%SO_NODES_NUM; /* generate new index */
                                                    /* check if it is already a friend */
                                                    j = 0;
                                                    while(j < SO_FRIENDS_NUM && !new)
                                                    {
                                                        if(id_new_friends[j] == -1)
                                                            new = 1; /* no friend in this position */
                                                        else if(id_new_friends[j] == index)
                                                            break; /* if friend already chosen, change index */
                                                        j++;
                                                    }
                                                }
                                            }

                                            /* adding new index friend to array */
                                            id_new_friends[i] = index;

                                            /* send a message on global queue to new node informing it of its new friend */
                                            
                                            /* we enter the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = -1;
                                            semop(userListSem, &sops, 1);
                                            *noUserSegReadersPtr++;
                                            if(*noUserSegReadersPtr == 1)
                                            {
                                                sops.sem_num = 2;
                                                sops.sem_op = -1; /* controllare se giusto!!! */
                                                semop(userListSem, &sops, 1);
                                                /* 
                                                 * se lo scrittore sta scrivendo, allora il primo lettore che entrerà in questo 
                                                 * ramo si addormenterà su questo semaforo.
                                                 * se lo scrittore non sta scrivendo, allora il primo lettore decrementerà di 1 il
                                                 * valore semaforico, in modo tale se lo scrittore vuole scrivere, si addormenterà 
                                                 * sul semaforo
                                                 */
                                            }
                                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = 1;
                                            semop(userListSem, &sops, 1);

                                            /* declaration of node to send to new friend */
                                            MsgGlobalQueue msg_to_node;
                                            msg_to_node.mType = getpid();
                                            msg_to_node.msgContent = FRIENDINIT;
                                            sops.sem_num = 
                                            msg_to_node.friend.procId = nodesList[index].procId; /* devo accedervi in mutua esclusione (vedi foto Fede) */
                                            msg_to_node.friend.procState = ACTIVE;
                                            
                                            /* we enter the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = -1;
                                            semop(userListSem, &sops, 1);
                                            *noUserSegReadersPtr--;
                                            if(*noUserSegReadersPtr == 0)
                                            {
                                                sops.sem_num = 2;
                                                sops.sem_op = 1; /* controllare se giusto!!! */
                                                semop(userListSem, &sops, 1);
                                                /* 
                                                 * se sono l'ultimo lettore e smetto di leggere, allora devo riportare a 0
                                                 * il valore semaforico in modo che se lo scrittore vuole scrivere possa farlo.
                                                 */
                                            }
                                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = 1;
                                            semop(userListSem, &sops, 1);

                                            if(msgsnd(globalQueueId, &msg_to_node, sizeof(msg_to_node)-sizeof(long), 0) == -1)
                                            {
                                                unsafeErrorPrint("Master: failed to send a friend node to the new node process. Error: ");
                                                exit(EXIT_FAILURE);
                                                /* This is necessary, otherwise the node won't be notified of its friend */
                                            }
                                        }

                                        /* resetting every entry of array to -1 (it rappresents "not chosen") */
                                        for(i = 0; i < SO_FRIENDS_NUM; i++)
                                            id_new_friends[i] = -1;

                                        /* Selection of random nodes which need to add the new node as a friend */
                                        for(i = 0; i < SO_FRIENDS_NUM; i++)
                                        {
                                            if(i == 0)
                                            {
                                                /* first node in array, no need to check if already chosen */
                                                index = rand()%SO_NODES_NUM; /* generate new index */
                                            } 
                                            else 
                                            {
                                                new = 0;
                                                /* choosing a new node */
                                                while(!new)
                                                {
                                                    index = rand()%SO_NODES_NUM; /* generate new index */
                                                    /* check if it has already been chosen */
                                                    j = 0;
                                                    while(j < SO_FRIENDS_NUM && !new)
                                                    {
                                                        if(id_new_friends[j] == -1)
                                                            new = 1; /* no node in this position */
                                                        else if(id_new_friends[j] == index)
                                                            break; /* if node already chosen, change index */
                                                        j++;
                                                    }
                                                }
                                            }

                                            /* adding new index node to array */
                                            id_new_friends[i] = index;

                                            /* we enter the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = -1;
                                            semop(userListSem, &sops, 1);
                                            *noUserSegReadersPtr++;
                                            if(*noUserSegReadersPtr == 1)
                                            {
                                                sops.sem_num = 2;
                                                sops.sem_op = -1; /* controllare se giusto!!! */
                                                semop(userListSem, &sops, 1);
                                                /* 
                                                 * se lo scrittore sta scrivendo, allora il primo lettore che entrerà in questo 
                                                 * ramo si addormenterà su questo semaforo.
                                                 * se lo scrittore non sta scrivendo, allora il primo lettore decrementerà di 1 il
                                                 * valore semaforico, in modo tale se lo scrittore vuole scrivere, si addormenterà 
                                                 * sul semaforo
                                                 */
                                            }
                                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = 1;
                                            semop(userListSem, &sops, 1);

                                            /* here we notice the friend node of its new friend (the new node created here) */
                                            MsgGlobalQueue msg_to_node;
                                            msg_to_node.mType = nodesList[index].procId; /* devo accedervi in mutua esclusione (vedi foto Fede) */
                                            msg_to_node.msgContent = NEWFRIEND;
                                            msg_to_node.friend.procId = getpid();
                                            msg_to_node.friend.procState = ACTIVE;

                                            /* we enter the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = -1;
                                            semop(userListSem, &sops, 1);
                                            *noUserSegReadersPtr--;
                                            if(*noUserSegReadersPtr == 0)
                                            {
                                                sops.sem_num = 2;
                                                sops.sem_op = 1; /* controllare se giusto!!! */
                                                semop(userListSem, &sops, 1);
                                                /* 
                                                 * se sono l'ultimo lettore e smetto di leggere, allora devo riportare a 0
                                                 * il valore semaforico in modo che se lo scrittore vuole scrivere possa farlo.
                                                 */
                                            }
                                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                                            sops.sem_num = 0;
                                            sops.sem_op = 1;
                                            semop(userListSem, &sops, 1);

                                            if(msgsnd(globalQueueId, &msg_to_node, sizeof(msg_to_node)-sizeof(long), 0) == -1)
                                            {
                                                unsafeErrorPrint("Master: failed to send a message to inform a node of its new friend. Error: ");
                                                exit(EXIT_FAILURE);
                                                /* This is necessary, otherwise the node won't be notified of its new friend (?) */
                                            }
                                        }

                                        /* CAPIRE SE DA ULTIME DISPOSIZIONI SI DEVE ANCORA FARE O NO */

                                        /* add a new entry to the tpList array */
                                        tpList = (TPElement *)realloc(tpList, sizeof(*tpList) + sizeof(TPElement));
                                        int tpl_length = sizeof(*tpList)/sizeof(TPElement); /* get tpList length */
                                        /* Initialize messages queue for transactions pools */
                                        tpList[tpl_length-1].procId = getpid();
                                        tpList[tpl_length-1].msgQId = msgget(ftok(MSGFILEPATH, getpid()), IPC_CREAT | IPC_EXCL | 0600);
                                        
                                        if(tpList[tpl_length-1].msgQId == -1)
                                        {
                                            unsafeErrorPrint("Master: failed to create the message queue for the transaction pool of the new node process. Error: ");
                                            exit(EXIT_FAILURE);
                                        }

                                        int tp_new_node = tpList[tpl_length-1].msgQId;
                                        /* here we have to insert transactions read from global queue in new node TP*/
                                        for(tr_written = 0; tr_written < c_msg_read; tr_written++)
                                        {   /* c_msg_read is the number of transactions actually read */
                                            MsgTP new_trans;
                                            new_trans.mType = getpid();
                                            memcpy(&new_trans.transaction, &transanctions_read[tr_written], sizeof(new_trans.transaction));
                                            if(msgsnd(tp_new_node, &new_trans, sizeof(new_trans)-sizeof(long), 0) == -1)
                                            {
                                                unsafeErrorPrint("Master: failed to send a transaction to the new node process. Error: ");
                                                exit(EXIT_FAILURE);
                                                /* This is necessary, otherwise a transaction could be lost forever */
                                            }
                                        }

                                        /* TO COMPLETE....... */
                                        /*execve(...);*/ 
                                        break;
                                    default:
                                        /* MASTER */
                                        break;
                                }
                            }
                        }
                        else 
                        {
                            unsafeErrorPrint("Master: failed to retrieve messages from global queue. Error: ");
                            /* 
                             * DEVO FARE EXIT????? 
                             * Dipende, perché se è un errore momentaneo che al prossimo ciclo non riaccade, allora non 
                             * è necessario fare la exit, ma se si verifica un errore a tutti i cicli non è possibile 
                             * leggere messaggi dalla coda, quindi si finisce con il non crare un nuovo nodo, non processare
                             * alcune transazioni e si può riempire la coda globale, rischiando di mandare in wait tutti i 
                             * restanti processi nodi e utenti. Quindi sarebbe opportuno fare exit appena si verifica un errore
                             * oppure utilizzare un contatore (occorre stabilire una soglia di ripetizione dell'errore). Per 
                             * ora lo lasciamo.
                             */
                            exit(EXIT_FAILURE);
                        }
                        
                        /* AGGIUNGERE PARTE IN CUI CONTROLLO SE DEGLI UTENTI SONO TERMINATI PER AGGIORNARE LA LISTA DI UTENTI IN MEMORIA CONDIVISA!!!! */
                        child_pid = waitpid(-1, &status, WNOHANG); /* we put WNOHANG flag so that the master does not block waiting for a child to terminate */ 
                        if(child_pid > 0) 
                        {
                            /* If child_pid is bigger than 0, a child process as terminated */
                            /* 
                             * How do we check if it is a user process or a node process ? 
                             * We don't do it explicitly, we check every element in the users list
                             * and we check if the child_pid is in the users list. If yes, we update
                             * its state to TERMINATED, otherwise we notice that. [DO WE HAVE TO CHECK IF ITS A NODE PROCESS ???]
                             */

                            /* we enter the critical section for the usersList */
                            sops.sem_num = 2;
                            sops.sem_op = -1;
                            semop(userListSem, &sops, 1);
                            
                            int pc_found = 0; /* flag to check if terminated process is a user or not */

                            /* cycle to search for the user process */
                            for(i = 0; i < SO_USERS_NUM; i++)
                            {
                                if(usersList[i].procId == child_pid)
                                {
                                    /* we found the user process terminated */
                                    usersList[i].procState = TERMINATED;
                                    pc_found = 1;
                                    break;
                                    /* we stop the cycle now that we found the process */
                                }
                            }

                            /* we exit the critical section for the usersList */
                            sops.sem_num = 2;
                            sops.sem_op = 1;
                            semop(userListSem, &sops, 1);

                            if(!pc_found) 
                            {
                                char * msg = NULL;
                                sprintf(msg, "Master: the terminated process with pid %5d was not a user process\n", child_pid);
                                unsafeErrorPrint(msg);
                            }
                            else
                                printf("Master: a user process has terminated\n");
                        }

                        /* now sleep for 1 second */
                        nanosleep(&onesec, &tim);
                    }
                }
            }
        }
    }

    /* POSTCONDIZIONE: all'esecuzione di questa system call
		l'handler di fine simulazione è già stato eseguito*/
    exit(exitCode);
}

/* Function to free the space dedicated to the budget list */
void budgetlist_free(budgetlist p)
{
	if (p == NULL) return;
	
	budgetlist_free(p->next);
	free(p);
}

void endOfSimulation(int sig)
{ /* IT MUST BE REENTRANT!!!!
	// Notify children
	// sends termination signal to all the processes
	// that are part of the master's group (in this case we
	// reach every children with just one system call).
	// how to check if everyone was signaled (it returns true even if
	// only one signal was sent)*/
    char *terminationMessage = NULL;
    int ret = -1;
    char *aus;
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
	// viene inviato anche al master stesso ? In teoria no
	// come assicurarsi che venga inviato a tutti?
	// fallisce se non viene inviato a nessuno
	// ma inviato != consegnato???*/
    for (i = 0; i < NO_ATTEMPS && !done; i++)
    {
        /* error check*/
        write(STDOUT_FILENO,
              "Master: trying to terminate simulation...\n",
              strlen("Master: trying to terminate simulation...\n"));
        if (kill(0, SIGUSR1) == 0)
        {
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
            write(STDOUT_FILENO,
                  "Master: waiting for children to terminate...\n",
                  strlen("Master: waiting for children to terminate...\n"));
            while (wait(NULL) != -1)
                ;

            if (errno != ECHILD)
            {
                /*
				// print report: we use the write system call: slower, but async-signal-safe
				// Termination message composition
				// we use only one system call for performances' sake.
				// termination reason*/
                write(STDOUT_FILENO,
                      "Master: simulation terminated successfully. Printing report...\n",
                      strlen("Master: simulation terminated successfully. Printing report...\n"));
                if (sig == SIGALRM)
                    aus = "Termination reason: end of simulation.\n";
                else
                    aus = "Termination reason: register book is full.\n";

                /* Users and nodes budgets*/
                printBudget();

                /*Per la stampa degli errori non si può usare perror, perchè non è elencata* tra la funzioni signal
				in teoria non si può usare nemmno sprintf*/

                /* processes terminated before end of simulation*/
                ret = sprintf(terminationMessage,
                              "Processes terminated before end of simulation: %d\n",
                              noTerminated);
                if (ret <= 0)
                {
                    safeErrorPrint("Master: sprintf failed to format process count's string. ");
                    exitCode = EXIT_FAILURE;
                }

                /* Blocks in register*/
                ret = sprintf(aus, "There are %d blocks in the register.\n",
                              regPtrs[0]->nBlocks + regPtrs[1]->nBlocks + regPtrs[2]->nBlocks);
                if (ret <= 0)
                {
                    safeErrorPrint("Master: sprintf failed to format number of blocks' string. ");
                    exitCode = EXIT_FAILURE;
                }

                /* Writes termination message on standard output*/
                ret = write(STDOUT_FILENO, terminationMessage, strlen(terminationMessage));
                if (ret == -1)
                {
                    safeErrorPrint("Master: failed to write termination message. Error: ");
                    exitCode = EXIT_FAILURE;
                }

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

            /* Releasing local variables' memory*/
            free(terminationMessage);
            free(aus);
        }
        else
            safeErrorPrint("Master: failed to signal children for end of simulation. Error: ");
    }

    if (!done)
        exitCode = EXIT_FAILURE;

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

    char *aus = NULL;
    int msgLength = 0;
    int i = 0;

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
            if (msgLength < 0)
                safeErrorPrint("Master: failed to format output message in IPC deallocation.");
            else
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
                if (msgLength < 0)
                    safeErrorPrint("Master: failed to format output message in IPC deallocation.");
                else
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
        free(regPtrs[i]);
    }
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
            free(usersList);
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
            free(nodesListId);
        }
    }

    /* Partition one shared variable deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating partition one shared variable...\n",
          strlen("Master: deallocating partition one shared variable..\n"));
    if (shmdt(noReadersPtr[0]) == -1)
    {
        safeErrorPrint("Master: failed to detach from partition one shared variable. Error: ");
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        if (shmctl(noReaders[0], IPC_RMID, NULL) == -1)
        {
            safeErrorPrint("Master: failed to remove partition one shared variable. Error: ");
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: partition one shared variable successfully removed.\n",
                  strlen("Master: partition one shared variable successfully removed.\n"));
        }
    }

    /* Partition two shared variable deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating partition two shared variable...\n",
          strlen("Master: deallocating partition two shared variable..\n"));
    if (shmdt(noReadersPtr[1]) == -1)
    {
        safeErrorPrint("Master: failed to detach from partition two shared variable. Error: ");
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        if (shmctl(noReaders[1], IPC_RMID, NULL) == -1)
        {
            safeErrorPrint("Master: failed to remove partition two shared variable. Error: ");
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: partition two shared variable successfully removed.\n",
                  strlen("Master: partition two shared variable successfully removed.\n"));
        }
    }

    /* Partition three shared variable deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating partition three shared variable...\n",
          strlen("Master: deallocating partition three shared variable..\n"));
    if (shmdt(noReadersPtr[2]) == -1)
    {
        safeErrorPrint("Master: failed to detach from partition three shared variable. Error: ");
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        if (shmctl(noReaders[2], IPC_RMID, NULL) == -1)
        {
            safeErrorPrint("Master: failed to remove partition three shared variable. Error: ");
            *exitCode = EXIT_FAILURE;
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: partition three shared variable successfully removed.\n",
                  strlen("Master: partition three shared variable successfully removed.\n"));
        }
    }

    /* Transaction pools list deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating transaction pools...\n",
          strlen("Master: deallocating transaction pools...\n"));
    while (tpList != NULL)
    {

        if (msgctl(tpList->msgQId, IPC_RMID, NULL) == -1)
        {
            msgLength = sprintf(aus,
                                "Master: failed to remove transaction pool of process %ld",
                                (long)tpList->procId);
            if (msgLength < 0)
                safeErrorPrint("Master: failed to format output message in IPC deallocation.");
            else
                write(STDERR_FILENO, aus, msgLength);

            *exitCode = EXIT_FAILURE;
        }
        else
        {
            msgLength = sprintf(aus,
                                "Master: transaction pool of node of PID %ld successfully removed.\n",
                                tpList->procId);
            write(STDOUT_FILENO, aus, msgLength);
        }
    }
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
        if (msgLength < 0)
            safeErrorPrint("Master: failed to format output message in IPC deallocation.");
        else
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
        if (msgLength < 0)
            safeErrorPrint("Master: failed to format output message in IPC deallocation.");
        else
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
        if (msgLength < 0)
            safeErrorPrint("Master: failed to format output message in IPC deallocation.");
        else
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
        if (msgLength < 0)
            safeErrorPrint("Master: failed to format output message in IPC deallocation.");
        else
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
        sprintf(aus, "Master: failed to remove users' list semaphores");
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
        sprintf(aus, "Master: failed to remove register's paritions mutex semaphores");
        write(STDERR_FILENO, aus, msgLength);
        *exitCode = EXIT_FAILURE;
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: register's paritions mutex semaphores successfully removed.\n",
              strlen("Master: register's paritions mutex semaphores successfully removed.\n"));
    }

    // deallocare segmenti var condivise e semfori rimanenti

    /* Releasing local variables' memory*/
    free(aus);

    /* Releasing budget list's memory */
    budgetlist_free(bud_list);
}
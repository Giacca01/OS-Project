/*
    Cose da fare:
        Test: Testare persistenza associazione
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

int fairStartSem=-1; /* Id of the set that contais the three semaphores*/
                  /* used to write on the register's partitions*/
int wrPartSem=-1;    /* Id of the set that contais the three semaphores*/
                  /* used to write on the register's partitions*/
int rdPartSem=-1;    /* Id of the set that contais the three semaphores*/
                  /* used to read from the register's partitions*/
int mutexPartSem = -1; /* id of the set that contains the three sempagores used to
                        to access the number of readers variables of the registers partitions
                        in mutual exclusion*/

/* Si dovrebbe fare due vettori*/
int *noReadersPartitions = NULL; /* Pointer to the array contains the ids of the shared memory segments 
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
int userListSem = -1; /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                    // to read and write users list*/
int noUserSegReaders = -1; /* id of the shared memory segment that contains the variable used to syncronize
                           // readers and writes access to users list*/
int *noUserSegReadersPtr = NULL;

int nodeListSem = -1; // Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                      // to read and write nodes list

int noTerminated = 0; // NUmber of processes that terminated before end of simulation
/******************************************/

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
/* Long???*/
int SO_USERS_NUM,   /* Number of user processes NOn è "statico" ???*/
    SO_NODES_NUM,   /* Number of node processes ?? NOn è "statico" ???*/
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
    /* Reading line by line, max 128 bytes*/
    const unsigned MAX_LENGTH = 128;
    /* Array that will contain the lines read from the file
    // each "row" of the "matrix" will contain a different file line*/
    char line[14][MAX_LENGTH];
    /* Counter of the number of lines in the file*/
    int k = 0;
    char *aus = NULL;
    int exitCode = 0;

    /* Handles any error in opening the file*/
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

        /* It inserts the parameters read into environment variables*/
        for (int i = 0; i < k; i++)
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
    int j = 0;

    /* CORREGGERE USANDO CALLOC E FARE SEGNALAZIONE ERRORI
    // calloc???*/
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

    usersList = (ProcListElem *)malloc(SO_USERS_NUM * sizeof(ProcListElem));

    nodesList = (ProcListElem *)malloc(SO_NODES_NUM * sizeof(ProcListElem));

    tpList = (TPElement *)malloc(SO_NODES_NUM * sizeof(TPElement));

    noReadersPartitions = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    if (noReadersPartitions == NULL)
        unsafeErrorPrint("Master: failed to allocate shared variables' ids array. ");
    else {
        noReadersPartitionsPtrs = (int **)calloc(REG_PARTITION_COUNT, sizeof(int *));
        if (noReadersPartitionsPtrs == NULL)
            unsafeErrorPrint("Master: failed to allocate shared variables' array. ");
        else {
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

    /* Aggiungere segmenti per variabili condivise*/
    noReadersPartitions[0] = shmget(ftok(SHMFILEPATH, NOREADERSONESEED), sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, 0);
    *(noReadersPartitionsPtrs[0]) = 0;

    noReadersPartitions[1] = shmget(ftok(SHMFILEPATH, NOREADERSTWOSEED), sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    *(noReadersPartitionsPtrs[1]) = 0;

    noReadersPartitions[2] = shmget(ftok(SHMFILEPATH, NOREADERSTHREESEED), sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    *(noReadersPartitionsPtrs[2]) = 0;

    noUserSegReaders = shmget(ftok(SHMFILEPATH, NOUSRSEGRDERSSEED), sizeof(SO_USERS_NUM), S_IRUSR | S_IWUSR);
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
                    /* Read configuration parameters from
                    // file and save them as environment variables*/
                    if (readConfigParameters() == -1)
                        exit(EXIT_FAILURE);

                    /*****  Creates and initialize the IPC Facilities   *****/
                    /********************************************************/
                    if (createIPCFacilties() == TRUE){ 
                        initializeIPCFacilities();
                        /********************************************************/
                        /********************************************************/

                        /*****  Creates SO_USERS_NUM children   *****/
                        /********************************************/
                        for (int i = 0; i < SO_USERS_NUM; i++)
                        {
                            /*
                                CORREGGERE: manca l'error handling
                                e tutte queste semop son ogiuste??
                            */
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

                        /* The father also waits for all the children
                        // to be ready to continue the execution*/

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
                        }
                    } else {
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

void freeGlobalVariables(){

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

    /* Partitions' shared variable deallocation*/
    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(noReadersPartitionsPtrs[i]) == -1)
        {
            msgLength = sprintf(aus,
                                "Master: failed to detach from partition number %d shared variable segment.\n",
                                (i + 1));
            if (msgLength < 0)
                safeErrorPrint("Master: failed to format output message in IPC deallocation.");
            else
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
        free(noReadersPartitionsPtrs[i]);
    }
    free(noReadersPartitions);
    

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
}
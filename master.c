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

/**** Headers inclusion ****/
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "info.h"
/**** End of Headers inclusion ****/

/**** Constants definition ****/
#define NO_ATTEMPS_TERM 3           /* Maximum number of attemps to terminate the simulation*/
#define MAX_PRINT_PROCESSES 15      /* Maximum number of processes of which we show budget, if noEffectiveNodes + noEffectiveUsers > MAX_PRINT_PROCESSES we only print max and min budget */
#define NO_ATTEMPTS_UPDATE_BUDGET 3 /* Number of attempts to update budget reading a block on register */
/**** End of Constants definition ****/

/**********  Function prototypes  *****************/
boolean assignEnvironmentVariables();
boolean readConfigParameters();
boolean allocateGlobalStructures();
boolean initializeIPCFacilities();

void endOfSimulation(int);
void printBudget();
boolean deallocateFacilities(int *);
void freeGlobalVariables();
void checkNodeCreationRequests();
/**************************************************/

/*****        Global structures        *****/

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

Register **regPtrs = NULL; /* Array of pointers to register's partitions */
int *regPartsIds = NULL;   /* Array of ids of register's partitions */

int usersListId = -1;
ProcListElem *usersList = NULL;

int nodesListId = -1;
ProcListElem *nodesList = NULL;

TPElement *tpList = NULL;

int globalQueueId = -1;

int fairStartSem = -1; /* Id of the set that contais the three semaphores*/
                       /* used to write on the register's partitions*/

int wrPartSem = -1; /* Id of the set that contais the three semaphores*/
                    /* used to write on the register's partitions*/

int rdPartSem = -1; /* Id of the set that contais the three semaphores*/
                    /* used to read from the register's partitions*/

int mutexPartSem = -1; /* id of the set that contains the three sempagores used to
                        to access the number of readers variables of the registers partitions
                        in mutual exclusion*/

/* Si dovrebbe fare due vettori*/
int *noReadersPartitions = NULL;      /* Pointer to the array contains the ids of the shared memory segments
                                        where the variables used to syncronize
                                        readers and writes access to register's partition are stored, e.g:
                                        noReadersPartitions[0]: id of first partition's shared variable
                                        */
int **noReadersPartitionsPtrs = NULL; /* Pointer to the array contains the variables used to syncronize
                                        readers and writes access to register's partition. E.g:
                                        noReadersPartitions[0]: pointer to the first partition's shared variable
                                        */
int userListSem = -1;                 /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                                        to read and write users list*/
int noUserSegReaders = -1;            /* id of the shared memory segment that contains the variable used to syncronize
                                        readers and writes access to users list*/
int *noUserSegReadersPtr = NULL;      /* Pointer to the shared memory segment described above */

int nodeListSem = -1; /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                        to read and write nodes list
                        */

int noNodeSegReaders = -1; /* id of the shared memory segment that contains the variable used to syncronize
                              readers and writers access to nodes list */
int *noNodeSegReadersPtr = NULL;

/*
    We use a long int variable to handle an outstanding number
    of child processes
*/
long noTerminatedUsers = 0; /* NUmber of users that terminated before end of simulation*/
long noTerminatedNodes = 0; /* NUmber of processes that terminated before end of simulation*/

long noEffectiveNodes = 0; /* Holds the effective number of nodes */
long noEffectiveUsers = 0; /* Holds the effective number of users */
long noAllTimesNodes = 0;  /* Historical number of nodes: it counts also the terminated ones */
long noAllTimesUsers = 0;  /* Historical number of users: it counts also the terminated ones */

long tplLength = 0; /* keeps tpList length */

extern char **environ;
struct timespec now;
int *extractedFriendsIndex;
/***** End of Global structures *****/

/***** Configuration parameters *****/
long SO_USERS_NUM,
    SO_NODES_NUM,
    SO_REWARD,
    SO_MIN_TRANS_GEN_NSEC,
    SO_MAX_TRANS_GEN_NSEC,
    SO_RETRY,
    SO_TP_SIZE,
    SO_MIN_TRANS_PROC_NSEC,
    SO_MAX_TRANS_PROC_NSEC,
    SO_BUDGET_INIT,
    SO_SIM_SEC,
    SO_FRIENDS_NUM,
    SO_HOPS;
/***** End of Configuration parameters ***********/
long maxNumNode = 0;
char line[CONF_MAX_LINE_NO][CONF_MAX_LINE_SIZE];

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
    /*
        Carica in extractedFriendsIndex la posizione
        in nodesList degli amici
        (così facendo estrae gli amici)
    */
    int x, p;
    int count;
    int i = 0;

    int n;

    /*
        CORREGGERE: controllare lo stato dell'amico
    */
    for (count = 0; count < SO_FRIENDS_NUM; count++)
    {
        do
        {
            clock_gettime(CLOCK_REALTIME, &now);
            n = now.tv_nsec % maxNumNode;
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

/**************** CAPIRE SE SPOSTARE IN INFO.H O SE LASCIARE QUI ****************/

/* struct that rappresents a process and its budget*/
typedef struct proc_budget
{
    pid_t proc_pid;
    int budget;
    int p_type;               /* type of node: 0 if user, 1 if node */
    struct proc_budget *prev; /* keeps link to previous node */
    struct proc_budget *next; /* keeps link to next node */
} proc_budget;

/* linked list of budgets for every user and node process */
/*
    L'idea è che il libro mastro sia immutabile e non sia quindi
    necessario scorrerlo tutto ogni volta.
    Per migliorare l'efficienza del calcolo del budget
    possiamo limitarci ad aggiornare i budget sulla base
    delle sole transazioni inserite nel libro mastro
    tra un aggiornamento e l'altro
*/
typedef proc_budget *budgetlist;
/* budgetlist is implemented as a linked list */

/* Function to free the space dedicated to the budget list p passed as argument */
void budgetlist_free(budgetlist p);

/*
 * Function that inserts in the global list bud_list the node passed as
 * argument in an ordered way (the list is ordered in ascending order).
 * We want to keep the list sorted to implement a more efficient
 * budget calculation.
 */
void insert_ordered(budgetlist);

/*
 * Function that searches in the gloabl list bud_list for an element with
 * proc_pid as the one passed as first argument; if it's found, upgrades its budget
 * adding the second argument, which is a positive or negative amount.
 * It returns -1 in case an error happens, otherwise it returns 0 on success.
 */
int update_budget(pid_t, int);

/* initialization of the budgetlist head - array to maintain budgets read from ledger */
budgetlist bud_list_head = NULL;
/* initialization of the budgetlist tail - array to maintain budgets read from ledger */
budgetlist bud_list_tail = NULL;
long masterPid = -1;

/**/

/**************** CAPIRE SE SPOSTARE IN INFO.H O SE LASCIARE QUI ****************/

int main(int argc, char *argv[])
{
    pid_t child_pid;
    struct sembuf sops[3];
    msgbuff tmpFriend;
    sigset_t set;
    struct sigaction act;
    int fullRegister = TRUE;
    int exitCode = EXIT_FAILURE;
    key_t key;
    int i = 0, j = 0;

    /* elements for creation of budgetlist */
    budgetlist new_el;
    budgetlist el_list;

    /* definition of objects necessary for nanosleep */
    struct timespec onesec, tim;

    /* definition of indexes for cycles */
    int k, ct_updates;

    /* array that keeps memory of the block we stopped reading budgets for every partition of register  */
    int prev_read_nblock[REG_PARTITION_COUNT];

    int ind_block;           /* indice per scorrimento blocchi */
    int ind_tr_in_block = 0; /* indice per scorrimento transazioni in blocco */

    /* variable that keeps track of the attempts to update a budget,
    if > NO_ATTEMPTS_UPDATE_BUDGET we switch to next block */
    int bud_update_attempts = 0;

    /* variable that keeps memory of the previous budget, used in print of budget */
    /*int prev_bud = 0;*/

    /* counters for active user and node processes */
    /*int c_users_active, c_nodes_active;*/

    /* declaring message structures used with global queue */
    MsgGlobalQueue msg_from_node, msg_from_user, msg_to_node;

    /* declaring counter for transactions read from global queue */
    /*int c_msg_read;*/

    /* declaration of array of transactions for new node creation*/
    /*Transaction *transanctions_read;*/

    /* variables for new node creation */
    /*int *id_new_friends;*/ /* array to keep track of already chosen new friends */
    /* int new;*/            /* flag */
    /*int index, tr_written;*/

    /* declaring of message for new transaction pool of new node */
    /* MsgTP new_trans;*/

    /* declaring of variable for sending transactions over TP of new node */
    /*int tp_new_node;*/

    /* declaring of variables for budget update */
    Block block;
    Transaction trans;

    /* variables that keeps track of user terminated */
    /*int noUserTerminated = 0;*/

    /*char *argVec[] = {NULL};*/
    /*char *envVec[] = {NULL};*/

    char *aus = NULL;

    /* Set common semaphore options*/
    sops[0].sem_num = 0;
    sops[0].sem_flg = 0;
    sops[1].sem_num = 2;
    sops[1].sem_flg = 0;
    sops[2].sem_num = 2;
    sops[2].sem_flg = 0;

    extractedFriendsIndex = (int *)malloc(SO_FRIENDS_NUM * sizeof(int));

    /* setting data for waiting for one second */
    onesec.tv_sec = 1;
    onesec.tv_nsec = 0;

    /* erasing of prev_read_nblock array */
    for (i = 0; i < REG_PARTITION_COUNT; i++)
        prev_read_nblock[i] = 0; /* qui memorizzo il blocco a cui mi sono fermato allo scorso ciclo nella i-esima partizione */

    masterPid = (long)getpid();

    signal(SIGINT, endOfSimulation);

    /* Read configuration parameters from
                    // file and save them as environment variables*/
    printf("PID MASTER: %ld\n", masterPid);
    printf("**** Master: simulation configuration started ****\n");

    printf("Master: reading configuration parameters...\n");
    if (readConfigParameters() == FALSE)
        endOfSimulation(-1);

    printf("Master: setting up simulation timer...\n");
    printf("Master simulation lasts %ld seconds\n", SO_SIM_SEC);
    /* No previous alarms were set, so it must return 0*/
    /*
        Usando alarm i figli non ricevono nessuno di questi timer
    */
    if (alarm(SO_SIM_SEC) != 0)
        unsafeErrorPrint("Master: failed to set up simulation timer. ", __LINE__);
    else
    {
        printf("Master: setting up signal mask...\n");
        if (sigfillset(&set) == -1)
            unsafeErrorPrint("Master: failed to initialize signals mask. Error", __LINE__);
        else
        {
            /* We block all the signals during the execution of the handler*/
            act.sa_handler = endOfSimulation;
            act.sa_mask = set;
            printf("Master: signal mask initialized successfully.\n");

            printf("Master: setting end of timer disposition...\n");
            if (sigaction(SIGALRM, &act, NULL) == -1)
                unsafeErrorPrint("Master: failed to set end of timer disposition. Error", __LINE__);
            else
            {
                printf("Master: setting end of simulation disposition...\n");
                if (sigaction(SIGUSR1, &act, NULL) == -1)
                    unsafeErrorPrint("Master: failed to set end of simulation disposition. Error", __LINE__);
                else
                {

                    maxNumNode = SO_NODES_NUM + MAX_ADDITIONAL_NODES;

                    printf("Master: creating IPC facilitites...\n");
                    if (allocateGlobalStructures() == TRUE)
                    {
                        printf("Master: initializating IPC facilitites...\n");
                        if (initializeIPCFacilities() == TRUE)
                        {
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
                                    unsafeErrorPrint("Master: fork failed. Error", __LINE__);
                                    /*
                                                (**)
                                                In case we failed to create a process we end
                                                the simulation.
                                                This solution is extended to every operation required to create a node/user.
                                                This solution is quite restrictive, but we have to consider
                                                that loosing even one process before it even started
                                                means violating the project requirments
                                            */
                                    endOfSimulation(-1);
                                case 0:
                                    /*
                                        // The process tells the father that it is ready to run
                                        // and that it waits for all processes to be ready*/
                                    printf("User of PID %ld starts its execution....\n", (long)getpid());
                                    /*
                                            For test's sake
                                        */

                                    signal(SIGALRM, SIG_IGN);
                                    /*
                                        (Almeno) questa serve davvero
                                        perchè il segnale di fine simulazione potrebe arrivare
                                        prima che i figli inizino la loro computazione
                                    */
                                    signal(SIGUSR1, tmpHandler);
                                    /*
                                sops[0].sem_op = -1;
                                semop(fairStartSem, &sops[0], 1);*/

                                    printf("User %d is waiting for simulation to start....\n", i);
                                    sops[0].sem_op = 0;
                                    sops[0].sem_num = 0;
                                    sops[0].sem_flg = 0;
                                    if (semop(fairStartSem, &sops[0], 1) == -1)
                                    {
                                        /*
                                            See comment above (**)
                                        */
                                        unsafeErrorPrint("User: failed to wait for zero on start semaphore. Error ", __LINE__);
                                        endOfSimulation(-1);
                                    }
                                    else
                                    {
                                        /* Temporary part to get the process to do something*/
                                        if (execle("user.out", "user", NULL, environ) == -1)
                                        {
                                            unsafeErrorPrint("User: failed to load user's code. Error", __LINE__);
                                            endOfSimulation(-1);
                                        }
                                        /*
                                                do_stuff(1);
                                                printf("Eseguo user...\n");
                                                printf("User done! PID:%d\n", getpid());
                                                busy_cpu(1);
                                                exit(i);*/
                                    }
                                    break;

                                default:
                                    noEffectiveUsers++;
                                    noAllTimesUsers++;
                                    sops[0].sem_num = 0;
                                    sops[0].sem_op = -1;
                                    sops[0].sem_flg = IPC_NOWAIT;
                                    if (semop(fairStartSem, &sops[0], 1) == -1)
                                    {
                                        safeErrorPrint("Master: failed to decrement start semaphore. Error", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    /* Save users processes pid and state into usersList*/

                                    /*
                                                No user or node is writing or reading on the
                                                read but it's better to be one hundred percent
                                                to check no one is reading or writing from the list
                                            */
                                    /*
                                                ENTRY SECTION:
                                                Reserve read semaphore and Reserve write semaphore
                                            */
                                    sops[0].sem_op = -1;
                                    sops[0].sem_num = 1;

                                    sops[1].sem_op = -1;
                                    sops[1].sem_num = 2;
                                    if (semop(userListSem, sops, 2) == -1)
                                    {
                                        unsafeErrorPrint("Master: failed to reserve users list semaphore for writing operation. Error ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    usersList[i].procId = child_pid;
                                    usersList[i].procState = ACTIVE;

                                    /*
                                                Exit section
                                            */
                                    sops[0].sem_op = 1;
                                    sops[0].sem_num = 1;

                                    sops[1].sem_op = 1;
                                    sops[1].sem_num = 2;
                                    if (semop(userListSem, sops, 2) == -1)
                                    {
                                        unsafeErrorPrint("Master: failed to release users list semaphore for writing operation. Error ", __LINE__);
                                        endOfSimulation(-1);
                                    }

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
                                    unsafeErrorPrint("Master: fork failed. Error", __LINE__);
                                    endOfSimulation(-1);
                                case 0:
                                    /*
                                        // The process tells the father that it is ready to run
                                        // and that it waits for all processes to be ready*/
                                    printf("Node of PID %ld starts its execution....\n", (long)getpid());
                                    /*sops[0].sem_op = -1;
                                        semop(fairStartSem, &sops[0], 1);*/

                                    signal(SIGALRM, SIG_IGN);
                                    /*
                                        (Almeno) questa serve davvero
                                        perchè il segnale di fine simulazione potrebe arrivare
                                        prima che i figli inizino la loro computazione
                                    */
                                    signal(SIGUSR1, tmpHandler);

                                    /* Temporary part to get the process to do something*/
                                    if (execle("node.out", "node", "NORMAL", NULL, environ) == -1)
                                        unsafeErrorPrint("Node: failed to load node's code. Error", __LINE__);
                                    /*
                                            do_stuff(2);
                                            printf("Eseguo nodo...\n");
                                            printf("Node done! PID:%d\n", getpid());
                                            busy_cpu(1);
                                            exit(i);*/
                                    break;

                                default:
                                    noEffectiveNodes++;
                                    noAllTimesNodes++;

                                    sops[0].sem_num = 0;
                                    sops[0].sem_op = -1;
                                    sops[0].sem_flg = IPC_NOWAIT;
                                    if (semop(fairStartSem, &sops[0], 1) == -1)
                                    {
                                        unsafeErrorPrint("User: failed to wait for zero on start semaphore. Error ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    /*Initialize messages queue for transactions pools*/
                                    tpList[i].procId = (long)child_pid;
                                    key = ftok(MSGFILEPATH, child_pid);
                                    if (key == -1)
                                    {
                                        unsafeErrorPrint("Master: failed to initialize process' transaction pool. Error", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    tpList[i].msgQId = msgget(key, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
                                    if (tpList[i].msgQId == -1)
                                    {
                                        unsafeErrorPrint("Master: failed to initialize process' transaction pool. Error", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    tplLength++; /* updating tpList length */

                                    /* Save users processes pid and state into usersList*/
                                    sops[0].sem_op = -1;
                                    sops[0].sem_num = 1;
                                    sops[1].sem_op = -1;
                                    sops[1].sem_num = 2;
                                    if (semop(nodeListSem, sops, 2) == -1)
                                    {
                                        unsafeErrorPrint("Master: failed to reserve nodes list semaphore for writing operation. Error ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    nodesList[i].procId = child_pid;
                                    nodesList[i].procState = ACTIVE;

                                    sops[0].sem_op = 1;
                                    sops[0].sem_num = 1;
                                    sops[1].sem_op = 1;
                                    sops[1].sem_num = 2;
                                    if (semop(nodeListSem, sops, 2) == -1)
                                    {
                                        unsafeErrorPrint("Master: failed to release nodes list semaphore for writing operation. Error ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    break;
                                }
                            }

                            /********************************************/
                            /********************************************/

                            /*
                                CORREGGERE: Mettere più stampe per segnalare cosa stia succedendo
                            */
                            /************** INITIALIZATION OF BUDGETLIST **************/
                            /**********************************************************/

                            /* we enter the critical section for the noUserSegReadersPtr variabile */
                            sops[0].sem_op = -1;
                            sops[0].sem_num = 0;
                            sops[1].sem_op = -1;
                            sops[1].sem_num = 1;
                            if (semop(userListSem, sops, 2) == -1)
                            {
                                safeErrorPrint("Master: failed to reserve usersList semaphore for reading operation. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            (*noUserSegReadersPtr)++;
                            if ((*noUserSegReadersPtr) == 1)
                            {
                                sops[0].sem_num = 2;
                                sops[0].sem_op = -1;
                                if (semop(userListSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("Master: failed to reserve write usersList semaphore. Error", __LINE__);
                                    endOfSimulation(-1);
                                }
                                /*
                                 * se lo scrittore sta scrivendo, allora il primo lettore che entrerà in questo
                                 * ramo si addormenterà su questo semaforo.
                                 * se lo scrittore non sta scrivendo, allora il primo lettore decrementerà di 1 il
                                 * valore semaforico, in modo tale se lo scrittore vuole scrivere, si addormenterà
                                 * sul semaforo
                                 */
                            }
                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                            sops[0].sem_num = 0;
                            sops[0].sem_op = 1;
                            sops[1].sem_num = 1;
                            sops[1].sem_op = 1;
                            if (semop(userListSem, sops, 2) == -1)
                            {
                                safeErrorPrint("Master: failed to release usersList semaphore after reading operation. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            /* initializing budget for users processes */
                            printf("Master: initializing budget users processes...\n");
                            bud_list_head = NULL;
                            bud_list_tail = NULL;
                            for (i = 0; i < SO_USERS_NUM; i++)
                            {
                                new_el = malloc(sizeof(*new_el));
                                new_el->proc_pid = usersList[i].procId;
                                new_el->budget = SO_BUDGET_INIT;
                                new_el->p_type = 0;
                                insert_ordered(new_el); /* insert user on budgetlist */
                            }

                            /* we enter the critical section for the noUserSegReadersPtr variabile */
                            sops[0].sem_num = 0;
                            sops[0].sem_op = -1;
                            if (semop(userListSem, &sops[0], 1) == -1)
                            {
                                safeErrorPrint("Master: failed to reserve mutex usersList semaphore. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            (*noUserSegReadersPtr)--;
                            if ((*noUserSegReadersPtr) == 0)
                            {
                                sops[0].sem_num = 2;
                                sops[0].sem_op = 1;
                                if (semop(userListSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("Master: failed to reserve write usersList semaphore. Error", __LINE__);
                                    endOfSimulation(-1);
                                }
                                /*
                                 * se sono l'ultimo lettore e smetto di leggere, allora devo riportare a 0
                                 * il valore semaforico in modo che se lo scrittore vuole scrivere possa farlo.
                                 */
                            }

                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                            sops[0].sem_num = 0;
                            sops[0].sem_op = 1;
                            if (semop(userListSem, &sops[0], 1) == -1)
                            {
                                safeErrorPrint("Master: failed to release mutex usersList semaphore. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            /**** Initializing budget for nodes processes ****/
                            printf("Master: initializing budget nodes processes...\n");
                            /* we enter the critical section for the noNodeSegReadersPtr variabile */
                            sops[0].sem_num = 0;
                            sops[0].sem_op = -1;
                            sops[1].sem_num = 1;
                            sops[1].sem_op = -1;
                            if (semop(nodeListSem, sops, 2) == -1)
                            {
                                safeErrorPrint("Master: failed to reserve nodeList semaphore for reading operation. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            (*noNodeSegReadersPtr)++;
                            if ((*noNodeSegReadersPtr) == 1)
                            {
                                sops[0].sem_num = 2;
                                sops[0].sem_op = -1;
                                if (semop(nodeListSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("Master: failed to reserve write nodeList semaphore. Error", __LINE__);
                                    endOfSimulation(-1);
                                }
                            }
                            /* we exit the critical section for the noNodeSegReadersPtr variabile */
                            sops[0].sem_num = 0;
                            sops[0].sem_op = 1;
                            sops[1].sem_num = 1;
                            sops[1].sem_op = 1;
                            if (semop(nodeListSem, sops, 2) == -1)
                            {
                                safeErrorPrint("Master: failed to release nodeList semaphore after reading operation. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            /* initializing budget for nodes processes */
                            /*
                                    Ci basta scorrere fino a SO_NODES_NUM
                                    perchè a questo punto non ci sono ancora nodi addizionali
                                */
                            for (i = 0; i < SO_NODES_NUM; i++)
                            {
                                new_el = malloc(sizeof(*new_el));
                                new_el->proc_pid = nodesList[i].procId;
                                new_el->budget = 0;
                                new_el->p_type = 1;
                                insert_ordered(new_el); /* insert node on budgetlist */
                            }

                            /************** END OF INITIALIZATION OF BUDGETLIST **************/
                            /*****************************************************************/

                            /* Devo ancora lavorare su nodesList, faccio dopo la UNLOCK della zona critica */

                            /**** Friends estraction ***/
                            printf("Master: extracting friends for nodes...\n");
                            for (i = 0; i < SO_NODES_NUM; i++)
                            {
                                estrai(i);
                                msg_to_node.mtype = nodesList[i].procId;
                                msg_to_node.msgContent = FRIENDINIT;
                                /*tmpFriend.mtype = nodesList[i].procId;**/
                                for (j = 0; j < SO_FRIENDS_NUM; j++)
                                {
                                    msg_to_node.friend = nodesList[extractedFriendsIndex[j]].procId;
                                    if (msgsnd(globalQueueId, &msg_to_node, sizeof(msg_to_node) - sizeof(long), 0) == -1)
                                    {
                                        unsafeErrorPrint("Master: failed to initialize node friends. Error", __LINE__);
                                        endOfSimulation(-1);
                                    }
                                }
                            }

                            /* we enter the critical section for the noNodeSegReadersPtr variabile */
                            sops[0].sem_num = 0;
                            sops[0].sem_op = -1;
                            if (semop(nodeListSem, &sops[0], 1) == -1)
                            {
                                safeErrorPrint("Master: failed to reserve mutex nodeList semaphore. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            (*noNodeSegReadersPtr)--;
                            if ((*noNodeSegReadersPtr) == 0)
                            {
                                sops[0].sem_num = 2;
                                sops[0].sem_op = 1;
                                if (semop(nodeListSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("Master: failed to reserve write nodeList semaphore. Error", __LINE__);
                                    endOfSimulation(-1);
                                }
                            }
                            /* we exit the critical section for the noNodeSegReadersPtr variabile */
                            sops[0].sem_num = 0;
                            sops[0].sem_op = 1;
                            sops[1].sem_num = 1;
                            sops[1].sem_op = 1;
                            if (semop(nodeListSem, sops, 2) == -1)
                            {
                                safeErrorPrint("Master: failed to release nodeList semaphore after reading operation. Error", __LINE__);
                                endOfSimulation(-1);
                            }

                            /*sops[0].sem_op = -1;
                            semop(fairStartSem, &sops[0], 1);*/
                            printf("Master: about to start simulation...\n");
                            sops[0].sem_op = -1;
                            sops[0].sem_num = 0;
                            sops[0].sem_flg = 0;
                            semop(fairStartSem, &sops[0], 1);

                            /* master lifecycle*/
                            printf("**** Master: starting lifecycle... ****\n");
                            /*sleep(10);*/ /*CORREGGERE*/
                            while (1 && child_pid)
                            {
                                printf("Master: checking if register's partitions are full...\n");
                                fullRegister = TRUE;
                                for (i = 0; i < REG_PARTITION_COUNT && fullRegister; i++)
                                {
                                    /*
                                        printf("Master: number of blocks is %d\n", regPtrs[i]->nBlocks);
                                        printf("Master: Max size %d\n", REG_PARTITION_SIZE);*/
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

                                /*
                                        L'informazione memorizzata nelle righe successive è memorizzata
                                        in noEffectiveUsers e noEffectiveNodes, manutenute dal SO.
                                        Questo, oltre a servire per altri scopi, accorcia il ciclo di vita
                                    */
                                /**** COUNT NUMBER OF ACTIVE NODE AND USER PROCESSES ****/
                                /********************************************************/

                                /**** END OF COUNT NUMBER OF ACTIVE NODE AND USER PROCESSES ****/
                                /***************************************************************/

                                /**** CYCLE THAT UPDATES BUDGETLIST OF PROCESSES BEFORE PRINTING IT ****/
                                /***********************************************************************/

                                /* cycle that updates the budget list before printing it */
                                /* at every cycle we do the count of budgets in blocks of the i-th partition */
                                printf("Master: updating budget list before printing...\n");
                                for (i = 0; i < REG_PARTITION_COUNT; i++)
                                {
                                    /* setting options for getting access to i-th partition of register */

                                    /* NUOVO ACCESSO A SEMAFORO IN LETTURA */
                                    /* we enter the critical section for the noReadersPartitions variabile of i-th partition */
                                    sops[0].sem_num = i;
                                    sops[0].sem_op = -1;
                                    if (semop(rdPartSem, &sops[0], 1) == -1)
                                    {
                                        sprintf(aus, "Master: failed to reserve read semaphore for %d-th partition. Error", i);
                                        unsafeErrorPrint(aus, __LINE__);
                                        /*
                                                Computing the budget is a critical operation, so we end the simulation
                                                in case of error
                                            */
                                        endOfSimulation(-1);
                                    }
                                    else
                                    {
                                        sops[0].sem_num = i;
                                        sops[0].sem_op = -1;
                                        if (semop(mutexPartSem, &(sops[0]), 1) == -1)
                                        {
                                            sprintf(aus, "Master: failed to reserve mutex semaphore for %d-th partition. Error", i);
                                            unsafeErrorPrint(aus, __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        (*noReadersPartitionsPtrs[i])++;
                                        if ((*noReadersPartitionsPtrs[i]) == 1)
                                        {
                                            sops[0].sem_num = i;
                                            sops[0].sem_op = -1;
                                            if (semop(wrPartSem, &sops[0], 1) == -1)
                                            {
                                                sprintf(aus, "Master: failed to reserve write semaphore for %d-th partition. Error", i);
                                                unsafeErrorPrint(aus, __LINE__);
                                                endOfSimulation(-1);
                                            }
                                        }

                                        /* we exit the critical section for the noUserSegReadersPtr variabile */
                                        sops[0].sem_num = i;
                                        sops[0].sem_op = 1;
                                        if (semop(mutexPartSem, &sops[0], 1) == -1)
                                        {
                                            sprintf(aus, "Master: failed to release mutex semaphore for %d-th partition. Error", i);
                                            unsafeErrorPrint(aus, __LINE__);
                                            endOfSimulation(-1);
                                        }
                                        else
                                        {
                                            sops[0].sem_num = i;
                                            sops[0].sem_op = 1;
                                            if (semop(rdPartSem, &sops[0], 1) == -1)
                                            {
                                                sprintf(aus, "Master: failed to release read semaphore for %d-th partition. Error", i);
                                                unsafeErrorPrint(aus, __LINE__);
                                                endOfSimulation(-1);
                                            }
                                            /*
                                                    CORREGGERE: togliere in debug
                                                */
                                            printf("Master: gained access to %d-th partition of register\n", i);

                                            /* inizializzo l'indice al blocco in cui mi ero fermato allo scorso ciclo */
                                            ind_block = prev_read_nblock[i];

                                            /* ciclo di scorrimento dei blocchi della i-esima partizione */
                                            while (ind_block < regPtrs[i]->nBlocks)
                                            {
                                                /* restituisce il blocco di indice ind_block */
                                                block = regPtrs[i]->blockList[ind_block];
                                                ind_tr_in_block = 0;
                                                bud_update_attempts = 0; /* reset attempts */

                                                /* scorro la lista di transizioni del blocco di indice ind_block */
                                                while (ind_tr_in_block < SO_BLOCK_SIZE)
                                                {
                                                    trans = block.transList[ind_tr_in_block]; /* restituisce la transazione di indice ind_tr_in_block */

                                                    ct_updates = 0; /* conta il numero di aggiornamenti di budget fatti per la transazione (totale 2, uno per sender e uno per receiver) */
                                                    if (trans.sender == -1)
                                                    {
                                                        ct_updates++;
                                                        /*
                                                         * se il sender è -1, rappresenta transazione di pagamento reward del nodo,
                                                         * quindi non bisogna aggiornare il budget del sender, ma solo del receiver.
                                                         */
                                                    }

                                                    /* update budget of sender of transaction, the amount is negative */
                                                    /* error checking not needed, already done in function */
                                                    else if (update_budget(trans.sender, -(trans.amountSend)) == 0)
                                                        ct_updates++;

                                                    /* update budget of receiver of transaction, the amount is positive */
                                                    /* error checking not needed, already done in function */
                                                    if (update_budget(trans.receiver, trans.amountSend) == 0)
                                                        ct_updates++;

#if 0
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
#endif
                                                    /* if we have done two updates, we can switch to next block, otherwise we stay on this */
                                                    if (ct_updates == 2)
                                                    {
                                                        ind_tr_in_block++;
                                                    }
                                                    else
                                                    {
                                                        /* we had a problem updating budgets from this block */
                                                        bud_update_attempts++;
                                                        /* if we already tryied NO_ATTEMPTS_UPDATE_BUDGET to update budget from this block, we change block */
                                                        if (bud_update_attempts > NO_ATTEMPTS_UPDATE_BUDGET)
                                                            ind_tr_in_block++;
                                                    }
                                                }

                                                ind_block++;
                                            }

                                            prev_read_nblock[i] = ind_block; /* memorizzo il blocco a cui mi sono fermato */

                                            /* NUOVO ACCESSO A SEMAFORO IN LETTURA */
                                            /* we enter the critical section for the noReadersPartitions variabile of i-th partition */
                                            sops[0].sem_num = i;
                                            sops[0].sem_op = -1;
                                            if (semop(mutexPartSem, &sops[0], 1) == -1)
                                            {
                                                sprintf(aus, "Master: failed to reserve mutex semaphore for %d-th partition. Error", i);
                                                unsafeErrorPrint(aus, __LINE__);
                                                endOfSimulation(-1);
                                            }
                                            else
                                            {
                                                (*noReadersPartitionsPtrs[i])--;
                                                if ((*noReadersPartitionsPtrs[i]) == 0)
                                                {
                                                    sops[0].sem_num = i;
                                                    sops[0].sem_op = 1; /* controllare se giusto!!! */
                                                    if (semop(wrPartSem, &sops[0], 1) == -1)
                                                    {
                                                        sprintf(aus, "Master: failed to reserve write semaphore for %d-th partition. Error", i);
                                                        unsafeErrorPrint(aus, __LINE__);
                                                        endOfSimulation(-1);
                                                    }
                                                }
                                                /* we exit the critical section for the noUserSegReadersPtr variabile */
                                                sops[0].sem_num = i;
                                                sops[0].sem_op = 1;
                                                if (semop(mutexPartSem, &sops[0], 1) == -1)
                                                {
                                                    sprintf(aus, "Master: failed to release read semaphore for %d-th partition. Error", i);
                                                    unsafeErrorPrint(aus, __LINE__);
                                                    endOfSimulation(-1);
                                                }
                                            }
                                        }
                                    }
                                }

                                /**** END OF CYCLE THAT UPDATES BUDGETLIST OF PROCESSES ****/
                                /***********************************************************/

                                /**** PRINT BUDGET OF EVERY PROCESS ****/
                                /***************************************/
                                /*
                                        CORREGGERE: Qui si controllava noEffective, ma il budget
                                        è da stamapre anche per i processi terminati
                                    */
                                if (noAllTimesNodes + noAllTimesUsers <= MAX_PRINT_PROCESSES)
                                {
                                    /*
                                     * the number of effective processes is lower or equal than the maximum we established,
                                     * so we print budget of all processes
                                     */
                                    printf("Master: Printing budget of all the processes.\n");

                                    printf("Master: Number of active nodes: %d\n", noEffectiveNodes);
                                    printf("Master: Number of active users: %d\n", noEffectiveUsers);

                                    for (el_list = bud_list_head; el_list != NULL; el_list = el_list->next)
                                    {
                                        if (el_list->p_type) /* Budget of node process */
                                            printf("Master:  - NODE PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
                                        else /* Budget of user process */
                                            printf("Master:  - USER PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
                                    }
                                }
                                else
                                {
                                    /*
                                     * the number of effective processes is bigger than the maximum we established, so
                                     * we print only the maximum and minimum budget in the list
                                     */

                                    printf("Master: There are too many processes. Printing only minimum and maximum budgets.\n");

                                    /* printing minimum budget in budgetlist - we print all processes' budget that is minimum */
                                    /*
                                            Here we take advantage of the sorted budget list: finding the minimum budget
                                            is just a matter of checking if it's equal tot that on top of the list
                                        */
                                    el_list = bud_list_head;
                                    while (el_list != NULL && el_list->budget == bud_list_head->budget)
                                    {
                                        if (el_list->p_type) /* Budget of node process */
                                            printf("Master:  - NODE PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
                                        else /* Budget of user process */
                                            printf("Master:  - USER PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);

                                        el_list = el_list->next;
                                    }

                                    /* printing maximum budget in budgetlist - we print all processes' budget that is maximum */
                                    el_list = bud_list_tail;
                                    while (el_list != NULL && el_list->budget == bud_list_tail->budget)
                                    {
                                        if (el_list->p_type) /* Budget of node process */
                                            printf("Master:  - NODE PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
                                        else /* Budget of user process */
                                            printf("Master:  - USER PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);

                                        el_list = el_list->prev;
                                    }
                                }

                                /**** END OF PRINT BUDGET OF EVERY PROCESS ****/
                                /**********************************************/

                                printf("Master: checking if there are node creation requests to be served...\n");
                                checkNodeCreationRequests();

                                /**** USER TERMINATION CHECK ****/
                                /********************************/

                                /* Check if a user process has terminated to update the usersList */
                                /*noUserTerminated = 0;*/ /* resetting user terminated counter */

                                while (msgrcv(globalQueueId, &msg_from_user, sizeof(MsgGlobalQueue) - sizeof(long), masterPid, IPC_NOWAIT) != -1)
                                {
                                    /* come dimensione specifichiamo sizeof(msg_from_user)-sizeof(long) perché bisogna specificare la dimensione del testo, non dell'intera struttura */
                                    /* come mtype prendiamo i messaggi destinati al Master, cioè il suo pid (prende il primo messaggio con quel mtype) */

                                    /* in questo caso cerchiamo i messaggi con msgContent TERMINATEDUSER */
                                    if (msg_from_user.msgContent == TERMINATEDUSER)
                                    {
                                        /* we enter the critical section for the usersList */
                                        sops[0].sem_num = 1;
                                        sops[0].sem_op = -1;
                                        sops[1].sem_num = 2;
                                        sops[1].sem_op = -1;
                                        if (semop(userListSem, sops, 2) == -1)
                                        {
                                            safeErrorPrint("Master: failed to reserve usersList semaphore for writing operation. Error", __LINE__);
                                            endOfSimulation(-1);
                                        }
                                        else
                                        {
                                            /* cycle to search for the user process */
                                            for (i = 0; i < SO_USERS_NUM; i++)
                                            {
                                                if (usersList[i].procId == msg_from_user.terminatedPid)
                                                {
                                                    /* we found the user process terminated */
                                                    usersList[i].procState = TERMINATED;
                                                    /* Updating number of terminated processes */
                                                    noTerminatedUsers++;
                                                    /* Updating number of effective active processes */
                                                    noEffectiveUsers--;
                                                    break;
                                                    /* we stop the cycle now that we found the process */
                                                }
                                            }

                                            /* we exit the critical section for the usersList */
                                            sops[0].sem_num = 2;
                                            sops[0].sem_op = 1;
                                            sops[1].sem_num = 1;
                                            sops[1].sem_op = 1;
                                            if (semop(userListSem, sops, 2) == -1)
                                            {
                                                safeErrorPrint("Master: failed to release usersList semaphore for writing operation. Error", __LINE__);
                                                /*
                                                        CORREGGERE: terminiamo la simulazione??
                                                        È la soluzione più sensata, perchè il rischio è quello di bloccare tutti gli altri processi
                                                        in attesa su questa lista
                                                    */
                                                endOfSimulation(-1);
                                            }
                                            else
                                            {
                                                printf("Master: the user process with pid %5d has terminated\n", msg_from_user.terminatedPid);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        /* Reinserting the message that we have consumed from the global queue */
                                        if (msgsnd(globalQueueId, &msg_from_user, sizeof(MsgGlobalQueue) - sizeof(long), 0) == -1)
                                        {
                                            /* This is necessary, otherwise the message won't be reinserted in queue and lost forever */
                                            unsafeErrorPrint("Master: failed to reinsert the message read from the global queue while checking for terminated users. Error", __LINE__);
                                            endOfSimulation(-1);
                                        }
                                    }
                                }

                                /* If errno is ENOMSG, no message of user termination on global queue, otherwise an error occured */
                                if (errno != ENOMSG)
                                {
                                    unsafeErrorPrint("Master: failed to retrieve user termination messages from global queue. Error", __LINE__);
                                    /*
                                     * DEVO FARE EXIT?????
                                     * Dipende, perché se è un errore momentaneo che al prossimo ciclo non riaccade, allora non
                                     * è necessario fare la exit, ma se si verifica un errore a tutti i cicli non è possibile
                                     * leggere messaggi dalla coda, quindi si finisce con il non creare un nuovo nodo, non processare
                                     * alcune transazioni e si può riempire la coda globale, rischiando di mandare in wait tutti i
                                     * restanti processi nodi e utenti. Quindi sarebbe opportuno fare exit appena si verifica un errore
                                     * oppure utilizzare un contatore (occorre stabilire una soglia di ripetizione dell'errore). Per
                                     * ora lo lasciamo.
                                     */
                                    endOfSimulation(-1);
                                }

                                /**** END OF USER TERMINATION CHECK ****/
                                /***************************************/

                                /**** NODE TERMINATION CHECK ****/
                                /********************************/

                                /* Check if a node process has terminated to update the nodes list */
                                while (msgrcv(globalQueueId, &msg_from_node, sizeof(MsgGlobalQueue) - sizeof(long), masterPid, IPC_NOWAIT) != -1)
                                {
                                    printf("***Master: checking if there are terminated nodes...\n");
                                    if (msg_from_node.msgContent == TERMINATEDNODE)
                                    {
                                        sops[0].sem_num = 1;
                                        sops[0].sem_op = -1;
                                        sops[1].sem_num = 2;
                                        sops[1].sem_op = -1;
                                        if (semop(nodeListSem, sops, 2) == -1)
                                        {
                                            safeErrorPrint("Master: failed to reserve nodesList semaphore for writing operation. Error", __LINE__);
                                            endOfSimulation(-1);
                                        }
                                        else
                                        {
                                            for (i = 0; i < SO_NODES_NUM; i++)
                                            {
                                                if (nodesList[i].procId == msg_from_node.terminatedPid)
                                                {
                                                    nodesList[i].procState = TERMINATED;
                                                    noTerminatedNodes++;
                                                    noEffectiveNodes--;
                                                    break;
                                                }
                                            }

                                            /* we exit the critical section for the usersList */
                                            sops[0].sem_num = 2;
                                            sops[0].sem_op = 1;
                                            sops[1].sem_num = 1;
                                            sops[1].sem_op = 1;
                                            if (semop(nodeListSem, sops, 2) == -1)
                                            {
                                                safeErrorPrint("Master: failed to release nodeslist semaphore for writing operation. Error", __LINE__);
                                                /*
                                                        CORREGGERE: terminiamo la simulazione??
                                                        È la soluzione più sensata, perchè il rischio è quello di bloccare tutti gli altri processi
                                                        in attesa su questa lista
                                                    */
                                                endOfSimulation(-1);
                                            }
                                            else
                                            {
                                                printf("Master: the node process with pid %5d has terminated\n", msg_from_node.terminatedPid);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        /* Reinserting the message that we have consumed from the global queue */
                                        if (msgsnd(globalQueueId, &msg_from_node, sizeof(MsgGlobalQueue) - sizeof(long), 0) == -1)
                                        {
                                            /* This is necessary, otherwise the message won't be reinserted in queue and lost forever */
                                            unsafeErrorPrint("Master: failed to reinsert the message read from the global queue while checking for terminated nodes. Error", __LINE__);
                                            endOfSimulation(-1);
                                        }
                                    }
                                }

                                /* If errno is ENOMSG, no message of user termination on global queue, otherwise an error occured */
                                if (errno != ENOMSG)
                                {
                                    unsafeErrorPrint("Master: failed to retrieve node termination messages from global queue. Error", __LINE__);
                                    /*
                                     * DEVO FARE EXIT?????
                                     * Dipende, perché se è un errore momentaneo che al prossimo ciclo non riaccade, allora non
                                     * è necessario fare la exit, ma se si verifica un errore a tutti i cicli non è possibile
                                     * leggere messaggi dalla coda, quindi si finisce con il non creare un nuovo nodo, non processare
                                     * alcune transazioni e si può riempire la coda globale, rischiando di mandare in wait tutti i
                                     * restanti processi nodi e utenti. Quindi sarebbe opportuno fare exit appena si verifica un errore
                                     * oppure utilizzare un contatore (occorre stabilire una soglia di ripetizione dell'errore). Per
                                     * ora lo lasciamo.
                                     */
                                    endOfSimulation(-1);
                                }

                                /**** END OF NODE TERMINATION CHECK ****/
                                /***************************************/

                                printf("--------------- END OF CYCLE ---------------\n"); /* for debug purpose */

                                /* now sleep for 1 second */
                                nanosleep(&onesec, &tim);

                                printf("**** Master: starting a new lifecycle ****\n");
                            }
                        }
                    }
                    deallocateFacilities(&exitCode);
                }
            }
        }
    }

    /* POSTCONDIZIONE: all'esecuzione di questa system call
        l'handler di fine simulazione è già stato eseguito*/
    exit(exitCode);
}

/***** Function that assigns the values ​​of the environment *****/
/***** variables to the global variables defined above     *****/
/***************************************************************/
boolean assignEnvironmentVariables()
{
    /*
        We use strtol because it can detect error (due to overflow)
        while atol can't
    */

    printf("Master: loading environment...\n");
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

/***** Function that reads the file containing the configuration    *****/
/***** parameters to save them as environment variables             *****/
/************************************************************************/
boolean readConfigParameters()
{
    char *filename = "params.txt";
    FILE *fp = fopen(filename, "r");
    /* Reading line by line, max 128 bytes*/
    /*
        Array that will contain the lines read from the file:
        each "row" of the "matrix" will contain a different file line
    */
    /* Counter of the number of lines in the file*/
    int k = 0;
    char *aus = NULL;
    int i = 0;
    boolean ret = TRUE;

    printf("Master: reading configuration parameters...\n");

    aus = (char *)calloc(35, sizeof(char));
    if (aus == NULL)
        unsafeErrorPrint("Master: failed to allocate memory. Error", __LINE__);
    else
    {
        /* Handles any error in opening the file*/
        if (fp == NULL)
        {
            sprintf(aus, "Master: could not open file %s", filename);
            unsafeErrorPrint(aus, __LINE__);
            ret = FALSE;
        }
        else
        {
            /* Inserts the lines read from the file into the array*/
            /* It also inserts the parameters read into environment variables*/
            while (fgets(line[k], CONF_MAX_LINE_SIZE, fp) != NULL)
            {
                putenv(line[k]);
                k++;
            }

            if (line[k] == NULL && errno)
            {
                unsafeErrorPrint("Master: failed to read cofiguration parameters. Error", __LINE__);
                ret = FALSE;
            }
            else
            {
                ret = assignEnvironmentVariables();
            }

            /* Close the file*/
            fclose(fp);
        }

        /* frees the auxiliary char array */
        free(aus);
    }

    return ret;
}

/************************************************************************/
/************************************************************************/

/****   Allocation of global structures    *****/
/***********************************************/
boolean allocateGlobalStructures()
{
    regPtrs = (Register **)calloc(REG_PARTITION_COUNT, sizeof(Register *));
    TEST_MALLOC_ERROR(regPtrs, "Master: failed to allocate register paritions' pointers array. Error");

    regPartsIds = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(regPartsIds, "Master: failed to allocate register paritions' ids array. Error");

    tpList = (TPElement *)calloc(SO_NODES_NUM, sizeof(TPElement));
    TEST_MALLOC_ERROR(tpList, "Master: failed to allocate transaction pools list. Error");

    noReadersPartitions = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(noReadersPartitions, "Master: failed to allocate registers partitions' shared variables ids. Error");

    noReadersPartitionsPtrs = (int **)calloc(REG_PARTITION_COUNT, sizeof(int *));
    TEST_MALLOC_ERROR(noReadersPartitionsPtrs, "Master: failed to allocate registers partitions' shared variables pointers. Error");

    return TRUE;
}
/****************************************************************************/
/****************************************************************************/

/*****  Ipc structures allocation *****/
/****************************************************************************/
boolean initializeIPCFacilities()
{
    /*
        Sostituire la macro attuale con un meccanismo che deallochi
        le risorse IPC in caso di errore
    */
    union semun arg;
    unsigned short aux[REG_PARTITION_COUNT] = {1, 1, 1};
    int res = -1;
    /* Initialization of semaphores*/
    key_t key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during fair start semaphore creation. Error");

    fairStartSem = semget(key, 1, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(fairStartSem, "Master: semget failed during fair start semaphore creation. Error");

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during partitions writing semaphores creation. Error");
    wrPartSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(wrPartSem, "Master: semget failed during partitions writing semaphores creation. Error");

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during partitions reading semaphores creation. Error");
    rdPartSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(rdPartSem, "Master: semget failed during partitions reading semaphores creation. Error");

    key = ftok(SEMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during user list semaphore creation. Error");
    userListSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(userListSem, "Master: semget failed during user list semaphore creation. Error");

    key = ftok(SEMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during nodes list semaphore creation. Error");
    nodeListSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(nodeListSem, "Master: semget failed during nodes list semaphore creation. Error");

    key = ftok(SEMFILEPATH, PARTMUTEXSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during partitions mutex semaphores creation. Error");
    mutexPartSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(mutexPartSem, "Master: semget failed during partitions mutex semaphores creation. Error");

    /*
        Each process will subtract one by waiting on the sempahore
    */
    arg.val = SO_USERS_NUM + SO_NODES_NUM + 1;
    semctl(fairStartSem, 0, SETVAL, arg);
    SEMCTL_TEST_ERROR(fairStartSem, "Master: semctl failed while initializing fair start semaphore. Error");

    arg.array = aux;
    res = semctl(wrPartSem, 0, SETALL, arg);
    SEMCTL_TEST_ERROR(res, "Master: semctl failed while initializing register partitions writing semaphores. Error");

    /*
    aux[0] = SO_USERS_NUM + SO_NODES_NUM + 1;
    aux[1] = SO_USERS_NUM + SO_NODES_NUM + 1;
    aux[2] = SO_USERS_NUM + SO_NODES_NUM + 1;
    arg.array = aux;*/
    res = semctl(rdPartSem, 0, SETALL, arg);
    SEMCTL_TEST_ERROR(res, "Master: semctl failed while initializing register partitions reading semaphores. Error");

    res = semctl(mutexPartSem, 0, SETALL, arg);
    SEMCTL_TEST_ERROR(res, "Master: semctl failed while initializing register partitions mutex semaphores. Error");

    /*CORREGGERE mettendolo nel master, prima della sleep su fairStart*/
    arg.array = aux;
    res = semctl(userListSem, 0, SETALL, arg); /* mutex, read, write*/
    SEMCTL_TEST_ERROR(res, "Master: semctl failed while initializing users list semaphore. Error");

    /*CORREGGERE*/
    /*arg.val = 1;*/
    res = semctl(nodeListSem, 0, SETALL, arg); /* mutex, read, write*/
    SEMCTL_TEST_ERROR(res, "Master: semctl failed while initializing nodes list semaphore. Error");

    /* Creation of the global queue*/
    key = ftok(MSGFILEPATH, GLOBALMSGSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during global queue creation. Error");
    globalQueueId = msgget(key, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    MSG_TEST_ERROR(globalQueueId, "Master: msgget failed during global queue creation. Error");

    /* Creation of register's partitions */
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during register parition one creation. Error");
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(regPartsIds[0], "Master: shmget failed during partition one creation. Error");

    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during register parition two creation. Error");
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(regPartsIds[1], "Master: shmget failed during partition two creation. Error");

    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during register parition three creation. Error");
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(regPartsIds[2], "Master: shmget failed during partition three creation. Error");

    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(regPtrs[0], "Master: failed to attach to partition one's memory segment. Error");
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(regPtrs[1], "Master: failed to attach to partition two's memory segment. Error");
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(regPtrs[2], "Master: failed to attach to partition three's memory segment. Error");
    printf("Master: initializing blocks...\n");
    regPtrs[0]->nBlocks = 0;
    regPtrs[1]->nBlocks = 0;
    regPtrs[2]->nBlocks = 0;
    printf("Blocks: %d %d %d\n", regPtrs[0]->nBlocks, regPtrs[1]->nBlocks, regPtrs[2]->nBlocks);

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during users list creation. Error");
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(usersListId, "Master: shmget failed during users list creation. Error");
    usersList = (ProcListElem *)shmat(usersListId, NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(usersList, "Master: failed to attach to users list's memory segment. Error");

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during nodes list creation. Error");
    nodesListId = shmget(key, maxNumNode * sizeof(ProcListElem), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(nodesListId, "Master: shmget failed during nodes list creation. Error");
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(nodesList, "Master: failed to attach to nodes list's memory segment. Error");

    key = ftok(SHMFILEPATH, NOREADERSONESEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during parition one's shared variable creation. Error");
    noReadersPartitions[0] = shmget(key, sizeof(SO_USERS_NUM), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noReadersPartitions[0], "Master: shmget failed during parition one's shared variable creation. Error");
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[0], "Master: failed to attach to parition one's shared variable segment. Error");
    /*
        At the beginning we have no processes reading from the register's paritions
    */
    *(noReadersPartitionsPtrs[0]) = 0;

    key = ftok(SHMFILEPATH, NOREADERSTWOSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during parition two's shared variable creation. Error");
    noReadersPartitions[1] = shmget(key, sizeof(SO_USERS_NUM), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noReadersPartitions[1], "Master: shmget failed during parition two's shared variable creation. Error");
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[1], "Master: failed to attach to parition rwo's shared variable segment. Error");
    *(noReadersPartitionsPtrs[1]) = 0;

    key = ftok(SHMFILEPATH, NOREADERSTHREESEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during parition three's shared variable creation. Error");
    noReadersPartitions[2] = shmget(key, sizeof(SO_USERS_NUM), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noReadersPartitions[2], "Master: shmget failed during parition three's shared variable creation. Error");
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[2], "Master: failed to attach to parition three's shared variable segment. Error");
    *(noReadersPartitionsPtrs[2]) = 0;

    key = ftok(SHMFILEPATH, NOUSRSEGRDERSSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during users list's shared variable creation. Error");
    noUserSegReaders = shmget(key, sizeof(SO_USERS_NUM), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(key, "Master: shmget failed during users list's shared variable creation. Error");
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noUserSegReadersPtr, "Master: failed to attach to users list's shared variable segment. Error");
    /*
        At the beginning of the simulation there's no one
        reading from the user's list
    */
    *noUserSegReadersPtr = 0;

    /* AGGIUNTO DA STEFANO */
    key = ftok(SHMFILEPATH, NONODESEGRDERSSEED);
    FTOK_TEST_ERROR(key, "Master: ftok failed during nodes list's shared variable creation. Error");
    noNodeSegReaders = shmget(key, sizeof(SO_NODES_NUM), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noNodeSegReaders, "Master: shmget failed during nodes list's shared variable creation. Error");
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noNodeSegReadersPtr, "Master: failed to attach to nodes list's shared variable segment. Error");
    *noNodeSegReadersPtr = 0;
    /* END */

    return TRUE;
}
/****************************************************************************/
/****************************************************************************/

/* Function to free the space dedicated to the budget list p passed as argument */
void budgetlist_free(budgetlist p)
{
    if (p == NULL)
        return;

    budgetlist_free(p->next);
    free(p);
}

/*
 * Function that inserts in the global list bud_list the node passed as
 * argument in an ordered way (the list is ordered in ascending order).
 */
void insert_ordered(budgetlist new_el)
{
    budgetlist el;
    budgetlist prev;

    /* insertion on empty list */
    if (bud_list_head == NULL)
    {
        new_el->prev = NULL;
        new_el->next = NULL;
        bud_list_head = new_el;
        bud_list_tail = new_el;
        return;
    }

    /* insertion on head of list */
    if (new_el->budget <= bud_list_head->budget)
    {
        new_el->prev = NULL;
        new_el->next = bud_list_head;
        bud_list_head->prev = new_el;
        bud_list_head = new_el;
        return;
    }

    /* insertion on tail of list */
    if (new_el->budget >= bud_list_tail->budget)
    {
        new_el->next = NULL;
        new_el->prev = bud_list_tail;
        bud_list_tail->next = new_el;
        bud_list_tail = new_el;
        return;
    }

    /* insertion in middle of list */
    prev = bud_list_head;

    for (el = bud_list_head->next; el != NULL; el = el->next)
    {
        if (new_el->budget <= el->budget)
        {
            new_el->next = el;
            el->prev = new_el;

            prev->next = new_el;
            new_el->prev = prev;
            return;
        }
        prev = el;
    }
}

/*
 * Function that searches in the gloabl list bud_list for an element with
 * proc_pid as the one passed as first argument; if it's found, upgrades its budget
 * adding the second argument, which is a positive or negative amount.
 */
int update_budget(pid_t remove_pid, int amount_changing)
{
    /*
        Dobbiamo rimuovere il nodo e poi reinserirlo
        per mantenere la lista dei budget ordinata
    */
    budgetlist new_el;
    budgetlist el;
    budgetlist prev;
    int found = 0;
    char *msg = NULL;

    /* check if budgetlist is NULL, if yes its an error */
    if (bud_list_head == NULL)
    {
        safeErrorPrint("Master: Error in function update_budget: NULL list passed to the function.", __LINE__);
        return -1;
    }

    if (bud_list_head->proc_pid == remove_pid)
    {
        /* se il nodo di cui aggiornare il budget è il primo della lista,
        allora lo togliamo dalla lista (dopo lo reinseriremo) */
        new_el = bud_list_head;
        bud_list_head = bud_list_head->next;
        bud_list_head->prev = NULL;
        found = 1;
    }
    else if (bud_list_tail->proc_pid == remove_pid)
    {
        /* se il nodo di cui aggiornare il budget è l'ultimo della lista,
        allora lo togliamo dalla lista (dopo lo reinseriremo) */
        new_el = bud_list_tail;
        bud_list_tail = bud_list_tail->prev;
        bud_list_tail->next = NULL;
        found = 1;
    }
    else
    {
        /* il nodo di cui aggiornare il budget si trova a metà della lista,
        dobbiamo cercarlo e rimuoverlo dalla lista (dopo lo reinseriremo) */
        prev = bud_list_head;

        for (el = bud_list_head->next; el != NULL && !found; el = el->next)
        {
            if (el->proc_pid == remove_pid)
            {
                /* ho trovato il nodo da aggiornare, devo rimuoverlo */
                /* devo modificare sia il riferimento al next che al prev */
                prev->next = el->next;
                (el->next)->prev = prev;
                new_el = el;
                found = 1;
            }
            prev = el;
        }
    }

    if (found == 0)
    {
        sprintf(msg, "Master: Trying to update budget but no element in budgetlist with pid %5d\n", remove_pid);
        safeErrorPrint(msg, __LINE__);
        return -1;
    }

    /* update budget of removed element */
    new_el->budget += amount_changing; /* amount_changing is a positive or negative value */

    insert_ordered(new_el);
    return 0;
}

void tmpHandler(int sig)
{
    printf("ONE LAST KEKW OF PID %ld\n", (long)getpid());
    fflush(stdout);
    exit(EXIT_SUCCESS);
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
    /*printf("Master: PID %ld\nMaster: parent PID %ld\n", (long int)getpid(), (long)getppid());*/
    if (terminationMessage == NULL || aus == NULL || getpid() != masterPid)
        safeErrorPrint("Master: failed to alloacate memory. Error", __LINE__);
    else
    {
        /*
            CORREGGERE: reimpostare maschera ed associazione segnaliip
        */
        signal(SIGUSR1, SIG_IGN);
        printf("Master: segnale ricevuto %d\n", sig);
        printf("Master: pid %ld", (long)getpid());
        write(STDOUT_FILENO,
              "Master: trying to terminate simulation...\n",
              strlen("Master: trying to terminate simulation...\n"));
        /* error check*/
        fflush(stdout);
        if (noTerminatedUsers + noTerminatedNodes < noEffectiveNodes + noEffectiveUsers)
        {
        	/*
        		Caso in cui non tutti i processi sono terminati prima della fine della simulazione
        	*/
            /*
                There are still active children that need
                to be notified the end of simulation
            */
            for (i = 0; i < NO_ATTEMPS_TERM && !done; i++)
            {
                /*
                    Correggere; verificare se ci siano problemi
                    derivanti dall'invio ad un processo zombie
                */
                if (kill(0, SIGUSR1) == -1)
                {
                    safeErrorPrint("Master: failed to signal children for end of simulation. Error", __LINE__);
                }
                else
                {
                    write(STDOUT_FILENO,
                          "Master: end of simulation notified successfully to children.\n",
                          strlen("Master: end of simulation notified successfully to children.\n"));
                    done = TRUE;
                }
            }
        }
        else
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
        if (done)
        {
            write(STDOUT_FILENO,
                  "Master: waiting for children to terminate...\n",
                  strlen("Master: waiting for children to terminate...\n"));
            /*
                Conviene fare comunque la wait anche se tutti sono già terminati
                in modo che non ci siano zombies
            */
            while (wait(NULL) != -1)
                ;
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
                /*printf("Processes terminated before end of simulation: %d\n", noTerminatedUsers);*/
                ret = sprintf(terminationMessage, "Processes terminated before end of simulation: %ld\n", noTerminatedUsers);

                /* Blocks in register*/
                ret = sprintf(aus, "There are %d blocks in the register.\n",
                              regPtrs[0]->nBlocks + regPtrs[1]->nBlocks + regPtrs[2]->nBlocks);
                strcat(terminationMessage, aus);

                if (sig == SIGALRM)
                    strcat(terminationMessage, "Termination reason: end of simulation.\n");
                else if (sig == SIGUSR1)
                    strcat(terminationMessage, "Termination reason: register is full.\n");
                else if (sig == -1)
                    strcat(terminationMessage, "Termination reason: critical error.\n");

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
                safeErrorPrint("Master: an error occurred while waiting for children. Description: ", __LINE__);
            }
            write(STDOUT_FILENO, "Master: simulation terminated successfully!!!\n", strlen("Master: simulation terminated successfully!!!\n"));
        }
        else
        {
            deallocateFacilities(&exitCode);
            exitCode = EXIT_FAILURE;
            write(STDOUT_FILENO,
                  "Master: failed to terminate children. IPC facilties will be deallocated anyway.\n",
                  strlen("Master: failed to terminate children. IPC facilties will be deallocated anyway.\n"));
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

boolean deallocateFacilities(int *exitCode)
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
    /*TPElement *tmp;*/

    /* Deallocating register's partitions*/
    write(STDOUT_FILENO,
          "Master: deallocating register's paritions...\n",
          strlen("Master: deallocating register's paritions...\n"));
    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        /*
            We don't want to display any error if the shared memory
            segment we are trying to detach doesn't exist:
            since no one can remove it, this case can only happen
            when the IPC allocation procedure fails
        */
        if (shmdt(regPtrs[i]) == -1 && errno != EINVAL)
        {
            if (errno != EINVAL)
            {
                msgLength = sprintf(aus,
                                    "Master: failed to detach from register's partition number %d.\n",
                                    (i + 1));
                write(STDERR_FILENO, aus, msgLength); /* by doing this we avoid calling strlength*/
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            if (shmctl(regPartsIds[i], IPC_RMID, NULL) == -1 && errno != EINVAL)
            {
                if (errno != EINVAL)
                {
                    msgLength = sprintf(aus,
                                        "Master: failed to remove register's partition number %d.",
                                        (i + 1));
                    write(STDERR_FILENO, aus, msgLength);

                    *exitCode = EXIT_FAILURE;
                }
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

    if (regPtrs != NULL)
        free(regPtrs);

    if (regPartsIds != NULL)
        free(regPartsIds);

    /* Users list deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating users' list segment...\n",
          strlen("Master: deallocating users' list segment...\n"));
    if (shmdt(usersList) == -1)
    {
        if (errno != EINVAL)
        {
            safeErrorPrint("Master: failed to detach from users' list segment. Error", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(usersListId, IPC_RMID, NULL) == -1)
        {
            if (errno != EAGAIN)
            {
                safeErrorPrint("Master: failed to remove users' list segment. Error", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: users' list memory segment successfully removed.\n",
                  strlen("Master: users' list memory segment successfully removed.\n"));
        }
    }

    /* Nodes list deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating nodes' list segment...\n",
          strlen("Master: deallocating nodes' list segment...\n"));
    if (shmdt(nodesList) == -1)
    {
        if (errno != EINVAL)
        {
            safeErrorPrint("Master: failed to detach from nodes' list segment. Error", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(nodesListId, IPC_RMID, NULL) == -1)
        {
            if (errno != EINVAL)
            {
                safeErrorPrint("Master: failed to remove nodes' list segment. Error", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: nodes' list memory segment successfully removed.\n",
                  strlen("Master: nodes' list memory segment successfully removed.\n"));
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
            if (errno != EINVAL)
            {
                msgLength = sprintf(aus,
                                    "Master: failed to detach from partition number %d shared variable segment.\n",
                                    (i + 1));

                write(STDERR_FILENO, aus, msgLength); /* by doing this we avoid calling strlength*/
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            if (shmctl(noReadersPartitions[i], IPC_RMID, NULL) == -1)
            {
                if (errno != EINVAL)
                {
                    msgLength = sprintf(aus,
                                        "Master: failed to remove partition number %d shared variable segment.",
                                        (i + 1));
                    write(STDERR_FILENO, aus, msgLength);

                    *exitCode = EXIT_FAILURE;
                }
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

    if (noReadersPartitions)
        free(noReadersPartitions);

    /* Transaction pools list deallocation*/
    write(STDOUT_FILENO,
          "Master: deallocating transaction pools...\n",
          strlen("Master: deallocating transaction pools...\n"));
    for (i = 0; i < tplLength; i++)
    {
        /*printf("Id coda: %d\n", tpList[i].msgQId);
        printf("Id Processo: %ld\n", tpList[i].procId);
        fflush(stdout);*/
        if (msgctl(tpList[i].msgQId, IPC_RMID, NULL) == -1)
        {
            if (errno != EINVAL)
            {
                msgLength = sprintf(aus,
                                    "Master: failed to remove transaction pool of process %ld",
                                    (long)tpList[i].procId);
                write(STDERR_FILENO, aus, msgLength);

                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            msgLength = sprintf(aus,
                                "Master: transaction pool of node of PID %ld successfully removed.\n",
                                tpList[i].procId);
            write(STDOUT_FILENO, aus, msgLength);
        }
    }

    if (tpList)
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
        if (errno != EINVAL)
        {
            msgLength = sprintf(aus, "Master: failed to remove global message queue");
            write(STDERR_FILENO, aus, msgLength);
            *exitCode = EXIT_FAILURE;
        }
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
        if (errno != EINVAL)
        {
            msgLength = sprintf(aus, "Master: failed to remove partions' writing semaphores");
            write(STDERR_FILENO, aus, msgLength);
            *exitCode = EXIT_FAILURE;
        }
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
        if (errno != EINVAL)
        {
            msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
            write(STDERR_FILENO, aus, msgLength);
            *exitCode = EXIT_FAILURE;
        }
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
        if (errno != EINVAL)
        {
            msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
            write(STDERR_FILENO, aus, msgLength);
            *exitCode = EXIT_FAILURE;
        }
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
        if (errno != EINVAL)
        {
            sprintf(aus, "Master: failed to remove users' list semaphores\n");
            write(STDERR_FILENO, aus, msgLength);
            *exitCode = EXIT_FAILURE;
        }
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
        if (errno != EINVAL)
        {
            sprintf(aus, "Master: failed to remove register's paritions mutex semaphores\n");
            write(STDERR_FILENO, aus, msgLength);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: register's paritions mutex semaphores successfully removed.\n",
              strlen("Master: register's paritions mutex semaphores successfully removed.\n"));
    }

    write(STDOUT_FILENO,
          "Master: deallocating user list's shared variable...\n",
          strlen("Master: deallocating user list's shared variable...\n"));
    if (shmdt(noUserSegReadersPtr) == -1)
    {
        if (errno != EINVAL)
        {
            safeErrorPrint("Master: failed to detach from user list's shared variable. Error", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(noUserSegReaders, IPC_RMID, NULL) == -1)
        {
            if (errno != EINVAL)
            {
                safeErrorPrint("Master: failed to remove user list's shared variable. Error", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
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
        if (errno != EINVAL)
        {
            sprintf(aus, "Master: failed to remove nodes list's semaphores");
            write(STDERR_FILENO, aus, msgLength);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        write(STDOUT_FILENO,
              "Master: nodes list's semaphores successfully removed.\n",
              strlen("Master: nodes list's semaphores successfully removed.\n"));
    }

    write(STDOUT_FILENO,
          "Master: deallocating node list's shared variable...\n",
          strlen("Master: deallocating node list's shared variable...\n"));
    if (shmdt(noNodeSegReadersPtr) == -1)
    {
        if (errno != EINVAL)
        {
            safeErrorPrint("Master: failed to detach from node list's shared variable. Error", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(noNodeSegReaders, IPC_RMID, NULL) == -1)
        {
            if (errno != EINVAL)
            {
                safeErrorPrint("Master: failed to remove node list's shared variable. Error", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            write(STDOUT_FILENO,
                  "Master: node list's shared variable successfully removed.\n",
                  strlen("Master: node list's shared variable successfully removed.\n"));
        }
    }

    /* Releasing local variables' memory*/
    free(aus);

    return TRUE;
}

void checkNodeCreationRequests()
{
    /*
        CORREGGERE: vogliamo fare un ciclo??
        Anche in questo caso secondo me è meglio di no
        perchè così è possibile ripartire le chiamante in
        modo uniforme tra le varie iterazioni del ciclo
        di vita del master
    */
    MsgGlobalQueue aus;
    pid_t procPid = -1;
    int tpId = -1;
    pid_t currPid = getpid();
    MsgTP firstTrans;
    int tmp = -1;
    int j = 0;
    ProcListElem newNode;
    budgetlist new_el;
    struct sembuf sops[3];
    long childPid = -1;

    if (msgrcv(globalQueueId, &aus, sizeof(MsgGlobalQueue) - sizeof(long), currPid, IPC_NOWAIT) == -1)
    {
        /*
            Su questa coda l'unico tipo di messaggio per il master è NEWNODE
        */
        if (aus.msgContent == NEWNODE)
        {

            if (noAllTimesNodes + 1 < maxNumNode)
            {
                noEffectiveNodes++;
                noAllTimesNodes++;
                procPid = fork();
                if (procPid == 0)
                {
                    /*
                        CORREGERE: Queste operazioni andrebber ofatte nel master??
                        Il punto è che si manipolano dati normalmente scrivibili solo dal master
                    */
                    /*
                    1) Creazione tp: Ok
                    2) aggiunta messaggio: Ok
                    3) Assegnazione amici: Ok
                    4) Eseguire codice nodo (opportunamente modificato): Ok
                    */
                    childPid = getpid();
                    newNode.procId = childPid;
                    newNode.procState = ACTIVE;

                    sops[0].sem_flg = 0;
                    sops[0].sem_num = 1;
                    sops[0].sem_op = -1;
                    sops[1].sem_flg = 0;
                    sops[1].sem_num = 2;
                    sops[1].sem_op = -1;
                    if (semop(nodeListSem, sops, 2) == -1)
                    {
                        unsafeErrorPrint("Master: failed to reserve nodes' list semphore. Error", __LINE__);
                        endOfSimulation(-1);
                    }

                    nodesList[noAllTimesNodes - 1] = newNode;

                    sops[0].sem_flg = 0;
                    sops[0].sem_num = 2;
                    sops[0].sem_op = -1;
                    sops[1].sem_flg = 0;
                    sops[1].sem_num = 1;
                    sops[1].sem_op = -1;

                    if (semop(nodeListSem, sops, 2) == -1)
                    {
                        unsafeErrorPrint("Master: failed to release nodes' list semphore. Error", __LINE__);
                        endOfSimulation(-1);
                    }

                    /* Adding new node to budgetlist */
                    new_el = malloc(sizeof(*new_el));
                    new_el->proc_pid = childPid;
                    new_el->budget = 0;
                    new_el->p_type = 1;
                    insert_ordered(new_el);

                    tpId = msgget(ftok("msgfile.txt", currPid), IPC_EXCL | IPC_CREAT);
                    if (tpId == -1)
                        safeErrorPrint("Master: failed to create additional node's transaction pool. Error", __LINE__);
                    else
                    {
                        /* add a new entry to the tpList array */
                        tplLength++;
                        tpList = (TPElement *)realloc(tpList, sizeof(TPElement) * tplLength);
                        /* Initialize messages queue for transactions pools */
                        tpList[tplLength - 1].procId = childPid;
                        tpList[tplLength - 1].msgQId = tpId;

                        if (tpList[tplLength - 1].msgQId == -1)
                        {
                            unsafeErrorPrint("Master: failed to create the message queue for the transaction pool of the new node process. Error", __LINE__);
                            exit(EXIT_FAILURE); /* VA SOSTITUITO CON EndOfSimulation ??? */
                        }

                        /*
                            CORREGGERE:
                            MsgTP è inutile
                            Toglierlo
                        */
                        firstTrans.mtype = childPid;
                        firstTrans.transaction = aus.transaction;

                        if (msgsnd(tpId, &(aus.transaction), sizeof(MsgTP) - sizeof(long), 0) == -1)
                        {
                            safeErrorPrint("Master: failed to initialize additional node's transaction pool. Error", __LINE__);
                        }
                        else
                        {
                            aus.mtype = childPid;
                            aus.msgContent = FRIENDINIT;
                            estrai(noAllTimesNodes - 1);
                            for (j = 0; j < SO_FRIENDS_NUM; j++)
                            {
                                aus.friend = nodesList[extractedFriendsIndex[j]].procId;
                                if (msgsnd(globalQueueId, &aus, sizeof(MsgGlobalQueue) - sizeof(long), 0) == -1)
                                {
                                    /*
                                        CORREGGERE: segnalazione fallimento trasazione a sender
                                    */
                                    safeErrorPrint("Master: failed to send a friend to new node. Error", __LINE__);
                                }
                            }

                            if (execle("node.out", "node", "ADDITIONAL", NULL, environ) == -1)
                                safeErrorPrint("Master: failed to load node's code. Error", __LINE__);
                        }
                    }
                }
                else if (procPid > 0)
                {
                    /*
                    1) Chiedere ad altri nodi
                    di aggiungere il nuovo processo agli amici:Ok
               */
                    /*
                CORREGGERE: è giusto passare noEffectiveNodes????
              */
                    sops[0].sem_flg = 0;
                    sops[0].sem_num = 1;
                    sops[0].sem_op = -1;
                    sops[1].sem_flg = 0;
                    sops[1].sem_num = 0;
                    sops[1].sem_op = -1;
                    if (semop(nodeListSem, sops, 2) == -1)
                    {
                        unsafeErrorPrint("Master: failed to reserve nodes' list read/mutex semphore. Error", __LINE__);
                        endOfSimulation(-1);
                    }

                    *(noNodeSegReadersPtr)++;
                    if (*(noNodeSegReadersPtr) == 1)
                    {
                        sops[2].sem_flg = 0;
                        sops[2].sem_num = 2;
                        sops[2].sem_op = -1;
                        if (semop(nodeListSem, sops, 1) == -1)
                        {
                            unsafeErrorPrint("Master: failed to reserve nodes' list write semphore. Error", __LINE__);
                            endOfSimulation(-1);
                        }
                    }

                    sops[0].sem_flg = 0;
                    sops[0].sem_num = 0;
                    sops[0].sem_op = 1;
                    sops[1].sem_flg = 0;
                    sops[1].sem_num = 1;
                    sops[1].sem_op = 1;
                    if (semop(nodeListSem, sops, 2) == -1)
                    {
                        unsafeErrorPrint("Master: failed to release nodes' list mutex/read semphore. Error", __LINE__);
                        endOfSimulation(-1);
                    }

                    newNode.procId = procPid;
                    newNode.procState = ACTIVE;

                    aus.friend = newNode.procId;
                    aus.msgContent = NEWFRIEND;
                    estrai(noEffectiveNodes);
                    for (j = 0; j < SO_FRIENDS_NUM; j++)
                    {
                        aus.mtype = nodesList[extractedFriendsIndex[j]].procId;
                        if (msgsnd(globalQueueId, &aus, sizeof(MsgGlobalQueue) - sizeof(long), 0) == -1)
                        {
                            /*
                                        CORREGGERE: possiamo semplicemente segnalare l'errore senza fare nulla?
                                        Io direi di sì, non mi sembra che gestioni più complicate siano utili
                                    */
                            safeErrorPrint("Master: failed to ask a node to add the new process to its fiends' list. Error", __LINE__);
                        }
                    }

                    sops[0].sem_flg = 0;
                    sops[0].sem_num = 0;
                    sops[0].sem_op = -1;
                    if (semop(nodeListSem, sops, 1) == -1)
                    {
                        unsafeErrorPrint("Master: failed to reserve nodes' list mutex semphore. Error", __LINE__);
                        endOfSimulation(-1);
                    }

                    if (*noNodeSegReadersPtr == 0)
                    {
                        sops[2].sem_flg = 0;
                        sops[2].sem_num = 2;
                        sops[2].sem_op = 1;
                        if (semop(nodeListSem, sops, 1) == -1)
                        {
                            unsafeErrorPrint("Master: failed to release nodes' list write semphore. Error", __LINE__);
                            endOfSimulation(-1);
                        }
                    }

                    sops[0].sem_flg = 0;
                    sops[0].sem_num = 0;
                    sops[0].sem_op = 1;
                    if (semop(nodeListSem, sops, 1) == -1)
                    {
                        unsafeErrorPrint("Master: failed to release nodes' list mutex semphore. Error", __LINE__);
                        endOfSimulation(-1);
                    }
                }
                else
                {
                    unsafeErrorPrint("Master: no more resources for new node. Simulation will be terminated.", __LINE__);
                    /*
                Sono finite le risorse, cosa facciamo?
                    1) Segnaliamo stampando la cosa a video e basta (del resto
                    nodi ed utenti esistenti possono continuare, però una transazione viene scartata
                    quindi sarebbe carino segnalarlo al sender)
                    2) Terminiamo la simulazione: mi sembra eccessivo

                    Io propenderei per la prima soluzione, cercando anche di segnalare il fallimento
                    al sender
               */
                    endOfSimulation(-1);
                }
            }
            else
            {
                unsafeErrorPrint("Master: no space left for storing new node information. Simulation will be terminated.", __LINE__);
                /*
                    CORREGGERE: Mettere qui la segnalazione di fine simulazione
                    oppure quella di fallimento transazione
                */
                endOfSimulation(-1);
            }
        }
        else
        {
            printf("Master: no node creation requests to be served. \n");
            /*
                Reinserire transazione
            */
        }
    }
    else
    {
        printf("Master: no node creation requests to be served. \n");
    }
}

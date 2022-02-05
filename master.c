#define _GNU_SOURCE

/**** Headers inclusion ****/
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "info.h"
/**** End of Headers inclusion ****/

/**** Constants definition ****/
#define NO_ATTEMPS_TERM 3                    /* Maximum number of attemps to terminate the simulation*/
#define MAX_PRINT_PROCESSES 15               /* Maximum number of processes of which we show budget, if noEffectiveNodes + noEffectiveUsers > MAX_PRINT_PROCESSES we only print max and min budget */
#define NO_ATTEMPTS_UPDATE_BUDGET 3          /* Number of attempts to update budget reading a block on register */
#define NO_ATTEMPTS_NEW_NODE_REQUESTS 5      /* Number of attempts to check for new node requests */
#define NO_ATTEMPTS_CHECK_USER_TERMINATION 10 /* Number of attempts to check for user terminations */
#define NO_ATTEMPTS_CHECK_NODE_TERMINATION 10 /* Number of attempts to check for node terminations */
/**** End of Constants definition ****/

/*** GLOBAL VARIABLES FOR IPC ***/
#pragma region GLOBAL VARIABLES FOR IPC

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

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
Register * regPtrs[REG_PARTITION_COUNT] = {NULL, NULL, NULL};

/* Id of the shared memory segment that contains the users list */
int usersListId = -1;

/* Pointer to the users list */
ProcListElem *usersList = NULL;

/* Id of the shared memory segment that contains the nodes list */
int nodesListId = -1;

/* Pointer to the nodes list */
ProcListElem *nodesList = NULL;

/* Pointer to the tp list */
TPElement *tpList = NULL;

/* Id of the global message queue where users, nodes and master communicate */
int nodeCreationQueue = -1;
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
int noReadersPartitions[REG_PARTITION_COUNT] = {0, 0, 0};

/* Pointer to the array containing the variables used to syncronize readers and writers access to register's partition.
 * noReadersPartitionsPtrs[0]: pointer to the first partition's shared variable
 * noReadersPartitionsPtrs[1]: pointer to the second partition's shared variable
 * noReadersPartitionsPtrs[2]: pointer to the third partition's shared variable
 */
int * noReadersPartitionsPtrs[REG_PARTITION_COUNT] = {NULL, NULL, NULL};

/* Id of the set that contains the semaphores (mutex = 0, read = 1, write = 2) used to read and write users list */
int userListSem = -1;

/* Id of the shared memory segment that contains the variable used to syncronize readers and writers access to users list */
int noUserSegReaders = -1;

/* Pointer to the variable that counts the number of readers, used to syncronize readers and writers access to users list */
int *noUserSegReadersPtr = NULL;

/* Id of the set that contains the semaphores (mutex = 0, read = 1, write = 2) used to read and write nodes list */
int nodeListSem = -1;
/*
 * We need a variable to count the number of readers because to extract a node which
 * to send the transaction to process we need to read the nodes' list.
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

/*
    We use a long int variable to handle an outstanding number
    of child processes
*/
/* Number of users that terminated before end of simulation*/
long noTerminatedUsers = 0;

/* Number of processes that terminated before end of simulation*/
long noTerminatedNodes = 0;

/* Holds the effective number of nodes */
long noEffectiveNodes = 0;

/* Holds the effective number of users */
long noEffectiveUsers = 0;
/*long noAllTimesNodes = 0;  */ /* Historical number of nodes: it counts also the terminated ones */
/* Historical number of users: it counts also the terminated ones */
long noAllTimesUsers = 0;

/* keeps tpList length */
long tplLength = 0;

#pragma endregion
/*** END GLOBAL VARIABLES FOR IPC ***/

/*** GLOBAL VARIABLES ***/
#pragma region GLOBAL VARIABLES
/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
extern char **environ;
struct timespec now;
int *extractedFriendsIndex;

/* struct that rappresents a process and its budget*/
typedef struct proc_budget
{
    pid_t proc_pid;
    double budget;
    int p_type;               /* type of node: 0 if user, 1 if node */
    struct proc_budget *prev; /* keeps link to previous node */
    struct proc_budget *next; /* keeps link to next node */
} proc_budget;

/*
 * The idea is that the register is immutable and therefore is not
 * need to go through it all every time.
 * To improve the efficiency of budget calculation
 * we can just update the budgets on the basis
 * of transactions entered in the register only
 * between updates
 */
/* linked list of budgets for every user and node process */
typedef proc_budget *budgetlist;

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

/* max number of nodes */
long maxNumNode = 0;
/* used in file reading */
char line[CONF_MAX_LINE_NO][CONF_MAX_LINE_SIZE];
/* initialization of the budgetlist head - array to maintain budgets read from ledger */
budgetlist bud_list_head = NULL;
/* initialization of the budgetlist tail - array to maintain budgets read from ledger */
budgetlist bud_list_tail = NULL;
/* master's pid */
long masterPid = -1;
/* keep track if simulation is terminated */
boolean simTerminated = FALSE;
/* keeps track of segmentation fault catched by handler */
int segFaultHappened = 0;
#pragma endregion
/*** END GLOBAL VARIABLES ***/

/*** FUNCTIONS PROTOTYPES DECLARATION ***/
#pragma region FUNCTIONS PROTOTYPES DECLARATION
/**
 * @brief Function that assigns the values ​​of the environment variables to the global variables defined above.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean assignEnvironmentVariables();

/**
 * @brief Function that reads the file containing the configuration parameters to save them as environment variables.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean readConfigParameters();

/**
 * @brief Allocation of global structures.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean allocateGlobalStructures();

/**
 * @brief Ipc structures allocation.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean initializeIPCFacilities();

/**
 * @brief Function that ends the execution of the user; this can happen in three different ways,
 * rappresented by the values that the parameter might assume.
 * @param sig the parameters value are: 0 -> only end of execution; -1 -> end of execution and deallocation (called from error);
 * SIGUSR1 -> end of execution and deallocation (called by signal from master)
 */
void endOfSimulation(int);

/**
 * @brief Function that deallocates the IPC facilities for the user.
 * @param exitcode indicates whether the simulation ends successfully or not
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean deallocateFacilities(int *);

/**
 * @brief Function that checks for node creation requests.
 */
void checkNodeCreationRequests();

/**
 * @brief Function to frees the space dedicated to the budget list p passed as argument.
 * @param p pointer to linked list budgetlist
 */
void budgetlist_free(budgetlist);

/**
 * @brief Function that inserts in the global list bud_list the node passed as
 * argument in an ordered way (the list is ordered in ascending order).
 * We want to keep the list sorted to implement a more efficient
 * budget calculation.
 * @param pid pid of new process to insert
 * @param budget budget of new node to insert
 * @param p_type type of new node to insert (0 = user, 1 = node)
 */
void insert_ordered(pid_t, double, int);

/**
 * @brief Function that searches in the gloabl list bud_list for an element with
 * proc_pid as the one passed as first argument; if it's found, upgrades its budget
 * adding the second argument, which is a positive or negative amount.
 * @param remove_pid pid of the item to be searched for in the budgetlist.
 * @param amount_changing positive or negative amount.
 */
int update_budget(pid_t, double);

/**
 * @brief Function that print remained transactions.
 */
void printRemainedTransactions();

/**
 * @brief Upload the location to extractedFriendsIndex in friends' nodesList
 * (doing so extracts friends)
 * @param k index of the process that cannot be extracted, i.e. the one calling the function.
 */
void estrai(int);

/**
 * @param sig signal that fired the handler
 */
void tmpHandler(int sig);

/**
 * @brief Function that catches any segmentation fault error during execution and
 * avoids brutal termination.
 * @param sig signal that fired the handler
 */
void segmentationFaultHandler(int);
#pragma endregion
/*** END FUNCTIONS PROTOTYPES DECLARATION ***/

/*** MAIN FUNCTION ***/
int main(int argc, char *argv[])
{
    /* ONLY FOR DEBUG!!!!*/
    int fdReport;

    pid_t child_pid;
    struct sembuf sops[3];
    sigset_t set;
    struct sigaction act;
    struct sigaction actSegFaultHandler;
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
    int ct_updates;

    /* array that keeps memory of the block we stopped reading budgets for every partition of register  */
    int prev_read_nblock[REG_PARTITION_COUNT];

    /* index for scrolling blocks */
    int ind_block;
    /* index for scrolling bulk transactions */
    int ind_tr_in_block = 0;

    /* variable that keeps track of the attempts to update a budget,
    if > NO_ATTEMPTS_UPDATE_BUDGET we switch to next block */
    int bud_update_attempts = 0;

    /* declaring message structures used with global queue */
    ProcQueue msg_to_node, msg_from_user, msg_from_node;
    MsgGlobalQueue msg_new_node;

    /* declaring of variables for budget update */
    Block block;
    Transaction trans;
    struct msqid_ds tpStruct;

    /* Number of all processes created during the simulation */
    long noAllTimeProcesses = 0;

    /* Number of user/node attempts of termination */
    int noAttemptsCheckUserTerm = 0;
    int noAttemptsCheckNodeTerm = 0;
    long k = 0;
    
    /* initializing print string message */
    char *aus = NULL;

    /* Set common semaphore options*/
    sops[0].sem_num = 0;
    sops[0].sem_flg = 0;
    sops[1].sem_num = 2;
    sops[1].sem_flg = 0;
    sops[2].sem_num = 2;
    sops[2].sem_flg = 0;

    /* allocation of extractedFriendsIndex */
    extractedFriendsIndex = (int *)malloc(SO_FRIENDS_NUM * sizeof(int));
    /* initializing print string message */
    aus = (char *)calloc(200, sizeof(char));

    /* setting data for waiting for one second */
    onesec.tv_sec = 1;
    onesec.tv_nsec = 0;

    /* erasing of prev_read_nblock array */
    for (i = 0; i < REG_PARTITION_COUNT; i++)
        prev_read_nblock[i] = 0; /* qui memorizzo il blocco a cui mi sono fermato allo scorso ciclo nella i-esima partizione */

    /* saves master pid */
    masterPid = (long)getpid();

    signal(SIGINT, endOfSimulation);

    printf("[MASTER]: my pid is %5ld\n", masterPid);
    printf("**** [MASTER]: simulation configuration started ****\n");

    printf("[MASTER]: reading configuration parameters...\n");
    if (readConfigParameters() == FALSE)
        endOfSimulation(-1);

    printf("[MASTER]: setting up simulation timer...\n");
    printf("[MASTER]: simulation lasts %ld seconds\n", SO_SIM_SEC);
    /* No previous alarms were set, so it must return 0*/
    if (alarm(SO_SIM_SEC) != 0)
        unsafeErrorPrint("[MASTER]: failed to set up simulation timer. ", __LINE__);
    else
    {
        printf("[MASTER]: setting up signal mask...\n");
        if (sigfillset(&set) == -1)
            unsafeErrorPrint("[MASTER]: failed to initialize signals mask. Error: ", __LINE__);
        else
        {
            /* We block all the signals during the execution of the handler*/
            act.sa_handler = endOfSimulation;
            act.sa_mask = set;
            printf("[MASTER]: signal mask initialized successfully.\n");

            printf("[MASTER]: setting end of timer disposition...\n");
            if (sigaction(SIGALRM, &act, NULL) == -1)
                unsafeErrorPrint("[MASTER]: failed to set end of timer disposition. Error: ", __LINE__);
            else
            {
                printf("[MASTER]: setting end of simulation disposition...\n");
                if (sigaction(SIGUSR1, &act, NULL) == -1)
                    unsafeErrorPrint("[MASTER]: failed to set end of simulation disposition. Error: ", __LINE__);
                else
                {
                    
                    /*actSegFaultHandler.sa_handler = segmentationFaultHandler;
                    actSegFaultHandler.sa_mask = set;
                    if (sigaction(SIGSEGV, &actSegFaultHandler, NULL) == -1)
                    {
                        unsafeErrorPrint("[MASTER]: failed to set segmentation fault handler. Error: ", __LINE__);
                    }
                    else
                    {*/
                        maxNumNode = SO_NODES_NUM + MAX_ADDITIONAL_NODES;

                        printf("[MASTER]: creating IPC facilitites...\n");
                        if (allocateGlobalStructures() == TRUE)
                        {
                            printf("[MASTER]: initializating IPC facilitites...\n");
                            if (initializeIPCFacilities() == TRUE)
                            {
                                /*****  Creates SO_USERS_NUM children   *****/
                                /********************************************/
                                printf("[MASTER]: forking user processes...\n");
                                for (i = 0; i < SO_USERS_NUM; i++)
                                {
                                    printf("[MASTER]: user number %d\n", i);
                                    switch (child_pid = fork())
                                    {
                                    case -1:
                                        /*Handle error*/
                                        unsafeErrorPrint("[MASTER]: fork failed. Error: ", __LINE__);
                                        /*
                                            *    (**)
                                            *    In case we failed to create a process we end
                                            *    the simulation.
                                            *    This solution is extended to every operation required to create a node/user.
                                            *    This solution is quite restrictive, but we have to consider
                                            *    that loosing even one process before it even started
                                            *    means violating the project requirments
                                            */
                                        endOfSimulation(-1);
                                    case 0:
                                        /*
                                            * The process tells the father that it is ready to run
                                            *and that it waits for all processes to be ready
                                            */
                                        printf("[USER %5ld]: starting execution....\n", (long)getpid());
                                        signal(SIGALRM, SIG_IGN);
                                        signal(SIGUSR1, tmpHandler);

                                        printf("[USER %5ld]: waiting for simulation to start....\n", (long)getpid());
                                        sops[0].sem_op = 0;
                                        sops[0].sem_num = 0;
                                        sops[0].sem_flg = 0;
                                        if (semop(fairStartSem, &sops[0], 1) == -1)
                                        {
                                            /*
                                                See comment above (**)
                                            */
                                            snprintf(aus, 199, "[USER %5ld]: failed to wait for zero on start semaphore. Error: ", (long)getpid());
                                            unsafeErrorPrint(aus, __LINE__);
                                            endOfSimulation(-1);
                                        }
                                        else
                                        {
                                            if (execle("user.out", "user", NULL, environ) == -1)
                                            {
                                                snprintf(aus, 199, "[USER %5ld]: failed to load user's code. Error: ", (long)getpid());
                                                unsafeErrorPrint(aus, __LINE__);
                                                endOfSimulation(-1);
                                            }
                                        }
                                        break;

                                    default:
                                        fdReport = open("processes_killer/processes_created.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                                        dprintf(fdReport, "%d\n", child_pid);
                                        close(fdReport);

                                        /* Increments number of effective users and alltime users */
                                        noEffectiveUsers++;
                                        noAllTimesUsers++;

                                        /* Process notify his creatorion */
                                        sops[0].sem_num = 0;
                                        sops[0].sem_op = -1;
                                        sops[0].sem_flg = IPC_NOWAIT;
                                        if (semop(fairStartSem, &sops[0], 1) == -1)
                                        {
                                            safeErrorPrint("[MASTER]: failed to decrement start semaphore. Error: ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        /*
                                            *    No user or node is writing or reading on the
                                            *    read but it's better to be one hundred percent
                                            *    to check no one is reading or writing from the list
                                            *
                                            *    ENTRY SECTION:
                                            *    Reserve read semaphore and Reserve write semaphore
                                            */
                                        sops[0].sem_op = -1;
                                        sops[0].sem_num = 1;

                                        sops[1].sem_op = -1;
                                        sops[1].sem_num = 2;
                                        if (semop(userListSem, sops, 2) == -1)
                                        {
                                            unsafeErrorPrint("[MASTER]: failed to reserve users list semaphore for writing operation. Error:  ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        /* Save users processes pid and state into usersList*/
                                        usersList[i].procId = child_pid;
                                        usersList[i].procState = ACTIVE;

                                        /*
                                            *Exit section
                                            */
                                        sops[0].sem_op = 1;
                                        sops[0].sem_num = 1;

                                        sops[1].sem_op = 1;
                                        sops[1].sem_num = 2;
                                        if (semop(userListSem, sops, 2) == -1)
                                        {
                                            unsafeErrorPrint("[MASTER]: failed to release users list semaphore for writing operation. Error:  ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        break;
                                    }
                                }
                                /********************************************/
                                /********************************************/

                                printf("[MASTER]: forking nodes processes...\n");
                                /*****  Creates SO_NODES_NUM children   *****/
                                /********************************************/
                                for (i = 0; i < SO_NODES_NUM; i++)
                                {
                                    printf("[MASTER]: node number %d\n", i);
                                    switch (child_pid = fork())
                                    {
                                    case -1:
                                        /* Handle error*/
                                        unsafeErrorPrint("[MASTER]: fork failed. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    case 0:
                                        /*
                                            * The process tells the father that it is ready to run
                                            *and that it waits for all processes to be ready
                                            */
                                        printf("[NODE %5ld]: starting execution....\n", (long)getpid());

                                        signal(SIGALRM, SIG_IGN);
                                        signal(SIGUSR1, tmpHandler);

                                        /* Temporary part to get the process to do something*/
                                        if (execle("node.out", "node", "NORMAL", NULL, environ) == -1)
                                        {
                                            snprintf(aus, 199, "[NODE %5ld]: failed to load node's code. Error: ", (long)getpid());
                                            unsafeErrorPrint(aus, __LINE__);
                                        }
                                        break;

                                    default:
                                        fdReport = open("processes_killer/processes_created.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                                        dprintf(fdReport, "%d\n", child_pid);
                                        close(fdReport);

                                        /* Process notify his creatorion */
                                        sops[0].sem_num = 0;
                                        sops[0].sem_op = -1;
                                        if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                                        {
                                            safeErrorPrint("[MASTER]: failed to reserve number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        /* Incrementing number of effective and all times nodes */
                                        noEffectiveNodes++;
                                        (*noAllTimesNodesPtr)++;

                                        sops[0].sem_num = 0;
                                        sops[0].sem_op = 1;
                                        if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                                        {
                                            safeErrorPrint("[MASTER]: failed to release number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        sops[0].sem_num = 0;
                                        sops[0].sem_op = -1;
                                        sops[0].sem_flg = IPC_NOWAIT;
                                        if (semop(fairStartSem, &sops[0], 1) == -1)
                                        {
                                            safeErrorPrint("[MASTER]: failed to decrement start semaphore. Error: ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        /*Initialize messages queue for transactions pools*/
                                        tpList[i].procId = (long)child_pid;
                                        key = ftok(MSGFILEPATH, child_pid);
                                        if (key == -1)
                                        {
                                            unsafeErrorPrint("[MASTER]: failed to initialize process' transaction pool. Error: ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        tpList[i].msgQId = msgget(key, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
                                        if (tpList[i].msgQId == -1)
                                        {
                                            unsafeErrorPrint("[MASTER]: failed to initialize process' transaction pool. Error: ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        fdReport = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                                        dprintf(fdReport, "q;%d\n", tpList[i].msgQId);
                                        close(fdReport);

                                        if (msgctl(tpList[i].msgQId, IPC_STAT, &tpStruct) == -1)
                                        {
                                            unsafeErrorPrint("[MASTER]: failed to retrive process transaction pool's size. Error", __LINE__);
                                            endOfSimulation(-1);
                                        }
                                        else
                                        {
                                            /*
                                                tpStruct.msg_qbytes was set to the maximum possible value
                                                during the msgget
                                            */
                                            /*
                                            tpStruct.msg_qbytes = (sizeof(MsgTP) - sizeof(long)) * SO_TP_SIZE;
                                            if (msgctl(tpList[i].msgQId, IPC_SET, &tpStruct) == -1)
                                            {
                                                unsafeErrorPrint("[MASTER]: failed to set process transaction pool's size. Error", __LINE__);
                                                endOfSimulation(-1);
                                            }
                                            printf("Master: impostata nuova dimensione coda.\n");*/

                                            
                                            if (tpStruct.msg_qbytes > (sizeof(MsgTP) - sizeof(long)) * SO_TP_SIZE)
                                            {
                                                tpStruct.msg_qbytes = (sizeof(MsgTP) - sizeof(long)) * SO_TP_SIZE;
                                                if (msgctl(tpList[i].msgQId, IPC_SET, &tpStruct) == -1)
                                                {
                                                    unsafeErrorPrint("[MASTER]: failed to set process transaction pool's size. Error", __LINE__);
                                                    endOfSimulation(-1);
                                                }
                                                printf("Master: impostata nuova dimensione coda.\n");
                                            }

                                            /*tpStruct.msg_qbytes = sizeof(MsgTP) * SO_TP_SIZE;*/
                                            
                                            /*
                                                *   If the size is larger than the maximum size then
                                                *   we do not make any changes
                                                */
                                        }

                                        /* updating tpList length */
                                        tplLength++;

                                        /* Save nodes processes pid and state into nodesList */
                                        sops[0].sem_op = -1;
                                        sops[0].sem_num = 1;
                                        sops[1].sem_op = -1;
                                        sops[1].sem_num = 2;
                                        if (semop(nodeListSem, sops, 2) == -1)
                                        {
                                            unsafeErrorPrint("[MASTER]: failed to reserve nodes list semaphore for writing operation. Error ", __LINE__);
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
                                            unsafeErrorPrint("[MASTER]: failed to release nodes list semaphore for writing operation. Error ", __LINE__);
                                            endOfSimulation(-1);
                                        }

                                        break;
                                    }
                                }

                                /********************************************/
                                /********************************************/

                                /*****  Initialize budget for users processes   *****/
                                /****************************************************/

                                /* we enter the critical section for the noUserSegReadersPtr variabile */
                                sops[0].sem_op = -1;
                                sops[0].sem_num = 0;
                                sops[1].sem_op = -1;
                                sops[1].sem_num = 1;
                                if (semop(userListSem, sops, 2) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to reserve usersList semaphore for reading operation. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                (*noUserSegReadersPtr)++;
                                if ((*noUserSegReadersPtr) == 1)
                                {
                                    sops[0].sem_num = 2;
                                    sops[0].sem_op = -1;
                                    if (semop(userListSem, &sops[0], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to reserve write usersList semaphore. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    }
                                    /*
                                        * if the writer is writing, then the first reader who will enter this
                                        * branch will fall asleep on this traffic light.
                                        * if the writer is not writing, then the first reader will decrement the by 1
                                        * traffic light value, so if the writer wants to write, he will fall asleep
                                        * on the traffic light
                                        */
                                }

                                /* We exit the critical section for the noUserSegReadersPtr variabile */
                                sops[0].sem_num = 0;
                                sops[0].sem_op = 1;
                                sops[1].sem_num = 1;
                                sops[1].sem_op = 1;
                                if (semop(userListSem, sops, 2) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to release usersList semaphore after reading operation. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                printf("[MASTER]: initializing budget users processes...\n");
                                bud_list_head = NULL;
                                bud_list_tail = NULL;
                                for (i = 0; i < SO_USERS_NUM; i++)
                                {
                                    /*new_el = malloc(sizeof(*new_el));
                                    new_el->proc_pid = usersList[i].procId;
                                    new_el->budget = SO_BUDGET_INIT;
                                    new_el->p_type = 0;*/
                                    insert_ordered(usersList[i].procId, SO_BUDGET_INIT, 0); /* insert user on budgetlist */
                                }

                                /* we enter the critical section for the noUserSegReadersPtr variabile */
                                sops[0].sem_num = 0;
                                sops[0].sem_op = -1;
                                if (semop(userListSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to reserve mutex usersList semaphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                (*noUserSegReadersPtr)--;
                                if ((*noUserSegReadersPtr) == 0)
                                {
                                    sops[0].sem_num = 2;
                                    sops[0].sem_op = 1;
                                    if (semop(userListSem, &sops[0], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to reserve write usersList semaphore. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    }
                                    /*
                                        * if I am the last reader and stop reading then I need to reset to 0
                                        * the semaphore value so that if the writer wants to write he can do it.
                                        */
                                }

                                /* we exit the critical section for the noUserSegReadersPtr variabile */
                                sops[0].sem_num = 0;
                                sops[0].sem_op = 1;
                                if (semop(userListSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to release mutex usersList semaphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }
                                /****************************************************/
                                /****************************************************/

                                /*****  Initialize budget for nodes processes   *****/
                                /****************************************************/
                                printf("[MASTER]: initializing budget nodes processes...\n");
                                /* we enter the critical section for the noNodeSegReadersPtr variabile */
                                sops[0].sem_num = 0;
                                sops[0].sem_op = -1;
                                sops[1].sem_num = 1;
                                sops[1].sem_op = -1;
                                if (semop(nodeListSem, sops, 2) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to reserve nodeList semaphore for reading operation. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                (*noNodeSegReadersPtr)++;
                                if ((*noNodeSegReadersPtr) == 1)
                                {
                                    sops[0].sem_num = 2;
                                    sops[0].sem_op = -1;
                                    if (semop(nodeListSem, &sops[0], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to reserve write nodeList semaphore. Error: ", __LINE__);
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
                                    safeErrorPrint("[MASTER]: failed to release nodeList semaphore after reading operation. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                for (i = 0; i < SO_NODES_NUM; i++)
                                {
                                    /*new_el = malloc(sizeof(*new_el));
                                    new_el->proc_pid = nodesList[i].procId;
                                    new_el->budget = 0;
                                    new_el->p_type = 1;*/
                                    insert_ordered(nodesList[i].procId, 0, 1); /* insert node on budgetlist */
                                }
                                /****************************************************/
                                /****************************************************/

                                /************** END OF INITIALIZATION OF BUDGETLIST **************/
                                /*****************************************************************/

                                /* Still have to work on nodesList, I do after the UNLOCK of the critical zone */

                                /*****  Friends estraction   *****/
                                /*********************************/
                                printf("[MASTER]: extracting friends for nodes...\n");
                                for (i = 0; i < SO_NODES_NUM; i++)
                                {
                                    estrai(i);
                                    msg_to_node.mtype = nodesList[i].procId;
                                    msg_to_node.msgContent = FRIENDINIT;
                                    /*tmpFriend.mtype = nodesList[i].procId;**/
                                    for (j = 0; j < SO_FRIENDS_NUM; j++)
                                    {
                                        msg_to_node.procPid = nodesList[extractedFriendsIndex[j]].procId;
                                        if (msgsnd(procQueue, &msg_to_node, sizeof(msg_to_node) - sizeof(long), 0) == -1)
                                        {
                                            unsafeErrorPrint("[MASTER]: failed to initialize node friends. Error: ", __LINE__);
                                            endOfSimulation(-1);
                                        }
                                    }
                                }

                                /* Now UNLOCK of the critical zone */
                                /* we enter the critical section for the noNodeSegReadersPtr variabile */
                                sops[0].sem_num = 0;
                                sops[0].sem_op = -1;
                                if (semop(nodeListSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to reserve mutex nodeList semaphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                (*noNodeSegReadersPtr)--;
                                if ((*noNodeSegReadersPtr) == 0)
                                {
                                    sops[0].sem_num = 2;
                                    sops[0].sem_op = 1;
                                    if (semop(nodeListSem, &sops[0], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to reserve write nodeList semaphore. Error: ", __LINE__);
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
                                    safeErrorPrint("[MASTER]: failed to release nodeList semaphore after reading operation. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }
                                /*****  End of Friends estraction   *****/
                                /****************************************/

                                printf("[MASTER]: about to start simulation...\n");
                                sops[0].sem_op = -1;
                                sops[0].sem_num = 0;
                                sops[0].sem_flg = 0;
                                semop(fairStartSem, &sops[0], 1);

                                /* master lifecycle*/
                                printf("**** [MASTER]: starting lifecycle... ****\n");
                                while (1 && child_pid)
                                {
                                    /* checking if register's partitions are full */
                                    printf("[MASTER]: checking if register's partitions are full...\n");
                                    fullRegister = TRUE;
                                    for (i = 0; i < REG_PARTITION_COUNT && fullRegister; i++)
                                    {
                                        if (regPtrs[i]->nBlocks < REG_PARTITION_SIZE)
                                            fullRegister = FALSE;
                                    }

                                    if (fullRegister)
                                    {
                                        /* it contains an exit call
                                        so no need to set exit code*/
                                        printf("[MASTER]: all register's partitions are full. Terminating simulation...\n");
                                        endOfSimulation(SIGUSR1);
                                    }

                                    /**** CYCLE THAT UPDATES BUDGETLIST OF PROCESSES BEFORE PRINTING IT ****/
                                    /***********************************************************************/

                                    /* cycle that updates the budget list before printing it */
                                    /* at every cycle we do the count of budgets in blocks of the i-th partition */
                                    printf("[MASTER]: updating budget list before printing...\n");
                                    for (i = 0; i < REG_PARTITION_COUNT; i++)
                                    {
                                        /* setting options for getting access to i-th partition of register */

                                        /* we enter the critical section for the noReadersPartitions variabile of i-th partition */
                                        sops[0].sem_num = i;
                                        sops[0].sem_op = -1;
                                        if (semop(rdPartSem, &sops[0], 1) == -1)
                                        {
                                            snprintf(aus, 199, "[MASTER]: failed to reserve read semaphore for %d-th partition. Error: ", i);
                                            unsafeErrorPrint(aus, __LINE__);
                                            /*
                                                *Computing the budget is a critical operation, so we end the simulation
                                                *in case of error
                                                */
                                            endOfSimulation(-1);
                                        }
                                        else
                                        {
                                            sops[0].sem_num = i;
                                            sops[0].sem_op = -1;
                                            if (semop(mutexPartSem, &(sops[0]), 1) == -1)
                                            {
                                                snprintf(aus, 199, "[MASTER]: failed to reserve mutex semaphore for %d-th partition. Error: ", i);
                                                unsafeErrorPrint(aus, __LINE__);
                                                endOfSimulation(-1);
                                            }

                                            printf("Yo soy un puntator %p valor %d\n", noReadersPartitionsPtrs[i], *(noReadersPartitionsPtrs[i]));
                                            /*fdReport = open("pointer_values.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                                            dprintf(fdReport, "[MASTER]: before increment %d puntatore %p valore %ld\n", __LINE__, noReadersPartitionsPtrs[i], *(noReadersPartitionsPtrs[i]));
                                            close(fdReport);*/
                                            /*k = *(noReadersPartitionsPtrs[i]);
                                            *(noReadersPartitionsPtrs[i]) = *(noReadersPartitionsPtrs[i]) + 1;
                                            k++;*/
                                            *(noReadersPartitionsPtrs[i])++;
                                            if (*(noReadersPartitionsPtrs[i]) == 1)
                                            {
                                                sops[0].sem_num = i;
                                                sops[0].sem_op = -1;
                                                if (semop(wrPartSem, &sops[0], 1) == -1)
                                                {
                                                    snprintf(aus, 199, "[MASTER]: failed to reserve write semaphore for %d-th partition. Error: ", i);
                                                    unsafeErrorPrint(aus, __LINE__);
                                                    endOfSimulation(-1);
                                                }
                                            }

                                            /* we exit the critical section for the noUserSegReadersPtr variabile */
                                            sops[0].sem_num = i;
                                            sops[0].sem_op = 1;
                                            if (semop(mutexPartSem, &sops[0], 1) == -1)
                                            {
                                                snprintf(aus, 199, "[MASTER]: failed to release mutex semaphore for %d-th partition. Error: ", i);
                                                unsafeErrorPrint(aus, __LINE__);
                                                endOfSimulation(-1);
                                            }
                                            else
                                            {
                                                sops[0].sem_num = i;
                                                sops[0].sem_op = 1;
                                                if (semop(rdPartSem, &sops[0], 1) == -1)
                                                {
                                                    snprintf(aus, 199, "[MASTER]: failed to release read semaphore for %d-th partition. Error: ", i);
                                                    unsafeErrorPrint(aus, __LINE__);
                                                    endOfSimulation(-1);
                                                }
                                                printf("[MASTER]: gained access to %d-th partition of register\n", i);

                                                /* Initialize the index to the block where I stopped in the last loop */
                                                ind_block = prev_read_nblock[i];

                                                /*scrolling cycle of the blocks of the i - th partition */
                                                while (ind_block < regPtrs[i]->nBlocks)
                                                {
                                                    /* returns the index block ind_block */
                                                    block = regPtrs[i]->blockList[ind_block];
                                                    ind_tr_in_block = 0;
                                                    bud_update_attempts = 0; /* reset attempts */

                                                    /* Scroll through the list of transitions of the index block ind_block */
                                                    while (ind_tr_in_block < SO_BLOCK_SIZE)
                                                    {
                                                        /* returns the index transaction ind_tr_in_block */
                                                        trans = block.transList[ind_tr_in_block];

                                                        /* counts the number of budget updates made for the transaction (total 2, one for sender and one for receiver) */
                                                        ct_updates = 0;
                                                        if (trans.sender == -1)
                                                        {
                                                            ct_updates++;
                                                            /*
                                                                * if the sender is -1, it represents the node's reward payment transaction,
                                                                * therefore you do not need to update the budget of the sender, but only of the receiver.
                                                                */
                                                        }

                                                        /* update budget of sender of transaction, the amount is negative */
                                                        /* error checking not needed, already done in function */
                                                        else if (update_budget(trans.sender, -(trans.amountSend + trans.reward)) == 0)
                                                            ct_updates++;

                                                        /* update budget of receiver of transaction, the amount is positive */
                                                        /* error checking not needed, already done in function */
                                                        if (update_budget(trans.receiver, trans.amountSend) == 0)
                                                            ct_updates++;

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

                                                /* Memorize the block I stopped at */
                                                prev_read_nblock[i] = ind_block;

                                                /* we enter the critical section for the noReadersPartitions variabile of i-th partition */
                                                sops[0].sem_num = i;
                                                sops[0].sem_op = -1;
                                                if (semop(mutexPartSem, &sops[0], 1) == -1)
                                                {
                                                    snprintf(aus, 199, "[MASTER]: failed to reserve mutex semaphore for %d-th partition. Error: ", i);
                                                    unsafeErrorPrint(aus, __LINE__);
                                                    endOfSimulation(-1);
                                                }
                                                else
                                                {
                                                    /*fdReport = open("pointer_values.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                                                    dprintf(fdReport, "[MASTER]: before decrement %d puntatore %p valore %ld\n", __LINE__, noReadersPartitionsPtrs[i], *(noReadersPartitionsPtrs[i]));
                                                    close(fdReport);*/
                                                    *(noReadersPartitionsPtrs[i])--;
                                                    if (*(noReadersPartitionsPtrs[i]) == 0)
                                                    {
                                                        sops[0].sem_num = i;
                                                        sops[0].sem_op = 1;
                                                        if (semop(wrPartSem, &sops[0], 1) == -1)
                                                        {
                                                            snprintf(aus, 199, "[MASTER]: failed to reserve write semaphore for %d-th partition. Error: ", i);
                                                            unsafeErrorPrint(aus, __LINE__);
                                                            endOfSimulation(-1);
                                                        }
                                                    }
                                                    /* we exit the critical section for the noUserSegReadersPtr variabile */
                                                    sops[0].sem_num = i;
                                                    sops[0].sem_op = 1;
                                                    if (semop(mutexPartSem, &sops[0], 1) == -1)
                                                    {
                                                        snprintf(aus, 199, "[MASTER]: failed to release read semaphore for %d-th partition. Error: ", i);
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
                                    sops[0].sem_num = 0;
                                    sops[0].sem_op = -1;
                                    if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to reserve number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    /* Counting number of all times processes */
                                    noAllTimeProcesses = (*noAllTimesNodesPtr) + noAllTimesUsers;

                                    sops[0].sem_num = 0;
                                    sops[0].sem_op = 1;
                                    if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to release number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    if (noAllTimeProcesses <= MAX_PRINT_PROCESSES)
                                    {
                                        /*
                                            * the number of effective processes is lower or equal than the maximum we established,
                                            * so we print budget of all processes
                                            */
                                        printf("[MASTER]: Printing budget of all the processes.\n");

                                        for (el_list = bud_list_head; el_list != NULL; el_list = el_list->next)
                                        {
                                            if (el_list->p_type) /* Budget of node process */
                                                printf("[MASTER]:  - NODE PROCESS PID %5ld: actual budget %4.2f\n", (long)el_list->proc_pid, el_list->budget);
                                            else /* Budget of user process */
                                                printf("[MASTER]:  - USER PROCESS PID %5ld: actual budget %4.2f\n", (long)el_list->proc_pid, el_list->budget);
                                        }
                                    }
                                    else
                                    {
                                        /*
                                            * the number of effective processes is bigger than the maximum we established, so
                                            * we print only the maximum and minimum budget in the list
                                            */

                                        printf("[MASTER]: There are too many processes. Printing only minimum and maximum budgets.\n");

                                        /* printing minimum budget in budgetlist - we print all processes' budget that is minimum */
                                        /*
                                            *Here we take advantage of the sorted budget list: finding the minimum budget
                                            *is just a matter of checking if it's equal to that on top of the list
                                            */

                                        /* Printing budget of process with minimum budget */
                                        if (bud_list_head->p_type) /* Budget of node process */
                                            printf("[MASTER]:  - NODE PROCESS PID %5ld: actual budget %4.2f\n", (long)bud_list_head->proc_pid, bud_list_head->budget);
                                        else /* Budget of user process */
                                            printf("[MASTER]:  - USER PROCESS PID %5ld: actual budget %4.2f\n", (long)bud_list_head->proc_pid, bud_list_head->budget);

                                        /* Printing budget of process with maximum budget */
                                        if (bud_list_tail->p_type) /* Budget of node process */
                                            printf("[MASTER]:  - NODE PROCESS PID %5ld: actual budget %4.2f\n", (long)bud_list_tail->proc_pid, bud_list_tail->budget);
                                        else /* Budget of user process */
                                            printf("[MASTER]:  - USER PROCESS PID %5ld: actual budget %4.2f\n", (long)bud_list_tail->proc_pid, bud_list_tail->budget);
                                    }

                                    /* Printing number of active nodes and users */
                                    printf("[MASTER]: Number of active nodes: %ld\n", noEffectiveNodes);
                                    printf("[MASTER]: Number of active users: %ld\n", noEffectiveUsers);

                                    /**** END OF PRINT BUDGET OF EVERY PROCESS ****/
                                    /**********************************************/

                                    /* Checks if there are node creation requests */
                                    printf("[MASTER]: checking if there are node creation requests to be served...\n");
                                    checkNodeCreationRequests();

                                    /**** USER TERMINATION CHECK ****/
                                    /********************************/
                                    /* Check if a user process has terminated to update the usersList */
                                    noAttemptsCheckUserTerm = 0;

                                    while (noAttemptsCheckUserTerm < NO_ATTEMPTS_CHECK_USER_TERMINATION && msgrcv(procQueue, &msg_from_user, sizeof(ProcQueue) - sizeof(long), masterPid, IPC_NOWAIT) != -1)
                                    {
                                        /*
                                        fdReport = open("master_msgrcv_content.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                                        dprintf(fdReport, "MASTER: in user termination check msgContent is %d\n", msg_from_user.msgContent);
                                        close(fdReport);*/

                                        noAttemptsCheckUserTerm++;
                                        /* as size we specify sizeof (msg_from_user) -sizeof (long) because you have to specify the size of the text, not the whole structure */
                                        /* as mtype we take the messages destined to the Master, that is its pid (it takes the first message with that mtype) */

                                        /* in this case we look for messages with msgContent TERMINATEDUSER */
                                        if (msg_from_user.msgContent == TERMINATEDUSER)
                                        {
                                            /* we enter the critical section for the usersList */
                                            sops[0].sem_num = 1;
                                            sops[0].sem_op = -1;
                                            sops[1].sem_num = 2;
                                            sops[1].sem_op = -1;
                                            if (semop(userListSem, sops, 2) == -1)
                                            {
                                                safeErrorPrint("[MASTER]: failed to reserve usersList semaphore for writing operation. Error: ", __LINE__);
                                                endOfSimulation(-1);
                                            }
                                            else
                                            {
                                                /* cycle to search for the user process */
                                                for (i = 0; i < SO_USERS_NUM; i++)
                                                {
                                                    if (usersList[i].procId == msg_from_user.procPid)
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
                                                    safeErrorPrint("[MASTER]: failed to release usersList semaphore for writing operation. Error: ", __LINE__);
                                                    endOfSimulation(-1);
                                                }
                                                else
                                                {
                                                    printf("[MASTER]: the user process with pid %5d has terminated\n", msg_from_user.procPid);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            /* Reinserting the message that we have consumed from the global queue */
                                            if (msgsnd(procQueue, &msg_from_user, sizeof(ProcQueue) - sizeof(long), 0) == -1)
                                            {
                                                /* This is necessary, otherwise the message won't be reinserted in queue and lost forever */
                                                unsafeErrorPrint("[MASTER]: failed to reinsert the message read from the global queue while checking for terminated users. Error: ", __LINE__);
                                                endOfSimulation(-1);
                                            }
                                        }
                                    }

                                    /* If errno is ENOMSG, no message of user termination on global queue, otherwise an error occured */
                                    if (errno != 0 && errno != ENOMSG)
                                    {
                                        unsafeErrorPrint("[MASTER]: failed to retrieve user termination messages from global queue. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    /**** END OF USER TERMINATION CHECK ****/
                                    /***************************************/

                                    /**** NODE TERMINATION CHECK ****/
                                    /********************************/
                                    noAttemptsCheckNodeTerm = 0;

                                    /* Check if a node process has terminated to update the nodes list */
                                    while (noAttemptsCheckNodeTerm < NO_ATTEMPTS_CHECK_NODE_TERMINATION && msgrcv(procQueue, &msg_from_node, sizeof(ProcQueue) - sizeof(long), masterPid, IPC_NOWAIT) != -1)
                                    {
                                        /*
                                        fdReport = open("master_msgrcv_content.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                                        dprintf(fdReport, "MASTER: in node termination check msgContent is %d\n", msg_from_user.msgContent);
                                        close(fdReport);*/

                                        noAttemptsCheckNodeTerm++;

                                        if (msg_from_node.msgContent == TERMINATEDNODE)
                                        {
                                            sops[0].sem_num = 1;
                                            sops[0].sem_op = -1;
                                            sops[1].sem_num = 2;
                                            sops[1].sem_op = -1;
                                            if (semop(nodeListSem, sops, 2) == -1)
                                            {
                                                safeErrorPrint("[MASTER]: failed to reserve nodesList semaphore for writing operation. Error: ", __LINE__);
                                                endOfSimulation(-1);
                                            }
                                            else
                                            {
                                                for (i = 0; i < SO_NODES_NUM; i++)
                                                {
                                                    if (nodesList[i].procId == msg_from_node.procPid)
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
                                                    safeErrorPrint("[MASTER]: failed to release nodeslist semaphore for writing operation. Error: ", __LINE__);
                                                    endOfSimulation(-1);
                                                }
                                                else
                                                {
                                                    printf("[MASTER]: the node process with pid %5d has terminated\n", msg_from_node.procPid);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            /* Reinserting the message that we have consumed from the global queue */
                                            if (msgsnd(procQueue, &msg_from_node, sizeof(ProcQueue) - sizeof(long), 0) == -1)
                                            {
                                                /* This is necessary, otherwise the message won't be reinserted in queue and lost forever */
                                                unsafeErrorPrint("[MASTER]: failed to reinsert the message read from the global queue while checking for terminated nodes. Error: ", __LINE__);
                                                endOfSimulation(-1);
                                            }
                                        }
                                    }

                                    /* If errno is ENOMSG, no message of user termination on global queue, otherwise an error occured */
                                    if (errno != 0 && errno != ENOMSG)
                                    {
                                        unsafeErrorPrint("[MASTER]: failed to retrieve node termination messages from global queue. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    }

                                    /**** END OF NODE TERMINATION CHECK ****/
                                    /***************************************/

                                    printf("--------------- END OF CYCLE ---------------\n");

                                    if (noEffectiveUsers == 0)
                                    {
                                        endOfSimulation(-2);
                                    }
                                    else if (noEffectiveNodes == 0)
                                        endOfSimulation(-3);

                                    /* now sleep for 1 second */
                                    nanosleep(&onesec, &tim);

                                    printf("**** [MASTER]: starting a new lifecycle ****\n");
                                }
                            }
                        }
                        deallocateFacilities(&exitCode);
                    /*}*/
                }
            }
        }
    }

    exit(exitCode);
}

/**
 * @brief Function that assigns the values ​​of the environment variables to the global variables defined above.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean assignEnvironmentVariables()
{
    /*
        We use strtol because it can detect error (due to overflow)
        while atol can't
    */

    printf("[MASTER]: loading environment...\n");
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
 * @brief Function that reads the file containing the configuration parameters to save them as environment variables.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean readConfigParameters()
{
    char *filename = "params_1.txt";
    FILE *fp = fopen(filename, "r");
    /* Reading line by line, max 128 bytes*/
    /*
        Array that will contain the lines read from the file:
        each "row" of the "matrix" will contain a different file line
    */
    /* Counter of the number of lines in the file*/
    int k = 0;
    char *aus = NULL;
    boolean ret = TRUE;

    printf("[MASTER]: reading configuration parameters...\n");

    aus = (char *)calloc(100, sizeof(char));
    if (aus == NULL)
        unsafeErrorPrint("[MASTER]: failed to allocate memory. Error: ", __LINE__);
    else
    {
        /* Handles any error in opening the file*/
        if (fp == NULL)
        {
            snprintf(aus, 99, "[MASTER]: could not open file %s", filename);
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
                unsafeErrorPrint("[MASTER]: failed to read cofiguration parameters. Error: ", __LINE__);
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
        if (aus != NULL)
            free(aus);
    }

    return ret;
}

/**
 * @brief Allocation of global structures.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean allocateGlobalStructures()
{
    /*
    regPtrs = (Register **)calloc(REG_PARTITION_COUNT, sizeof(Register *));
    TEST_MALLOC_ERROR(regPtrs, "[MASTER]: failed to allocate register paritions' pointers array. Error: ");*/

    /*
    regPartsIds = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(regPartsIds, "[MASTER]: failed to allocate register paritions' ids array. Error: ");*/

    
    tpList = (TPElement *)calloc(SO_NODES_NUM, sizeof(TPElement));
    TEST_MALLOC_ERROR(tpList, "[MASTER]: failed to allocate transaction pools list. Error: ");

    /*
    noReadersPartitions = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(noReadersPartitions, "[MASTER]: failed to allocate registers partitions' shared variables ids. Error: ");*/

    /*
    noReadersPartitionsPtrs = (int **)calloc(REG_PARTITION_COUNT, sizeof(int *));
    TEST_MALLOC_ERROR(noReadersPartitionsPtrs, "[MASTER]: failed to allocate registers partitions' shared variables pointers. Error: ");
    */
    return TRUE;
}

/**
 * @brief Ipc structures allocation.
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean initializeIPCFacilities()
{
    union semun arg;
    unsigned short aux[REG_PARTITION_COUNT] = {1, 1, 1};
    int res = -1;
    struct msqid_ds globalQueueStruct;
    int fd;

    /* Initialization of semaphores*/
    key_t key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during fair start semaphore creation. Error: ");

    fairStartSem = semget(key, 1, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(fairStartSem, "[MASTER]: semget failed during fair start semaphore creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "s;%d\n", fairStartSem);
    close(fd);

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during partitions writing semaphores creation. Error: ");
    wrPartSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(wrPartSem, "[MASTER]: semget failed during partitions writing semaphores creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "s;%d\n", wrPartSem);
    close(fd);

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during partitions reading semaphores creation. Error: ");
    rdPartSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(rdPartSem, "[MASTER]: semget failed during partitions reading semaphores creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "s;%d\n", rdPartSem);
    close(fd);

    key = ftok(SEMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during user list semaphore creation. Error: ");
    userListSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(userListSem, "[MASTER]: semget failed during user list semaphore creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "s;%d\n", userListSem);
    close(fd);

    key = ftok(SEMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during nodes list semaphore creation. Error: ");
    nodeListSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(nodeListSem, "[MASTER]: semget failed during nodes list semaphore creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "s;%d\n", nodeListSem);
    close(fd);

    key = ftok(SEMFILEPATH, PARTMUTEXSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during partitions mutex semaphores creation. Error: ");
    mutexPartSem = semget(key, 3, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(mutexPartSem, "[MASTER]: semget failed during partitions mutex semaphores creation. Error: ");
    
    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "s;%d\n", mutexPartSem);
    close(fd);

    key = ftok(SEMFILEPATH, NOALLTIMESNODESSEMSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during number of all times nodes' shared variable semaphore creation. Error: ");
    noAllTimesNodesSem = semget(key, 1, IPC_CREAT | IPC_EXCL | MASTERPERMITS);
    SEM_TEST_ERROR(noAllTimesNodesSem, "[MASTER]: semget failed during number of all times nodes' shared variable semaphore creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "s;%d\n", noAllTimesNodesSem);
    close(fd);

    /*
        Each process will subtract one by waiting on the sempahore
    */
    arg.val = SO_USERS_NUM + SO_NODES_NUM + 1;
    semctl(fairStartSem, 0, SETVAL, arg);
    SEMCTL_TEST_ERROR(fairStartSem, "[MASTER]: semctl failed while initializing fair start semaphore. Error: ");

    arg.array = aux;
    res = semctl(wrPartSem, 0, SETALL, arg);
    SEMCTL_TEST_ERROR(res, "[MASTER]: semctl failed while initializing register partitions writing semaphores. Error: ");

    res = semctl(rdPartSem, 0, SETALL, arg);
    SEMCTL_TEST_ERROR(res, "[MASTER]: semctl failed while initializing register partitions reading semaphores. Error: ");

    res = semctl(mutexPartSem, 0, SETALL, arg);
    SEMCTL_TEST_ERROR(res, "[MASTER]: semctl failed while initializing register partitions mutex semaphores. Error: ");

    arg.array = aux;
    res = semctl(userListSem, 0, SETALL, arg); /* mutex, read, write*/
    SEMCTL_TEST_ERROR(res, "[MASTER]: semctl failed while initializing users list semaphore. Error: ");

    res = semctl(nodeListSem, 0, SETALL, arg); /* mutex, read, write*/
    SEMCTL_TEST_ERROR(res, "[MASTER]: semctl failed while initializing nodes list semaphore. Error: ");

    arg.val = 1;
    semctl(noAllTimesNodesSem, 0, SETVAL, arg);
    SEMCTL_TEST_ERROR(noAllTimesNodesSem, "[MASTER]: semctl failed while initializing number of all times nodes' shared variable semaphore. Error: ");

    /* Creation of the processes global queue*/
    key = ftok(MSGFILEPATH, PROC_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during processes global queue creation. Error: ");
    procQueue = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    MSG_TEST_ERROR(procQueue, "[MASTER]: msgget failed during processes global queue creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "q;%d\n", procQueue);
    close(fd);
    
    printf("[MASTER]: setting processes global queue size...\n");
    if (msgctl(procQueue, IPC_STAT, &globalQueueStruct) == -1)
    {
        unsafeErrorPrint("[MASTER]: failed to retrive processes global queue size. Error: ", __LINE__);
        endOfSimulation(-1);
    }
    else
    {
        
        if (globalQueueStruct.msg_qbytes > (sizeof(ProcQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM)){
            globalQueueStruct.msg_qbytes = (sizeof(ProcQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM);
            if (msgctl(procQueue, IPC_SET, &globalQueueStruct) == -1)
            {
                unsafeErrorPrint("[MASTER]: failed to set processes global queue size. Error: ", __LINE__);
                endOfSimulation(-1);
            }
        }
        /*
        globalQueueStruct.msg_qbytes = (sizeof(ProcQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM);
        if (msgctl(procQueue, IPC_SET, &globalQueueStruct) == -1)
        {
            unsafeErrorPrint("[MASTER]: failed to set processes global queue size. Error: ", __LINE__);
            endOfSimulation(-1);
        }*/
    }

    /* Creation of the processes global queue*/
    key = ftok(MSGFILEPATH, NODE_CREATION_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during nodes global queue creation. Error: ");
    nodeCreationQueue = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    MSG_TEST_ERROR(nodeCreationQueue, "[MASTER]: msgget failed during nodes global queue creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "q;%d\n", nodeCreationQueue);
    close(fd);
    
    printf("[MASTER]: setting nodes global queue size...\n");
    if (msgctl(nodeCreationQueue, IPC_STAT, &globalQueueStruct) == -1)
    {
        unsafeErrorPrint("[MASTER]: failed to retrive nodes global queue size. Error: ", __LINE__);
        endOfSimulation(-1);
    }
    else
    {
        
        if (globalQueueStruct.msg_qbytes > (sizeof(NodeCreationQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM)){
            globalQueueStruct.msg_qbytes = (sizeof(NodeCreationQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM);
            if (msgctl(nodeCreationQueue, IPC_SET, &globalQueueStruct) == -1)
            {
                unsafeErrorPrint("[MASTER]: failed to set nodes global queue size. Error: ", __LINE__);
                endOfSimulation(-1);
            }
        }

        /*
        printf("Queue size: %ld\n", globalQueueStruct.msg_qbytes);
        globalQueueStruct.msg_qbytes = (sizeof(NodeCreationQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM);
        if (msgctl(nodeCreationQueue, IPC_SET, &globalQueueStruct) == -1)
        {
            unsafeErrorPrint("[MASTER]: failed to set nodes global queue size. Error: ", __LINE__);
            endOfSimulation(-1);
        }*/
    }

    key = ftok(MSGFILEPATH, TRANS_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during transactions global queue creation. Error: ");
    transQueue = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    MSG_TEST_ERROR(transQueue, "[MASTER]: msgget failed during transactions global queue creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "q;%d\n", transQueue);
    close(fd);
    
    printf("[MASTER]: setting transactions global queue size...\n");
    if (msgctl(transQueue, IPC_STAT, &globalQueueStruct) == -1)
    {
        unsafeErrorPrint("[MASTER]: failed to retrive transactions global queue size. Error: ", __LINE__);
        endOfSimulation(-1);
    }
    else
    {
        
        if (globalQueueStruct.msg_qbytes > (sizeof(TransQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM)){
            globalQueueStruct.msg_qbytes = (sizeof(TransQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM);
            if (msgctl(transQueue, IPC_SET, &globalQueueStruct) == -1)
            {
                unsafeErrorPrint("[MASTER]: failed to set transactions global queue size. Error: ", __LINE__);
                endOfSimulation(-1);
            }
        }

        /*
        globalQueueStruct.msg_qbytes = (sizeof(TransQueue) - sizeof(long)) * (SO_USERS_NUM + SO_NODES_NUM);
        if (msgctl(transQueue, IPC_SET, &globalQueueStruct) == -1)
        {
            unsafeErrorPrint("[MASTER]: failed to set transactions global queue size. Error: ", __LINE__);
            endOfSimulation(-1);
        }*/
    }

    /* Creation of register's partitions */
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during register parition one creation. Error: ");
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(regPartsIds[0], "[MASTER]: shmget failed during partition one creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", regPartsIds[0]);
    close(fd);

    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during register parition two creation. Error: ");
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(regPartsIds[1], "[MASTER]: shmget failed during partition two creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", regPartsIds[1]);
    close(fd);

    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during register parition three creation. Error: ");
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(regPartsIds[2], "[MASTER]: shmget failed during partition three creation. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", regPartsIds[2]);
    close(fd);

    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(regPtrs[0], "[MASTER]: failed to attach to partition one's memory segment. Error: ");
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(regPtrs[1], "[MASTER]: failed to attach to partition two's memory segment. Error: ");
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(regPtrs[2], "[MASTER]: failed to attach to partition three's memory segment. Error: ");
    printf("[MASTER]: initializing blocks...\n");
    regPtrs[0]->nBlocks = 0;
    regPtrs[1]->nBlocks = 0;
    regPtrs[2]->nBlocks = 0;
    printf("Blocks: %d %d %d\n", regPtrs[0]->nBlocks, regPtrs[1]->nBlocks, regPtrs[2]->nBlocks);

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during users list creation. Error: ");
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(usersListId, "[MASTER]: shmget failed during users list creation. Error: ");
    usersList = (ProcListElem *)shmat(usersListId, NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(usersList, "[MASTER]: failed to attach to users list's memory segment. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", usersListId);
    close(fd);

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during nodes list creation. Error: ");
    nodesListId = shmget(key, maxNumNode * sizeof(ProcListElem), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(nodesListId, "[MASTER]: shmget failed during nodes list creation. Error: ");
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(nodesList, "[MASTER]: failed to attach to nodes list's memory segment. Error: ");

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", nodesListId);
    close(fd);

    key = ftok(SHMFILEPATH, NOREADERSONESEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during parition one's shared variable creation. Error: ");
    noReadersPartitions[0] = shmget(key, sizeof(int), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noReadersPartitions[0], "[MASTER]: shmget failed during parition one's shared variable creation. Error: ");
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[0], "[MASTER]: failed to attach to parition one's shared variable segment. Error: ");
    /*
        At the beginning we have no processes reading from the register's paritions
    */
    *(noReadersPartitionsPtrs[0]) = 0;

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", noReadersPartitions[0]);
    close(fd);

    key = ftok(SHMFILEPATH, NOREADERSTWOSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during parition two's shared variable creation. Error: ");
    noReadersPartitions[1] = shmget(key, sizeof(int), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noReadersPartitions[1], "[MASTER]: shmget failed during parition two's shared variable creation. Error: ");
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[1], "[MASTER]: failed to attach to parition rwo's shared variable segment. Error: ");
    *(noReadersPartitionsPtrs[1]) = 0;

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", noReadersPartitions[1]);
    close(fd);

    key = ftok(SHMFILEPATH, NOREADERSTHREESEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during parition three's shared variable creation. Error: ");
    noReadersPartitions[2] = shmget(key, sizeof(int), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noReadersPartitions[2], "[MASTER]: shmget failed during parition three's shared variable creation. Error: ");
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, MASTERPERMITS);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[2], "[MASTER]: failed to attach to parition three's shared variable segment. Error: ");
    *(noReadersPartitionsPtrs[2]) = 0;

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", noReadersPartitions[2]);
    close(fd);

    key = ftok(SHMFILEPATH, NOUSRSEGRDERSSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during users list's shared variable creation. Error: ");
    noUserSegReaders = shmget(key, sizeof(int), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(key, "[MASTER]: shmget failed during users list's shared variable creation. Error: ");
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noUserSegReadersPtr, "[MASTER]: failed to attach to users list's shared variable segment. Error: ");
    /*
        At the beginning of the simulation there's no one
        reading from the user's list
    */
    *noUserSegReadersPtr = 0;

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", noUserSegReaders);
    close(fd);

    key = ftok(SHMFILEPATH, NONODESEGRDERSSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during nodes list's shared variable creation. Error: ");
    noNodeSegReaders = shmget(key, sizeof(int), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noNodeSegReaders, "[MASTER]: shmget failed during nodes list's shared variable creation. Error: ");
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noNodeSegReadersPtr, "[MASTER]: failed to attach to nodes list's shared variable segment. Error: ");
    *noNodeSegReadersPtr = 0;

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", noNodeSegReaders);
    close(fd);

    key = ftok(SHMFILEPATH, NOALLTIMESNODESSEED);
    FTOK_TEST_ERROR(key, "[MASTER]: ftok failed during number of all times nodes' shared variable creation. Error: ");
    noAllTimesNodes = shmget(key, sizeof(long), IPC_CREAT | MASTERPERMITS);
    SHM_TEST_ERROR(noAllTimesNodes, "[MASTER]: shmget failed during number of all times nodes' shared variable creation. Error: ");
    noAllTimesNodesPtr = (long *)shmat(noAllTimesNodes, NULL, 0);
    TEST_SHMAT_ERROR(noAllTimesNodesPtr, "[MASTER]: failed to attach to number of all times nodes' shared variable segment. Error: ");
    *noAllTimesNodesPtr = 0;

    fd = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
    dprintf(fd, "m;%d\n", noAllTimesNodes);
    close(fd);

    return TRUE;
}

/**
 * @brief Function to frees the space dedicated to the budget list p passed as argument.
 * @param p pointer to linked list budgetlist
 */
void budgetlist_free(budgetlist p)
{
    if (p == NULL)
        return;

    budgetlist_free(p->next);

    if (p != NULL)
        free(p);
}

/**
 * @brief Function that inserts in the global list bud_list the node passed as
 * argument in an ordered way (the list is ordered in ascending order).
 * We want to keep the list sorted to implement a more efficient
 * budget calculation.
 * @param pid pid of new process to insert
 * @param budget budget of new node to insert
 * @param p_type type of new node to insert (0 = user, 1 = node)
 */
void insert_ordered(pid_t pid, double budget, int p_type)
{
    budgetlist el;
    budgetlist prev;
    budgetlist new_el;

    new_el = (budgetlist)malloc(sizeof(*new_el));
    new_el->proc_pid = pid;
    new_el->budget = budget;
    new_el->p_type = p_type;

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

/**
 * @brief Function that searches in the gloabl list bud_list for an element with
 * proc_pid as the one passed as first argument; if it's found, upgrades its budget
 * adding the second argument, which is a positive or negative amount.
 * @param remove_pid pid of the item to be searched for in the budgetlist.
 * @param amount_changing positive or negative amount.
 */
int update_budget(pid_t remove_pid, double amount_changing)
{
    budgetlist new_el;
    budgetlist el;
    budgetlist prev;
    int found = 0;
    char *msg = NULL;
    msg = (char *)calloc(100, sizeof(char));

    /* check if budgetlist is NULL, if yes its an error */
    if (bud_list_head == NULL)
    {
        safeErrorPrint("[MASTER]: Error in function update_budget: NULL list passed to the function.", __LINE__);
        return -1;
    }

    if (bud_list_head->proc_pid == remove_pid)
    {
        /* if the node to update the budget is the first in the list,
         then we remove it from the list (afterwards we will reinsert it) */
        new_el = bud_list_head;
        bud_list_head = bud_list_head->next;
        bud_list_head->prev = NULL;
        found = 1;
    }
    else if (bud_list_tail->proc_pid == remove_pid)
    {
        /* if the node to update the budget is the last one in the list,
         then we remove it from the list (afterwards we will reinsert it) */
        new_el = bud_list_tail;
        bud_list_tail = bud_list_tail->prev;
        bud_list_tail->next = NULL;
        found = 1;
    }
    else
    {
        /* the node to update the budget is in the middle of the list,
         we have to search for it and remove it from the list (we will re-insert it later) */
        prev = bud_list_head;

        for (el = bud_list_head->next; el != NULL && !found; el = el->next)
        {
            if (el->proc_pid == remove_pid)
            {
                /* I found the node to update, I need to remove it */
                /* I have to change both the reference to next and prev */
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
        snprintf(msg, 99, "[MASTER]: Trying to update budget but no element in budgetlist with pid %5ld\n", (long)remove_pid);
        safeErrorPrint(msg, __LINE__);
        return -1;
    }

    /* update budget of removed element */
    new_el->budget += amount_changing; /* amount_changing is a positive or negative value */

    insert_ordered(new_el->proc_pid, new_el->budget, new_el->p_type);
    free(msg);
    return 0;
}

/**
 * @param sig signal that fired the handler
 */
void tmpHandler(int sig)
{
    printf("ONE LAST KEKW OF PID %ld\n", (long)getpid());
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

/**
 * @brief Function that ends the execution of the user; this can happen in three different ways,
 * rappresented by the values that the parameter might assume.
 * @param sig the parameters value are: 0 -> only end of execution; -1 -> end of execution and deallocation (called from error);
 * SIGUSR1 -> end of execution and deallocation (called by signal from master)
 */
void endOfSimulation(int sig)
{
    /*
     *Notify children
     *sends termination signal to all the processes
     *that are part of the master's group (in this case we
     *reach every children with just one system call).
     *how to check if everyone was signaled (it returns true even if
     *only one signal was sent)
     */
    int i = 0, ret = -1;
    char *terminationMessage = (char *)calloc(100, sizeof(char));
    char *aus = (char *)calloc(200, sizeof(char));
    /*
     * Contains an exit, because it could be invoked in such a way
     * asynchronous during life cycle execution
     * in that case execution should terminate after execution
     * of the handler without running the rest of the lifecycle code
     * which could also cause errors
     * for referencing areas of memory that are no longer allocated
     * in case of error we don't stop the whole procedure
     * but we signal it by setting the exit code to EXIT_FAILURE
     */
    int exitCode = EXIT_SUCCESS;
    boolean done = FALSE;

    if (terminationMessage == NULL || aus == NULL || getpid() != masterPid)
        safeErrorPrint("[MASTER]: failed to alloacate memory. Error: ", __LINE__);
    else
    {
        signal(SIGUSR1, SIG_IGN);

        printf("[MASTER]: received signal %d\n", sig);

        printf("[MASTER]: pid %5ld\n", masterPid);

        printf(
            "[MASTER]: trying to terminate simulation...\n");
        /* error check*/
        fflush(stdout);
        if (noEffectiveNodes > 0 || noEffectiveUsers > 0)
        {
            /*
             *   There are still active children that need
             *    to be notified the end of simulation
             */
            for (i = 0; i < NO_ATTEMPS_TERM && !done; i++)
            {
                if (kill(0, SIGUSR1) == -1)
                {
                    safeErrorPrint("[MASTER]: failed to signal children for end of simulation. Error: ", __LINE__);
                }
                else
                {
                    printf(
                        "[MASTER]: end of simulation notified successfully to children.\n");
                    done = TRUE;
                }
            }
        }
        else
            done = TRUE;

        if (done)
        {
            printf(
                "[MASTER]: waiting for children to terminate...\n");

            while (wait(NULL) != -1)
                ;
            if (errno == ECHILD)
            {
                /*
                 * print report: we use the write system call: slower, but async-signal-safe
                 * Termination message composition
                 * we use only one system call for performances' sake.
                 * termination reason
                 */
                printf(
                    "[MASTER]: simulation terminated successfully. Printing report...\n");

                /* Users and nodes budgets */
                printRemainedTransactions();

                /* processes terminated before end of simulation*/
                printf("Processes terminated before end of simulation: %ld\n", noTerminatedUsers);

                /* Blocks in register*/
                printf("There are %d blocks in the register.\n",
                       regPtrs[0]->nBlocks + regPtrs[1]->nBlocks + regPtrs[2]->nBlocks);

                if (sig == SIGALRM)
                    printf("Termination reason: end of simulation.\n");
                else if (sig == SIGUSR1)
                    printf("Termination reason: register is full.\n");
                else if (sig == -2)
                    printf("Termination reason: no more users alive.\n");
                else if (sig == -3)
                    printf("Termination reason: no more nodes alive.\n");
                else if (sig == -1)
                    printf("Termination reason: critical error.\n");

                printf(
                    "[MASTER]: report printed successfully. Deallocating IPC facilities...\n");
                /* deallocate facilities*/
                deallocateFacilities(&exitCode);
                done = TRUE;
            }
            else
            {
                safeErrorPrint("[MASTER]: an error occurred while waiting for children. Error: ", __LINE__);
            }
            printf(
                "[MASTER]: simulation terminated successfully!\n");

            simTerminated = TRUE;
        }
        else
        {
            deallocateFacilities(&exitCode);
            exitCode = EXIT_FAILURE;
            printf(
                "[MASTER]: failed to terminate children. IPC facilties will be deallocated anyway.\n");
        }
    }
    exit(exitCode);
}

/**
 * @brief Function that deallocates the IPC facilities for the user.
 * @param exitcode indicates whether the simulation ends successfully or not
 * @return Returns TRUE if successfull, FALSE in case an error occurrs.
 */
boolean deallocateFacilities(int *exitCode)
{
    /*
     * Obviously the child processes will have to disconnect
     * from the segments and close their references to the queue
     * and at the semaphores before ending
     *
     * Precondition: all child processes have disconnected from memory segments
     * In general, all children have closed their references to IPC facilities
     * we are sure because this procedure is called only after waiting for the termination of each child
     * The idea is to implement the elimination of each facility independently
     * from the others (i.e. the elimination of the n + 1 is carried out even if that of the nth has failed)
     * but not to implement a mechanism whereby repeated attempts are made to eliminate
     * the nth facility if one of the system calls involved fails
    */

    /*char *printMsg;*/
    int msgLength = 0;
    int i = 0;

    /*printMsg = (char *)calloc(200, sizeof(char));*/

    /* Deallocating register's partitions*/
    printf(
        "[MASTER]: deallocating register's paritions...\n");

    /*
    if (regPtrs != NULL)
    {*/
        for (i = 0; i < REG_PARTITION_COUNT; i++)
        {
            /*
             * We don't want to display any error if the shared memory
             *  segment we are trying to detach doesn't exist:
             *   since no one can remove it, this case can only happen
             *    when the IPC allocation procedure fails
             */
            if (regPtrs[i] != NULL && shmdt(regPtrs[i]) == -1 && errno != 0 && errno != EINVAL)
            {
                if (errno != 0 && errno != EINVAL)
                {
                    printf("[MASTER]: failed to detach from register's partition number %d.\n",
                           (i + 1));
                    *exitCode = EXIT_FAILURE;
                }
            }
            else
            {
                if (shmctl(regPartsIds[i], IPC_RMID, NULL) == -1 && errno != 0 && errno != EINVAL)
                {
                    if (errno != 0 && errno != EINVAL)
                    {
                        printf(
                            "[MASTER]: failed to remove register's partition number %d.\n",
                            (i + 1));

                        *exitCode = EXIT_FAILURE;
                    }
                }
                else
                {
                    printf("[MASTER]: register's partition number %d removed successfully.\n",
                           (i + 1));
                }
            }
        }
        /*
        free(regPtrs);
    }*/

    /*
    if (regPartsIds != NULL)
        free(regPartsIds);*/

    /* Users list deallocation*/
    printf(
        "[MASTER]: deallocating users' list segment...\n");
    if (usersList != NULL && shmdt(usersList) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            safeErrorPrint("[MASTER]: failed to detach from users' list segment. Error: ", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(usersListId, IPC_RMID, NULL) == -1)
        {
            if (errno != 0 && errno != EAGAIN)
            {
                safeErrorPrint("[MASTER]: failed to remove users' list segment. Error: ", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            printf(
                "[MASTER]: users' list memory segment successfully removed.\n");
        }
    }

    /* Nodes list deallocation*/
    printf(
        "[MASTER]: deallocating nodes' list segment...\n");
    if (nodesList != NULL && shmdt(nodesList) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            safeErrorPrint("[MASTER]: failed to detach from nodes' list segment. Error: ", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(nodesListId, IPC_RMID, NULL) == -1)
        {
            if (errno != 0 && errno != EINVAL)
            {
                safeErrorPrint("[MASTER]: failed to remove nodes' list segment. Error: ", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            printf(
                "[MASTER]: nodes' list memory segment successfully removed.\n");
        }
    }

    /* Partitions' shared variable deallocation*/
    /*
    if (noReadersPartitionsPtrs != NULL)
    {*/
        for (i = 0; i < REG_PARTITION_COUNT; i++)
        {
            if (noReadersPartitionsPtrs[i] != NULL && shmdt(noReadersPartitionsPtrs[i]) == -1)
            {
                if (errno != 0 && errno != EINVAL)
                {
                    printf("[MASTER]: failed to detach from partition number %d shared variable segment.\n",
                           (i + 1));
                    *exitCode = EXIT_FAILURE;
                }
            }
            else
            {
                if (shmctl(noReadersPartitions[i], IPC_RMID, NULL) == -1)
                {
                    if (errno != 0 && errno != EINVAL)
                    {
                        printf("[MASTER]: failed to remove partition number %d shared variable segment.\n",
                               (i + 1));

                        *exitCode = EXIT_FAILURE;
                    }
                }
                else
                {
                    printf("[MASTER]: register's partition number %d shared variable segment removed successfully.\n",
                           (i + 1));
                }
            }
        }

        /*
        if (noReadersPartitions != NULL)
            free(noReadersPartitions);*/
    /*}*/

    /* Transaction pools list deallocation*/
    printf(
        "[MASTER]: deallocating transaction pools...\n");
    if (tpList != NULL)
    {
        for (i = 0; i < tplLength; i++)
        {
            if (msgctl(tpList[i].msgQId, IPC_RMID, NULL) == -1)
            {
                if (errno != 0 && errno != EINVAL)
                {
                    printf("[MASTER]: failed to remove transaction pool of process %ld.\n",
                           (long)tpList[i].procId);

                    *exitCode = EXIT_FAILURE;
                }
            }
            else
            {
                printf("[MASTER]: transaction pool of node of PID %ld successfully removed.\n",
                       tpList[i].procId);
            }
        }

        free(tpList);
    }

    /* Global queue deallocation*/
    printf("[MASTER]: deallocating global processes queue...\n");
    if (msgctl(procQueue, IPC_RMID, NULL) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            perror("[MASTER]: failed to remove global processes queue: ");
            /*
                msgLength = snprintf(printMsg, 199, "[MASTER]: failed to remove global processes queue.\n");
            write(STDERR_FILENO, printMsg, msgLength);*/
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf("[MASTER]: global processes queue successfully removed.\n");
    }

    printf("[MASTER]: deallocating global nodes queue...\n");
    if (msgctl(nodeCreationQueue, IPC_RMID, NULL) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            perror("[MASTER]: failed to remove global nodes queue: ");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf("[MASTER]: global nodes queue successfully removed.\n");
    }

    printf("[MASTER]: deallocating global transactions queue...\n");
    if (msgctl(transQueue, IPC_RMID, NULL) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            perror("[MASTER]: failed to remove global transactions queue: ");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf("[MASTER]: global transactions queue successfully removed.\n");
    }

    /* Writing Semaphores deallocation*/
    printf(
        "[MASTER]: deallocating writing semaphores...\n");
    if (semctl(wrPartSem, 0, IPC_RMID) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            printf("[MASTER]: failed to remove partions' writing semaphores.\n");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "[MASTER]: Writing Semaphores successfully removed.\n");
    }

    /* Reading Semaphores deallocation*/
    printf(
        "[MASTER]: deallocating reading semaphores...\n");
    if (semctl(rdPartSem, 0, IPC_RMID) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            printf("[MASTER]: failed to remove partions' reading semaphores.\n");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "[MASTER]: Reading Semaphores successfully removed.\n");
    }

    /* Fair start semaphore deallocation*/
    printf(
        "[MASTER]: deallocating fair start semaphores...\n");
    if (semctl(fairStartSem, 0, IPC_RMID) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            printf("[MASTER]: failed to remove fair start semaphore.\n");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "[MASTER]: Fair start semaphore successfully removed.\n");
    }

    /* Users' list semaphores deallocation*/
    printf(
        "[MASTER]: deallocating users' list semaphores...\n");
    if (semctl(userListSem, 0, IPC_RMID) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            printf("[MASTER]: failed to remove users' list semaphores.\n");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "[MASTER]: users' list semaphores successfully removed.\n");
    }

    /* Register's paritions mutex semaphores deallocation*/
    printf(
        "[MASTER]: deallocating register's paritions mutex semaphores...\n");
    if (semctl(mutexPartSem, 0, IPC_RMID) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            printf("[MASTER]: failed to remove register's paritions mutex semaphores.\n");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "[MASTER]: register's paritions mutex semaphores successfully removed.\n");
    }

    printf(
        "[MASTER]: deallocating user list's shared variable...\n");
    if (noUserSegReadersPtr != NULL && shmdt(noUserSegReadersPtr) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            safeErrorPrint("[MASTER]: failed to detach from user list's shared variable. Error: ", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(noUserSegReaders, IPC_RMID, NULL) == -1)
        {
            if (errno != 0 && errno != EINVAL)
            {
                safeErrorPrint("[MASTER]: failed to remove user list's shared variable. Error: ", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            printf(
                "[MASTER]: user list's shared variable successfully removed.\n");
        }
    }

    printf(
        "[MASTER]: deallocating nodes list's semaphores...\n");
    if (semctl(nodeListSem, 0, IPC_RMID) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            printf("[MASTER]: failed to remove nodes list's semaphores.\n");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "[MASTER]: nodes list's semaphores successfully removed.\n");
    }

    printf(
        "[MASTER]: deallocating node list's shared variable...\n");
    if (noNodeSegReadersPtr != NULL && shmdt(noNodeSegReadersPtr) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            safeErrorPrint("[MASTER]: failed to detach from node list's shared variable. Error: ", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(noNodeSegReaders, IPC_RMID, NULL) == -1)
        {
            if (errno != 0 && errno != EINVAL)
            {
                safeErrorPrint("[MASTER]: failed to remove node list's shared variable. Error: ", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            printf(
                "[MASTER]: node list's shared variable successfully removed.\n");
        }
    }

    printf(
        "[MASTER]: deallocating number of all times nodes' shared variable...\n");
    if (noAllTimesNodesPtr != NULL && shmdt(noAllTimesNodesPtr) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            safeErrorPrint("[MASTER]: failed to detach from number of all times nodes' shared variable. Error: ", __LINE__);
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        if (shmctl(noAllTimesNodes, IPC_RMID, NULL) == -1)
        {
            if (errno != 0 && errno != EINVAL)
            {
                safeErrorPrint("[MASTER]: failed to remove number of all times nodes' shared variable. Error: ", __LINE__);
                *exitCode = EXIT_FAILURE;
            }
        }
        else
        {
            printf(
                "[MASTER]: number of all times nodes' shared variable successfully removed.\n");
        }
    }

    /* Number of all times nodes' shared variable semaphore deallocation */
    printf(
        "[MASTER]: deallocating number of all times nodes' shared variable semaphore...\n");
    if (semctl(noAllTimesNodesSem, 0, IPC_RMID) == -1)
    {
        if (errno != 0 && errno != EINVAL)
        {
            printf("[MASTER]: failed to remove number of all times nodes' shared variable semaphore.");
            *exitCode = EXIT_FAILURE;
        }
    }
    else
    {
        printf(
            "[MASTER]: Number of all times nodes' shared variable semaphore successfully removed.\n");
    }

    /* Releasing local variables' memory*/
    /*if (printMsg != NULL)
        free(printMsg);*/

    return TRUE;
}

/**
 * @brief Function that checks for node creation requests.
 */
void checkNodeCreationRequests()
{
    NodeCreationQueue ausNode;
    TransQueue ausTrans;
    ProcQueue ausProc;
    pid_t procPid = -1;
    int tpId = -1, j = 0;
    pid_t currPid = getpid();
    MsgTP firstTrans;
    budgetlist new_el;
    struct sembuf sops[3];
    long childPid = -1;
    long indexNodesList = 0;
    int attempts = 0;
    int msg_length;
    char *printMsg;
    union semun arg;
    struct msqid_ds tpStruct;
    int fdReport; /* ONLY FOR DEBUG PURPOSE */

    printMsg = (char *)calloc(200, sizeof(char));

    while (attempts < NO_ATTEMPTS_NEW_NODE_REQUESTS && msgrcv(nodeCreationQueue, &ausNode, sizeof(NodeCreationQueue) - sizeof(long), currPid, IPC_NOWAIT) != -1)
    {
        /* ONLY FOR DEBUG PURPOSE */
        /*fdReport = open("master_msgrcv_content.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
        dprintf(fdReport, "MASTER: in new node requests check msgContent is %d\n", ausNode.msgContent);
        close(fdReport);*/
        
        /* Increasing the number of attempts to check for new node requests*/
        attempts++;

        if (ausNode.msgContent == NEWNODE)
        {
            /* ONLY FOR DEBUG PURPOSE */
            /*
            fdReport = open("node_creation_report.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
            dprintf(fdReport, "MASTER: handled creation of new node request\n");
            close(fdReport);*/

            printf("[MASTER]: creating new node...\n");
            /* entering critical section for number of all times nodes' shared variable */
            sops[0].sem_num = 0;
            sops[0].sem_op = -1;
            if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
            {
                safeErrorPrint("[MASTER]: failed to reserve number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                endOfSimulation(-1);
            }

            if ((*noAllTimesNodesPtr) + 1 < maxNumNode)
            {
                /* Saving the old number of all times nodes to use it as index later */
                indexNodesList = (*noAllTimesNodesPtr);

                /* Incrementing number of effective and all times node processes */
                noEffectiveNodes++;
                (*noAllTimesNodesPtr)++;

                /* exiting the critical section entered before the if statement */
                sops[0].sem_num = 0;
                sops[0].sem_op = 1;
                if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                {
                    safeErrorPrint("[MASTER]: failed to release number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                    endOfSimulation(-1);
                }

                /* Set the traffic light to make the new node wait to start */
                arg.val = 1;
                semctl(fairStartSem, 0, SETVAL, arg);
                if (fairStartSem == -1)
                    safeErrorPrint("[MASTER]: semctl failed while initializing fair start semaphore for new node creation. Error: ", __LINE__);
                else
                {
                    procPid = fork();
                    if (procPid == 0)
                    {
                        signal(SIGALRM, SIG_IGN);
                        signal(SIGUSR1, tmpHandler);

                        sops[0].sem_op = 0;
                        sops[0].sem_num = 0;
                        sops[0].sem_flg = 0;
                        if (semop(fairStartSem, &sops[0], 1) == -1)
                        {
                            snprintf(printMsg, 199, "[NODE %5ld]: failed to wait for zero on start semaphore. Error: ", (long)getpid());
                            safeErrorPrint(printMsg, __LINE__);
                            exit(EXIT_FAILURE);
                        }
                        else
                        {
                            printf("[NODE]: I'm a new node, my pid is %ld\n", (long)getpid());
                            if (execle("node.out", "node", "ADDITIONAL", NULL, environ) == -1)
                                safeErrorPrint("[MASTER]: failed to load node's code. Error: ", __LINE__);
                        }
                    }
                    else if (procPid > 0)
                    {
                        /*fdReport = open("processes_killer/processes_created.txt", O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                        dprintf(fdReport, "%d\n", procPid);
                        close(fdReport);*/

                        tpId = msgget(ftok(MSGFILEPATH, (int)procPid), IPC_CREAT | IPC_EXCL | MASTERPERMITS);
                        if (tpId == -1)
                        {
                            safeErrorPrint("[MASTER]: failed to create additional node's transaction pool. Error: ", __LINE__);
                            
                            /* Kill the node processes, it won't start */
                            kill(procPid, SIGUSR1);

                            /* Reinserting the message that we have consumed from the global queue */
                            if (msgsnd(nodeCreationQueue, &ausNode, sizeof(NodeCreationQueue) - sizeof(long), 0) == -1)
                            {
                                /* This is necessary, otherwise the message won't be reinserted in queue and lost forever */
                                safeErrorPrint("[MASTER]: failed to reinsert the message read from the global queue of new node creation requests. Error: ", __LINE__);
                                /* Dovremmo segnalare all'utente il fallimento della transazione o terminare la simulazione ??? */
                                endOfSimulation(-1);
                            }
                            else
                            {
                                /* entering critical section for number of all times nodes' shared variable */
                                sops[0].sem_num = 0;
                                sops[0].sem_op = -1;
                                if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to reserve number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                /* decrementing the number of effective and all times node processes */
                                noEffectiveNodes--;
                                (*noAllTimesNodesPtr)--;

                                /* exiting the critical section entered before */
                                sops[0].sem_num = 0;
                                sops[0].sem_op = 1;
                                if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to release number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }
                            }
                        }
                        else
                        {
                            /*
                            fdReport = open(IPCREMOVERFILEPATH, O_CREAT | O_APPEND | O_WRONLY,  S_IRWXU | S_IRWXG | S_IRWXO);
                            dprintf(fdReport, "q;%d\n", tpId);
                            close(fdReport);*/

                            sops[0].sem_flg = 0;
                            sops[0].sem_num = 1;
                            sops[0].sem_op = -1;
                            sops[1].sem_flg = 0;
                            sops[1].sem_num = 2;
                            sops[1].sem_op = -1;
                            if (semop(nodeListSem, sops, 2) == -1)
                            {
                                safeErrorPrint("[MASTER]: failed to reserve nodes' list semphore. Error: ", __LINE__);
                                endOfSimulation(-1);
                            }

                            printf("Lista nodi nuovo nodo\n");
                            nodesList[indexNodesList].procId = (long)procPid;
                            nodesList[indexNodesList].procState = ACTIVE;
                            printf("FIne Lista nodi nuovo nodo\n");

                            sops[0].sem_flg = 0;
                            sops[0].sem_num = 2;
                            sops[0].sem_op = 1;
                            sops[1].sem_flg = 0;
                            sops[1].sem_num = 1;
                            sops[1].sem_op = 1;

                            if (semop(nodeListSem, sops, 2) == -1)
                            {
                                safeErrorPrint("[MASTER]: failed to release nodes' list semphore. Error: ", __LINE__);
                                endOfSimulation(-1);
                            }

                            /* Adding new node to budgetlist */
                            /*printf("Iniziato budget list nuovo nodo");
                            new_el = malloc(sizeof(*new_el));
                            printf("Finito budget list nuovo nodo");
                            new_el->proc_pid = procPid;
                            new_el->budget = 0;
                            new_el->p_type = 1;*/
                            insert_ordered(procPid, 0, 1);

                            /* add a new entry to the tpList array */
                            tplLength++;
                            tpList = (TPElement *)realloc(tpList, sizeof(TPElement) * tplLength);
                            /* Initialize messages queue for transactions pools */
                            tpList[tplLength - 1].procId = (long)procPid;
                            tpList[tplLength - 1].msgQId = tpId;

                            if (tpList[tplLength - 1].msgQId == -1)
                            {
                                safeErrorPrint("[MASTER]: failed to create the message queue for the transaction pool of the new node process. Error: ", __LINE__);
                                endOfSimulation(-1);
                            }

                            if (msgctl(tpList[tplLength - 1].msgQId, IPC_STAT, &tpStruct) == -1)
                            {
                                unsafeErrorPrint("[MASTER]: failed to retrive new node transaction pool's size. Error: ", __LINE__);
                                endOfSimulation(-1);
                            }
                            else
                            {
                                /*
                                 *   tpStruct.msg_qbytes was set to the maximum possible value
                                 *    during the msgget
                                 */

                                printf("Queue size: %ld\n", tpStruct.msg_qbytes);
                                /*
                                tpStruct.msg_qbytes = (sizeof(MsgTP) - sizeof(long)) * SO_TP_SIZE;
                                if (msgctl(tpList[tplLength - 1].msgQId, IPC_SET, &tpStruct) == -1)
                                {
                                    unsafeErrorPrint("[MASTER]: failed to set new node transaction pool's size. Error", __LINE__);
                                    endOfSimulation(-1);
                                }*/
                                
                                
                                if (tpStruct.msg_qbytes > (sizeof(MsgTP) - sizeof(long)) * SO_TP_SIZE)
                                {
                                    tpStruct.msg_qbytes = (sizeof(MsgTP) - sizeof(long)) * SO_TP_SIZE;
                                    if (msgctl(tpList[tplLength - 1].msgQId, IPC_SET, &tpStruct) == -1)
                                    {
                                        unsafeErrorPrint("[MASTER]: failed to set new node transaction pool's size. Error", __LINE__);
                                        endOfSimulation(-1);
                                    }
                                    printf("Master: impostata nuova dimensione coda per nuovo nodo.\n");
                                }

                                
                                /*
                                 *  If the size is larger than the maximum size then
                                 *   we do not make any changes
                                 */
                            }

                            printf("Coda nuovo nodo ok\n");
                            firstTrans.mtype = (long)procPid;
                            firstTrans.transaction = ausNode.transaction;

                            if (msgsnd(tpId, &(firstTrans.transaction), sizeof(MsgTP) - sizeof(long), 0) == -1)
                            {
                                safeErrorPrint("[MASTER]: failed to send transaction to new node's transaction pool . Error: ", __LINE__);

                                /* informing sender of transaction that it wasn't processed */
                                ausTrans.mtype = firstTrans.transaction.sender;
                                ausTrans.msgContent = FAILEDTRANS;
                                ausTrans.transaction = firstTrans.transaction;

                                if (msgsnd(transQueue, &ausTrans, sizeof(TransQueue) - sizeof(long), 0) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", __LINE__);
                                }
                            }
                            else
                            {
                                /* friend node's generation */
                                ausProc.mtype = (long)procPid;
                                ausProc.msgContent = FRIENDINIT;
                                estrai(indexNodesList);
                                for (j = 0; j < SO_FRIENDS_NUM; j++)
                                {
                                    ausProc.procPid = nodesList[extractedFriendsIndex[j]].procId;
                                    if (msgsnd(procQueue, &ausProc, sizeof(ProcQueue) - sizeof(long), 0) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to send a friend to new node. Error: ", __LINE__);
                                        /* informing sender of transaction that it wasn't processed */
                                        ausTrans.mtype = firstTrans.transaction.sender;
                                        ausTrans.msgContent = FAILEDTRANS;
                                        ausTrans.transaction = firstTrans.transaction;

                                        if (msgsnd(transQueue, &ausTrans, sizeof(TransQueue) - sizeof(long), 0) == -1)
                                        {
                                            safeErrorPrint("[MASTER]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", __LINE__);
                                        }
                                    }
                                }

                                sops[0].sem_flg = 0;
                                sops[0].sem_num = 1;
                                sops[0].sem_op = -1;
                                sops[1].sem_flg = 0;
                                sops[1].sem_num = 0;
                                sops[1].sem_op = -1;
                                if (semop(nodeListSem, sops, 2) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to reserve nodes' list read/mutex semphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                (*noNodeSegReadersPtr)++;
                                if (*noNodeSegReadersPtr == 1)
                                {
                                    sops[2].sem_flg = 0;
                                    sops[2].sem_num = 2;
                                    sops[2].sem_op = -1;
                                    if (semop(nodeListSem, &sops[2], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to reserve nodes' list write semphore. Error: ", __LINE__);
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
                                    safeErrorPrint("[MASTER]: failed to release nodes' list mutex/read semphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                ausNode.procPid = procPid;
                                ausNode.msgContent = NEWFRIEND;
                                estrai(noEffectiveNodes);
                                for (j = 0; j < SO_FRIENDS_NUM; j++)
                                {
                                    ausNode.mtype = nodesList[extractedFriendsIndex[j]].procId;
                                    if (msgsnd(nodeCreationQueue, &ausNode, sizeof(NodeCreationQueue) - sizeof(long), 0) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to ask a node to add the new process to its friends' list. Error: ", __LINE__);
                                    }
                                }

                                sops[0].sem_flg = 0;
                                sops[0].sem_num = 0;
                                sops[0].sem_op = -1;
                                if (semop(nodeListSem, sops, 1) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to reserve nodes' list mutex semphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                (*noNodeSegReadersPtr)--;
                                if (*noNodeSegReadersPtr == 0)
                                {
                                    sops[2].sem_flg = 0;
                                    sops[2].sem_num = 2;
                                    sops[2].sem_op = 1;
                                    if (semop(nodeListSem, &sops[2], 1) == -1)
                                    {
                                        safeErrorPrint("[MASTER]: failed to release nodes' list write semphore. Error: ", __LINE__);
                                        endOfSimulation(-1);
                                    }
                                }

                                sops[0].sem_flg = 0;
                                sops[0].sem_num = 0;
                                sops[0].sem_op = 1;
                                if (semop(nodeListSem, sops, 1) == -1)
                                {
                                    safeErrorPrint("[MASTER]: failed to release nodes' list mutex semphore. Error: ", __LINE__);
                                    endOfSimulation(-1);
                                }

                                /* faccio partire il nodo appena creato */
                                sops[0].sem_op = -1;
                                sops[0].sem_num = 0;
                                sops[0].sem_flg = 0;
                                semop(fairStartSem, &sops[0], 1);

                                printf("Ciao\n");
                                msg_length = snprintf(printMsg, 199, "[MASTER]: created new node on request with pid %5ld\n", (long)procPid);
                                write(STDOUT_FILENO, printMsg, msg_length);
                            }
                        }

                        if (printMsg != NULL)
                            free(printMsg);
                    }
                    else
                    {
                        safeErrorPrint("[MASTER]: no more resources for new node. Simulation will be terminated.", __LINE__);
                        endOfSimulation(-1);
                    }
                }
            }
            else
            {
                /* exiting the critical section entered before the if statement */
                sops[0].sem_num = 0;
                sops[0].sem_op = 1;
                if (semop(noAllTimesNodesSem, &sops[0], 1) == -1)
                {
                    safeErrorPrint("[MASTER]: failed to release number of all times nodes' shared variable semaphore. Error: ", __LINE__);
                    endOfSimulation(-1);
                }

                safeErrorPrint("[MASTER]: no space left for storing new node information. Simulation will be terminated.", __LINE__);
                endOfSimulation(-1);
            }
        }
        else
        {
            /* Reinserting the message that we have consumed from the global queue */
            if (msgsnd(nodeCreationQueue, &ausNode, sizeof(NodeCreationQueue) - sizeof(long), 0) == -1)
            {
                /* This is necessary, otherwise the message won't be reinserted in queue and lost forever */
                safeErrorPrint("[MASTER]: failed to reinsert the message read from the global queue while checking for new node creation requests. Error: ", __LINE__);
                endOfSimulation(-1);
            }
        }
    }

    if (errno != 0 && errno != ENOMSG)
        safeErrorPrint("[MASTER]: failed to check for node creation requests on global queue. Error: ", __LINE__);
    else if (attempts == 0)
        printf(
            "[MASTER]: no node creation requests to be served.\n");
    else if (attempts > 0)
        printf(
            "[MASTER]: no more node creation requests to be served.\n");
}

/**
 * @brief Function that print remained transactions.
 */
void printRemainedTransactions()
{
    int i = 0;
    int tpId = -1;
    MsgTP aus;
    boolean error = FALSE;
    int cnt = 0;

    for (i = 0; i < tplLength && !error; i++)
    {
        printf("[MASTER]: printing remaining transactions of Node of pid %ld...\n", (long)tpList[i].procId);
        tpId = tpList[i].msgQId;
        cnt = 0;
        while (msgrcv(tpId, &aus, sizeof(aus) - sizeof(long), 0, IPC_NOWAIT) != -1)
        {
            printf("[MASTER]:  - Timestamp: %ld\n [MASTER]:  - Sender: %ld\n [MASTER]:  - Receiver: %ld\n [MASTER]:  -  Amount sent: %f\n [MASTER]:  - Reward: %f\n",
                   aus.transaction.timestamp.tv_nsec,
                   aus.transaction.sender,
                   aus.transaction.receiver,
                   aus.transaction.amountSend,
                   aus.transaction.reward);
            cnt++;
        }

        if (errno != 0 && errno != ENOMSG)
        {
            unsafeErrorPrint("[MASTER]: an error occurred while printing remaining transactions. Error: ", __LINE__);
            error = TRUE;
        }
        else if (cnt == 0)
            printf("[MASTER]: no transactions left.\n");
    }
}

/**
 * @brief Upload the location to extractedFriendsIndex in friends' nodesList
 * (doing so extracts friends)
 * @param k index of the process that cannot be extracted, i.e. the one calling the function.
 */
void estrai(int k)
{
    int x, count, n, i = 0;

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

/**
 * @brief Function that catches any segmentation fault error during execution and
 * avoids brutal termination.
 *
 * @param sig signal that fired the handler
 */
void segmentationFaultHandler(int sig)
{
    if(segFaultHappened == 0)
        segFaultHappened++;
    else if(segFaultHappened == 1)
    {
        /* 
            abbiamo ottenuto due segmentation fault di fila, l'errore potrebbe essere grave.
            Proviamo a terminare la simulazione con la funzione dedicata.
        */
        segFaultHappened++;
        endOfSimulation(-1);
    }
    else 
    {
        /* 
            sono avvenuti più segmentation fault, non possiamo terminare usando la 
            funzione dedicata. Bisogna terminare brutalmente.
        */
        kill(0, SIGUSR1); /* l'idea sarebbe di far terminare tutti i figli anche, ma non so se va bene così */
        exit(EXIT_FAILURE);
    }

    

    dprintf(STDERR_FILENO, "[MASTER]: a segmentation fault error happened. Terminating...\n");

    if (!simTerminated)
        endOfSimulation(-1);
    else
        exit(EXIT_FAILURE);    
}

#include "node.h"

/*** GLOBAL VARIABLES FOR IPC OBJECTS ***/
#pragma region GLOBAL VARIABLES FOR IPC OBJECTS
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

/* Id of the shared memory segment that contains the nodes list */
int nodesListId = -1;

/* Pointer to the nodes list */
ProcListElem *nodesList = NULL;

/* Id of the global message queues where users, nodes and master communicate */
int nodeCreationQueue = -1, procQueue = -1, transQueue = -1;

/* Id of the set that contains a semaphore used to wait for the simulation to start */
int fairStartSem = -1;

/* Id of the set that contains the three semaphores used to write on the register's partitions */
int wrPartSem = -1;

/* Id of the set that contains the three semaphores used to read on the register's partitions */
int rdPartSem = -1;

/* Id of the set that contains the semaphores (mutex = 0, read = 1, write = 2) used to read and write nodes list */
int nodeListSem = -1;

/* Id of the shared memory segment that contains the variable used to syncronize readers and writers access to nodes list */
int noNodeSegReaders = -1;

/* Pointer to the variable that counts the number of readers, used to syncronize readers and writers access to nodes list */
int *noNodeSegReadersPtr = NULL;

/* Id of the Transaction Pool of the node */
int tpId = -1;

#pragma endregion
/*** END GLOBAL VARIABLES FOR IPC OBJECTS ***/

/*** GLOBAL VARIABLES ***/
#pragma region GLOBAL VARIABLES

/* List of node friends */
pid_t *friends_node;
/* variable that keeps friends_node list length */
int friends_node_len = 0;

/* current node's pid */
long my_pid; 

/*** Configuration parameters ***/
/* Number of user processes */
long SO_USERS_NUM;
/* Number of node processes */
long SO_NODES_NUM;
/* Percentage of node's reward of the transaction */
long SO_REWARD;
/* Min time for wait for transaction's processing */
long SO_MIN_TRANS_GEN_NSEC;
/* Max time for wait for transaction's processing */
long SO_MAX_TRANS_GEN_NSEC;
/* Attempts to send a transaction before termination of user */
long SO_RETRY;
/* Size of Transaction Pool of node processes */
long SO_TP_SIZE;
/* Min time for transactions' block processing */
long SO_MIN_TRANS_PROC_NSEC;
/* Max time for transactions' block processing */
long SO_MAX_TRANS_PROC_NSEC;
/* Initial budget of user processes */
long SO_BUDGET_INIT;
/* Duration of the simulation */
long SO_SIM_SEC;
/* Number of friends */
long SO_FRIENDS_NUM;
/* Attempts to insert a transaction in a node's TP before elimination */
long SO_HOPS;
/*** End of Configuration parameters ***/

/*
 * variable used to know if when the node is interrupted by end 
 * of simulation signal, it was creating a new block of transactions
 */
boolean creatingBlock = FALSE;

/* block of transactions extracted from the Transactions Pool */
Block extractedBlock;

/* keeps track if execution is terminated */
boolean execTerminated = FALSE;

/* keeps track of segmentation fault catched by handler */
int segFaultHappened = 0;

#pragma endregion
/*** END GLOBAL VARIABLES ***/

/*** FUNCTIONS PROTOTYPES DECLARATION ***/
#pragma region FUNCTIONS PROTOTYPES DECLARATION
/**
 * Function that assigns the values of the environment variables to the global
 * variables defined above.
 * @return Returns TRUE if successfull, FALSE in case an error occurred.
 */
boolean assignEnvironmentVariables();

/**
 * Function that initialize the ipc structures used in the node.
 * @return Returns TRUE if successfull, FALSE in case an error occured.
 */
boolean initializeIPCFacilities();

/**
 * Function that reinserts the transaction of a block on the TP of the node in case
 * it was not possible to insert the block on the register.
 * @param failedTrs the block of transactions that couldn't be inserted in register.
 */
void reinsertTransactions(Block);

/**
 * Function that sends a transaction from this node's transaction pool to
 * a friend chosen randomly.
 */
void dispatchToFriend();

/**
 * Function that gets a message for this node from the global queue and if its msgContent is
 * TRANSTPFULL it sends the transaction to a friend or on global queue if transaction remaining hops are 0;
 * in case the msgContent of the message received is NEWNODE, the functions adds the new friend to the
 * friends list.
 */
void sendTransaction();

/**
 * Function that sends on the global queue a message depending on the parameters that
 * the function receives.
 * @param trans message to send on the global queue
 * @param pid pid of the receiver of the message
 * @param cnt type of content of the message
 * @param hp value to add/subract to the hops of the transaction
 * @return Returns TRUE if successfull, FALSE in case an error occurred while sending the message.
 */
boolean sendOnGlobalQueue(TransQueue *, pid_t, GlobalMsgContent, long);

/**
 * Function that extracts randomly a friend node which to send a transaction.
 * @return Returns the index of the selected friend node to pick from the list of friends,
 * -1 if the function generates an error.
 */
int extractFriendNode();

/**
 * Function that end the execution of the node.
 * @param sig the signal that called the handler
 */
void endOfExecution(int sig);

/**
 * Function used during termination to print transactions read from
 * TP but still memorized in extractedBlock (transaction that node wasn't
 * able to process because its execution terminated before).
 */
void printTransactionsNotProcessed();

/**
 * Function that deallocates the IPC facilities allocated for the node.
 */
void deallocateIPCFacilities();

/**
 * Function that catches any segmentation fault error during execution and
 * avoids brutal termination.
 *
 * @param sig signal that fired the handler
 */
void segmentationFaultHandler(int);
#pragma endregion
/*** END FUNCTIONS PROTOTYPES DECLARATION ***/

int main(int argc, char *argv[], char *envp[])
{
    int exitCode = EXIT_FAILURE, i = 0, num_bytes = 0, tmp = 0, k = 0, contMex = 0;
    int *newBlockPos = NULL;
    boolean available = FALSE;
    boolean waitForTerm = FALSE;
    boolean error = FALSE;
    struct sigaction actEndOfSim;
    struct sigaction actSendTrans;
    struct sigaction actSegFaultHandler;
    struct sembuf sops[3];
    sigset_t mask;
    Block candidateBlock;
    MsgTP new_trans;
    Transaction rew_tran;
    ProcQueue friendFromList;
    ProcQueue msgOnGQueue;
    /* 
     * simTime = simulation length; 
     * remTime = remaining time to wait (in case a signal wakes up process)
     */
    struct timespec simTime, remTime; 
    char *printMsg;

    /* initializing print string message */
    printMsg = (char *)calloc(200, sizeof(char));
    my_pid = (long)getpid();

    /* Assigns the values ​​of the environment variables to the global variables */
    if (assignEnvironmentVariables())
    {
        /* Allocate the array that will contain friends pid */
        friends_node = (pid_t *)calloc(SO_FRIENDS_NUM, sizeof(pid_t));
        if (friends_node != NULL)
        {
            printf("[NODE %5ld]: initializing IPC facilities...\n", my_pid);
            if (initializeIPCFacilities() == TRUE)
            {
                NOT_ESSENTIAL_PRINT(printf("[NODE %5ld]: reading friends from global queue...\n", my_pid);)

                /* Receives all friends pid from global message queue and stores them in the array */
                while (contMex < SO_FRIENDS_NUM && !error)
                {
                    num_bytes = msgrcv(procQueue, &friendFromList, sizeof(ProcQueue) - sizeof(long), my_pid, 0);
                    if (num_bytes == -1)
                    {
                        snprintf(printMsg, 199, "[NODE %5ld]: failed to initialize friends' list. Error: ", my_pid);
                        unsafeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0;
                        error = TRUE;
                    }
                    else
                    {
                        if (friendFromList.msgContent == FRIENDINIT)
                        {
                            friends_node[contMex] = friendFromList.procPid;
                            friends_node_len++;
                            contMex++;
                        }
                        else
                        {
                            /* the message wasn't the one we were looking for, so we reinsert it on the global queue */
                            if (msgsnd(procQueue, &friendFromList, sizeof(ProcQueue) - sizeof(long), 0) == -1)
                            {
                                snprintf(printMsg, 199, "[NODE %5ld]: failed to initialize friends' list. Error: ", my_pid);
                                unsafeErrorPrint(printMsg, __LINE__);
                                printMsg[0] = 0;
                                error = TRUE;
                            }
                        }
                    }
                }

                /* If an error occurred (error == TRUE) while initializing friends' list, the node terminates. */
                if (!error)
                {
                    /*
                     * argv[1] is the type of node, if NODE it has to wait for simulation to start,
                     * so we set the sops varriabile to access to the fairStartSem semaphore
                     */
                    if (strcmp(argv[1], "NORMAL") == 0)
                    {
                        /* Wait all processes are ready to start the simulation */
                        printf("[NODE %5ld]: waiting for simulation to start...\n", my_pid);
                        sops[0].sem_op = 0;
                        sops[0].sem_num = 0;
                        sops[0].sem_flg = 0;
                    }
                    else
                        printf("[NODE %5ld]: additional node initializing...\n", my_pid);

                    /* if node is of type NORMAL, it has to wait the simulation to start, otherwise no */
                    if (strcmp(argv[1], "NORMAL") == 0 && semop(fairStartSem, &sops[0], 1) == -1)
                    {
                        snprintf(printMsg, 199, "[NODE %5ld]: failed to wait for simulation to start. Error: ", my_pid);
                        unsafeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0;
                    }
                    else
                    {
                        NOT_ESSENTIAL_PRINT(printf("[NODE %5ld]: setting up signal mask...\n", my_pid);)

                        if (sigfillset(&mask) == -1)
                        {
                            snprintf(printMsg, 199, "[NODE %5ld]: failed to initialize signal mask. Error: ", my_pid);
                            unsafeErrorPrint(printMsg, __LINE__);
                            printMsg[0] = 0;
                        }
                        else
                        {
                            actEndOfSim.sa_handler = endOfExecution;
                            actEndOfSim.sa_mask = mask;
                            if (sigaction(SIGUSR1, &actEndOfSim, NULL) == -1)
                            {
                                snprintf(printMsg, 199, "[NODE %5ld]: failed to set up end of simulation handler. Error: ", my_pid);
                                unsafeErrorPrint(printMsg, __LINE__);
                                printMsg[0] = 0;
                            }
                            else
                            {
                                NOT_ESSENTIAL_PRINT(printf("[NODE %5ld]: performing setup operations...\n", my_pid);)
                                newBlockPos = (int *)malloc(sizeof(int));

                                if (newBlockPos == NULL)
                                {
                                    snprintf(printMsg, 199, "[NODE %5ld]: failed to allocate memory for temporary variable. Error: ", my_pid);
                                    unsafeErrorPrint(printMsg, __LINE__);
                                    printMsg[0] = 0;
                                }
                                else
                                {
                                    actSendTrans.sa_handler = dispatchToFriend;
                                    actSendTrans.sa_mask = mask;
                                    if (sigaction(SIGALRM, &actSendTrans, NULL) == -1)
                                    {
                                        snprintf(printMsg, 199, "[NODE %5ld]: failed to set transaction dispatch handler. Error: ", my_pid);
                                        unsafeErrorPrint(printMsg, __LINE__);
                                        printMsg[0] = 0; 
                                    }
                                    else
                                    {
                                        actSegFaultHandler.sa_handler = segmentationFaultHandler;
                                        actSegFaultHandler.sa_mask = mask;
                                        if (sigaction(SIGSEGV, &actSegFaultHandler, NULL) == -1)
                                        {
                                            snprintf(printMsg, 199, "[NODE %5ld]: failed to set segmentation fault handler. Error: ", my_pid);
                                            unsafeErrorPrint(printMsg, __LINE__);
                                            printMsg[0] = 0;
                                        }
                                        else
                                        {
                                            printf("[NODE %5ld]: starting lifecycle...\n", my_pid);
                                            while (!waitForTerm)
                                            {
                                                /* Generating a Block of SO_BLOCK_SIZE-1 Transitions from TP */
                                                i = 0;

                                                /* Generating reward transaction for node an put it in extractedBlock */
                                                rew_tran.sender = NO_SENDER;
                                                rew_tran.receiver = my_pid;
                                                rew_tran.reward = 0.0;
                                                /* we now set it to 0, later we will count the rewards */
                                                rew_tran.amountSend = 0.0;
                                                /* get timestamp for transaction */
                                                clock_gettime(CLOCK_REALTIME, &rew_tran.timestamp); 

                                                /* Extracts SO_BLOCK_SIZE-1 transactions from the transaction pool */
                                                printf("[NODE %5ld]: starting transactions' block creation...\n", my_pid);
                                                while (i < SO_BLOCK_SIZE - 1)
                                                {
                                                    creatingBlock = TRUE;

                                                    /* now receiving the message (transaction from TP) */
                                                    num_bytes = msgrcv(tpId, &new_trans, sizeof(new_trans) - sizeof(long), my_pid, 0);

                                                    if (num_bytes >= 0)
                                                    {
                                                        /* read transaction from tpList */
                                                        extractedBlock.transList[i] = new_trans.transaction;
                                                        /* adding reward of transaction in amountSend of reward_transaction */
                                                        rew_tran.amountSend += new_trans.transaction.reward;

                                                        candidateBlock.transList[i] = new_trans.transaction;

                                                        extractedBlock.bIndex = i;
                                                        candidateBlock.bIndex = i++;
                                                    }
                                                    else
                                                    {
                                                        if (errno != 0 && errno != EINTR)
                                                        {
                                                            sprintf(printMsg, "[NODE %5ld]: failed to retrieve transaction from Transaction Pool. Error", my_pid);
                                                            unsafeErrorPrint(printMsg, __LINE__);
                                                            printMsg[0] = 0;
                                                        }
                                                    }

                                                    /*
                                                     * NOTE: if in the TP there aren't SO_BLOCK_SIZE-1 transactions, the node blocks on msgrcv
                                                     * and waits for a message on queue; it will exit this cycle when it reads the requested
                                                     * number of transactions (put in extractedBlock.transList)
                                                     */
                                                }

                                                creatingBlock = FALSE;

                                                /* putting reward transaction in extracted block */
                                                candidateBlock.transList[i] = rew_tran;
                                                candidateBlock.bIndex = i;
                                                printf("[NODE %5ld]: transactions' block creation completed.\n", my_pid);


                                                printf("[NODE %5ld]: elaborating transactions' block...\n", my_pid);
                                                clock_gettime(CLOCK_REALTIME, &simTime);
                                                simTime.tv_sec = 0;
                                                /* generates a random number in [SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC] */
                                                simTime.tv_nsec = (simTime.tv_nsec % (SO_MAX_TRANS_PROC_NSEC + 1 - SO_MIN_TRANS_PROC_NSEC)) + SO_MIN_TRANS_PROC_NSEC;

                                                /*
                                                 * Adjusting wait time, if number of nanoseconds is greater or equal to 1 second (10^9 nanoseconds)
                                                 * we increase the number of seconds.
                                                 * We do this because we have a representation error
                                                 * when tv_nsec >= 1000000000.
                                                 * The underlying idea is to rewrite tv_nsec in seconds
                                                 */
                                                while (simTime.tv_nsec >= 1000000000)
                                                {
                                                    simTime.tv_sec++;
                                                    simTime.tv_nsec -= 1000000000;
                                                }

                                                /* Simulates the computation by waiting a certain amount of time */
                                                if (nanosleep(&simTime, &remTime) == 0)
                                                {
                                                    /*
                                                     *    Writes the block of transactions "elaborated"
                                                     *    on the register
                                                     *
                                                     *    PRECONDITION:
                                                     *     extractedBlock.bIndex == SO_BLOCK_SIZE - 1
                                                     *     extractedBlock.transList == Transactions to be written on the block
                                                     *                          extracted from the transaction pool
                                                     *
                                                     *     candidateBlock.bIndex == SO_BLOCK_SIZE
                                                     *     candidateBlock.transList == Transactions to be written on the block
                                                     *                          extracted from the transaction pool + reward transaction
                                                     *
                                                     *    Two possibilities:
                                                     *        -execute the waits on the semaphores of the partitions in an atomic way, by doing
                                                     *          that is, the process is unlocked only when it will be possible to access the mutual benefit
                                                     *          exclusion to all three partitions
                                                     *        -or, wait on the i-th partition, see if there is room for a new one
                                                     *          block and only in the negative case proceed with the wait on the semaphore of the i + 1-th partition
                                                     *
                                                     *    The first approach allows you to perform n operations with a single system call, while the second
                                                     *    prevents the process from being suspended to access partitions on which
                                                     *    will not write
                                                     *
                                                     *    We have implemented the first approach
                                                     */

                                                    printf("[NODE %5ld]: trying to write transactions on register...\n", my_pid);
                                                    sops[0].sem_flg = 0;
                                                    sops[0].sem_num = 0;
                                                    sops[0].sem_op = -1;

                                                    sops[1].sem_flg = 0;
                                                    sops[1].sem_num = 1;
                                                    sops[1].sem_op = -1;

                                                    sops[2].sem_flg = 0;
                                                    sops[2].sem_num = 2;
                                                    sops[2].sem_op = -1;

                                                    if (semop(rdPartSem, sops, REG_PARTITION_COUNT) == -1)
                                                    {
                                                        snprintf(printMsg, 199, "[NODE %5ld]: failed to reserve register partitions' reading semaphore. Error: ", my_pid);
                                                        unsafeErrorPrint(printMsg, __LINE__);
                                                        printMsg[0] = 0;
                                                    }
                                                    else
                                                    {
                                                        sops[0].sem_flg = 0;
                                                        sops[0].sem_num = 0;
                                                        sops[0].sem_op = -1;

                                                        sops[1].sem_flg = 0;
                                                        sops[1].sem_num = 1;
                                                        sops[1].sem_op = -1;

                                                        sops[2].sem_flg = 0;
                                                        sops[2].sem_num = 2;
                                                        sops[2].sem_op = -1;
                                                        if (semop(wrPartSem, sops, REG_PARTITION_COUNT) == -1)
                                                        {
                                                            snprintf(printMsg, 199, "[NODE %5ld]: failed to reserve register partitions' writing semaphore. Error: ", my_pid);
                                                            unsafeErrorPrint(printMsg, __LINE__);
                                                            printMsg[0] = 0;
                                                        }
                                                        else
                                                        {
                                                            /* Check for free space on the registry */
                                                            available = FALSE;
                                                            tmp = 0;
                                                            k = 0;
                                                            for (k = 0; k < REG_PARTITION_COUNT && !available; k++)
                                                            {
                                                                newBlockPos = &(regPtrs[k]->nBlocks);
                                                                if (regPtrs[k]->nBlocks < REG_PARTITION_SIZE)
                                                                {
                                                                    available = TRUE;
                                                                    tmp = k;
                                                                }
                                                            }

                                                            /*
                                                             *    Postcondition: i == address of the free partition
                                                             *     if available == FALSE ==> register full
                                                             */
                                                            if (available)
                                                            {
                                                                /* Block insertion */
                                                                /* Precondition: nBlocks == First free position in the block */
                                                                newBlockPos = &(regPtrs[tmp]->nBlocks);
                                                                regPtrs[tmp]->blockList[*newBlockPos] = candidateBlock;
                                                                (*newBlockPos)++;
                                                                printf("[NODE %5ld]: transactions block inserted successfully!\n", my_pid);
                                                            }
                                                            else
                                                            {
                                                                /*
                                                                 *   Register full ==> sending signal of end of simulation
                                                                 */
                                                                printf("[NODE %5ld]: no space left on register. Rollingback and signaling end of simulation...\n", my_pid);
                                                                reinsertTransactions(extractedBlock);
                                                                if (kill(getppid(), SIGUSR1) == -1)
                                                                {
                                                                    /*
                                                                     *  Register full ==> sending signal of end of simulation
                                                                     */
                                                                    snprintf(printMsg, 199, "[NODE %5ld]: failed to signal Master for the end of simulation. Error: ", my_pid);
                                                                    unsafeErrorPrint(printMsg, __LINE__);
                                                                    printMsg[0] = 0;

                                                                    waitForTerm = TRUE;
                                                                }
                                                            }

                                                            /* Exit section */
                                                            sops[0].sem_flg = 0;
                                                            sops[0].sem_num = 0;
                                                            sops[0].sem_op = 1;

                                                            sops[1].sem_flg = 0;
                                                            sops[1].sem_num = 1;
                                                            sops[1].sem_op = 1;

                                                            sops[2].sem_flg = 0;
                                                            sops[2].sem_num = 2;
                                                            sops[2].sem_op = 1;

                                                            NOT_ESSENTIAL_PRINT(printf("[NODE %5ld]: releasing register's partition...\n", my_pid);)
                                                            if (semop(wrPartSem, sops, REG_PARTITION_COUNT) == -1)
                                                            {
                                                                snprintf(printMsg, 199, "[NODE %5ld]: failed to release register partitions' writing semaphore. Error: ", my_pid);
                                                                unsafeErrorPrint(printMsg, __LINE__);
                                                                printMsg[0] = 0;
                                                            }
                                                            else
                                                            {
                                                                sops[0].sem_flg = 0;
                                                                sops[0].sem_num = 0;
                                                                sops[0].sem_op = 1;

                                                                sops[1].sem_flg = 0;
                                                                sops[1].sem_num = 1;
                                                                sops[1].sem_op = 1;

                                                                sops[2].sem_flg = 0;
                                                                sops[2].sem_num = 2;
                                                                sops[2].sem_op = 1;
                                                                if (semop(rdPartSem, sops, REG_PARTITION_COUNT) == -1)
                                                                {
                                                                    snprintf(printMsg, 199, "[NODE %5ld]: failed to release register partitions' reading semaphore. Error: ", my_pid);
                                                                    unsafeErrorPrint(printMsg, __LINE__);
                                                                    printMsg[0] = 0;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    /*
                                                     * The wait of the node can be interrupted by the end of simulation signal
                                                     * or by the dispatch to friend signal
                                                     */
                                                    snprintf(printMsg, 199, "[NODE %5ld]: an unexpected event occured before the end of the computation. Error: ", my_pid);
                                                    unsafeErrorPrint(printMsg, __LINE__);
                                                    printMsg[0] = 0;
                                                }

                                                sendTransaction();
                                                dispatchToFriend();
                                            }
                                        
                                            /*
                                             * Node wait for the master to detect that the register is full.
                                             * By doing this we take the process out of the ready queue, therefore
                                             * increasing the chance of the master being scheduled and detecting the
                                             * end of simulation (it will happen, because the master checks it every second)
                                             * (or at least, the timer will elapse and the simulation will ultimately end)
                                             *
                                             * In the case the node has successfully signaled the master, the process
                                             * waits to be signaled so that its end-of-execution handler will be executed.
                                             *
                                             * Now the node process must wait for end of simulation signal; we do it
                                             * with pause, but a signal could wake it up. We only want the end of simulation
                                             * signal to wake up the process, so we must ignore the SIGALRM signal that
                                             * might arrive for the periodic sending of a transaction to a friend node.
                                             */
                                            if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
                                            {
                                                snprintf(printMsg, 199, "[NODE %5ld]: failed to set ignoring of SIGALRM signal before pause of process. Error: ", my_pid);
                                                unsafeErrorPrint(printMsg, __LINE__);
                                                printMsg[0] = 0;
                                            }

                                            printf("[NODE %5ld]: waiting for end of simulation signal...\n", my_pid);
                                            pause();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                printf("[NODE %5ld]: releasing dynamically allocated memory...\n", my_pid);
            }
            else
            {
                printf("[NODE %5ld]: failed to initialize one or more IPC facilities. Stopping execution...\n", my_pid);
                deallocateIPCFacilities();
            }
        }
        else
        {
            snprintf(printMsg, 199, "[NODE %5ld]: failed to allocate friends' array. Error: ", my_pid);
            unsafeErrorPrint(printMsg, __LINE__);
            printMsg[0] = 0;
        }
    }
    else    
        printf("[NODE %5ld]: failed to assign value to environment variables. Stopping execution...\n", my_pid);


    printf("[NODE %5ld]: about to terminate execution...\n", my_pid);

    /* notify master that node process terminated before expected */
    msgOnGQueue.mtype = getppid();
    msgOnGQueue.msgContent = TERMINATEDNODE;
    msgOnGQueue.procPid = (pid_t)my_pid;
    if (msgsnd(procQueue, &msgOnGQueue, sizeof(ProcQueue) - sizeof(long), IPC_NOWAIT) == -1)
    {
        if (errno == EAGAIN)
            snprintf(printMsg, 199, "[NODE %5ld]: failed to inform master of my termination (global queue was full). Error: ", my_pid);
        else
            snprintf(printMsg, 199, "[NODE %5ld]: failed to inform master of my termination. Error: ", my_pid);
        unsafeErrorPrint(printMsg, __LINE__);
    }

    /* freeing print string message */
    if (printMsg != NULL)
        free(printMsg);

    exit(exitCode);
}

/*** FUNCTIONS IMPLEMENTATION ***/
#pragma region FUNCTIONS IMPLEMENTATION
/**
 * Function that assigns the values of the environment variables to the global
 * variables defined above.
 * @return Returns TRUE if successfull, FALSE in case an error occurred.
 */
boolean assignEnvironmentVariables()
{
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
 * Function that initialize the ipc structures used in the node.
 * @return Returns TRUE if successfull, FALSE in case an error occured.
 */
boolean initializeIPCFacilities()
{
    key_t key;

    /* Initialization of semaphores*/
    key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during fair start semaphore creation. Error: ");

    fairStartSem = semget(key, 1, 0600);
    SEM_TEST_ERROR(fairStartSem, "[NODE]: semget failed during fair start semaphore creation. Error: ");

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during partitions writing semaphores creation. Error: ");
    wrPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(wrPartSem, "[NODE]: semget failed during partitions writing semaphores creation. Error: ");

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during partitions reading semaphores creation. Error: ");
    rdPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(rdPartSem, "[NODE]: semget failed during partitions reading semaphores creation. Error: ");

    key = ftok(SEMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during nodes list semaphore creation. Error: ");
    nodeListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(nodeListSem, "[NODE]: semget failed during nodes list semaphore creation. Error: ");

    /* Creates the global queues*/
    key = ftok(MSGFILEPATH, PROC_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during processes global queue creation. Error: ");
    procQueue = msgget(key, 0600);
    MSG_TEST_ERROR(procQueue, "[NODE]: msgget failed during processes global queue creation. Error: ");

    key = ftok(MSGFILEPATH, NODE_CREATION_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during nodes global queue creation. Error: ");
    nodeCreationQueue = msgget(key, 0600);
    MSG_TEST_ERROR(nodeCreationQueue, "[NODE]: msgget failed during nodes global queue creation. Error: ");

    key = ftok(MSGFILEPATH, TRANS_QUEUE_SEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during transactions global queue creation. Error: ");
    transQueue = msgget(key, 0600);
    MSG_TEST_ERROR(transQueue, "[NODE]: msgget failed during transactions global queue creation. Error: ");

    /* Initialization of shared memory segments */
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during register parition one creation. Error: ");
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[0], "[NODE]: shmget failed during partition one creation. Error: ");

    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during register parition two creation. Error: ");
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[1], "[NODE]: shmget failed during partition two creation. Error: ");

    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during register parition three creation. Error: ");
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[2], "[NODE]: shmget failed during partition three creation. Error: ");

    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[0], "[NODE]: failed to attach to partition one's memory segment. Error: ");
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[1], "[NODE]: failed to attach to partition two's memory segment. Error: ");
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[2], "[NODE]: failed to attach to partition three's memory segment. Error: ");

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during nodes list creation. Error: ");
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(nodesListId, "[NODE]: shmget failed during nodes list creation. Error: ");
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, SHM_RDONLY);
    TEST_SHMAT_ERROR(nodesList, "[NODE]: failed to attach to nodes list's memory segment. Error: ");

    noNodeSegReaders = shmget(ftok(SHMFILEPATH, NONODESEGRDERSSEED), sizeof(int), 0600);
    SHM_TEST_ERROR(noNodeSegReaders, "[NODE]: ftok failed during nodes list's shared variable creation. Error: ");
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noNodeSegReadersPtr, "[NODE]: shmget failed during nodes list's shared variable creation. Error: ");

    key = ftok(MSGFILEPATH, (int)my_pid);
    FTOK_TEST_ERROR(key, "[NODE]: ftok failed during transaction pool creation. Error: ");
    tpId = msgget(key, 0600);
    MSG_TEST_ERROR(tpId, "[NODE]: msgget failed during transaction pool creation. Error: ");

    return TRUE;
}

/**
 * Function that reinserts the transaction of a block on the TP of the node in case
 * it was not possible to insert the block on the register.
 * @param failedTrs the block of transactions that couldn't be inserted in register.
 */
void reinsertTransactions(Block failedTrs)
{
    int msg_length;
    char *aus = NULL;
    TransQueue temp;
    MsgTP msg;

    aus = (char *)calloc(200, sizeof(char));
    while (failedTrs.bIndex != 0)
    {
        failedTrs.bIndex--;
        msg.mtype = my_pid;
        msg.transaction = failedTrs.transList[failedTrs.bIndex];
        if (msgsnd(tpId, &msg, sizeof(MsgTP) - sizeof(long), 0) == -1)
        {
            snprintf(aus, 199, "[NODE %5ld]: failed to reinsert transaction number %d.", my_pid, failedTrs.bIndex);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;

            /* Inform the sender the transaction's processing failed */
            temp.transaction = failedTrs.transList[failedTrs.bIndex];
            temp.msgContent = FAILEDTRANS;
            if (!sendOnGlobalQueue(&temp, failedTrs.transList[failedTrs.bIndex].sender, FAILEDTRANS, 0))
            {
                snprintf(aus, 199, "[NODE %5ld]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
            }
        }
    }

    msg_length = snprintf(aus, 199, "[NODE %5ld]: transactions successfully reinserted on queue!\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);

    if (aus != NULL)
        free(aus);
}

/**
 * Function that sends a transaction from this node's transaction pool to
 * a friend chosen randomly.
 */
void dispatchToFriend()
{
    int i = 0, msg_length, friendTp = -1;
    char *printMsg;
    key_t key;
    MsgTP aus;
    Block temp;

    printMsg = (char *)calloc(200, sizeof(char));

    msg_length = snprintf(printMsg, 199, "[NODE %5ld]: dispatching transaction to friend...\n", my_pid);
    write(STDOUT_FILENO, printMsg, msg_length);
    printMsg[0] = 0;

    if (msgrcv(tpId, &aus, sizeof(MsgTP) - sizeof(long), my_pid, IPC_NOWAIT) == -1)
    {
        if (errno != 0 && errno != ENOMSG && errno != EINTR)
        {
            snprintf(printMsg, 199, "[NODE %5ld]: failed to extract a transaction to send it to a friend. Error: ", my_pid);
            safeErrorPrint(printMsg, __LINE__);
            printMsg[0] = 0;
        }
    }
    else
    {
        /*
         * Precondition: aus contains the transaction to be sent to a friend
         */
        /* generating a random index to access the array of nodes friend */
        i = extractFriendNode();
        if (i == -1)
        {
            snprintf(printMsg, 199, "[NODE %5ld]: failed to extract a friend node which to send the transaction. Error: ", my_pid);
            safeErrorPrint(printMsg, __LINE__);
            printMsg[0] = 0;

            temp.bIndex = 0;
            temp.transList[0] = aus.transaction;
            reinsertTransactions(temp);
        }
        else
        {
            key = ftok(MSGFILEPATH, *(friends_node + i));
            if (key == -1)
            {
                snprintf(printMsg, 199, "[NODE %5ld]: failed to connect to friend's transaction pool. Error: ", my_pid);
                safeErrorPrint(printMsg, __LINE__);
                printMsg[0] = 0;
                /*  Reinsert transaction  */
                temp.bIndex = 0;
                temp.transList[0] = aus.transaction;
                reinsertTransactions(temp);
            }
            else
            {
                friendTp = msgget(key, 0600);
                if (friendTp == -1)
                {
                    snprintf(printMsg, 199, "[NODE %5ld]: failed to connect to friend's transaction pool. Error: ", my_pid);
                    safeErrorPrint(printMsg, __LINE__);
                    printMsg[0] = 0;

                    temp.bIndex = 0;
                    temp.transList[0] = aus.transaction;
                    reinsertTransactions(temp);
                }
                else
                {
                    aus.mtype = *(friends_node + i);
                    if (msgsnd(friendTp, &aus, sizeof(MsgTP) - sizeof(long), 0) == -1)
                    {
                        snprintf(printMsg, 199, "[NODE %5ld]: failed to dispatch transaction to friend. Error: ", my_pid);
                        safeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0;

                        temp.bIndex = 0;
                        temp.transList[0] = aus.transaction;
                        reinsertTransactions(temp);
                    }
                    else
                    {
                        msg_length = snprintf(printMsg, 199, "[NODE %5ld]: transaction successfully dispatched to friend\n", my_pid);
                        write(STDOUT_FILENO, printMsg, msg_length);
                        printMsg[0] = 0;
                    }
                }
            }
        }
    }

    if (printMsg != NULL)
        free(printMsg);
}

/**
 * Function that gets a message for this node from the global queue and if its msgContent is
 * TRANSTPFULL it sends the transaction to a friend or on global queue if transaction remaining hops are 0;
 * in case the msgContent of the message received is NEWNODE, the functions adds the new friend to the
 * friends list.
 */
void sendTransaction()
{
    int i = 0, msg_length, friendTp = -1, j = 0;
    key_t key = -1;
    boolean found = FALSE;
    char *printMsg;
    TransQueue trans;
    NodeCreationQueue ausNode;
    MsgTP aus;

    printMsg = (char *)calloc(200, sizeof(char));

    /*  Recovery of excess transactions */
    while (j < NO_SEND_TRANSACTION_ATTEMPS && 
        msgrcv(transQueue, &trans, sizeof(TransQueue) - sizeof(long), my_pid, IPC_NOWAIT) != -1
    ){
        j++;
        /* Now trans contains the message read from the global queue, we need to check the message content */
        if (trans.msgContent == TRANSTPFULL)
        {
            /* 
             * trans contains the transaction to send to a friend / master 
             * if it is not in the current node pool or if hops == 0 
             */
            if (trans.hops == 0)
            {
                ausNode.msgContent = NEWNODE;
                ausNode.mtype = getppid();
                ausNode.transaction = trans.transaction;
                if (msgsnd(nodeCreationQueue, &ausNode, sizeof(ausNode) - sizeof(long), 0) == -1)
                {
                    snprintf(printMsg, 199, "[NODE %5ld]: failed to dispatch transaction to master. Error: ", my_pid);
                    safeErrorPrint(printMsg, __LINE__);
                    printMsg[0] = 0;

                    /* Inform the sender the transaction's processing failed */
                    if (!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                    {
                        snprintf(printMsg, 199, "[NODE %5ld]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", my_pid);
                        safeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0;
                    }
                }
                else
                {
                    NOT_ESSENTIAL_PRINT(
                        msg_length = snprintf(printMsg, 199, "[NODE %5ld]: requested creation of a new node to serve a transaction...\n", my_pid);
                        write(STDOUT_FILENO, printMsg, msg_length);
                        printMsg[0] = 0;
                    )
                }
            }
            else
            {
                /* generating a random index to access the array of nodes friend */
                i = extractFriendNode();
                if (i == -1)
                {
                    snprintf(printMsg, 199, "[NODE %5ld]: failed to extract a friend node which to send the transaction. Error: ", my_pid);
                    safeErrorPrint(printMsg, __LINE__);
                    printMsg[0] = 0;

                    /* Inform the sender the transaction's processing failed */
                    if (!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                    {
                        snprintf(printMsg, 199, "[NODE %5ld]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", my_pid);
                        safeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0;
                    }
                }
                else
                {
                    key = ftok(MSGFILEPATH, *(friends_node + i));
                    if (key == -1)
                    {
                        snprintf(printMsg, 199, "[NODE %5ld]: failed to connect to friend's transaction pool. Error: ", my_pid);
                        safeErrorPrint(printMsg, __LINE__);
                        printMsg[0] = 0;

                        if (sendOnGlobalQueue(&trans, *(friends_node + i), TRANSTPFULL, -1))
                        {
                            msg_length = snprintf(printMsg, 199, "[NODE %5ld]: transaction successfully dispatched to friend via global queue.\n", my_pid);
                            write(STDOUT_FILENO, printMsg, msg_length);
                            printMsg[0] = 0;
                        }
                        else
                        {
                            snprintf(printMsg, 199, "[NODE %5ld]: failed to dispatch transaction to friend via global queue. Error: ", my_pid);
                            safeErrorPrint(printMsg, __LINE__);
                            printMsg[0] = 0;

                            /* Inform the sender the transaction's processing failed */
                            if (!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                            {
                                snprintf(printMsg, 199, "[NODE %5ld]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", my_pid);
                                safeErrorPrint(printMsg, __LINE__);
                                printMsg[0] = 0;
                            }
                        }
                    }
                    else
                    {
                        friendTp = msgget(key, 0600);
                        if (friendTp == -1)
                        {
                            snprintf(printMsg, 199, "[NODE %5ld]: failed to connect to friend's transaction pool. Error: ", my_pid);
                            safeErrorPrint(printMsg, __LINE__);
                            printMsg[0] = 0;

                            if (sendOnGlobalQueue(&trans, *(friends_node + i), TRANSTPFULL, -1))
                            {
                                msg_length = snprintf(printMsg, 199, "[NODE %5ld]: transaction successfully dispatched to friend via global queue.\n", my_pid);
                                write(STDOUT_FILENO, printMsg, msg_length);
                                printMsg[0] = 0;
                            }
                            else
                            {
                                snprintf(printMsg, 199, "[NODE %5ld]: failed to dispatch transaction to friend via global queue. Error: ", my_pid);
                                safeErrorPrint(printMsg, __LINE__);
                                printMsg[0] = 0;

                                /* Inform the sender the transaction's processing failed */
                                if (!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                                {
                                    snprintf(printMsg, 199, "[NODE %5ld]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", my_pid);
                                    safeErrorPrint(printMsg, __LINE__);
                                    printMsg[0] = 0;
                                }
                            }
                        }
                        else
                        {
                            /* Create a copy of the message read */
                            aus.transaction = trans.transaction;
                            aus.mtype = *(friends_node + i);
                            if (msgsnd(friendTp, &aus, sizeof(MsgTP) - sizeof(long), IPC_NOWAIT) == -1)
                            {
                                /*
                                 *  Friend queue full ==> send on global TP
                                 */
                                if (sendOnGlobalQueue(&trans, *(friends_node + i), TRANSTPFULL, -1))
                                {
                                    msg_length = snprintf(printMsg, 199, "[NODE %5ld]: transaction successfully dispatched to friend via global queue.\n", my_pid);
                                    write(STDOUT_FILENO, printMsg, msg_length);
                                    printMsg[0] = 0;
                                }
                                else
                                {
                                    snprintf(printMsg, 199, "[NODE %5ld]: failed to dispatch transaction to friend via global queue. Error: ", my_pid);
                                    safeErrorPrint(printMsg, __LINE__);
                                    printMsg[0] = 0;

                                    /* Inform the sender the transaction's processing failed */
                                    if (!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                                    {
                                        snprintf(printMsg, 199, "[NODE %5ld]: failed to inform sender of transaction that the transaction wasn't processed. Error: ", my_pid);
                                        safeErrorPrint(printMsg, __LINE__);
                                        printMsg[0] = 0;
                                    }
                                }
                            }
                            else
                            {
                                msg_length = snprintf(printMsg, 199, "[NODE %5ld]: transaction successfully dispatched to friend.\n", my_pid);
                                write(STDOUT_FILENO, printMsg, msg_length);
                                printMsg[0] = 0;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            /* the message wasn't the one we were looking for, so we reinsert it on the global queue */
            if (msgsnd(transQueue, &trans, sizeof(TransQueue) - sizeof(long), 0) == -1)
            {
                snprintf(printMsg, 199, "[NODE %5ld]: failed to reinsert on global queue a message read from it. Error: ", my_pid);
                safeErrorPrint(printMsg, __LINE__);
            }
        }
    }

    while (msgrcv(nodeCreationQueue, &ausNode, sizeof(ausNode) - sizeof(long), my_pid, IPC_NOWAIT) != -1)
    {
        if (ausNode.msgContent == NEWFRIEND)
        {
            /* Friend added on master request */
            for (i = 0; i < SO_FRIENDS_NUM && !found; i++)
            {
                if (friends_node[i] == 0)
                    found = TRUE;
            }

            if (found)
                friends_node[i] = ausNode.procPid;
            else
            {
                friends_node_len++;
                friends_node = (pid_t *)realloc(friends_node, sizeof(pid_t) * friends_node_len);
                if (friends_node != NULL)
                    friends_node[i] = ausNode.procPid;
                else
                {
                    snprintf(printMsg, 199, "[NODE %5ld]: failed to reallocate friends' array. Error: ", my_pid);
                    unsafeErrorPrint(printMsg, __LINE__);
                    printMsg[0] = 0;
                }
            }
        }

        /*
            We don't have to reinsert anything: on nodeCreationQueue we send messages
            of type NEWFRIEND or NEWNODE, but only the former can be sent to a node
        */
    }

    if ((errno != 0 && errno != ENOMSG && errno != EINTR) || j == NO_SEND_TRANSACTION_ATTEMPS)
    {
        snprintf(printMsg, 199, "[NODE %5ld]: failed to check existence of transactions on global queue. Error: ", my_pid);
        safeErrorPrint(printMsg, __LINE__);
    }

    if (printMsg != NULL)
        free(printMsg);
}

/**
 * Function that sends on the global queue a message depending on the parameters that
 * the function receives.
 * @param trans message to send on the global queue
 * @param pid pid of the receiver of the message
 * @param cnt type of content of the message
 * @param hp value (positive or negative) to add to the number of hops of the transaction
 * @return Returns TRUE if successfull, FALSE in case an error occurred while sending the message.
 */
boolean sendOnGlobalQueue(TransQueue *trans, pid_t pid, GlobalMsgContent cnt, long hp)
{
    boolean ret = TRUE;
    NodeCreationQueue msgToMaster;

    trans->mtype = pid;
    trans->msgContent = cnt;
    trans->hops += hp;

    if(cnt == TRANSTPFULL)
    {
        if (msgsnd(transQueue, trans, sizeof(TransQueue) - sizeof(long), IPC_NOWAIT) == -1)
        {
            if(errno == EAGAIN)
            {
                
                /*
                 * the queue is full, so to avoid the node from blocking we ask the 
                 * master to create a new node with the transaction that the node 
                 * tried to insert on the full queue.
                */
                msgToMaster.msgContent = NEWNODE;
                msgToMaster.mtype = getppid();
                msgToMaster.transaction = trans->transaction;
                if (msgsnd(nodeCreationQueue, &msgToMaster, sizeof(NodeCreationQueue) - sizeof(long), IPC_NOWAIT) == -1)
                    ret = FALSE;
            }
            else
                ret = FALSE;
        }
    }
    else
    {
        if (msgsnd(transQueue, trans, sizeof(TransQueue) - sizeof(long), 0) == -1)
            ret = FALSE;
    }

    return ret;
}

/**
 * Function that extracts randomly a friend node which to send a transaction.
 * @return Returns the index of the selected friend node to pick from the list of friends,
 * -1 if the function generates an error.
 */
int extractFriendNode()
{
    int n = -1;
    struct sembuf sops;
    struct timespec now;
    boolean errInWriteSemaphore = FALSE;
    char *aus;

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
                snprintf(aus, 199, "[NODE %5ld]: failed to reserve write nodesList semaphore. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;
                errInWriteSemaphore = TRUE;
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if (semop(nodeListSem, &sops, 1) != -1)
        {
            if (errInWriteSemaphore)
                return (pid_t)-1;

            do
            {
                clock_gettime(CLOCK_REALTIME, &now);
                n = now.tv_nsec % (SO_FRIENDS_NUM);
            } while (nodesList[n].procState != ACTIVE);
            /* It loops until the randomly chosen node is active */

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
                        snprintf(aus, 199, "[NODE %5ld]: failed to release write nodesList semaphore. Error: ", my_pid);
                        safeErrorPrint(aus, __LINE__);
                        aus[0] = 0;
                    }
                }

                sops.sem_num = 0;
                sops.sem_op = 1;
                if (semop(nodeListSem, &sops, 1) != -1)
                {
                    return n;
                }
                else
                {
                    snprintf(aus, 199, "[NODE %5ld]: failed to release mutex nodesList semaphore. Error: ", my_pid);
                    safeErrorPrint(aus, __LINE__);
                    aus[0] = 0;
                }
            }
            else
            {
                snprintf(aus, 199, "[NODE %5ld]: failed to reserve mutex nodesList semaphore. Error: ", my_pid);
                safeErrorPrint(aus, __LINE__);
                aus[0] = 0;
            }
        }
        else
        {
            snprintf(aus, 199, "[NODE %5ld]: failed to release mutex nodesList semaphore. Error: ", my_pid);
            safeErrorPrint(aus, __LINE__);
            aus[0] = 0;
        }
    }
    else
    {
        snprintf(aus, 199, "[NODE %5ld]: failed to reserve mutex nodesList semaphore. Error: ", my_pid);
        safeErrorPrint(aus, __LINE__);
    }

    if (aus != NULL)
        free(aus);

    return -1;
}

/**
 * Function that end the execution of the node.
 * @param sig the signal that called the handler
 */
void endOfExecution(int sig)
{
    ProcQueue msgOnGQueue;
    char *aus;

    aus = (char *)calloc(200, sizeof(char));

    /* printing transactions read from TP but not processed */
    if (creatingBlock)
        printTransactionsNotProcessed();

    deallocateIPCFacilities();

    /* notify master that user process terminated before expected */
    msgOnGQueue.mtype = getppid();
    msgOnGQueue.msgContent = TERMINATEDNODE;
    msgOnGQueue.procPid = (pid_t)my_pid;
    if (msgsnd(procQueue, &msgOnGQueue, sizeof(msgOnGQueue) - sizeof(long), IPC_NOWAIT) == -1)
    {
        if (errno == EAGAIN)
            snprintf(aus, 199, "[NODE %5ld]: failed to inform master of my termination (global queue was full). Error: ", my_pid);
        else
            snprintf(aus, 199, "[NODE %5ld]: failed to inform master of my termination. Error: ", my_pid);
        safeErrorPrint(aus, __LINE__);
    }

    execTerminated = TRUE;

    if (aus != NULL)
        free(aus);

    exit(EXIT_SUCCESS);
}

/**
 * Function used during termination to print transactions read from
 * TP but still memorized in extractedBlock (transaction that node wasn't
 * able to process because its execution terminated before).
 */
void printTransactionsNotProcessed()
{
    int msg_length;
    char *aus = NULL;
    Transaction trans;

    aus = (char *)calloc(300, sizeof(char));

    msg_length = snprintf(aus, 299, "[NODE %5ld]: printing remaining transactions:\n", my_pid);
    write(STDOUT_FILENO, aus, msg_length);

    while (extractedBlock.bIndex != 0)
    {
        extractedBlock.bIndex--;
        trans = extractedBlock.transList[extractedBlock.bIndex];
        msg_length = snprintf(aus, 299, "[NODE %5ld]: - Timestamp: %ld : %ld\n [NODE %5ld]:  - Sender: %ld\n [NODE %5ld]:  - Receiver: %ld\n [NODE %5ld]:  - Amount sent: %f\n [NODE %5ld]:  - Reward: %f\n",
                              my_pid,
                              trans.timestamp.tv_sec,
                              trans.timestamp.tv_nsec,
                              my_pid,
                              trans.sender,
                              my_pid,
                              trans.receiver,
                              my_pid,
                              trans.amountSend,
                              my_pid,
                              trans.reward);
        write(STDOUT_FILENO, aus, msg_length);
    }

    if (aus != NULL)
        free(aus);
}

/**
 * Function that deallocates the IPC facilities allocated for the node.
 */
void deallocateIPCFacilities()
{
    int i = 0, msg_length;
    char *printMsg;

    printMsg = (char *)calloc(200, sizeof(char));

    msg_length = snprintf(printMsg, 199, "[NODE %5ld]: detaching from register's partitions...\n", my_pid);
    write(STDOUT_FILENO, printMsg, msg_length);
    printMsg[0] = 0; /* resetting string's content */

    for (i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if (shmdt(regPtrs[i]) == -1)
        {
            if (errno != 0 && errno != EINVAL)
            {
                snprintf(printMsg, 199, "[NODE %5ld]: failed to detach from register's partition. Error: ", my_pid);
                safeErrorPrint(printMsg, __LINE__);
                printMsg[0] = 0;
            }
        }
    }

    msg_length = snprintf(printMsg, 199, "[NODE %5ld]: detaching from nodes list...\n", my_pid);
    write(STDOUT_FILENO, printMsg, msg_length);
    printMsg[0] = 0; /* resetting string's content */

    if (nodesList != NULL && shmdt(nodesList) == -1)
    {
        if (errno != 0 && errno != EAGAIN)
        {
            snprintf(printMsg, 199, "[NODE %5ld]: failed to detach from nodes list. Error: ", my_pid);
            safeErrorPrint(printMsg, __LINE__);
            printMsg[0] = 0;
        }
    }

    msg_length = snprintf(printMsg, 199, "[NODE %5ld]: detaching from nodes list's number of readers shared variable...\n", my_pid);
    write(STDOUT_FILENO, printMsg, msg_length);
    printMsg[0] = 0; /* resetting string's content */

    if (noNodeSegReadersPtr != NULL && shmdt(noNodeSegReadersPtr) == -1)
    {
        if (errno != 0 && errno != EAGAIN)
        {
            snprintf(printMsg, 199, "[NODE %5ld]: failed to detach from nodes list's number of readers shared variable. Error: ", my_pid);
            safeErrorPrint(printMsg, __LINE__);
        }
    }

    if (friends_node != NULL)
        free(friends_node);

    msg_length = snprintf(printMsg, 199, "[NODE %5ld]: cleanup operations completed. Process is about to end its execution...\n", my_pid);
    write(STDOUT_FILENO, printMsg, msg_length);

    if (printMsg != NULL)
        free(printMsg);
}

/**
 * Function that catches any segmentation fault error during execution and
 * avoids brutal termination.
 *
 * @param sig signal that fired the handler
 */
void segmentationFaultHandler(int sig)
{
    if (segFaultHappened == 0)
        segFaultHappened++;
    else if (segFaultHappened == 1)
    {
        /*
         *  We got two segmentation faults in a row, the fault could be serious.
         *  Let's try to end the execution with the dedicated function.
         */
        segFaultHappened++;
        endOfExecution(-1);
    }
    else
    {
        /*
         *  Multiple segmentation faults have occurred, we cannot terminate using the
         *  dedicated function. We must end brutally.
         */
        exit(EXIT_FAILURE);
    }

    dprintf(STDERR_FILENO, "[NODE %5ld]: a segmentation fault error happened. Terminating...\n", my_pid);

    if (!execTerminated)
        endOfExecution(-1);
    else
        exit(EXIT_FAILURE);
}
#pragma endregion
/*** END FUNCTIONS IMPLEMENTATION ***/
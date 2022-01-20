#include "node.h"

/*
    Aggiungere invio ad amici
    Test
    Sistemare file .h
    Memory leaks e chiamate unsafe con gcc
    Sostituire getpid con una var globale per
    ridurre il numero di chiamate di sistema
*/

/*** GLOBAL VARIABLES FOR IPC OBJECTS ***/
#pragma region GLOBAL VARIABLES FOR IPC OBJECTS
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

/* Id of the shared memory segment that contains the nodes list */
int nodesListId = -1;

/* Pointer to the nodes list */
ProcListElem *nodesList = NULL;

/* Id of the global message queue where users, nodes and master communicate */
int globalQueueId = -1;

/* Id of the set that contains the three semaphores used to write on the register's partitions */
int fairStartSem = -1;

/* Id of the set that contains the three semaphores used to write on the register's partitions */
int wrPartSem = -1;

int rdPartSem = -1;

/* Id of the set that contains the three semaphores used to access the number of readers 
 * variables of the registers partitions in mutual exclusion 
 */
int mutexPartSem = -1;

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

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
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

#pragma endregion
/*** END GLOBAL VARIABLES ***/

/*** FUNCTIONS PROTOTYPES DECLARATION ***/
#pragma region FUNCTIONS PROTOTYPES DECLARATION
/**
 * @brief Function that assigns the values of the environment variables to the global 
 * variables defined above.
 * @return Returns TRUE if successfull, FALSE in case an error occurred. 
 */
boolean assignEnvironmentVariables();

/**
 * @brief Function that creates the ipc structures used in the node.
 * @return Returns TRUE if successfull, FALSE in case an error occured.
 */
boolean createIPCFacilties();

/**
 * @brief Function that initialize the ipc structures used in the node.
 * @return Returns TRUE if successfull, FALSE in case an error occured.
 */
boolean initializeIPCFacilities();

/**
 * @brief Function that initializes the sops semaphore buffer passed as parameter.
 * @param sops the buffer to initialize
 * @param op the type of operation to do on semaphore
 * @return Returns TRUE if successfull, FALSE in case an error occurred.
 */
boolean sembufInit(struct sembuf *, int);

/**
 * @brief Function that reinserts the transaction of a block on the TP of the node in case
 * it was not possible to insert the block on the register.
 * @param failedTrs the block of transactions that couldn't be inserted in register.
 */
void reinsertTransactions(Block);

/**
 * @brief Function that sends a transaction from this node's transaction pool to 
 * a friend chosen randomly.
 */
void dispatchToFriend();

/**
 * @brief Function that gets a message for this node from the global queue and if its msgContent is
 * TRANSTPFULL it sends the transaction to a friend or on global queue if transaction remaining hops are 0;
 * in case the msgContent of the message received is NEWNODE, the functions adds the new friend to the 
 * friends list.
 */
void sendTransaction();

/**
 * @brief Function that sends on the global queue a message depending on the parameters that
 * the function receives.
 * @param trans message to send on the global queue
 * @param pid pid of the receiver of the message
 * @param cnt type of content of the message
 * @param hp value to add/subract to the hops of the transaction
 * @return Returns TRUE if successfull, FALSE in case an error occurred while sending the message.
 */
boolean sendOnGlobalQueue(MsgGlobalQueue *, pid_t, GlobalMsgContent, long);

/**
 * @brief Function that extracts randomly a friend node which to send a transaction.
 * @return Returns the index of the selected friend node to pick from the list of friends,
 * -1 if the function generates an error. 
 */
int extractFriendNode();

/**
 * @brief Function that end the execution of the node.
 * @param sig the signal that called the handler
 */
void endOfExecution(int);

/**
 * @brief Function that deallocates the IPC facilities allocated for the node.
 */
void deallocateIPCFacilities();
#pragma endregion
/*** END FUNCTIONS PROTOTYPES DECLARATION ***/

int main(int argc, char *argv[], char* envp[])
{
    int exitCode = EXIT_FAILURE;
    time_t timeSinceEpoch = (time_t)-1;
    Block extractedBlock;
    Block candidateBlock;
    struct sembuf *reservation;
    struct sembuf *release;
    int i = 0;
    boolean available = FALSE;
    int *newBlockPos = NULL;
    boolean waitForTerm = FALSE;
    struct sigaction actEndOfSim;
    struct sigaction actSendTrans;
    sigset_t mask;
    MsgTP new_trans;
    Transaction rew_tran;
    MsgGlobalQueue friendFromList;
    MsgGlobalQueue msgOnGQueue;
    struct sembuf sops[3];
    int num_bytes = 0;
    int contMex = 0;
    boolean error = FALSE;
    struct timespec simTime, remTime; /* simTime = simulation length; remTime = remaining time to wait (in case a signal wakes up process)*/

    /*
        Il nodo potrebbe essere interrotto soltanto
        dal segnale di fine simulazione, ma in tal caso
        l'esecuzione della procedura in corso non ripartirebbe 
        da capo, quindi qui si può usare codice non rientrante
        
        Falso!!! Un nodo può terminare in caso di errori non aspettati durante l'esecuzione!!!
    */

    /* Assigns the values ​​of the environment variables to the global variables */
    printf("Node: reading configuration parameters...\n");
    if (assignEnvironmentVariables())
    {
        /* Allocate the array that will contain friends pid */
        friends_node = (pid_t *)calloc(SO_FRIENDS_NUM, sizeof(pid_t));
        if (friends_node != NULL)
        {
            printf("Node %ld: hooking up of IPC facilitites...\n", (long)getpid());

            if (createIPCFacilties() == TRUE)
            {
                printf("Node: initializing IPC facilities...\n");
                if (initializeIPCFacilities() == TRUE)
                {
                    printf("Node: reading friends from global queue...\n");
                    /* Receives all friends pid from global message queue and stores them in the array */
                    while (contMex < SO_FRIENDS_NUM && !error)
                    {
                        num_bytes = msgrcv(globalQueueId, &friendFromList, sizeof(MsgGlobalQueue)-sizeof(long), getpid(), 0);
                        if (num_bytes == -1)
                        {
                            unsafeErrorPrint("Node: failed to initialize friends' list. Error: ");
                            error = TRUE;
                        } 
                        else 
                        {
                            if(friendFromList.msgContent == FRIENDINIT)
                            {
                                friends_node[contMex] = friendFromList.friend;
                                contMex++;
                            }
                            else
                            {
                                /* the message wasn't the one we were looking for, so we reinsert it on the global queue */
                                if(msgsnd(globalQueueId, &friendFromList, sizeof(MsgGlobalQueue)-sizeof(long), 0) == -1)
                                {
                                    unsafeErrorPrint("Node: failed to initialize friends' list. Error: ");
                                    error = TRUE;
                                }
                            }
                        }
                    }

                    /*
                        PROVVISORIO, CORREGGERE
                    */
                    /*for (i = 0; i < SO_FRIENDS_NUM; i++)
                    {
                        printf("Nodo %d -> Amico: %d\n", getpid(), friends_node[i]);
                    }*/

                    /* If an error occurred (error == TRUE) while initializing friends' list, the node terminates. */
                    if (!error)
                    {
                        /*
                            CORREGGERE: ma è giusto metterla qui??
                        */
                        /* 
                         * argv[1] is the type of node, if NODE it has to wait for simulation to start, 
                         * so we set the sops varriabile to access to the fairStartSem semaphore
                         */
                        if(argv[1] == NORMAL) 
                        {
                            /* Wait all processes are ready to start the simulation */
                            printf("Node %ld is waiting for simulation to start....\n", (long)getpid());
                            sops[0].sem_op = 0;
                            sops[0].sem_num = 0;
                            sops[0].sem_flg = 0;
                        }

                        /* if node is of type NORMAL, it has to wait the simulation to start, otherwise no */
                        if (argv[1] == NORMAL && semop(fairStartSem, &sops[0], 1) == -1)
                            safeErrorPrint("Node: failed to wait for simulation to start. Error: ");
                        else {
                            printf("Sono il nodo %d -> Eseguo!\n", getpid());
                            printf("Node done %d!\n", getpid());

                            printf("Node: setting up signal mask...\n");
                            if (sigfillset(&mask) == -1)
                                unsafeErrorPrint("Node: failed to initialize signal mask. Error: ");
                            else
                            {
                                actEndOfSim.sa_handler = endOfExecution;
                                actEndOfSim.sa_mask = mask;
                                if (sigaction(SIGUSR1, &actEndOfSim, NULL) == -1)
                                    unsafeErrorPrint("Node: failed to set up end of simulation handler. Error: ");
                                else
                                {
                                    printf("Node: performing setup operations...\n");
                                    newBlockPos = (int *)malloc(sizeof(int));

                                    if (newBlockPos == NULL)
                                    {
                                        unsafeErrorPrint("Node: failed to allocate memory for temporary variable. ");
                                    }
                                    else
                                    {
                                        actSendTrans.sa_handler = dispatchToFriend;
                                        actSendTrans.sa_mask = mask;
                                        if (sigaction(SIGALRM, &actSendTrans, NULL) == -1)
                                            unsafeErrorPrint("Node: failed to set transaction dispatch handler. Error: ");
                                        else
                                        {
                                            /*
                                                There should be no previous alarms set
                                            */
                                            /*
                                                Correggere: vedere se sia meglio metterlo nel while senza usare l'allarme
                                            */
                                            if (alarm(TRANS_FRIEND_INTERVAL) == 0)
                                            {
                                                timeSinceEpoch = time(NULL);
                                                if (timeSinceEpoch == (time_t)-1)
                                                    unsafeErrorPrint("Node: failed to initialize random generator's seed. Error");
                                                else
                                                {
                                                    if (sembufInit(reservation, -1) && sembufInit(release, 1))
                                                    {
                                                        printf("Node: starting lifecycle...\n");
                                                        while (!waitForTerm)
                                                        {   
                                                            /*
                                                                Check if a transaction was sent when TP was full
                                                                and dispatch it to another node
                                                            */
                                                            sendTransaction();

                                                            /* Generating a Block of SO_BLOCK_SIZE-1 Transitions from TP */
                                                            /* SO_BLOCK_SIZE is initialized reading the value from environment variables */
                                                            i = 0;

                                                            /* Generating reward transaction for node an put it in extractedBlock */
                                                            rew_tran.sender = NO_SENDER;
                                                            rew_tran.receiver = getpid();
                                                            rew_tran.reward = 0.0;
                                                            rew_tran.amountSend = 0.0;                          /* we now set it to 0, then we will count the rewards */
                                                            clock_gettime(CLOCK_REALTIME, &rew_tran.timestamp); /* get timestamp for transaction */

                                                            /* cycle for extract transaction from TP */
                                                            /*
                                                                Estrae SO_BLOCK_SIZE-1 transazioni dalla transaction pool
                                                            */
                                                            printf("Node: starting transactions' block creation...\n");
                                                            while (i < SO_BLOCK_SIZE-1)
                                                            {
                                                                /* now receiving the message (transaction from TP) */
                                                                num_bytes = msgrcv(tpId, &new_trans, sizeof(new_trans)-sizeof(long), getpid(), 0);

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
                                                                    /*
                                                                        Potrebbe avere senso far ripartire l'estrazione da capo ?
                                                                        No, non cambierebbe nulla, ricordare che le transazioni nel TP
                                                                        non sono legate, quindi in un blocco possono esserci transazioni qualsiasi
                                                                    */
                                                                    unsafeErrorPrint("Node: failed to retrieve transaction from Transaction Pool. Error: ");
                                                                }

                                                                /*
                                                                * NOTE: if in the TP there aren't SO_BLOCK_SIZE-1 transactions, the node blocks on msgrcv
                                                                * and waits for a message on queue; it will exit this cycle when it reads the requested 
                                                                * number of transactions (put in extractedBlock.transList)
                                                                */
                                                            }

                                                            /* putting reward transaction in extracted block */
                                                            candidateBlock.transList[i] = rew_tran;
                                                            candidateBlock.bIndex = i;
                                                            printf("Node: transactions' block creation completed.\n");
                                                            
                                                            /*
                                                                PRECONDIZIONE:
                                                                    SO_MIN_TRANS_PROC_NSEC e SO_MAX_TRANS_PROC_NSEC sono state caricate leggendole
                                                                    dalle variabili d'ambiente
                                                            */
                                                            printf("Node: elaborating transactions' block...\n");
                                                            
                                                            clock_gettime(CLOCK_REALTIME, &simTime); /* get a value in nanoseconds as a random value */
                                                            simTime.tv_sec = 0;
                                                            /*generates a random number in [SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC]*/
                                                            simTime.tv_nsec = (simTime.tv_nsec % (SO_MAX_TRANS_GEN_NSEC + 1)) + SO_MIN_TRANS_GEN_NSEC;

                                                            /* Simulates the computation by waiting a certain amount of time */
                                                            if (nanosleep(&simTime, &remTime) == 0) /* if equals 0, the process waited the amount of time requested */
                                                            {
                                                                /*
                                                                    Writes the block of transactions "elaborated"
                                                                    on the register
                                                                */
                                                                /*
                                                                        PRECONDIZIONE:
                                                                            extractedBlock.bIndex == SO_BLOCK_SIZE - 1
                                                                            extractedBlock.transList == Transazioni da scrivere sul blocco 
                                                                                                        estratte dalla transaction pool

                                                                            candidateBlock.bIndex == SO_BLOCK_SIZE
                                                                            candidateBlock.transList == Transazioni da scrivere sul blocco 
                                                                                                        estratte dalla transaction pool + transazione di reward
                                                                */
                                                                /*
                                                                    Due possibilità:
                                                                        -eseguire le wait sui semafori delle partizioni in maniera atomica, facendo
                                                                        cioè si che il processo venga sbloccato solo quanto sarà possibile accedere in mutua
                                                                        esclusione a tutte e tre le partizioni
                                                                        -oppure, eseguire la wait sulla partizione i-esima, vedere se ci sia spazio per un nuovo
                                                                        blocco e solo in caso negativo procedere con la wait sul semaforo della partizione i+1-esima

                                                                        Il primo approccio consente di eseguire n operazioni con una sola system call, mentre il secondo
                                                                        evita che il processo rimanga sospeso per accedere in mutua esclusione a partizioni sulle quali 
                                                                        non scriverà
                                                        
                                                                        Io ho implementato il primo approccio
                                                                */
                                                                                                                               
                                                                printf("Node: trying to write transactions on register...\n");
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
                                                                    unsafeErrorPrint("Node: failed to reserve register partitions' reading semaphore. Error: ");
                                                                else{
                                                                    if (semop(wrPartSem, reservation, REG_PARTITION_COUNT) == -1)
                                                                        unsafeErrorPrint("Node: failed to reserve register partitions' writing semaphore. Error: ");
                                                                    else
                                                                    {
                                                                        /*
                                                                            PRECONDIZIONE:
                                                                                Il processo arriva qui soolo dopo aver guadagnato l'accesso
                                                                                in mutua esclusione a tutte le partizioni
                                                                        */

                                                                                            /*
                                                                            Verifica esitenza spazio libero sul registro
                                                                        */
                                                                        for (i = 0; i < REG_PARTITION_COUNT && !available; i++)
                                                                        {
                                                                            if (regPtrs[i]->nBlocks != REG_PARTITION_SIZE)
                                                                                available = TRUE;
                                                                        }

                                                                        /*
                                                                            Postcondizione: i == indirizzo della partizione libera
                                                                            se available == FALSE ==> registro pieno
                                                                        */

                                                                        if (available)
                                                                        {
                                                                            /*
                                                                                Inserimento blocco
                                                                            */
                                                                                                /*
                                                                                Precondizione: nBlocks == Prima posizione libera nel blocco
                                                                            */
                                                                                                /*
                                                                                    Quando questa istruzione verà eseguita verrà fatta una copia
                                                                                    per valore di candidateBlock.
                                                                                    È inefficiente, ma non possiamo fare altrimenti
                                                                                    se vogliamo condividere la transazione tra processi registri
                                                                            */
                                                                                                /*
                                                                                regPtrs[i] PUNTATORE ad un di tipo register allocato nel segmento
                                                                                di memoria condivisa, che rappresenta l'i-esima partizione del registro
                                                                            */
                                                                            newBlockPos = &(regPtrs[i]->nBlocks);
                                                                            regPtrs[i]->blockList[*newBlockPos] = candidateBlock;
                                                                            (*newBlockPos)++;
                                                                        }
                                                                        else
                                                                        {
                                                                            /*
                                                                            Registro pieno ==> invio segnale di fine simulazione
                                                                        */
                                                                            printf("Node: no space left on register. Rollingback and signaling end of simulation...\n");
                                                                            reinsertTransactions(extractedBlock);
                                                                            if (kill(getppid(), SIGUSR1) == -1)
                                                                            {
                                                                                /*
                                                                                Registro pieno ==> invio segnale di fine simulazione
                                                                            */
                                                                                safeErrorPrint("Node: failed to signal Master for the end of simulation. Error: ");

                                                                                waitForTerm = TRUE;
                                                                            }

                                                                            /*
                                                                            Exit section
                                                                        */
                                                                            sops[0].sem_flg = 0;
                                                                            sops[0].sem_num = 0;
                                                                            sops[0].sem_op = 1;

                                                                            sops[1].sem_flg = 0;
                                                                            sops[1].sem_num = 1;
                                                                            sops[1].sem_op = 1;

                                                                            sops[2].sem_flg = 0;
                                                                            sops[2].sem_num = 2;
                                                                            sops[2].sem_op = 1;
                                                                            printf("Node: releasing register's partition...\n");
                                                                            if (semop(wrPartSem, release, REG_PARTITION_COUNT) == -1)
                                                                                unsafeErrorPrint("Node: failed to release register partitions' writing semaphore. Error: ");
                                                                            else
                                                                            {
                                                                                if (semop(rdPartSem, release, REG_PARTITION_COUNT) == -1)
                                                                                    unsafeErrorPrint("Node: failed to release register partitions' reading semaphore. Error: ");
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            else
                                                            {
                                                                /*
                                                                    The wait of the node can be interrupted by the end of simulation signal
                                                                    or by the dispatch to friend signal
                                                                */
                                                                unsafeErrorPrint("Node: an unexpected event occured before the end of the computation.");
                                                                if (errno != EINTR)
                                                                {
                                                                    /* Si è verificato un errore nella nanosleep (può succedere in caso di errore di settaggio di simTime) */
                                                                }
                                                                else
                                                                {
                                                                    /*
                                                                        La nanosleep (o meglio, l'attesa del processo) è stata interrotta da un segnale.
                                                                        Dovremmo far ripartire la nanosleep con il tempo rimanente? Ha senso?
                                                                        CONTROLLARE MAN PAGES NANOSLEEP NOTES PER PROBLEMA NEL RIESEGUIRE SUBITO 
                                                                        NANOSLEEP QUANDO IL PROCESSO VIENE RISVEGLIATO DA UN SEGNALE.
                                                                    */
                                                                }
                                                            }
                                                        }
                                                    }
                                                    
                                                    /*
                                                        Cosa succede in caso di errore?
                                                        Terminiamo il ciclo?
                                                        Oppure segnaliamo l'errore e procediamo
                                                        con la prossima elaborazione?
                                                    */
                                                    

                                                    /*
                                                        Node wait for the master to detect that the register is full.
                                                        By doing this we take the process out of the ready queue, therefore
                                                        increasing the chance of  the master being scheduled and detecting the
                                                        end of simulation (it will happen, because the master checks it every time)
                                                        (or at least, the timer will elapse and the simulation will utimately end)
                                                    */
                                                    /*
                                                        In the case tha node has successfully signaled the master, the process
                                                        waits to be signaled so that its end-of-execution handler will be executed.
                                                    */
                                                    
                                                    /* 
                                                     * now the node process must wait for end of simulation signal; we do it
                                                     * with pause, but a signal could wake it up. We only want the end of simulation
                                                     * signal to wake up the process, so we must ignore the SIGALRM signal that
                                                     * might arrive for the periodic sending of a transaction to a friend node.
                                                     */
                                                    if(signal(SIGALRM, SIG_IGN) == SIG_ERR)
                                                    {
                                                        unsafeErrorPrint("Node: failed to set ignoring of SIGALRM signal before pause of process. Error: ");
                                                    }
                                                    
                                                    printf("Node: waiting for end of simulation signal...\n");
                                                    pause();
                                                }
                                            }
                                            else
                                            {
                                                unsafeErrorPrint("Node: failed to set transaction's dispatch timer.");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    printf("Node: releasing dynamically allocated memory...\n");
                    free(reservation);
                    free(release);
                    free(newBlockPos);
                }
                else
                {
                    /* Initialization of one or more IPC facilities failed, deallocate the IPC facilities and end execution */
                    printf("Node: failed to initialize one or more IPC facilities. Stopping execution...\n");
                    deallocateIPCFacilities();
                }
            }
            else
            {
                /* Creation of one or more IPC facilities failed, deallocate the IPC facilities created and end execution */
                printf("Node: failed to create one or more IPC facilities. Stopping execution...\n");
                deallocateIPCFacilities();
            }
        } 
        else 
        {
            unsafeErrorPrint("Node: failed to allocate friends' array. Error: ");
        }    
    }
    else
    {
        /*
            Se un nodo è terminato ed un processo prova a mandargli una transazione bisogna
            segnalare un errore
        */
        printf("Node: failed to assign value to environment variables. Stopping execution...\n");
    }

    printf("Node: about to terminate execution...\n");
    
    /* notify master that user process terminated before expected */
    msgOnGQueue.mType = getppid();
    msgOnGQueue.msgContent = TERMINATEDNODE;
    msgOnGQueue.terminatedPid = getpid();
    if(msgsnd(globalQueueId, &msgOnGQueue, sizeof(msgOnGQueue)-sizeof(long), 0) == -1)
        safeErrorPrint("Node: failed to inform master of my termination. Error: ");

    exit(exitCode);
}

/*** FUNCTIONS IMPLEMENTATION ***/
#pragma region FUNCTIONS IMPLEMENTATION
/**
 * @brief Function that assigns the values of the environment variables to the global 
 * variables defined above.
 * @return Returns TRUE if successfull, FALSE in case an error occurred. 
 */
boolean assignEnvironmentVariables()
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
 * @brief Function that creates the ipc structures used in the node.
 * @return Returns TRUE if successfull, FALSE in case an error occured.
 */
boolean createIPCFacilties()
{
    regPtrs = (Register **)malloc(REG_PARTITION_COUNT * sizeof(Register *));
    TEST_MALLOC_ERROR(regPtrs, "Node: failed to allocate register paritions' pointers array. Error: ");

    regPartsIds = (int *)malloc(REG_PARTITION_COUNT * sizeof(int));
    TEST_MALLOC_ERROR(regPartsIds, "Node: failed to allocate register paritions' ids array. Error: ");

    return TRUE;
}

/**
 * @brief Function that initialize the ipc structures used in the node.
 * @return Returns TRUE if successfull, FALSE in case an error occured.
 */
boolean initializeIPCFacilities()
{
    /* Initialization of semaphores*/
    key_t key = ftok(SEMFILEPATH, FAIRSTARTSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during fair start semaphore creation. Error: ");

    fairStartSem = semget(key, 1, 0600);
    SEM_TEST_ERROR(fairStartSem, "Node: semget failed during fair start semaphore creation. Error: ");

    key = ftok(SEMFILEPATH, WRPARTSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during partitions writing semaphores creation. Error: ");
    wrPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(wrPartSem, "Node: semget failed during partitions writing semaphores creation. Error: ");

    key = ftok(SEMFILEPATH, RDPARTSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during partitions reading semaphores creation. Error: ");
    rdPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(rdPartSem, "Node: semget failed during partitions reading semaphores creation. Error: ");

    key = ftok(SEMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during nodes list semaphore creation. Error: ");
    nodeListSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(nodeListSem, "Node: semget failed during nodes list semaphore creation. Error: ");

    key = ftok(SEMFILEPATH, PARTMUTEXSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during partitions mutex semaphores creation. Error: ");
    mutexPartSem = semget(key, 3, 0600);
    SEM_TEST_ERROR(mutexPartSem, "Node: semget failed during partitions mutex semaphores creation. Error: ");

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    /* Creates the global queue*/
    key = ftok(MSGFILEPATH, GLOBALMSGSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during global queue creation. Error: ");
    globalQueueId = msgget(key, 0600);
    MSG_TEST_ERROR(globalQueueId, "Node: msgget failed during global queue creation. Error: ");
    /********************************************************/
    /********************************************************/

    /*****  Initialization of shared memory segments    *****/
    /********************************************************/
    key = ftok(SHMFILEPATH, REGPARTONESEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during register parition one creation. Error: ");
    regPartsIds[0] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[0], "Node: shmget failed during partition one creation. Error: ");

    key = ftok(SHMFILEPATH, REGPARTTWOSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during register parition two creation. Error: ");
    regPartsIds[1] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[1], "Node: shmget failed during partition two creation. Error: ");

    key = ftok(SHMFILEPATH, REGPARTTHREESEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during register parition three creation. Error: ");
    regPartsIds[2] = shmget(key, REG_PARTITION_SIZE * sizeof(Register), 0600);
    SHM_TEST_ERROR(regPartsIds[2], "Node: shmget failed during partition three creation. Error: ");

    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[0], "Node: failed to attach to partition one's memory segment. Error: ");
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[1], "Node: failed to attach to partition two's memory segment. Error: ");
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[2], "Node: failed to attach to partition three's memory segment. Error: ");

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key, "Node: ftok failed during nodes list creation. Error: ");
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(nodesListId, "Node: shmget failed during nodes list creation. Error: ");
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, SHM_RDONLY);
    TEST_SHMAT_ERROR(nodesList, "Node: failed to attach to nodes list's memory segment. Error: ");

    noNodeSegReaders = shmget(ftok(SHMFILEPATH, NONODESEGRDERSSEED), sizeof(SO_NODES_NUM), 0600);
    SHM_TEST_ERROR(noNodeSegReaders, "Node: ftok failed during nodes list's shared variable creation. Error: ");
    noNodeSegReadersPtr = (int *)shmat(noNodeSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noNodeSegReadersPtr, "Node: shmget failed during nodes list's shared variable creation. Error: ");

    key = ftok(MSGFILEPATH, getpid());
    FTOK_TEST_ERROR(key, "Node: ftok failed during transaction pool creation. Error: ");
    tpId = msgget(key, 0600);
    MSG_TEST_ERROR(tpId, "Node: msgget failed during transaction pool creation. Error: ");

    return TRUE;
}

/**
 * @brief Function that initializes the sops semaphore buffer passed as parameter.
 * @param sops the buffer to initialize
 * @param op the type of operation to do on semaphore
 * @return Returns TRUE if successfull, FALSE in case an error occurred.
 */
boolean sembufInit(struct sembuf *sops, int op)
{
    int i = 0;
    boolean ret = FALSE;

    sops = (struct sembuf *)calloc(REG_PARTITION_COUNT, sizeof(struct sembuf));
    if (sops == NULL)
        safeErrorPrint("Node: failed to allocate semaphores operations' array. ");
    else
    {
        for (i = 0; i < REG_PARTITION_COUNT; i++)
        {
            sops[i].sem_op = op;
            sops[i].sem_num = i;
            sops[i].sem_flg = 0;
        }

        ret = TRUE;
    }

    return ret;
}

/**
 * @brief Function that reinserts the transaction of a block on the TP of the node in case
 * it was not possible to insert the block on the register.
 * @param failedTrs the block of transactions that couldn't be inserted in register.
 */
void reinsertTransactions(Block failedTrs)
{
    char *aus = NULL;
    MsgGlobalQueue temp;

    aus = (char *)calloc(sizeof(char), 50);
    while (failedTrs.bIndex == 0)
    {
        failedTrs.bIndex--;
        if (msgsnd(tpId, &(failedTrs.transList[failedTrs.bIndex]), sizeof(Transaction), 0) == -1)
        {
            sprintf(aus, "Node: failed to reinsert transaction number %d.", failedTrs.bIndex);
            safeErrorPrint(aus);
            
            /* Inform the sender the transaction's processing failed */
            temp.transaction = failedTrs.transList[failedTrs.bIndex];
            temp.msgContent = FAILEDTRANS;
            if(!sendOnGlobalQueue(&temp, failedTrs.transList[failedTrs.bIndex].sender, FAILEDTRANS, 0))
            {
                /* Che facciamo in questo caso ???*/
                safeErrorPrint("Node: failed to inform sender of transaction that the transaction wasn't processed. Error: ");
            }
        }
    }
    write(STDOUT_FILENO, 
          "Node: Transactions reinserted successfully!\n",
          strlen("Node: Transactions reinserted successfully!\n"));

    free(aus);
}

/**
 * @brief Function that sends a transaction from this node's transaction pool to 
 * a friend chosen randomly.
 */
void dispatchToFriend()
{
    /*
        1. Prelievo transazione: OK
        2. Selezione amico: Ok
        3. Reset timer ed handler: Ok
    */
    MsgTP aus;
    int i = 0;
    key_t key;
    int friendTp = -1;
    sigset_t mask;
    struct sigaction actSendTrans;
    Block temp;

    write(STDOUT_FILENO,
          "Node: dispatching transaction to friend...\n",
          strlen("Node: dispatching transaction to friend...\n"));
    
    if (msgrcv(tpId, &aus, sizeof(Transaction), getpid(), 0) == -1)
    {
        safeErrorPrint("Node: failed to extract a transaction to send it to a friend. Error: ");
        /*
            CORREGGERE: In caso di errore procediamo limitandoci a segnalarlo??
        */
    }
    else
    {
        /*
            Precondizione: aus contiene la transazione da inviare ad un amico
        */
        
        /* generating a random index to access the array of nodes friend */
        i = extractFriendNode();
        if(i == -1)
        {
            safeErrorPrint("Node: failed to extract a friend node which to send the transaction. Error: ");
            temp.bIndex = 0;
            temp.transList[0] = aus.transaction;
            reinsertTransactions(temp);
        }
        else
        {
            key = ftok(MSGFILEPATH, *(friends_node + i));
            if (key == -1)
            {
                safeErrorPrint("Node: failed to connect to friend's transaction pool. Error: ");
                /*
                    Reinserire transazione
                */
                temp.bIndex = 0;
                temp.transList[0] = aus.transaction;
                reinsertTransactions(temp);
            }
            else
            {
                friendTp = msgget(key, 0600);
                if (friendTp == -1)
                {
                    safeErrorPrint("Node: failed to connect to friend's transaction pool. Error: ");
                    temp.bIndex = 0;
                    temp.transList[0] = aus.transaction;
                    reinsertTransactions(temp);
                }
                else
                {
                    aus.mType = *(friends_node + i);
                    if (msgsnd(friendTp, &aus, sizeof(Transaction), 0600) == -1)
                    {
                        safeErrorPrint("Node: failed to dispatch transaction to friend. Error: ");
                        temp.bIndex = 0;
                        temp.transList[0] = aus.transaction;
                        reinsertTransactions(temp);
                    }
                    else
                    {
                        write(STDOUT_FILENO,
                            "Node: transaction successfully dispatched to friend.\n",
                            strlen("Node: transaction successfully dispatched to friend.\n"));
                    }
                }
            }
        }
    }

    /*
        Vedere se togliere il segnale di fine simulazione
    */
    write(STDOUT_FILENO,
          "Node: resetting transaction's dispatch timer and handler...\n",
          strlen("Node: resetting timer and handler...\n"));
    
    if (sigfillset(&mask) == -1)
    {
        safeErrorPrint("Node: failed to set up signal mask. Error: ");
    }
    else
    {
        actSendTrans.sa_mask = mask;
        actSendTrans.sa_handler = dispatchToFriend;
        if (sigaction(SIGALRM, &actSendTrans, NULL) == -1)
        {
            safeErrorPrint("Node: failed to set simulation's dispatch handler. Error: ");
        }
        else
        {
            if (alarm(TRANS_FRIEND_INTERVAL) != 0)
            {
                safeErrorPrint("Node: failed to set transaction's dispatch timer.");
            }
        }
    }
}

/**
 * @brief Function that gets a message for this node from the global queue and if its msgContent is
 * TRANSTPFULL it sends the transaction to a friend or on global queue if transaction remaining hops are 0;
 * in case the msgContent of the message received is NEWNODE, the functions adds the new friend to the 
 * friends list.
 */
void sendTransaction()
{
    MsgGlobalQueue trans;
    int i = 0;
    key_t key = -1;
    int friendTp = -1;
    MsgTP aus;
    pid_t * listPtr = NULL;
    pid_t * prevPtr = NULL;
    boolean found = FALSE;

    /*
        Fare ciclo per tutte le transazioni ???
        Potrebbe avere senso, in fondo non dovrebbero essere troppe.
        Il rischio è quello di fare un numero di system call molto elevate
        in un'invocazione e molto poco nella altre.
        Così invece le system call vengono meglio distribuite
    */
    if (msgrcv(globalQueueId, &trans, sizeof(MsgGlobalQueue)-sizeof(long), getpid(), IPC_NOWAIT) == -1)
    {
        if (errno != ENOMSG)
        {
            safeErrorPrint("Node: failed to check existence of transactions on global queue. Error: ");
        }
    } 
    else 
    {
        /*
            Ora trans contiene il messaggio letto dalla coda globale, dobbiamo verificare il contenuto del messaggio
        */
        if (trans.msgContent == TRANSTPFULL)
        {
            /*
                trans contiene la transazione da mandare ad un amico/master se non sta nella pool del nodo attuale o se hops == 0
            */
            if (trans.hops == 0)
            {
                /* Invio al master */
                if (sendOnGlobalQueue(&trans, getppid(), NEWNODE, 0))
                {
                    safeErrorPrint("Node: failed to dispatch transaction to master. Error: ");
                    
                    /* Inform the sender the transaction's processing failed */
                    if(!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                    {
                        /* Che facciamo in questo caso ???*/
                        safeErrorPrint("Node: failed to inform sender of transaction that the transaction wasn't processed. Error: ");
                    }
                }
                else
                {
                    write(STDOUT_FILENO,
                          "Node: requested creation of a new node to serve a transaction...\n",
                          strlen("Node: requested creation of a new node to serve a transaction...\n"));
                }
            }
            else
            {
                /* generating a random index to access the array of nodes friend */
                i = extractFriendNode();
                if(i == -1)
                {
                    safeErrorPrint("Node: failed to extract a friend node which to send the transaction. Error: ");
                            
                    /* Inform the sender the transaction's processing failed */
                    if(!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                    {
                        /* Che facciamo in questo caso ??? */
                        safeErrorPrint("Node: failed to inform sender of transaction that the transaction wasn't processed. Error: ");
                    }
                }
                else
                {
                    key = ftok(MSGFILEPATH, *(friends_node + i));
                    /*
                        Dovremmo controllare se il nodo scelto è attivo? 
                        Sì, ma come facciamo a controllare il cambio di stato (attivo/terminato) di un nodo amico???
                    */
                    if (key == -1)
                    {
                        safeErrorPrint("Node: failed to connect to friend's transaction pool. Error: ");
                        if (sendOnGlobalQueue(&trans, *(friends_node + i), TRANSTPFULL, -1))
                            write(STDOUT_FILENO,
                                "Node: transaction successfully dispatched to friend via global queue.\n",
                                strlen("Node: transaction successfully dispatched to friend via global queue.\n"));
                        else
                        {
                            safeErrorPrint("Node: failed to dispatch transaction to friend via global queue. Error: ");
                            
                            /* Inform the sender the transaction's processing failed */
                            if(!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                            {
                                /* Che facciamo in questo caso ??? */
                                safeErrorPrint("Node: failed to inform sender of transaction that the transaction wasn't processed. Error: ");
                            }
                        }
                    }
                    else
                {
                    friendTp = msgget(key, 0600);
                    if (friendTp == -1)
                    {
                        safeErrorPrint("Node: failed to connect to friend's transaction pool. Error: ");
                        if (sendOnGlobalQueue(&trans, *(friends_node + i), TRANSTPFULL, -1))
                            write(STDOUT_FILENO,
                                  "Node: transaction successfully dispatched to friend via global queue.\n",
                                  strlen("Node: transaction successfully dispatched to friend via global queue.\n"));
                        else
                        {
                            safeErrorPrint("Node: failed to dispatch transaction to friend via global queue. Error: ");

                            /* Inform the sender the transaction's processing failed */
                            if(!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                            {
                                /* Che facciamo in questo caso ??? */
                                safeErrorPrint("Node: failed to inform sender of transaction that the transaction wasn't processed. Error: ");
                            }
                        }
                    }
                    else
                    {
                        /* Create a copy of the message read */
                        aus.transaction = trans.transaction;
                        aus.mType = *(friends_node + i);
                        if (msgsnd(friendTp, &aus, sizeof(Transaction), IPC_NOWAIT) == -1)
                        {
                            /*
                                Reinserirla nella coda globale con un'operazione di rollback sarebbe inutile:
                                tanto vale mandarla all'amico
                            */
                            /*
                                Coda dell'amico piena ==> inviare su TP globale
                            */
                            if (sendOnGlobalQueue(&trans, *(friends_node + i), TRANSTPFULL, -1))
                                write(STDOUT_FILENO,
                                      "Node: transaction successfully dispatched to friend via global queue.\n",
                                      strlen("Node: transaction successfully dispatched to friend via global queue.\n"));
                            else
                            {
                                safeErrorPrint("Node: failed to dispatch transaction to friend via global queue. Error: ");

                                /* Inform the sender the transaction's processing failed */
                                if(!sendOnGlobalQueue(&trans, trans.transaction.sender, FAILEDTRANS, 0))
                                {
                                    /* Che facciamo in questo caso ??? */
                                    safeErrorPrint("Node: failed to inform sender of transaction that the transaction wasn't processed. Error: ");
                                }
                            }
                        }
                        else
                        {
                            write(STDOUT_FILENO,
                                  "Node: transaction successfully dispatched to friend.\n",
                                  strlen("Node: transaction successfully dispatched to friend.\n"));
                        }
                    }
                }
                }
            }
        } 
        else if (trans.msgContent == NEWFRIEND)
        {
            /*
                Aggiunta amico su richiesta master
            */
            for (i = 0; i < SO_FRIENDS_NUM && !found; i++)
            {
                if (friends_node[i] == 0)
                    found = TRUE;
            }

            if (found)
                friends_node[i] = trans.friend;
            else
            {
                /*
                        CORREGGERE: Come gestiamo questo caso?
                        Estendiamo il vettore degli amici oppure segnaliamo un errore??
                    */
                unsafeErrorPrint("Node: maximun number of friends reached. New friend is discarded.\n");
            }
            /*
                    CORREGGERE: dovremmo testare lo stato??
                */
        }
        else
        {
            /*
                È POSSIBILE CHE IL MESSAGGIO LETTO NON SIA DEI DUE TIPI CERCATI??
                SE SÌ, OCCORRE REINSERIRLO SULLA CODA GLOBALE!
            */
            /* the message wasn't the one we were looking for, so we reinsert it on the global queue */
            if(msgsnd(globalQueueId, &trans, sizeof(MsgGlobalQueue)-sizeof(long), 0) == -1)
            {
                unsafeErrorPrint("Node: failed to reinsert on global queue a message read from it. Error: ");
            }
        }
    }
}

/**
 * @brief Function that sends on the global queue a message depending on the parameters that
 * the function receives.
 * @param trans message to send on the global queue
 * @param pid pid of the receiver of the message
 * @param cnt type of content of the message
 * @param hp value (positive or negative) to add to the number of hops of the transaction
 * @return Returns TRUE if successfull, FALSE in case an error occurred while sending the message.
 */
boolean sendOnGlobalQueue(MsgGlobalQueue * trans, pid_t pid, GlobalMsgContent cnt, long hp)
{
    boolean ret = TRUE;

    trans->mType = pid;
    trans->msgContent = cnt;
    trans->hops += hp;
    if (msgsnd(globalQueueId, &trans, sizeof(MsgGlobalQueue)-sizeof(long), 0) == -1)
    {
        ret = FALSE;
    }

    return ret;
}

/**
 * @brief Function that extracts randomly a friend node which to send a transaction.
 * @return Returns the index of the selected friend node to pick from the list of friends,
 * -1 if the function generates an error. 
 */
int extractFriendNode()
{
    int n = -1;
    struct sembuf sops;
    struct timespec now;
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
                safeErrorPrint("Node: failed to reserve write nodesList semaphore. Error: ");
                /* do we need to end execution ? */
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if(semop(nodeListSem, &sops, 1) != -1)
        {
            do
            {
                clock_gettime(CLOCK_REALTIME, &now);
                n = now.tv_nsec % (SO_FRIENDS_NUM + 1);
            } while (nodesList[n].procState != ACTIVE);
            /* cicla finché il nodo scelto casualmente non è attivo */

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
                        safeErrorPrint("Node: failed to release write nodesList semaphore. Error: ");
                        /* do we need to end execution ? */
                    }
                }

                sops.sem_num = 0;
                sops.sem_op = 1;
                if(semop(nodeListSem, &sops, 1) != -1)
                {
                    return n;
                }
                else
                {
                    safeErrorPrint("Node: failed to release mutex nodesList semaphore. Error: ");
                    /* do we need to end execution ? */
                }
            }
            else
            {
                safeErrorPrint("Node: failed to reserve mutex nodesList semaphore. Error: ");
                /* do we need to end execution ? */
            }
        }
        else
        {
            safeErrorPrint("Node: failed to release mutex nodesList semaphore. Error: ");
            /* do we need to end execution ? */
        }
    }
    else
    {
        safeErrorPrint("Node: failed to reserve mutex nodesList semaphore. Error: ");
        /* do we need to end execution ? */
    }

    return -1;
}

/**
 * @brief Function that end the execution of the node.
 * @param sig the signal that called the handler
 */
void endOfExecution(int sig){
    deallocateIPCFacilities();
}

/**
 * @brief Function that deallocates the IPC facilities allocated for the node.
 */
void deallocateIPCFacilities()
{
    /*
        Cose da eliminare:
            -la tp la elimina il master (in modo che possa contare
            le transazioni rimaste)
            -collegamento ai registri
            -i semafori li dealloca il master

            In sostanza bisogna soltanto scollegarsi dalla memoria condivisa
            in modo che l'eliminazione ordinata dal master sia effettiva
            e deallocare la memoria allocata dinamicamente
    */
    Register **aus = regPtrs;
    int **ausPtr = NULL;
    int i = 0;

    write(STDOUT_FILENO,
          "Node: deatching from register's partitions...\n",
          strlen("Node: deatching from register's partitions...\n"));
    
    for (i = 0; i < REG_PARTITION_COUNT; i++){
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
                safeErrorPrint("Node: failed to detach from register's partition. Error: ");
            }
        }
    }
    
    if (regPtrs != NULL)
        free(regPtrs);

    write(STDOUT_FILENO,
          "Node: deatching from nodes list...\n",
          strlen("Node: deatching from nodes list...\n"));
    
    if (shmdt(nodesList) == -1)
    {
        if (errno != EAGAIN)
            safeErrorPrint("Node: failed to detach from nodes list. Error: ");
    }

    write(STDOUT_FILENO,
          "Node: deatching from nodes list's number of readers shared variable...\n",
          strlen("Node: deatching from nodes list's number of readers shared variable...\n"));
    
    if (shmdt(noNodeSegReadersPtr) == -1)
    {
        if (errno != EAGAIN)
            safeErrorPrint("Node: failed to detach from nodes list's number of readers shared variable. Error: ");
    }

    if (friends_node != NULL)
        free(friends_node);

    write(STDOUT_FILENO, 
          "Node: cleanup operations completed. Process is about to end its execution...\n",
          strlen("Node: cleanup operations completed. Process is about to end its execution...\n"));
}
#pragma endregion
/*** END FUNCTIONS IMPLEMENTATION ***/
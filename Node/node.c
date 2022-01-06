#include "node.h"

/*
    Aggiungere invio ad amici
    Test
    Sistemare file .h
    Memory leaks e chiamate unsafe con gcc
    Sostituire getpid con una var globale per
    ridurre il numero di chiamate di sistema
*/

/* Macro that rappresents the sender with id -1 in Transactions */
#define NO_SENDER -1

/*****        Global structures        *****/
/*******************************************/
Register **regPtrs = NULL;
int *regPartsIds = NULL;

/*
    User e Nodes list sono inutili
*/
int usersListId = -1;
ProcListElem *usersList = NULL;

int nodesListId = -1;
ProcListElem *nodesList = NULL;

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
// noReadersPartitionsPtrs[0]: pointer to the first partition's shared variable
// noReadersPartitionsPtrs[1]: pointer to the second partition's shared variable
// noReadersPartitionsPtrs[2]: pointer to the third partition's shared variable*/
int userListSem = -1;                 /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                    // to read and write users list*/
int noUserSegReaders = -1;            /* id of the shared memory segment that contains the variable used to syncronize
                           // readers and writes access to users list*/
int *noUserSegReadersPtr = NULL;

int nodeListSem = -1; /* Id of the set that contais the semaphores (mutex = 0, read = 1, write = 2) used
                      // to read and write nodes list*/
int tpId = -1;

pid_t *friends_node;

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
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

boolean sembufInit(struct sembuf *, int);
void reinsertTransactions(Block);
void endOfExecution(int);
int readConfigParameters();
boolean createIPCFacilties();
boolean initializeIPCFacilities();
void dispatchToFriend();
boolean assignEnvironmentVariables();
void sendTransaction();
void deallocateIPCFacilities();
boolean sendOnGlobalQueue(MsgGlobalQueue *, pid_t, GlobalMsgContent, long);

int main()
{
    int exitCode = EXIT_FAILURE;
    /* To be read from environment variables */
    int minSim = 1;           /* milliseconds*/
    int maxSim = 10;          /* milliseconds*/
    unsigned int simTime = 0; /* simulation length in seconds*/
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
    msgbuff mybuf;
    struct sembuf sops[3];
    int num_bytes = 0;
    long rcv_type;
    int contMex = 0;
    boolean error = FALSE;

    /*
        Il nodo potrebbe essere interrotto soltanto
        dal segnale di fine simulazione, ma in tal caso
        l'esecuzione della procedura in corso non ripartirebbe 
        da capo, quindi qui si può usare codice non rientrante
    */

    /* Assigns the values ​​of the environment variables to the global variables */
    /*
        Testare codice d'errore
    */
    if (assignEnvironmentVariables())
    {
        /* Allocate the array that will contain friends pid */
        /* CORREGGERE: TESTARE ERRORI*/
        friends_node = calloc(SO_NODES_NUM, sizeof(pid_t));
        if (friends_node != NULL){
            printf("Node %d: hooking up of IPC facilitites...\n", getpid());

            if (createIPCFacilties() == TRUE)
            {
                /*
                CORREGGERE: SEGNALAZIONE ERRORI
            */

                if (initializeIPCFacilities() == TRUE){

                    /* Receives all friends pid from global message queue and stores them in the array */
                    while (contMex < SO_FRIENDS_NUM && !error)
                    {
                        rcv_type = getpid();
                        /*
                        CORREGGERE: aggiungere segnalazione errori
                    */
                        num_bytes = msgrcv(globalQueueId, &mybuf, sizeof(pid_t), rcv_type, 0);
                        if (num_bytes == -1){
                            safeErrorPrint("Node: failed to initialize friends list. Error: ");
                            error = TRUE;
                        } else {
                            friends_node[contMex] = mybuf.pid;
                            contMex++;
                        }
                    }

                    /*
                    PROVVISORIO, CORREGGERE
                */
                    for (i = 0; i < SO_FRIENDS_NUM; i++)
                    {
                        printf("Nodo %d -> Amico: %d\n", getpid(), friends_node[i]);
                    }

                    if (error){
                        /* Wait all processes are ready to start the simulation */
                        printf("Node %ld is waiting for simulation to start....\n", (long)getpid());
                        sops[0].sem_op = 0;
                        sops[0].sem_num = 0;
                        sops[0].sem_flg = 0;
                        if (semop(fairStartSem, &sops[0], 1) == -1)
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
                                        Correggere: vedere se si meglio metterlo nel while senza usare l'allarme
                                    */
                                            if (alarm(TRANS_FRIEND_INTERVAL) == 0)
                                            {
                                                timeSinceEpoch = time(NULL);
                                                if (timeSinceEpoch == (time_t)-1)
                                                    unsafeErrorPrint("Node: failed to initialize random generator's seed. Error");
                                                else
                                                {
                                                    /* Vedere se metterlo nel ciclo di vita */
                                                    srand(time(NULL) - getpid()); /*Inizializza il seme di generazione*/
                                                                                /*Essendoci più processi nodo in esecuzione, è probabile che
                                    alcuni nodi vengano eseguiti nello stesso secondo e che si abbia quindi 
                                    la stessa sequenza di numeri random. Per evitare ciò sottraiamo il PID*/
                                                    if (sembufInit(reservation, -1) && sembufInit(release, 1)){
                                                        printf("Node: starting lifecycle...\n");
                                                        while (!waitForTerm)
                                                        {
                                                            /*
                                                        Check if a transaction was sent when TP was full
                                                        and dispatch it to another node
                                                    */
                                                            sendTransaction();
                                                            /*
                                            PRECONDIZIONE:
                                                minSim e maxSim sono state caricate leggendole
                                                dalle variabili d'ambiente
                                        */
                                                            /*generates a random number in [minSim, maxSim]*/
                                                            printf("Node: elaborating transactions' block...\n");
                                                            simTime = rand() % (maxSim + 1) + minSim;
                                                            /* Simulates the computation by waiting a certain amount of time */
                                                            if (sleep(simTime / 1000) == 0)
                                                            {
                                                                /* Generating a Block of SO_BLOCK_SIZE-1 Transitions from TP */
                                                                /* SO_BLOCK_SIZE is initialized reading the value from environment variables */
                                                                i = 0;
                                                                MsgTP new_trans;

                                                                /* Generating reward transaction for node an put it in extractedBlock */
                                                                Transaction rew_tran;
                                                                rew_tran.sender = NO_SENDER;
                                                                rew_tran.receiver = getpid();
                                                                rew_tran.reward = 0.0;
                                                                rew_tran.amountSend = 0.0;                          /* we now set it to 0, then we will count the rewards */
                                                                clock_gettime(CLOCK_REALTIME, &rew_tran.timestamp); /* get timestamp for transaction */

                                                                /* cycle for extract transaction from TP */
                                                                /*
                                                Estrae SO_BLOCK_SIZE-1 transazioni dalla transaction pool
                                            */
                                                                while (i < SO_BLOCK_SIZE - 1)
                                                                {
                                                                    /* now receiving the message (transaction from TP) */
                                                                    num_bytes = msgrcv(tpId, &new_trans, sizeof(new_trans) - sizeof(long), getpid(), 0);

                                                                    if (num_bytes >= 0)
                                                                    {
                                                                        /* read transaction from tpList */
                                                                        extractedBlock.transList[i] = new_trans.transaction;
                                                                        /* adding reward of transaction in amountSend of reward_transaction */
                                                                        rew_tran.amountSend += new_trans.transaction.reward;

                                                                        candidateBlock.transList[i] = new_trans.transaction;

                                                                        extractedBlock.bIndex = i++;
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

                                                                /* creating candidate block by coping transactions in extracted block */
                                                                /* VEDERE SE CAMBIARE - PER ME È PERDITA DI TEMPO (es. togliendo candidateBlock e usando direttamente extractedBlock */
                                                                /*
                                                Per ridurre la perdita di tempo si può spostare questa operazione dentro il ciclo di estrazione
                                            */
                                                                /*
                                            i = 0;
                                            while(i < SO_BLOCK_SIZE-1)
                                            {
                                                candidateBlock.transList[i] = extractedBlock.transList[i];
                                                i++;
                                            }*/

                                                                /* putting reward transaction in extracted block */
                                                                candidateBlock.transList[i] = rew_tran;
                                                                /*candidateBlock.bIndex = i;*/

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
                                                                /*
                                                Entry section
                                            */
                                                                
                                                                printf("Node: trying to write transactions on register...\n");
                                                                if (semop(rdPartSem, reservation, REG_PARTITION_COUNT) == -1)
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
                                                                                safeErrorPrint("Node: failed to signal Master for the end of simulation. Error: ");
                                                                            }

                                                                            waitForTerm = TRUE;
                                                                        }

                                                                        /*
                                                        Exit section
                                                    */
                                                                        printf("Node: releasing register's partition...\n");
                                                                        if (semop(wrPartSem, release, REG_PARTITION_COUNT) == -1)
                                                                            unsafeErrorPrint("Node: failed to release register partitions' writing semaphore. Error: ");
                                                                        else {
                                                                            if (semop(rdPartSem, release, REG_PARTITION_COUNT) == -1)
                                                                                unsafeErrorPrint("Node: failed to release register partitions' reading semaphore. Error: ");
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            else
                                                            {
                                                                /*
                                                A node can only be interrupted by the end of simulation signal
                                            */
                                                                unsafeErrorPrint("Node: an unexpected event occured before the end of the computation.");
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
            }
            deallocateIPCFacilities();
        } else {
            unsafeErrorPrint("Node: failed to allocate friends' array. Error: ");
        }    
    }
    else
    {
        /*
        Se un nodo è terminato ed un processo prova a mandargli una transazione bisogna
        segnalare un errore
       */
        unsafeErrorPrint("Node: about to terminate execution...");
    }

    printf("Node: about to terminate execution...");
    exit(exitCode);
}

/***** Function that assigns the values ​​of the environment *****/
/***** variables to the global variables defined above     *****/
/***************************************************************/
boolean assignEnvironmentVariables()
{
    /*
        CORREGGERE: Vedere quale versione tenere
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
    SO_FRIENDS_NUM = atoi(getenv("SO_FRIENDS_NUM"));*/
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
/***************************************************************/
/***************************************************************/

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

void reinsertTransactions(Block failedTrs)
{
    char *aus = NULL;

    aus = (char *)calloc(sizeof(char), 50);
    while (failedTrs.bIndex == 0)
    {
        failedTrs.bIndex--;
        if (msgsnd(tpId, &(failedTrs.transList[failedTrs.bIndex]), sizeof(Transaction), 0) == -1)
        {
            /*
                CORREGGERE: ovremmo segnalarlo al sender???
            */
            sprintf(aus, "Node: failed to reinsert transaction number %d.", failedTrs.bIndex);
            unsafeErrorPrint(aus);
        }
    }
    printf("Node: Transactions reinserted successfully!\n");

    free(aus);
}

void endOfExecution(int sig){
    deallocateIPCFacilities();
}

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

    write(STDOUT_FILENO,
          "Node: deatching from register's partitions...\n",
          strlen("Node: deatching from register's partitions...\n"));
    while (regPtrs != NULL)
    {
        if (shmdt(*regPtrs) == -1)
        {
            if (errno != EINVAL) {
            /*
                Implementare un meccanismo di retry??
                Contando che non è un errore così frequente si potrebbe anche ignorare...
                Non vale la pena, possiamo limitarci a proseguire la deallocazione
                riducendo al minimo il memory leak
            */
            safeErrorPrint("Node: failed to detach from register's partition. Error: ");
            }
        }
        regPtrs++;
    }
    if (regPtrs != NULL)
        free(regPtrs);

    write(STDOUT_FILENO,
          "Node: deatching from users list...\n",
          strlen("Node: deatching from users list...\n"));
    if (shmdt(usersList) == -1)
    {
        if (errno != EAGAIN)
            safeErrorPrint("Node: failed to detach from users list. Error: ");
    }

    write(STDOUT_FILENO,
          "Node: deatching from nodes list...\n",
          strlen("Node: deatching from nodes list...\n"));
    if (shmdt(nodesList) == -1)
    {
        if (errno != EAGAIN)
            safeErrorPrint("Node: failed to detach from nodes list. Error: ");
    }

    if (noReadersPartitions != NULL)
        free(noReadersPartitions);

    write(STDOUT_FILENO,
          "Node: deatching from partitions' number of readers shared variable...\n",
          strlen("Node: deatching from partitions' number of readers shared variable...\n"));
    ausPtr = noReadersPartitionsPtrs;
    while (noReadersPartitionsPtrs != NULL)
    {
        if (shmdt(*noReadersPartitionsPtrs) == -1)
        {
            if (errno != EAGAIN)
                safeErrorPrint("Node: failed to detach from partitions' number of readers shared variable. Error: ");
        }

        noReadersPartitionsPtrs++;
    }
    if (ausPtr != NULL)
        free(ausPtr);

    write(STDOUT_FILENO,
          "Node: deatching from users list's number of readers shared variable...\n",
          strlen("Node: deatching from users list's number of readers shared variable...\n"));
    if (shmdt(noUserSegReadersPtr) == -1)
    {
        if (errno != EAGAIN)
            safeErrorPrint("Node: failed to detach from users list's number of readers shared variable. Error: ");
    }

    if (friends_node != NULL)
        free(friends_node);

    printf("Node: cleanup operations completed. Process is about to end its execution...\n");
}

/****   Function that creates the ipc structures used in the project    *****/
/****************************************************************************/
boolean createIPCFacilties()
{
    regPtrs = (Register **)malloc(REG_PARTITION_COUNT * sizeof(Register *));
    TEST_MALLOC_ERROR(regPtrs);

    regPartsIds = (int *)malloc(REG_PARTITION_COUNT * sizeof(int));
    TEST_MALLOC_ERROR(regPartsIds);
    /*
        noReadersPartitions e noReadersPartitionsPtrs vanno allocati
        perchè sono vettori, quindi dobbiamo allocare un'area di memoria
        abbastanza grande da contenere REG_PARTITION_COUNT interi/puntatori ad interi
    */
    noReadersPartitions = (int *)calloc(REG_PARTITION_COUNT, sizeof(int));
    TEST_MALLOC_ERROR(noReadersPartitions);

    noReadersPartitionsPtrs = (int **)calloc(REG_PARTITION_COUNT, sizeof(int *));
    TEST_MALLOC_ERROR(noReadersPartitions);

    return TRUE;
}
/****************************************************************************/
/****************************************************************************/

/*****  Function that initialize the ipc structures used in the project *****/
/****************************************************************************/
boolean initializeIPCFacilities()
{
    /* Initialization of semaphores*/
    /*
        CORREGGERE: modificare segnalazione errori
        in modo da deallocare le facilities (e terminare la simulazione ??)
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
    TEST_SHMAT_ERROR(regPtrs[0]);
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[1]);
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    TEST_SHMAT_ERROR(regPtrs[2]);

    key = ftok(SHMFILEPATH, USERLISTSEED);
    FTOK_TEST_ERROR(key);
    usersListId = shmget(key, SO_USERS_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(usersListId);
    usersList = (ProcListElem *)shmat(usersListId, NULL, SHM_RDONLY);
    TEST_SHMAT_ERROR(usersList);

    key = ftok(SHMFILEPATH, NODESLISTSEED);
    FTOK_TEST_ERROR(key);
    nodesListId = shmget(key, SO_NODES_NUM * sizeof(ProcListElem), 0600);
    SHM_TEST_ERROR(nodesListId);
    nodesList = (ProcListElem *)shmat(nodesListId, NULL, SHM_RDONLY);
    TEST_SHMAT_ERROR(nodesList);

    /* Aggiungere segmenti per variabili condivise*/
    noReadersPartitions[0] = shmget(ftok(SHMFILEPATH, NOREADERSONESEED), sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noReadersPartitions[0]);
    noReadersPartitionsPtrs[0] = (int *)shmat(noReadersPartitions[0], NULL, 0);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[0]);

    noReadersPartitions[1] = shmget(ftok(SHMFILEPATH, NOREADERSTWOSEED), sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noReadersPartitions[1]);
    noReadersPartitionsPtrs[1] = (int *)shmat(noReadersPartitions[1], NULL, 0);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[1]);

    noReadersPartitions[2] = shmget(ftok(SHMFILEPATH, NOREADERSTHREESEED), sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noReadersPartitions[2]);
    noReadersPartitionsPtrs[2] = (int *)shmat(noReadersPartitions[2], NULL, 0);
    TEST_SHMAT_ERROR(noReadersPartitionsPtrs[2]);

    noUserSegReaders = shmget(ftok(SHMFILEPATH, NOUSRSEGRDERSSEED), sizeof(SO_USERS_NUM), 0600);
    SHM_TEST_ERROR(noUserSegReaders);
    noUserSegReadersPtr = (int *)shmat(noUserSegReaders, NULL, 0);
    TEST_SHMAT_ERROR(noUserSegReadersPtr);

    key = ftok(MSGFILEPATH, getpid());
    FTOK_TEST_ERROR(key);
    tpId = msgget(key, 0600);
    MSG_TEST_ERROR(tpId);

    return TRUE;
}
/****************************************************************************/
/****************************************************************************/

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
    if (msgrcv(tpId, &aus, sizeof(MsgTP), getpid(), 0) == -1)
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
        /*
        Bisogna mettere srand????
      */
        i = rand() % (SO_FRIENDS_NUM + 1);
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

void sendTransaction()
{
    MsgGlobalQueue trans;
    int i = 0;
    key_t key = -1;
    int friendTp = -1;
    MsgTP aus;
    pid_t * listPtr = NULL;
    pid_t * prevPtr = NULL;

    /*
        Fare ciclo per tutte le transazioni ???
        Potrebbe avere senso, in fondo non dovrebbero essere troppe.
        Il rischio è quello di fare un numero di system call molto elevate
        in un'invocazione e molto poco nella altre.
        Così invece le system call vengono meglio distribuite
    */
    if (msgrcv(globalQueueId, &trans, sizeof(MsgGlobalQueue), getpid(), IPC_NOWAIT) == -1)
    {
        if (errno != ENOMSG)
        {
            unsafeErrorPrint("Node: failed to check existence of transactions on global queue. Error: ");
        }
    } else {
        /*
            Ora trans contiene la transazione da mandare ad un amico/master
        */
        if (trans.msgContent == TRANSTPFULL){
            if (trans.hoops == 0)
            {
                /* Invio al master */
                if (sendOnGlobalQueue(&trans, getppid(), NEWNODE, 0))
                {
                    /*
                Correggere: dovremmo avvisare il sender ??
               */
                    unsafeErrorPrint("Node: failed to dispatch transaction to master. Error: ");
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
                i = rand() % (SO_FRIENDS_NUM + 1);
                key = ftok(MSGFILEPATH, *(friends_node + i));
                if (key == -1)
                {
                    safeErrorPrint("Node: failed to connect to friend's transaction pool. Error: ");
                    if (sendOnGlobalQueue(&trans, *(friends_node + i), TRANSTPFULL, -1))
                        write(STDOUT_FILENO,
                              "Node: transaction successfully dispatched to friend via global queue.\n",
                              strlen("Node: transaction successfully dispatched to friend via global queue.\n"));
                    else
                    {
                        /*
                                Correggere: dovremmo avvisare il sender ??
                            */
                        unsafeErrorPrint("Node: failed to dispatch transaction to friend via global queue. Error: ");
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
                            /*
                                Correggere: dovremmo avvisare il sender ??
                            */
                            unsafeErrorPrint("Node: failed to dispatch transaction to friend via global queue. Error: ");
                        }
                    }
                    else
                    {
                        aus.transaction = trans.transaction;
                        aus.mType = *(friends_node + i);
                        if (msgsnd(friendTp, &aus, sizeof(Transaction), 0600 | IPC_NOWAIT) == -1)
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
                                /*
                                Correggere: dovremmo avvisare il sender ??
                            */
                                unsafeErrorPrint("Node: failed to dispatch transaction to friend via global queue. Error: ");
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
        } else if (trans.msgContent == NEWFRIEND){
            /*
                Mettere qui aggiunta amico su richiesta master
            */
           if (listPtr == NULL){
               /*
                È improbabile che la lista degli amici sia vuota
                ma il testo non lo esclude, quindi meglio prevenire
               */
              listPtr = trans.friend.procId;
           } else {
               listPtr = friends_node;
               prevPtr = NULL;
               while (listPtr != NULL)
               {
                   prevPtr = listPtr;
                   listPtr++;
               }
               /*
                POSTCONDIZIONE: prevPtr contiene l'ultimo nodo della lista
               */
              prevPtr = trans.friend.procId;
              /*
                CORREGGERE: dovremmo testare lo stato??
              */
           }
           
        }
        
    }

    
}

boolean sendOnGlobalQueue(MsgGlobalQueue * trans, pid_t pid, GlobalMsgContent cnt, long hp)
{
    boolean ret = TRUE;

    trans->mType = pid;
    trans->msgContent = cnt;
    trans->hoops += hp;
    if (msgsnd(globalQueueId, &trans, sizeof(MsgGlobalQueue), 0) == -1)
    {
        ret = FALSE;
    }

    return ret;
}
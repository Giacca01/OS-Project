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
int noNodeSegReaders = -1;            /* id of the shared memory segment that contains the variable used to syncronize
                           // readers and writers access to nodes list*/
int *noNodeSegReadersPtr = NULL;
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

TransList *transactionsSent = NULL; /* deve essere globale, altrimenti non posso utilizzarla nella funzione per la generazione della transazione */
int num_failure = 0; /* deve essere globale, altrimenti non posso utilizzarla nella funzione per la generazione della transazione */
struct timespec now;

void transactionGeneration(int);
void endOfExecution(int);
TransList * addTransaction(TransList * transSent, Transaction *t);
void freeTransList(TransList * transSent);
pid_t extractReceiver(pid_t);
pid_t extractNode();

int main(int argc, char * argv[])
{
    struct sigaction actEndOfExec;
    struct sigaction actGenTrans;
    sigset_t mask;

    if(sigfillset(&mask) == -1)
        unsafeErrorPrint("User: failed to initialize signal mask. Error: ");
    else
    {
        actEndOfExec.sa_handler = endOfExecution;
        actEndOfExec.sa_mask = mask;
        if(sigaction(SIGUSR1, &actEndOfExec, NULL) == -1)
            unsafeErrorPrint("User: failed to set up end of simulation handler. Error: ");
        else
        {
            actGenTrans.sa_handler = transactionGeneration;
            actGenTrans.sa_mask = mask;
            if(sigaction(SIGUSR2, &actGenTrans, NULL) == -1)
                unsafeErrorPrint("User: failed to set up transaction generation handler. Error: ");
            else
            {
                printf("User %5d: starting lifecycle...\n", getpid());
                while(1);
            }
        }
    }
}

void endOfExecution(int sig)
{
    /*
     * Cose da eliminare:
     *  - collegamento alla memoria condivisa
     *  - i semafori li dealloca il master
     *  - bisogna chiudere le write/read end della globalQueue? No, lo fa il master!
     */
    int exitCode = EXIT_FAILURE;
    int i = 0;

    /*
        CORREGGERE CON NUOVO MECCANISMO DI RILEVAZIONE ERRORI
    */

    /*
        La dprintf è utilissima.
        Fontana è un genio!!
    */
    dprintf(STDOUT_FILENO, "User: detaching from register's partitions...\n");
    
    for(i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if(shmdt(regPtrs[i]) == -1)
        {
            /* serve riprovare per un certo numero di tentativi? */
            safeErrorPrint("User: failed to detach from register's partition. Error: ");
        }
    }
    free(regPtrs);
    free(regPartsIds);

    dprintf(STDOUT_FILENO, "User: detaching from users list...\n");
    if(shmdt(usersList) == -1)
    {
        safeErrorPrint("User: failed to detach from users list. Error: ");
    }

    dprintf(STDOUT_FILENO, "User: detaching from nodes list...\n");
    if(shmdt(nodesList) == -1)
    {
        safeErrorPrint("User: failed to detach from nodes list. Error: ");
    }

    dprintf(STDOUT_FILENO, "User: detaching from partitions' number of readers shared variable...\n");
    for(i = 0; i < REG_PARTITION_COUNT; i++)
    {
        if(shmdt(noReadersPartitionsPtrs[i]) == -1)
        {
            safeErrorPrint("User: failed to detach from partitions' number of readers shared variable. Error: ");
        }
    }
    free(noReadersPartitions);
    free(noReadersPartitionsPtrs);

    dprintf(STDOUT_FILENO, "User: detaching from users list's number of readers shared variable...\n");
    if(shmdt(noUserSegReadersPtr) == -1)
    {
        safeErrorPrint("User: failed to detach from users list's number of readers shared variable. Error: ");
    }

    dprintf(STDOUT_FILENO, "User: detaching from nodes list's number of readers shared variable...\n");
    if(shmdt(noNodeSegReadersPtr) == -1)
    {
        safeErrorPrint("User: failed to detach from nodes list's number of readers shared variable. Error: ");
    }

    dprintf(STDOUT_FILENO, "User: cleanup operations completed. Process is about to end its execution...\n");
    
    /* freeing the list of sent transactions */
    freeTransList(transactionsSent);
    
    exit(exitCode); /* vedere se metterlo qui o spostarlo */
}

void transactionGeneration(int sig)
{
    int bilancio, receiver_node_index, queueId;
    long destUserPid;
    Transaction new_trans;
    MsgTP msg_to_node;
    key_t key;
    pid_t receiver_node, receiver_user;
    struct timespec request, remaining;
    MsgGlobalQueue msgOnGQueue;
    struct sembuf sops;
    
    sops.sem_flg = 0;

    /* controllare ?? */
    bilancio = computeBudget(transactionsSent); /* calcolo del bilancio */
    srand(getpid());

    /* devo fare controllo bilancio ???? Penso di sì */
    if(bilancio > 2)
    {
        /* deve essere globale */
        num_failure = 0; /* sono riuscito a mandare la transazione, azzero il counter dei fallimento consecutivi */

        /* Extracts the receiving user randomly */
        receiver_user = extractReceiver(getpid());
        if(receiver_user == -1)
            safeErrorPrint("User: failed to extract user receiver. Error: ");
        else 
        {
            /* Generating transaction */
            new_trans.sender = getpid();
            new_trans.receiver = receiver_user;
            new_trans.amountSend = (rand()%bilancio)+2; /* calcolo del budget fra 2 e il budget (così lo fa solo intero) */
            new_trans.reward = new_trans.amountSend*SO_REWARD; /* se supponiamo che SO_REWARD sia un valore (percentuale) espresso tra 0 e 1 */
            /*new_trans.reward = (new_trans.amountSend/100)*SO_REWARD; /* se supponiamo che SO_REWARD sia un valore (percentuale) espresso tra 1 e 100 */
            if(new_trans.reward < 1)
                new_trans.reward = 1;

            clock_gettime(CLOCK_REALTIME, &new_trans.timestamp); /* get timestamp for transaction */

            /* extracting node which to send the transaction */
            receiver_node = extractNode();
            if(receiver_node == -1)
                safeErrorPrint("User: failed to extract node which to send transaction on TP. Error: ");
            else
            {
                /* preparing message to send on node's queue */
                msg_to_node.mType = receiver_node;
                msg_to_node.transaction = new_trans;

                /* generating key to retrieve node's queue */
                key = ftok(MSGFILEPATH, receiver_node);
                if(key == -1)
                    safeErrorPrint("User: ftok failed during node's queue retrieving. Error: ");
                else
                {
                    /* retrieving the message queue connection */
                    queueId = msgget(key, 0600);
                    if(queueId == -1)
                        safeErrorPrint("User: failed to connect to node's transaction pool. Error: ");
                    else
                    {
                        /* Inserting new transaction on list of transaction sent */
                        transactionsSent = addTransaction(transactionsSent, &new_trans);
                        
                        /* sending the transaction to node */
                        dprintf(STDOUT_FILENO, "User: sending the created transaction to the node...\n");
                        if(msgsnd(queueId, &msg_to_node, sizeof(Transaction), IPC_NOWAIT) == -1)
                        {
                            if(errno == EAGAIN)
                            {
                                /* TP of Selected Node was full, we need to send the message on the global queue */
                                dprintf(STDOUT_FILENO, "User: transaction pool of selected node was full. Sending transaction on global queue...\n");
                                
                                msgOnGQueue.mType = receiver_node;
                                msgOnGQueue.msgContent = TRANSTPFULL;
                                msgOnGQueue.transaction = new_trans;
                                msgOnGQueue.hops = 0;
                                if(msgsnd(globalQueueId, &msgOnGQueue, sizeof(msgOnGQueue)-sizeof(long), 0) == -1)
                                    safeErrorPrint("User: failed to send transaction on global queue. Error: ");
                            }
                            else
                                safeErrorPrint("User: failed to send transaction generated on event to node. Error: ");
                        }
                        else
                        {
                            dprintf(STDOUT_FILENO, "User: transaction generated on event correctly sent to node.\n");
                            
                            /* Wait a random time in between SO_MIN_TRANS_GEN_NSEC and SO_MAX_TRANS_GEN_NSEC */
                            request.tv_sec = 0;
                            request.tv_nsec = (rand() % SO_MAX_TRANS_GEN_NSEC) + SO_MIN_TRANS_GEN_NSEC;
                            
                            dprintf(STDOUT_FILENO, "User: processing the transaction...\n");
                            
                            if (nanosleep(&request, &remaining) == -1)
                                safeErrorPrint("User: failed to simulate wait for processing the transaction. Error: ");
                        }
                    }
                }
            }
        }
    }
    else
    {
        dprintf(STDOUT_FILENO, "User: not enough money to make a transaction...\n");
        
        num_failure++; /* incremento il numero consecutivo di volte che non riesco a mandare una transazione */
        if(num_failure == SO_RETRY)
        {
            /* non sono riuscito a mandare la transazione per SO_RETRY volte, devo terminare */
            kill(getpid(), SIGUSR1);
        }
    }
}

TransList * addTransaction(TransList * transSent, Transaction *t)
{
    if(t == NULL) {
        dprintf(STDERR_FILENO, "User: transaction passed to function is a NULL pointer.\n");
        return NULL;
    }

    /* insertion of new transaction to list */
    TransList * new_el = (TransList*) malloc(sizeof(TransList));
    new_el->currTrans = *t;
    new_el->nextTrans = transSent;
    transSent = new_el;
    
    return transSent;
}

void freeTransList(TransList * transSent)
{
    if(transSent == NULL)
        return;

    freeTransList(transSent->nextTrans);
    free(transSent);
}

/**
 * Function that extracts randomly a receiver for the transaction which is not the same user 
 * that generated the transaction, whose pid is the argument passed to the function.
 * Returns the pid of the selected user in the usersList shared array, -1 if the function generates an error.
 */
pid_t extractReceiver(pid_t pid)
{
    int n = -1;
    struct sembuf sops;
    pid_t pid_to_return = -1;
    sops.sem_flg = 0;

    sops.sem_num = 0; 
    sops.sem_op = -1;
    if(semop(userListSem, &sops, 1) != -1)
    {
        (*noUserSegReadersPtr)++;
        if((*noUserSegReadersPtr) == 1)
        {
            sops.sem_num = 2;
            sops.sem_op = -1;
            if(semop(userListSem, &sops, 1) == -1)
            {
                safeErrorPrint("User: failed to reserve write usersList semaphore. Error: ");
                /* do we need to end execution ? */
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if(semop(userListSem, &sops, 1) != -1)
        {
            do
            {
                clock_gettime(CLOCK_REALTIME, &now);
                n = now.tv_nsec % SO_USERS_NUM;
            } while (pid == usersList[n].procId);
            
            pid_to_return = usersList[n].procId; /* save user pid so as to return it */

            sops.sem_num = 0;
            sops.sem_op = -1;
            if(semop(userListSem, &sops, 1) != -1)
            {
                (*noUserSegReadersPtr)--;
                if((*noUserSegReadersPtr) == 0)
                {
                    sops.sem_num = 2;
                    sops.sem_op = 1;
                    if(semop(userListSem, &sops, 1) == -1)
                    {
                        safeErrorPrint("User: failed to release write usersList semaphore. Error: ");
                        /* do we need to end execution ? */
                    }
                }

                sops.sem_num = 0;
                sops.sem_op = 1;
                if(semop(userListSem, &sops, 1) != -1)
                {
                    return pid_to_return;
                }
                else
                {
                    safeErrorPrint("User: failed to release mutex usersList semaphore. Error: ");
                    /* do we need to end execution ? */
                }
            }
            else
            {
                safeErrorPrint("User: failed to reserve mutex usersList semaphore. Error: ");
                /* do we need to end execution ? */
            }
        }
        else
        {
            safeErrorPrint("User: failed to release mutex usersList semaphore. Error: ");
            /* do we need to end execution ? */
        }
    }
    else
    {
        safeErrorPrint("User: failed to reserve mutex usersList semaphore. Error: ");
        /* do we need to end execution ? */
    }

    return (pid_t)-1;
}

/**
 * Function that extracts randomly a node which to send the generated transaction.
 * Returns the pid of the selected user in the usersList shared array, -1 if the function generates an error.
 */
pid_t extractNode()
{
    int n = -1;
    struct sembuf sops;
    pid_t pid_to_return = -1;
    sops.sem_flg = 0;

    clock_gettime(CLOCK_REALTIME, &now);
    n = now.tv_nsec % SO_NODES_NUM;

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
                safeErrorPrint("User: failed to reserve write nodesList semaphore. Error: ");
                /* do we need to end execution ? */
            }
        }

        sops.sem_num = 0;
        sops.sem_op = 1;
        if(semop(nodeListSem, &sops, 1) != -1)
        {
            pid_to_return = nodesList[n].procId;

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
                        safeErrorPrint("User: failed to release write nodesList semaphore. Error: ");
                        /* do we need to end execution ? */
                    }
                }

                sops.sem_num = 0;
                sops.sem_op = 1;
                if(semop(nodeListSem, &sops, 1) != -1)
                {
                    return pid_to_return;
                }
                else
                {
                    safeErrorPrint("User: failed to release mutex nodesList semaphore. Error: ");
                    /* do we need to end execution ? */
                }
            }
            else
            {
                safeErrorPrint("User: failed to reserve mutex nodesList semaphore. Error: ");
                /* do we need to end execution ? */
            }
        }
        else
        {
            safeErrorPrint("User: failed to release mutex nodesList semaphore. Error: ");
            /* do we need to end execution ? */
        }
    }
    else
    {
        safeErrorPrint("User: failed to reserve mutex nodesList semaphore. Error: ");
        /* do we need to end execution ? */
    }

    return (pid_t)-1;
}
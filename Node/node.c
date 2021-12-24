#include "node.h"
/*
    Persistenza associazione handler
    Reentrrancy
    Generazione numeri
    Correggere allocazione regPtrs
    Refactoring e stampe
*/

/* Macro that rappresents the sender with id -1 in Transactions */
#define NO_SENDER -1

int wrPartSem = -1;
Register **regPtrs = NULL;

void sembufInit(struct sembuf *, int);

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
    int i = 0, num_bytes;
    boolean available = FALSE;
    int * newBlockPos = NULL;
    int tpListId; /* TP of Node ID */

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
        sembufInit(reservation, -1);
        sembufInit(release, 1);
        /*
            Cosa succede in caso di errore?
            Terminiamo il ciclo?
            Oppure segnaliamo l'errore e procediamo
            con la prossima elaborazione?
        */
        while (1)
        {
            /*
                PRECONDIZIONE:
                    minSim e maxSim sono state caricate leggendole
                    dalle variabili d'ambiente
            */
            /*generates a random number in [minSim, maxSim]*/
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
                rew_tran.amountSend = 0.0; /* we now set it to 0, then we will count the rewards */
                clock_gettime(CLOCK_REALTIME, &rew_tran.timestamp); /* get timestamp for transaction */

                /* cycle for extract transaction from TP */
                while (i < SO_BLOCK_SIZE-1) 
                {
                    /* now receiving the message (transaction from TP) */
                    num_bytes = msgrcv(tpListId, &new_trans, sizeof(new_trans)-sizeof(long), getpid(), 0);

                    if (num_bytes >= 0) 
                    {
                        /* read transaction from tpList */
                        extractedBlock.transList[i++] = new_trans.transaction;
                        /* adding reward of transaction in amountSend of reward_transaction */
                        rew_tran.amountSend += new_trans.transaction.reward;
                        extractedBlock.bIndex = i;
                    }
                    else
                    {
                        unsafeErrorPrint("Node: failed to retrieve transaction from Transaction Pool. Error: ");
                    }

                    /*
                     * NOTE: if in the TP there aren't SO_BLOCK_SIZE-1 transactions, the node blocks on msgrcv
                     * and waits for a message on queue; we will exit this cycle when we read the requested 
                     * number of transactions (put in extractedBlock.transList)
                     */
                }

                /* putting reward transaction in extracted block */
                extractedBlock.transList[i] = rew_tran;

                /* creating candidate block by coping transactions in extracted block */
                i = 0;
                while(i < SO_BLOCK_SIZE)
                {
                    candidateBlock.transList[i] = extractedBlock.transList[i];
                    i++;
                }
                candidateBlock.bIndex = i;

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
                if (semop(wrPartSem, reservation, REG_PARTITION_COUNT) == -1)
                    safeErrorPrint("Node: failed to reserve register partitions' semaphore. Error: ");
                else
                {
                    /*
                            PRECONDIZIONE:
                                Il processo arriva qui soolo dopo aver guadagnato l'accesso
                                in mutua esclusione a tutte le partizioni
                        */

                    /*
                        Verifica eistenza spazio libero sul registro
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
                    }

                    /*
                        Exit section
                    */
                    if (semop(wrPartSem, release, REG_PARTITION_COUNT) == -1)
                        safeErrorPrint("Node: failed to release register partitions' semaphore. Error: ");
                }
            }
            else
            {
                /*
                    A node can only be interrupted by the end of simulation signal
                */
                safeErrorPrint("Node: an unexpected event occured before the end of the computation.");
            }
        }
    }

    free(reservation);
    free(release);

    exit(exitCode);
}

void sembufInit(struct sembuf *sops, int op)
{
    int i = 0;

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
    }
}
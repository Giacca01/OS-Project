#include "node.h"
int wrPartSem = -1;
Register **regPtrs = NULL;
int tpId = -1;

void sembufInit(struct sembuf *, int);
void reinsertTransactions(Block);
void endOfExecution();

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
    struct sigaction act;
    sigset_t mask;

    /*
        Il nodo potrebbe essere interrotto soltanto
        dal segnale di fine simulazione, ma in tal caso
        l'esecuzione della procedura in corso non ripartirebbe 
        da capo, quindi qui si può usare codice non rientrante
    */

    printf("Node: setting up signal mask...\n");
    if (sigfillset(&mask) == -1)
        unsafeErrorPrint("Node: failed to initialize signal mask. Error: ");
    else
    {
        //act.sa_hanlder = endOfExecution;
        act.sa_mask = mask;
        if (sigaction(SIGUSR1, &act, NULL))
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
                    printf("Node: starting lifecycle...\n");
                    while (!waitForTerm)
                    {
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
                            printf("Node: trying to write transactions on registr...\n");
                            if (semop(wrPartSem, reservation, REG_PARTITION_COUNT) == -1)
                                unsafeErrorPrint("Node: failed to reserve register partitions' semaphore. Error: ");
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
                                    unsafeErrorPrint("Node: failed to release register partitions' semaphore. Error: ");
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
        }
    }

    printf("Node: releasing dynamically allocated memory...\n");
    free(reservation);
    free(release);
    free(newBlockPos);

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

void reinsertTransactions(Block failedTrs)
{
    char * aus = NULL;

    aus = (char *)calloc(sizeof(char), 50);
    while (failedTrs.bIndex == 0)
    {
        failedTrs.bIndex--;
        if (msgsnd(tpId, &(failedTrs.transList[failedTrs.bIndex]), sizeof(Transaction), 0) == -1)
        {
            /*
                Dovremmo segnalarlo al sender???
            */
            sprintf(aus, "Node: failed to reinsert transaction number %d.", failedTrs.bIndex);
            unsafeErrorPrint(aus);
        }
    }
    printf("Node: Transactions reinserted successfully!\n");

    free(aus);
}

void endOfExecution()
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

    while (regPtrs != NULL)
    {
        if (shmdt(*regPtrs) == -1)
        {
            /*
                Implementare un meccanismo di retry??
                Contando che non è un errore così frequente si potrebbe anche ignorare...
            */
            unsafeErrorPrint("Node: failed to detach from register's partition. Error: ");
        }
        regPtrs++;
    }

    free(regPtrs);

    printf("Node: cleanup operations completed. Process is about to end its execution...\n");
}
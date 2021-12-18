#include "node.h"
/*
    Persistenza associazione handler
    Reentrrancy
    Generazione numeri
*/
int main(){
    int exitCode = EXIT_FAILURE;
    /* To be read from environment variables */
    int minSim = 1; /* milliseconds*/
    int maxSim = 10; /* milliseconds*/
    unsigned int simTime = 0; /* simulation length in seconds*/
    time_t timeSinceEpoch = (time_t)-1;
    Block extractedBlock;
    Block candidateBlock;

    timeSinceEpoch = time(NULL);
    if (timeSinceEpoch == (time_t)-1)
        unsafeErrorPrint("Node: failed to initialize random generator's seed. Error");
    else {
        /* Vedere se metterlo nel ciclo di vita */
        srand(time(NULL) - getpid()); /*Inizializza il seme di generazione*/
                                      /*Essendoci più processi nodo in esecuzione, è probabile che
                    alcuni nodi vengano eseguiti nello stesso secondo e che si abbia quindi 
                    la stessa sequenza di numeri random. Per evitare ciò sottraiamo il PID*/
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
    
    exit(exitCode);
}
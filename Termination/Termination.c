#include "../info.h"
#include <string.h>
/*
	TODO:
		-set simulation timer: Ok
		-detect termination conditions: Ok
		-notify children (so that they can print they transaction pool count): Ok
		-wait for children: Ok
		-print report: OK
		-deallocate IPC facilities: Ok
		-define error procedures: Ok
		-refactoring e aggiunta stampe
		-check memory leaks
		-add ctrl+c handler
		
	SIGUSR1 is sent to all the children by the master when it
	detects the end of the simulation.
	The meaning of this signal can be described as "register is full V timer elapsed".
	This signal can also be sent by a node to the master when it detects a full register.
	In that case, the master sends the signal to all the children (but the sending one???)
	and then executes the same handler.
	
	Create a macro for SIGUSR1
*/

void endOfSimulation(int);
void printBudget();
void deallocateFacilities();

/*** Global structures ***/
// contiene puntatori ad oggetti di tipo Register
// vettore di puntatori
Register ** regPtrs = NULL;						
int * regPartsIds = NULL;

ProcListElem * usersList = NULL;
ProcListElem * nodesList = NULL;

ProcListElem * processesFriends = NULL;


TPElement * tpList = NULL;

int globalQueueId = -1;

int wrPartSem = -1;
int rdPartSem = -1;
int fairStartSem = -1;

int noTerminated = 0;
/*** End of global structures ***/

int main()
{
	// to be read from file
	int SO_SIM_SEC = 5;
	sigset_t set;
	int fullRegister = 1; // do a boolean datatype

	struct sigaction act;

	// check errors
	alarm(SO_SIM_SEC);

	if (sifillset(&set) == -1)
	{
		unsafeErrorPrint("Master: failed to initialize signals mask. Error: ");
	}
	else
	{
		act.sa_handler = endOfSimulation;
		act.sa_mask = set;
		if (sigaction(SIGALRM, &act, NULL) == -1)
		{
			unsafeErrorPrint("Master: failed to set end of timer disposition. Error: ");
		}
		else
		{

			if (sigaction(SIGUSR1, &act, NULL) == -1)
			{
				unsafeErrorPrint("Master: failed to set end of simulation disposition. Error: ");
			}
			else
			{
				// check if register is full: in that case it must
				// signal itself ? No
				// this should be inserted in the master lifecycle
				fullRegister = 1;
				for (int i = 0; i < 3 && fullRegister; i++)
				{
					if (regPtrs[i]->nBlocks < REG_PARTITION_SIZE)
						fullRegister = 0;
				}

				if (fullRegister)
				{
					endOfSimulation(SIGUSR1);
				}
			}
		}
	}

	exit(EXIT_SUCCESS);
}

void endOfSimulation(int sig)
{ // IT MUST BE REENTRANT!!!!
	// Notify children
	// sends termination signal to all the processes
	// that are part of the master's group (in this case we
	// reach every children with just one system call).
	// how to check if everyone was signaled (it returns true even if
	// only one signal was sent)
	char * terminationMessage = NULL;
	int ret = -1;
	char * aus;

	// viene inviato anche al master stesso ? In teoria no
	if (kill(0, SIGUSR1) == 0)
	{
		// wait for children
		// dovremmo aspettare solo la ricezione del segnale di terminazione????
		// mettere nell'handler
		// dovremmo usare waitpid e teastare che i figli siano
		// terminati correttamente ? Sarebbe complicato
		// meglio inserire nel figlio un meccanismo che tenta più volte la stampa
		// in caso di errore
		while (wait(NULL) != -1);

		if (errno != ECHILD) {
			// print report: we use the write system call: slower, but async-signal-safe
			// termination reason
			if (sig == SIGALRM)
				aus = "Termination reason: end of simulation.\n";
			else
				aus = "Termination reason: register book is full.\n";

			// Users and nodes budgets
			printBudget();

			/*Per la stampa degli errori non si può usare perror, perchè non è elencata* tra la funzioni signal*/

			// processes terminated before end of simulation
			ret = sprintf(terminationMessage, "Processes terminated before end of simulation: %d\n", noTerminated);
			if (ret <= 0)
				write(STDERR_FILENO, "Master: sprintf failed to format process count's string.", strlen("Master: sprintf failed to format process count's string."));

			// Blocks in register
			ret = sprintf(aus, "There are %d blocks in the register.\n", regPtrs[0]->nBlocks + regPtrs[1]->nBlocks + regPtrs[2]->nBlocks);
			if (ret <= 0)
				write(STDERR_FILENO, "Master: sprintf failed to format number of blocks' string.", strlen("Master: sprintf failed to format number of blocks' string.")); // error handling

			ret = write(STDOUT_FILENO, terminationMessage, strlen(terminationMessage));
			if (ret == -1)
				write(STDERR_FILENO, "Master: failed to write termination message.", strlen("Master: failed to write termination message.")); // error handling

			// deallocate facilities
			deallocateFacilities();
		} else 
		{
			
		}

		// Releasing local variables' memory
		free(terminationMessage);
		free(aus);
	}
	else
	{
		// implement a retry mechanism
	}
}

void printBudget()
{ // Vedere se si possa sostituire la write con printf
	Register * reg = NULL;
	long int pid = -2;		   // -1 è usato per rappresentare la transazione di reward e di init
	ProcListElem * usr = NULL;  // non sono da deallocare, altrimenti cancelli la lista di processi
	ProcListElem * node = NULL;
	float balance = 0; // leggendo la transazione di inizializzazione (i.e. la prima indirizzata la processo)
					   // verrà inizializzata a SO_BUDGET_INIT
	char * balanceString = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	Transaction * tList = NULL;
	TPElement * tpPtr = NULL;
	struct msqid_ds * buf;
	int noRemainingMsg = 0;
	Transaction msg;
	int qId = -1;

		// Compute balance for users
		ret = write(STDOUT_FILENO, "Master prints users' balances...\n", strlen("Master prints users' balances...\n"));
	if (ret == -1)
		write(STDERR_FILENO, "Master: failed to write operation's description.", strlen("Master: failed to write operation's description."));
	else
	{
		usr = usersList;
		// è un algoritmo di complessità elevata
		// vedere se sia possibile ridurla
		// for each user...
		while (usr != NULL)
		{
			pid = (long)(usr->procId);
			balance = 0;
			// we scan all the register's partitions...
			for (i = 0; i < REG_PARTITION_COUNT; i++)
			{
				reg = regPtrs[i];
				// we scan every block in the partition
				// and every transaction in it to compute the balance
				for (j = 0; j < REG_PARTITION_SIZE; j++)
				{
					// necessary in order not to lose
					// the transaction list pointer
					tList = (reg->blockList[i]).transList;
					for (k = 0; k < SO_BLOCK_SIZE; k++) {
						if (tList[k].sender == pid)
							balance -= tList[k].amountSend;
						else if (tList[k].receiver == pid)
							balance += tList[k].amountSend;
					}
				}
			}

			ret = sprintf(balanceString, "The balance of the user of PID %ld is: %lf", pid, balance);
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

			// Se perdessimo il puntatore non riusciremmo più a deallocare tpList
			ret = sprintf(balanceString, "Master: printing remaining transactions of node of PID %ld\n", pid);
			write(STDOUT_FILENO, balanceString, ret);
			tpPtr = tpList;
			while (tpPtr != NULL){
				if (tpPtr->procId == pid) {
					// fare macro per permessi
					qId = msgget(tpPtr->procId, 0600);
					if (ret == -1){

					} else {
						if (msgctl(qId, IPC_STAT, buf) == -1)
						{
						}
						else{
							noRemainingMsg = buf->msg_qnum ;
							if (noRemainingMsg > 0)
							{
								for (k = 0; k < noRemainingMsg; k++){
									// reads the first message from the transaction pool
									// the last parameter is not  that necessary, given
									// that we know there's still at least one message
									// and no one is reading but the master
									if (msgrcv(qId, &msg, sizeof(msg), 0, IPC_NOWAIT) == -1)
									{
									}
									else
									{
										ret = sprintf(balanceString, "Transaction number %d:\n\tTimestamp: %ld\nSender: %ld\nReceiver: %ld\nAmount sent:%f\nReward:%f\n", (k + 1), 
													msg.timestamp.tv_nsec, msg.sender, msg.receiver, msg.amountSend, msg.reward);
										if (ret >= 0){
											if (write(STDOUT_FILENO, balanceString, strlen(balanceString)) == -1)
												write(STDERR_FILENO, "Master: failed to print transaction\n", strlen("Master: failed to print transaction\n"));
										} else {

										}
									}
								}
							}
							else
							{
								write(STDOUT_FILENO, "Transaction pool is empty.\n", strlen("Transaction pool is empty.\n"));
							}
						}
					}
				}
				
				tpPtr++;
			}


			balance = 0;
			for (i = 0; i < REG_PARTITION_COUNT; i++)
			{
				reg = regPtrs[i];
				// note that we start from one because there's no
				// initialization blocks for nodes
				for (j = 1; j < REG_PARTITION_SIZE; j++)
				{
					tList = (reg->blockList[i]).transList;
					for (k = 0; k < SO_BLOCK_SIZE; k++){
						// non c'è il rischio di contare più volte le transazioni
						/// perchè cerchiamo solo quella di reward e l'implementazione
						// del nodo garantisce che c'è ne sia una sola per blocco
						// serve testare
						if (tList[k].sender == REWARD_TRANSACTION && tList[k].receiver == pid)
							balance += tList[k].amountSend;
						tList++;
					}
				}
			}
			ret = sprintf(balanceString, "The balance of the node of PID %ld is: %lf", pid, balance);
			if (ret <= 0)
			{
				if (write(STDOUT_FILENO, balanceString, strlen(balanceString)) == -1)
					write(STDERR_FILENO, "Master: failed to write balance message for node.", strlen("Master: failed to write balance message for node."));
			}
			node++;
		}
	}

	free(reg);
	free(usr);
	free(node);
	free(balanceString);
	free(tList);
	free(tpPtr);
	free(buf);
}

void deallocateFacilities()
{
	// Ovviamente i processi figli dovranno scollegarsi
	// dai segmenti e chiudere i loro riferimenti alla coda
	// ed ai semafori prima di terminare

	// Precondizione: tutti i processi figli si sono scollegati dai segmenti di memoria
	// In generale, tutti i figli hanno chiuso i loro riferimenti alle facilities IPC
	// ne siamo certi perchè questa procedura viene richiamata solo dopo aver atteso la terminazione di ogni figlio

	// Cosa fare se una delle system call usate per la deallocazione fallisce?
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

	char * aus = NULL;
	int msgLength = 0;

	// Deallocating register's partitions
	for (int i = 0; i < REG_PARTITION_COUNT; i++)
	{
		if (shmdt(regPtrs[i]) == -1)
		{
			msgLength = sprintf(aus, "Master: failed to detach from register partition number %d.", (i + 1));
			if (msgLength < 0)
				write(STDERR_FILENO, "Master: failed to format output message in IPC deallocation.", strlen("Master: failed to format output message in IPC deallocation."));
			else
				write(STDERR_FILENO, aus, msgLength); // by doing this we avoid calling strlength
		}
		else
		{
			if (smhctl(regPtrs[i], IPC_RMID, NULL) == -1)
			{
				msgLength = sprintf(aus, "Master: failed to remove register partition number %d.", (i + 1));
				if (msgLength < 0)
					write(STDERR_FILENO, "Master: failed to format output message in IPC deallocation.", strlen("Master: failed to format output message in IPC deallocation."));
				else
					write(STDERR_FILENO, aus, msgLength);
			}
		}

		free(regPtrs[i]);
	}
	free(regPartsIds);

	// Users list deallocation
	free(usersList);

	// Nodes list deallocation
	free(nodesList);

	// Transaction pools list deallocation
	while (tpList != NULL) {
		
		if (msgctl(tpList->msgQId, IPC_RMID, NULL) == -1) {
			msgLength = sprintf(aus, "Master: failed to remove transaction pool of process %ld", (long)tpList->procId);
			if (msgLength < 0)
				write(STDERR_FILENO, "Master: failed to format output message in IPC deallocation.", strlen("Master: failed to format output message in IPC deallocation."));
			else
				write(STDERR_FILENO, aus, msgLength);
		}
	}
	free(tpList);

	// Deallocating process friends array
	free(processesFriends);

	// Global queue deallocation
	if (msgctl(globalQueueId, IPC_RMID, NULL) == -1){
		msgLength = sprintf(aus, "Master: failed to remove global message queue");
		if (msgLength < 0)
			write(STDERR_FILENO, "Master: failed to format output message in IPC deallocation.", strlen("Master: failed to format output message in IPC deallocation."));
		else
			write(STDERR_FILENO, aus, msgLength);
	}

	// Writing Semaphores deallocation
	if (semctl(wrPartSem, 0, IPC_RMID) == -1){
		msgLength = sprintf(aus, "Master: failed to remove partions' writing semaphores");
		if (msgLength < 0)
			write(STDERR_FILENO, "Master: failed to format output message in IPC deallocation.", strlen("Master: failed to format output message in IPC deallocation."));
		else
			write(STDERR_FILENO, aus, msgLength);
	}

	// Reading Semaphores deallocation
	if (semctl(rdPartSem, 0, IPC_RMID) == -1)
	{
		msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
		if (msgLength < 0)
			write(STDERR_FILENO, "Master: failed to format output message in IPC deallocation.", strlen("Master: failed to format output message in IPC deallocation."));
		else
			write(STDERR_FILENO, aus, msgLength);
	}

	// Fair start semaphore deallocation
	if (semctl(fairStartSem, 0, IPC_RMID) == -1){
		msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
		if (msgLength < 0)
			write(STDERR_FILENO, "Master: failed to format output message in IPC deallocation.", strlen("Master: failed to format output message in IPC deallocation."));
		else
			write(STDERR_FILENO, aus, msgLength);
	}

	// Releasing local variables' memory
	free(aus);
}

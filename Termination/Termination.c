#include "../info.h"
#include <string.h>
/*
	TODO:
		-set simulation timer: Ok
		-detect termination conditions: Ok
		-notify children (so that they can print they transaction pool count): Ok
		-wait for children: Ok
		-print report
		-deallocate IPC facilities
		
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

/*** IPC Pointers ***/

// each partition is an array of registers
Register regPtrs[REG_PARTITION_COUNT]; // to be loaded by master
ProcListElem usersList[SO_USER_NUM];	// Lista utenti (attivi e non)
// in cui mantenere gli ID dei nodi da usare durante il calcolo del bilancio
// ProcListElem nodesList[SO_NODES_NUM];

// Non serve: i nodi non cambiano durante la simulazione, quindi conviene passare la lista
// come argomento o variabile d'ambiente
// FriendsList processesFriends[SO_NODES_NUM]; // Ogni nodo ha una lista di amici
// Tutti i processi devono potersi connetere alle TP dei nodi
TPElement tpList[SO_NODES_NUM];				// Ogni processo nodo ha un transaction pool

int wrPartSem; // Id of the set that contais the three semaphores
	// used to write on the register's partitions
int rdPartSem; // Id of the set that contais the three semaphores
	// used to read from the register's partitions
int fairStartSem; // Id of the semaphore used to make all the processes
				  // start to compete for CPU at the same time

/*** End of IPC Pointers ***/

int main()
{
	// to be read from file
	int SO_SIM_SEC = 5;
	sigset_t set;
	int fullRegister = 1; // do a boolean datatype
	int noTerminated = 0;

	struct sigaction act;

	// check errors
	alarm(SO_SIM_SEC);

	if (sifillset(&set) == -1)
	{
		EXIT_ON_ERROR;
	}
	else
	{
		act.sa_handler = endOfSimulation;
		act.sa_mask = set;
		if (sigaction(SIGALRM, act, NULL) == -1)
		{
			EXIT_ON_ERROR;
		}
		else
		{

			if (sigaction(SIGUSR1, act, NULL) == -1)
			{
				EXIT_ON_ERROR;
			}
			else
			{
				// check if register is full: in that case it must
				// signal itself ? No
				// this should be inserted in the master lifecycle
				fullRegister = 1;
				for (int i = 0; i < 3 && fullRegister; i++)
				{
					if (regPtrs[i]->nBlocks < SO_REGISTER - SIZE / 3)
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
	char *terminationMessage = NULL;
	int ret = -1;
	char *aus;

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
		while (wait(NULL) != -1)
			;

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
			write(STDERR_FILENO, "Master: sprintf failed to format process count's string. Error: ");

		// Blocks in register
		ret = sprintf(aus, "There are %d blocks in the register.\n", shmPtrs[0].nBlocks + nBlocks[1].nBlocks + nBlocks[2].nBlocks);
		if (ret <= 0)
			write(STDERR_FILENO, "Master: sprintf failed to format number of blocks' string. Error: "); // error handling

		ret = write(STDOUT_FILENO, terminationMessage, strlen(terminationMessage));
		if (ret == -1)
			write(STDERR_FILENO, "Master: failed to write termination message. Error: "); // error handling

		// deallocate facilities
		deallocateFacilities();
	}
	else
	{
		// implement a retry mechanism
	}
}

void printBudget()
{ // Vedere se si possa sostituire la write con printf
	Register *reg = NULL;
	long int pid = -2;		   // -1 è usato per rappresentare la transazione di reward e di init
	ProcessesList *usr = NULL; // non sono da deallocare, altrimenti cancelli la lista di processi
	ProcessesList *node = NULL;
	Transaction *transPtr = NULL;
	float balance = 0; // leggendo la transazione di inizializzazione (i.e. la prima indirizzata la processo)
					   // verrà inizializzata a SO_BUDGET_INIT
	char *balanceString = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	TransactionList *tList = NULL;

	// Compute balance for users
	ret = write(STDOUT_FILENO, "Master prints users' balances...\n", strlen("Master prints users' balances...\n"));
	if (ret == -1)
		write(STDERR_FILENO, "Master: failed to write operation's description. Error: ");
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
				aus = regPtrs[i];
				// we scan every block in the partition
				// and every transaction in it to compute the balance
				for (j = 0; j < REG_PARTITION_SIZE; j++)
				{
					// necessary in order not to lose
					// the transaction list pointer
					tList = (blockList[i]->transList);
					while (tList != NULL)
					{
						transPtr = tList->trans;
						if (transPtr->sender == pid)
							balance -= transPtr->amountSend;
						else
							(transPtr->receiver == pid)
								balance += transPtr->amountSend;
						tList++;
					}
				}
			}

			ret = sprintf(balanceString, "The balance of the user of PID %ld is: %lf", pid, balance);
			if (ret <= 0)
			{
				if (write(STDOUT_FILENO, balanceString, strlen(balanceString)) == -1)
					write(STDERR_FILENO, "Master: failed to write balance message for user. Error: ");
			}
			usr++;
		}
	}

	ret = write(STDOUT_FILENO, "Master prints nodes' balances...\n", strlen("Master prints nodes' balances...\n"));
	if (ret == -1)
		write(STDERR_FILENO, "Master: failed to write operation's description. Error: ");
	else
	{
		node = nodesList;
		while (node != NULL)
		{
			pid = (long)(node->procId);
			balance = 0;
			for (i = 0; i < REG_PARTITION_COUNT; i++)
			{
				aus = regPtrs[i];
				// note that we start from one because there's no
				// initialization blocks for nodes
				for (j = 1; j < REG_PARTITION_SIZE; j++)
				{
					tList = (blockList[i]->transList);
					while (tList != NULL)
					{
						transPtr = tList->trans;
						// non c'è il rischio di contare più volte le transazioni
						/// perchè cerchiamo solo quella di reward e l'implementazione
						// del nodo garantisce che c'è ne sia una sola per blocco
						// serve testare
						if (transPtr->sender == REWARD_TRANSACTION && transPtr->receiver == pid)
							balance += transPtr->amountSend;
						tList++;
					}
				}
			}
			ret = sprintf(balanceString, "The balance of the node of PID %ld is: %lf", pid, balance);
			if (ret <= 0)
			{
				if (write(STDOUT_FILENO, balanceString, strlen(balanceString)) == -1)
					write(STDERR_FILENO, "Master: failed to write balance message for node. Error: ");
			}
			node++;
		}
	}
}

void deallocateFacilities()
{
	// Ovviamente i processi figli dovranno scollegarsi
	// dai segmenti e chiudere i loro riferimenti alla coda
	// ed ai semafori prima di terminare

	// Precondizione: tutti i processi figli si sono scollegati dai segmenti di memoria
	// In generale, tutti i figli hanno chiuso i loro riferimenti alle facilities IPC

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
			-deallocate users and nodes lists (serve fare la malloc o basta deallocare il segmento???)
			-deallocate transaction pools list' shared memory segment
			-deallocate friens' shared memory segment
			-deallocate transaction pools' message queues
			-deallocate semaphores
	*/

	char *aus = NULL;
	int msgLength = 0;

	// Deallocating register's partitions
	for (int i = 0; i < regPtrs; i++)
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
	}
}

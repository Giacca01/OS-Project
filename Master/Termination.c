#include "info.h"
#include <string.h>
#include <sys/wait.h>
#define NO_ATTEMPS 3 /* Maximun number of attemps to terminate the simulation*/
/*
	TODO:
		-set simulation timer: Ok
		-detect termination conditions: Ok
		-notify children (so that they can print they transaction pool count): Ok
		-wait for children: Ok
		-print report: OK
		-deallocate IPC facilities: Ok
		-define error procedures: Ok
		-refactoring e aggiunta stampe: ok
		-test:
		-check memory leaks: Ok
		-add ctrl+c handler:
			-signaling the master will cause the end of the simulation
			-signaling the user will cause the user's termination and the master will be notified
			-signaling the node will cause the node's termination and the master will be notified(??)
		
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
void deallocateFacilities(int *);

/*** Global structures ***/
/* contiene puntatori ad oggetti di tipo Register
 vettore di puntatori*/
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
	/* to be read from file*/
	int SO_SIM_SEC = 5;
	sigset_t set;
	int fullRegister = TRUE; 
	int exitCode = EXIT_FAILURE;
	int i = 0;

	struct sigaction act;

	/* check errors*/
	printf("Master: setting up simulation timer...\n");
	/* No previous alarms were set, so it must return 0*/
	if (alarm(SO_SIM_SEC) != 0)
		unsafeErrorPrint("Master: failed to set simulation timer. ");
	else {
		printf("Master: simulation alarm initilized successfully.\n");
		printf("Master: setting up signal mask...\n");
		if (sigfillset(&set) == -1)
			unsafeErrorPrint("Master: failed to initialize signals mask. Error: ");
		else
		{
			/* We block all the signals during the execution of the handler*/
			act.sa_handler = endOfSimulation;
			act.sa_mask = set;
			printf("Master: signal mask initialized successfully.\n");

			printf("Master: setting end of timer disposition...\n");
			if (sigaction(SIGALRM, &act, NULL) == -1)
				unsafeErrorPrint("Master: failed to set end of timer disposition. Error: ");
			else
			{
				printf("Master: End of timer disposition initialized successfully.\n");
				printf("Master: setting end of simulation disposition...\n");
				if (sigaction(SIGUSR1, &act, NULL) == -1)
					unsafeErrorPrint("Master: failed to set end of simulation disposition. Error: ");
				else
				{
					/* dummy cycle to be replaced by master lifecycle*/
					while (1)
					{
						/* check if register is full: in that case it must
						 signal itself ? No
						this should be inserted in the master lifecycle*/
						printf("Master: checking if register's partitions are full...\n");
						fullRegister = TRUE;
						for (i = 0; i < 3 && fullRegister; i++)
						{
							if (regPtrs[i]->nBlocks < REG_PARTITION_SIZE)
								fullRegister = FALSE;
						}

						if (fullRegister)
						{
							/* it contains an exit call
							 so no need to set exit code*/
							printf("Master: all register's partitions are full. Terminating simulation...\n");
							endOfSimulation(SIGUSR1);
						}
						printf("Master: register's partitions are not full. Starting a new cycle...\n");
					}
				}
			}
		}
	}

	/* POSTCONDIZIONE: all'esecuzione di questa system call
		l'handler di fine simulazione è già stato eseguito*/
	exit(exitCode);
}

void endOfSimulation(int sig)
{ /* IT MUST BE REENTRANT!!!!
	// Notify children
	// sends termination signal to all the processes
	// that are part of the master's group (in this case we
	// reach every children with just one system call).
	// how to check if everyone was signaled (it returns true even if
	// only one signal was sent)*/
	char * terminationMessage = NULL;
	int ret = -1;
	char * aus;
	int i = 0;
	/*
	// Contiene una exit, perchè potrebbe essere ivocato in maniera
	// asincrona durante l'esecuzione del ciclo di vita
	// in tal caso l'esecuzione dovrebbe terminare dopo l'esecuzione
	// dell'handler senza eseguire il codice rimanente del ciclo di vita
	// cosa che potrebbe anche causare degli errori
	// per il riferimento a zone di memoria non più allocate
	// in case of error we don't stop the whole procedure
	// but we signal it by setting the exit code to EXIT_FAILURE*/
	int exitCode = EXIT_SUCCESS;
	boolean done = FALSE;

	/*
	// viene inviato anche al master stesso ? In teoria no
	// come assicurarsi che venga inviato a tutti?
	// fallisce se non viene inviato a nessuno
	// ma inviato != consegnato???*/
	for (i = 0; i < NO_ATTEMPS && !done; i++){
		/* error check*/
		write(STDOUT_FILENO, 
			"Master: trying to terminate simulation...\n", 
			strlen("Master: trying to terminate simulation...\n")
		);
		if (kill(0, SIGUSR1) == 0)
		{
			/*
			// wait for children
			// dovremmo aspettare solo la ricezione del segnale di terminazione????
			// mettere nell'handler
			// dovremmo usare waitpid e teastare che i figli siano
			// terminati correttamente ? Sarebbe complicato
			// meglio inserire nel figlio un meccanismo che tenta più volte la stampa
			// in caso di errore
			// in teoria questo si sblocca solo dopo la terminazione di tutti i figli
			// quindi ha senso fare così*/
			write(STDOUT_FILENO, 
				"Master: waiting for children to terminate...\n", 
				strlen("Master: waiting for children to terminate...\n")
			);
			while (wait(NULL) != -1);

			if (errno != ECHILD) {
				/*
				// print report: we use the write system call: slower, but async-signal-safe
				// Termination message composition
				// we use only one system call for performances' sake.
				// termination reason*/
				write(STDOUT_FILENO, 
					"Master: simulation terminated successfully. Printing report...\n", 
					strlen("Master: simulation terminated successfully. Printing report...\n")
				);
				if (sig == SIGALRM)
					aus = "Termination reason: end of simulation.\n";
				else
					aus = "Termination reason: register book is full.\n";
					
				/* Users and nodes budgets*/
				printBudget();

				/*Per la stampa degli errori non si può usare perror, perchè non è elencata* tra la funzioni signal
				in teoria non si può usare nemmno sprintf*/

				/* processes terminated before end of simulation*/
				ret = sprintf(terminationMessage, 
							"Processes terminated before end of simulation: %d\n", 
							noTerminated);
				if (ret <= 0) {
					safeErrorPrint("Master: sprintf failed to format process count's string. ");
					exitCode = EXIT_FAILURE;
				}

				/* Blocks in register*/
				ret = sprintf(aus, "There are %d blocks in the register.\n", 
								regPtrs[0]->nBlocks + regPtrs[1]->nBlocks + regPtrs[2]->nBlocks);
				if (ret <= 0){
					safeErrorPrint("Master: sprintf failed to format number of blocks' string. ");
					exitCode = EXIT_FAILURE;
				}

					/* Writes termination message on standard output*/
				ret = write(STDOUT_FILENO, terminationMessage, strlen(terminationMessage));
				if (ret == -1){
					safeErrorPrint("Master: failed to write termination message. Error: ");
					exitCode = EXIT_FAILURE;
				}

				write(STDOUT_FILENO, 
					"Master: report printed successfully. Deallocating IPC facilities...\n", 
					strlen("Master: report printed successfully. Deallocating IPC facilities...\n")
				);
				/* deallocate facilities*/
				deallocateFacilities(&exitCode);
				done = TRUE;
			} else 
			{
				safeErrorPrint("Master: an error occurred while waiting for children. Description: ");
			}

			/* Releasing local variables' memory*/
			free(terminationMessage);
			free(aus);
		} else 
			safeErrorPrint("Master: failed to signal children for end of simulation. Error: ");
	}

	if (!done)
		exitCode = EXIT_FAILURE;

	exit(exitCode);
}

void printBudget()
{ /* Vedere se si possa sostituire la write con printf*/
	Register * reg = NULL;
	long int pid = -2;		   /* -1 è usato per rappresentare la transazione di reward e di init*/
	ProcListElem * usr = NULL;  /* non sono da deallocare, altrimenti cancelli la lista di processi*/
	ProcListElem * node = NULL;
	float balance = 0; /* leggendo la transazione di inizializzazione (i.e. la prima indirizzata la processo)
					    verrà inizializzata a SO_BUDGET_INIT*/
	char * balanceString = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	Transaction * tList = NULL;
	TPElement * tpPtr = NULL;
	struct msqid_ds * buf = NULL;
	int noRemainingMsg = 0;
	Transaction msg;
	int qId = -1;

	/* Compute balance for users*/
	ret = write(STDOUT_FILENO, 
		"Master prints users' balances...\n", 
		strlen("Master prints users' balances...\n")
	);

	if (ret == -1)
		safeErrorPrint("Master: failed to write operation's description.");
	else
	{
		usr = usersList;
		/* è un algoritmo di complessità elevata
		// vedere se sia possibile ridurla
		// for each user...*/
		while (usr != NULL)
		{
			pid = (long)(usr->procId);
			balance = 0;
			/* we scan all the register's partitions...*/
			for (i = 0; i < REG_PARTITION_COUNT; i++)
			{
				reg = regPtrs[i];
				/* we scan every block in the partition
				// and every transaction in it to compute the balance*/
				for (j = 0; j < REG_PARTITION_SIZE; j++)
				{
					/* necessary in order not to lose
					// the transaction list pointer*/
					tList = (reg->blockList[i]).transList;
					for (k = 0; k < SO_BLOCK_SIZE; k++) {
						if (tList[k].sender == pid)
							balance -= tList[k].amountSend;
						else if (tList[k].receiver == pid)
							balance += tList[k].amountSend;
					}
				}
			}

			ret = sprintf(balanceString, "The balance of the user of PID %ld is: %f", pid, balance);
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

			/* Se perdessimo il puntatore non riusciremmo più a deallocare tpList*/
			ret = sprintf(balanceString, "Master: printing remaining transactions of node of PID %ld\n", pid);
			write(STDOUT_FILENO, balanceString, ret);
			tpPtr = tpList;
			while (tpPtr != NULL){
				if (tpPtr->procId == pid) {
					/* fare macro per permessi*/
					qId = msgget(tpPtr->procId, 0600);
					if (ret == -1){
						sprintf(balanceString, 
							"Master: failed to open transaction pool of node of PID %ld\n", 
							pid
						);
						safeErrorPrint(balanceString);
					} else {
						if (msgctl(qId, IPC_STAT, buf) == -1)
						{
							sprintf(balanceString, 
								"Master: failed to retrive remaining transactio of node of PID %ld\n", 
								pid
							);
							safeErrorPrint(balanceString);
						}
						else{
							noRemainingMsg = buf->msg_qnum ;
							if (noRemainingMsg > 0)
							{
								for (k = 0; k < noRemainingMsg; k++){
									/* reads the first message from the transaction pool
									// the last parameter is not  that necessary, given
									// that we know there's still at least one message
									// and no one is reading but the master*/
									if (msgrcv(qId, &msg, sizeof(msg), 0, IPC_NOWAIT) == -1)
									{
										sprintf(balanceString,
												"Master: failed to read transaction number %d of PID %ld\n",
												(k + 1), pid);
									}
									else
									{
										ret = sprintf(balanceString, 
												"Transaction number %d:\n\tTimestamp: %ld\nSender: %ld\nReceiver: %ld\nAmount sent:%f\nReward:%f\n", 
												(k + 1), msg.timestamp.tv_nsec, msg.sender, 
												msg.receiver, msg.amountSend, msg.reward
										);
										write(STDOUT_FILENO, balanceString, ret);
									}
								}
							}
							else
							{
								write(STDOUT_FILENO, 
									"Transaction pool is empty.\n", 
									strlen("Transaction pool is empty.\n")
								);
							}
						}
					}
				}
				
				tpPtr++;
			}

			ret = sprintf(balanceString, "Master: printing budget of node of PID %ld\n", pid);
			write(STDOUT_FILENO, balanceString, ret);
			balance = 0;
			for (i = 0; i < REG_PARTITION_COUNT; i++)
			{
				reg = regPtrs[i];
				/* note that we start from one because there's no
				// initialization blocks for nodes*/
				for (j = 1; j < REG_PARTITION_SIZE; j++)
				{
					tList = (reg->blockList[i]).transList;
					for (k = 0; k < SO_BLOCK_SIZE; k++){
						/*
						// non c'è il rischio di contare più volte le transazioni
						/// perchè cerchiamo solo quella di reward e l'implementazione
						// del nodo garantisce che c'è ne sia una sola per blocco
						// serve testare*/
						if (tList[k].sender == REWARD_TRANSACTION && tList[k].receiver == pid)
							balance += tList[k].amountSend;
						tList++;
					}
				}
			}
			ret = sprintf(balanceString, "The balance of the node of PID %ld is: %f", pid, balance);
			if (ret <= 0)
			{
				if (write(STDOUT_FILENO, balanceString, strlen(balanceString)) == -1)
					write(STDERR_FILENO, "Master: failed to write balance message for node.", strlen("Master: failed to write balance message for node."));
			}
			node++;
		}
	}

	/* Deallocate memory*/
	free(reg);
	free(usr);
	free(node);
	free(balanceString);
	free(tList);
	free(tpPtr);
	free(buf);
}

void deallocateFacilities(int * exitCode)
{
	/*
	// Ovviamente i processi figli dovranno scollegarsi
	// dai segmenti e chiudere i loro riferimenti alla coda
	// ed ai semafori prima di terminare

	// Precondizione: tutti i processi figli si sono scollegati dai segmenti di memoria
	// In generale, tutti i figli hanno chiuso i loro riferimenti alle facilities IPC
	// ne siamo certi perchè questa procedura viene richiamata solo dopo aver atteso la terminazione di ogni figlio

	// Cosa fare se una delle system call usate per la deallocazione fallisce?*/
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
	int i = 0;

	/* Deallocating register's partitions*/
	write(STDOUT_FILENO, 
		"Master: deallocating register's paritions...\n", 
		strlen("Master: deallocating register's paritions...\n")
	);
	for (i = 0; i < REG_PARTITION_COUNT; i++)
	{
		if (shmdt(regPtrs[i]) == -1)
		{
			msgLength = sprintf(aus, 
							"Master: failed to detach from register's partition number %d.\n", 
							(i + 1)
			);
			if (msgLength < 0)
				safeErrorPrint("Master: failed to format output message in IPC deallocation.");
			else
				write(STDERR_FILENO, aus, msgLength); /* by doing this we avoid calling strlength*/
			*exitCode = EXIT_FAILURE;
		}
		else
		{
			if (shmctl(regPartsIds[i], IPC_RMID, NULL) == -1)
			{
				msgLength = sprintf(aus, 
								"Master: failed to remove register's partition number %d.", 
								(i + 1)
							);
				if (msgLength < 0)
					safeErrorPrint("Master: failed to format output message in IPC deallocation.");
				else
					write(STDERR_FILENO, aus, msgLength);

				*exitCode = EXIT_FAILURE;
			} else {
				msgLength = sprintf(aus,
									"Master: register's partition number %d removed successfully.\n",
									(i + 1));
				write(STDOUT_FILENO, aus, msgLength);
			}
		}
		free(regPtrs[i]);
	}
	free(regPartsIds);

	/* Users list deallocation*/
	free(usersList);

	/* Nodes list deallocation*/
	free(nodesList);

	/* Transaction pools list deallocation*/
	write(STDOUT_FILENO,
		  "Master: deallocating transaction pools...\n",
		  strlen("Master: deallocating transaction pools...\n")
	);
	while (tpList != NULL) {
		
		if (msgctl(tpList->msgQId, IPC_RMID, NULL) == -1) {
			msgLength = sprintf(aus, 
						"Master: failed to remove transaction pool of process %ld", 
						(long)tpList->procId
					);
			if (msgLength < 0)
				safeErrorPrint("Master: failed to format output message in IPC deallocation.");
			else
				write(STDERR_FILENO, aus, msgLength);

			*exitCode = EXIT_FAILURE;
		} else {
			msgLength = sprintf(aus,
								"Master: transaction pool of node of PID %ld successfully removed.\n",
								tpList->procId);
			write(STDOUT_FILENO, aus, msgLength);
		}
	}
	free(tpList);

	/* Deallocating process friends array*/
	free(processesFriends);

	/* Global queue deallocation*/
	write(STDOUT_FILENO,
		  "Master: deallocating global message queue...\n",
		  strlen("Master: deallocating global message queue...\n"));
	if (msgctl(globalQueueId, IPC_RMID, NULL) == -1){
		msgLength = sprintf(aus, "Master: failed to remove global message queue");
		if (msgLength < 0)
			safeErrorPrint("Master: failed to format output message in IPC deallocation.");
		else
			write(STDERR_FILENO, aus, msgLength);
		*exitCode = EXIT_FAILURE;
	} else {
		write(STDOUT_FILENO, 
			"Master: global message queue successfully removed.\n", 
			strlen("Master: global message queue successfully removed.\n")
		);
	}

	/* Writing Semaphores deallocation*/
	write(STDOUT_FILENO,
		  "Master: deallocating writing semaphores...\n",
		  strlen("Master: deallocating writing semaphores...\n")
	);
	if (semctl(wrPartSem, 0, IPC_RMID) == -1){
		msgLength = sprintf(aus, "Master: failed to remove partions' writing semaphores");
		if (msgLength < 0)
			safeErrorPrint("Master: failed to format output message in IPC deallocation.");
		else
			write(STDERR_FILENO, aus, msgLength);
		*exitCode = EXIT_FAILURE;
	} else {
		write(STDOUT_FILENO,
			  "Master: Writing Semaphores successfully removed.\n",
			  strlen("Master: Writing Semaphores successfully removed.\n"));
	}

	/* Reading Semaphores deallocation*/
	write(STDOUT_FILENO,
		  "Master: deallocating reading semaphores...\n",
		  strlen("Master: deallocating reading semaphores...\n"));
	if (semctl(rdPartSem, 0, IPC_RMID) == -1)
	{
		msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
		if (msgLength < 0)
			safeErrorPrint("Master: failed to format output message in IPC deallocation.");
		else
			write(STDERR_FILENO, aus, msgLength);
		*exitCode = EXIT_FAILURE;
	} else
	{
		write(STDOUT_FILENO,
			  "Master: Reading Semaphores successfully removed.\n",
			  strlen("Master: Reading Semaphores successfully removed.\n"));
	}

	/* Fair start semaphore deallocation*/
	write(STDOUT_FILENO,
		  "Master: deallocating fair start semaphores...\n",
		  strlen("Master: deallocating fair start semaphores...\n"));
	if (semctl(fairStartSem, 0, IPC_RMID) == -1){
		msgLength = sprintf(aus, "Master: failed to remove partions' reading semaphores");
		if (msgLength < 0)
			safeErrorPrint("Master: failed to format output message in IPC deallocation.");
		else
			write(STDERR_FILENO, aus, msgLength);
		*exitCode = EXIT_FAILURE;
	} else
	{
		write(STDOUT_FILENO,
			  "Master: Fair start semaphore successfully removed.\n",
			  strlen("Master: Fair start semaphore successfully removed.\n"));
	}

	/* Releasing local variables' memory*/
	free(aus);
}

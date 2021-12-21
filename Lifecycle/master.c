#define _GNU_SOURCE
#include "info.h"

/* struct that rappresents a process and its budget*/
typedef struct proc_budget {
	pid_t proc_pid;
	int budget;
	int p_type; /* type of node: 0 if user, 1 if node */
	struct proc_budget * next;
} proc_budget;

/* list of budgets for every user and node process */
typedef proc_budget* budgetlist;

/* function that frees the space allocated for list p */
void list_free(budgetlist p);


/*****        Global structures        *****/
/*******************************************/
Register **regPtrs;
int *regPartsIds;
ProcListElem *usersList;
ProcListElem *nodesList;
TPElement *tpList;

int globalQueueId;

int fairStartSem; /* Id of the set that contais the three semaphores
                   used to write on the register's partitions */
int wrPartSem;    /* Id of the set that contais the three semaphores
                  // used to write on the register's partitions */
int rdPartSem;    /* Id of the set that contais the three semaphores
                  // used to read from the register's partitions */
/*******************************************/
/*******************************************/

/***** IPC ID of global shared memory *****/
/******************************************/
/* IN TEORIA NON SERVE PIU'
//int shm_id_users; // Shared memory used to store id and state of users processes */
/******************************************/
/******************************************/

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
int SO_USERS_NUM,   /* Number of user processes */
    SO_NODES_NUM,   /* Number of node processes */
    SO_SIM_SEC,     /* Duration of the simulation */
    SO_FRIENDS_NUM; /* Number of friends */
/*******************************************************/
/*******************************************************/

/***** Function that assigns the values ​​of the environment *****/
/***** variables to the global variables defined above     *****/
/***************************************************************/
void assignEnvironmentVariables()
{
    SO_USERS_NUM = atoi(getenv("SO_USERS_NUM"));
    SO_NODES_NUM = atoi(getenv("SO_NODES_NUM"));
    SO_SIM_SEC = atoi(getenv("SO_SIM_SEC"));
    SO_FRIENDS_NUM = atoi(getenv("SO_FRIENDS_NUM"));
}
/***************************************************************/
/***************************************************************/

/***** Function that reads the file containing the configuration    *****/
/***** parameters to save them as environment variables             *****/
/************************************************************************/
int readConfigParameters()
{
    char *filename = "params.txt";
    FILE *fp = fopen(filename, "r");
    /* Reading line by line, max 128 bytes */
    const unsigned MAX_LENGTH = 128;
    /* Array that will contain the lines read from the file */
    char line[14][MAX_LENGTH];
    /* Counter of the number of lines in the file */
    int k = 0, i;

    /* Handles any error in opening the file */
    if (fp == NULL)
    {
        printf("Error: could not open file %s", filename);
        return -1;
    }

    /* Inserts the lines read from the file into the array */
    while (fgets(line[k], MAX_LENGTH, fp))
        k++;

    /* It inserts the parameters read into environment variables */
    for (i = 0; i < k; i++)
        putenv(line[i]);

    /* Assigns the values ​​of the environment */
    /* variables to the global variables defined above */
    assignEnvironmentVariables();

    /* Close the file */
    fclose(fp);

    return 0;
}
/************************************************************************/
/************************************************************************/

/****   Function that creates the ipc structures used in the project    *****/
/****************************************************************************/
void createIPCFacilties()
{
	int i;
    regPtrs = (Register **)malloc(REG_PARTITION_COUNT * sizeof(Register *));
    for (i = 0; i < REG_PARTITION_COUNT; i++)
        regPtrs[i] = (Register *)malloc(REG_PARTITION_SIZE * sizeof(Register));
    regPartsIds = (int *)malloc(REG_PARTITION_COUNT * sizeof(int));
    usersList = (ProcListElem *)malloc(SO_USERS_NUM * sizeof(ProcListElem));
    nodesList = (ProcListElem *)malloc(SO_NODES_NUM * sizeof(ProcListElem));
    tpList = (TPElement *)malloc(SO_NODES_NUM * sizeof(TPElement));
}
/****************************************************************************/
/****************************************************************************/

/*****  Function that initialize the ipc structures used in the project *****/
/****************************************************************************/
void initializeIPCFacilities()
{
    /* Initialization of semaphores */
    fairStartSem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    EXIT_ON_ERROR;

    wrPartSem = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600);
    EXIT_ON_ERROR;

    rdPartSem = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600);
    EXIT_ON_ERROR;
    semctl(fairStartSem, 0, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);

    semctl(rdPartSem, 0, SETVAL, 1);
    semctl(rdPartSem, 1, SETVAL, 1);
    semctl(rdPartSem, 2, SETVAL, 1);

    semctl(wrPartSem, 0, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);
    semctl(wrPartSem, 1, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);
    semctl(wrPartSem, 2, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    /* Creates the global queue */
    globalQueueId = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600); /* MODIFICATO IL 14/12/2021 */
    EXIT_ON_ERROR;
    /********************************************************/
    /********************************************************/

    /*****  Initialization of shared memory segments    *****/
    /********************************************************/
    regPartsIds[0] = shmget(IPC_PRIVATE, REG_PARTITION_SIZE * sizeof(Register), S_IRUSR | S_IWUSR);
    regPartsIds[1] = shmget(IPC_PRIVATE, REG_PARTITION_SIZE * sizeof(Register), S_IRUSR | S_IWUSR);
    regPartsIds[2] = shmget(IPC_PRIVATE, REG_PARTITION_SIZE * sizeof(Register), S_IRUSR | S_IWUSR);
    regPtrs[0] = (Register *)shmat(regPartsIds[0], NULL, 0);
    regPtrs[1] = (Register *)shmat(regPartsIds[1], NULL, 0);
    regPtrs[2] = (Register *)shmat(regPartsIds[2], NULL, 0);
    EXIT_ON_ERROR;

    /*shm_id_users = shmget(IPC_PRIVATE, SO_USERS_NUM * sizeof(ProcListElem), S_IRUSR | S_IWUSR);
    usersList = (ProcListElem *)shmat(shm_id_users, NULL, 0);
    EXIT_ON_ERROR;*/
    /********************************************************/
    /********************************************************/
}
/****************************************************************************/
/****************************************************************************/

/*****  Momentary functions created for testing purposes  *****/
/**************************************************************/
void busy_cpu(unsigned long loops)
{
    int i;
    double my_var = 0.25;

    for (i = 0; i < loops; i++)
    {
        my_var += 0.5;
        my_var = my_var > 1 ? my_var - 1 : my_var;
    }
}

void do_stuff(int t)
{
    if (t == 1)
        printf("Hi, I'm a user, my pid is %d\n", getpid());
    else
        printf("Hi, I'm a node, my pid is %d\n", getpid());
    srand(time(0));
    busy_cpu(rand() % 1000000000);
}
/**************************************************************/
/**************************************************************/

int main(int argc, char *argv[])
{
    pid_t child_pid;
    int status;
    struct sembuf sops;

	/* initialization of the budgetlist - array to maintain budgets read from ledger */
	budgetlist bud_list = NULL;
	budgetlist new_el;
	budgetlist el_list;

	/* definition of objects necessary for nanosleep */
	struct timespec onesec, tim;
	onesec.tv_sec=1;
	onesec.tv_nsec=0;
	
	/* definition of indexes for cycles */
	int i,j,k, ct_updates;
	/* da definire fuori da while per mantenerne i valori */
	int index_reg[REG_PARTITION_COUNT];
	for(i = 0; i < REG_PARTITION_COUNT; i++)
		index_reg[i] = i;

	/* da definire fuori da while per mantenerne i valori */
	int prev_read_nblock[REG_PARTITION_COUNT];
	for(i = 0; i < REG_PARTITION_COUNT; i++)
		prev_read_nblock[i] = 0; /* qui memorizzo il blocco a cui mi sono fermato allo scorso ciclo nella i-esima partizione */

	int ind_block; /* indice per scorrimento blocchi */
	int ind_tr_in_block = 0; /* indice per scorrimento transazioni in blocco */

	/***************** TEMPORANEO **********************/
	/***************** TEMPORANEO **********************/

    /* Set common semaphore options */
    sops.sem_num = 0;
    sops.sem_flg = 0;

    /* Read configuration parameters from */
    /* file and save them as environment variables */
    if (readConfigParameters() == -1)
        exit(EXIT_FAILURE);

    /*****  Creates and initialize the IPC Facilities   *****/
    /********************************************************/
    createIPCFacilties();

    initializeIPCFacilities();
    /********************************************************/
    /********************************************************/

    /*****  Creates SO_USERS_NUM children   *****/
    /********************************************/
    for (i = 0; i < SO_USERS_NUM; i++)
    {
        switch (child_pid = fork())
        {
        case -1:
            /* Handle error */
            EXIT_ON_ERROR;
            exit(EXIT_FAILURE);
        case 0:
            /* The process tells the father that it is ready to run
            // and that it waits for all processes to be ready */
            sops.sem_op = -1;
            semop(fairStartSem, &sops, 1);

            /* Save users processes pid and state into usersList */
            usersList[i].procId = getpid();
            usersList[i].procState = ACTIVE;

            sops.sem_op = 0;
            semop(fairStartSem, &sops, 1);

            /* Temporary part to get the process to do something */
            do_stuff(1);
            printf("User done! PID:%d\n", getpid());
            exit(i);
            break;

        default:
            break;
        }
    }
    /********************************************/
    /********************************************/

    /*****  Creates SO_NODES_NUM children   *****/
    /********************************************/
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        switch (child_pid = fork())
        {
        case -1:
            /* Handle error */
            EXIT_ON_ERROR;
            exit(EXIT_FAILURE);
        case 0:
            /* The process tells the father that it is ready to run
            // and that it waits for all processes to be ready */
            sops.sem_op = -1;
            semop(fairStartSem, &sops, 1);

            /* Save users processes pid and state into usersList */
            nodesList[i].procId = getpid();
            nodesList[i].procState = ACTIVE;

            /* Initialize messages queue for transactions pools */
            tpList[i].procId = getpid();
            tpList[i].msgQId = msgget(getpid(), IPC_CREAT | IPC_EXCL | 0600); /* MODIFICATO IL 14/12/2021 */
            EXIT_ON_ERROR;

            sops.sem_op = 0;
            semop(fairStartSem, &sops, 1);
			
            /* Temporary part to get the process to do something */
            do_stuff(2);
            printf("Node done! PID:%d\n", getpid());
            exit(i);
            break;

        default:
            break;
        }
    }
    /********************************************/
    /********************************************/

    /* The father also waits for all the children
    // to be ready to continue the execution */
    sops.sem_op = -1;
    semop(fairStartSem, &sops, 1);
    sops.sem_op = 0;
    semop(fairStartSem, &sops, 1);

	/* initializing budget for users processes */
	for (i = 0; i < SO_USERS_NUM; i++) {
		new_el = malloc(sizeof(*new_el));
		/* DEVO ACCEDERVI IN MUTUA ESCLUSIONE */
		new_el->proc_pid = 0;/*usersList[i].procId; /* NON VA, ARRAY NON PRENDE VALORI... */
		new_el->budget = atoi(getenv("SO_BUDGET_INIT"));
		new_el->p_type = 0;
		new_el->next = bud_list;
		bud_list = new_el;
	}

	/* initializing budget for nodes processes */
	for (i = 0; i < SO_NODES_NUM; i++) {
		new_el = malloc(sizeof(*new_el));
		/* DEVO ACCEDERVI IN MUTUA ESCLUSIONE */
		new_el->proc_pid = 0;/*nodesList[i].procId;*/
		new_el->budget = 0;
		new_el->p_type = 1;
		new_el->next = bud_list;
		bud_list = new_el;
	}

	/* life cycle of master */
	while(1) {
		
		/*
		 * we have to print on the terminal the actual budget of every
		 * user and node process, calculating it reading from ledger.
		 */
		
		/*
		 * here we should check the semaphore for the pointer to the
		 * first "partition" of the shared memory; when we get access,
		 * we count the budget for every process in the blocks of the
		 * current partition.
		 */
		
		/* 
		 * here we need to read every transaction in every block
 		 * and for every process we need to calculate the actual 
		 * budget (we start from the initial budget of every process
		 * and we remove/add the value of every transaction that has
		 * that process as receiver). We have to store the budget some-
		 * ways, so a good idea could be to use an integer array with 
		 * an entry for every process (node or user), initialized with
		 * the initial budget of every process (read from file at 
		 * start of execution); than for every transaction, we look 
		 * the receiver and update the corresponding entry in the array.
		 * There might be a problem: how can we link pid of a process
		 * with array entry??
		 * It could be better to use a sort of map or key-associative
		 * array, so as to use PID to address the correct entry in the
		 * array [CHECK IF IT'S POSSIBLE]. No, in standard c89 it doesn't
		 * exist. It could be used a list of a struct of two fields, one
		 * for the PID an the other for budget.
		 * NOTE: it could be a good idea to maintain the information of
		 * the budget of every process in the array so that at the next
		 * print (after 1 second) the master doesn't need to re-
		 * calculate every budget but only to update every budget with
		 * the new transaction inserted. To do this, we might have to 
		 * save the id of the last block read (process node insert in 
		 * the ledger only blocks of transaction, so after reading the
		 * master process completes the reading of ledger, he read 
		 * every block in the ledger, so we save the id of the last one
		 * and at the next cycle the master starts looking from that 
		 * place [CHECK IF IT'S TRUE THAT NO BLOCK CAN BE REMOVED FROM
		 * LEDGER]).
		 * Now that we decided to consider the shared memory as divided
		 * in 3 partitions, we need to store the id of the last block
		 * read for everyone of the 3 partitions.
		 */

		/* cycle that updates the budget list before printing it */
		/* at every cycle we do the count of budgets in blocks of the i-th partition */
		for(i = 0; i < REG_PARTITION_COUNT; i++) {
			/* setting options for getting access to i-th partition of register */
			sops.sem_num = i; /* we want to get access to i-th partition */
			sops.sem_op = -1; /* CHECK IF IT'S THE CORRECT VALUE */
			semop(rdPartSem, &sops, 1);

			printf("Master: gained access to %d-th partition of register\n", i);

			ind_block = prev_read_nblock[i]; /* inizializzo l'indice al blocco in cui mi ero fermato allo scorso ciclo */

			/* ciclo di scorrimento dei blocchi della i-esima partizione */
			while(ind_block < regPtrs[index_reg[i]]->nBlocks) { /* CONTROLLARE SE GIUSTO O SE DEVO USARE REG_PARTITION_SIZE */
				Block block = regPtrs[index_reg[i]]->blockList[ind_block]; /* restituisce il blocco di indice ind_block */
				ind_tr_in_block = 0;
				
				/* scorro la lista di transizioni del blocco di indice ind_block */
				while(ind_tr_in_block < SO_BLOCK_SIZE) {
					Transaction trans = block.transList[ind_tr_in_block]; /* restituisce la transazione di indice ind_tr_in_block */
					
					ct_updates = 0; /* conta il numero di aggiornamenti di budget fatti per la transazione (totale 2, uno per sender e uno per receiver) */
					if(trans.sender == -1) {
						ct_updates++;
						/* 
						 * se il sender è -1, rappresenta transazione di pagamento reward del nodo,
						 * quindi non bisogna aggiornare il budget del sender, ma solo del receiver.
						 */
					}
						
					for(el_list = bud_list; el_list != NULL; el_list = el_list->next) {
						/* guardo sender --> devo decrementare di amountSend il suo budget */
						/* se il sender è -1, non si entrerà mai nel ramo then (???) */
						if(trans.sender == el_list->proc_pid) {
							/* aggiorno il budget */
							el_list->budget -= trans.amountSend;
							ct_updates++;
						}

						/* guardo receiver --> devo incrementare di amountSend il suo budget */
						if(trans.receiver == el_list->proc_pid) {
							/* aggiorno il budget */
							el_list->budget += trans.amountSend;
							ct_updates++;
						}

						/* 
						 * condizione di terminazione del ciclo, per velocizzare (non serve controllare il resto 
						 * degli elementi della lista perché ho già aggiornato il budget di sender e receiver della corrente transazione) 
						 */
						if(ct_updates == 2)
							break;
					}

					ind_tr_in_block++;
				}

				ind_block++;
			}

			prev_read_nblock[i] = ind_block; /* memorizzo il blocco a cui mi sono fermato */

			/* setting options for releasing resource of i-th partition of register */
			sops.sem_num = i; /* try to release semaphore for patition i */
			sops.sem_op = 1; /* CHECK IF IT'S THE CORRECT VALUE */
			semop(rdPartSem, &sops, 1);
		}

		/* I've read all the data, now it's time to print them */
		  
		/* print budget of every process with associated PID */
		dprintf(0, "MASTER: Budget of processes:\n");
		for(el_list = bud_list; el_list != NULL; el_list = el_list->next) {
			if(el_list->p_type) /* Budget of user process */
				dprintf(0, "MASTER:  - USER PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
			else /* Budget of node process */
				dprintf(0, "MASTER:  - NODE PROCESS PID %5d: actual budget %4d\n", el_list->proc_pid, el_list->budget);
		}

		/* 
		 * handling of creation of a new node process if a transaction
		 * doesn't fit in any transaction pool of existing node processes
		 */

		/*
		 * Here we need to check if in the msg queue between master and nodes
		 * processes there are messages requesting for a new process to be made.
		 * We read at maximum SO_TP_SIZE messages so that we create a new node
		 * process with that number of transactions in its transaction pool to 
		 * process (SO_TP_SIZE is the maximum number of transactions in nodes
		 * processes' transaction pool).
		 * From every message we extract the transaction and we put it a new
		 * transaction pool that it will be the one of the node process (to do
		 * so, check the node processes creation); than, after we initialize all
		 * the data needed by new node process, we use execve to start it (check
		 * nodes creation, it needs fork).
		 * Also, we need to choose a list of friends node processes for the new
		 * node process; for every process chosen, we need to send it a message
		 * on the appropriate queue that informs it of its new friend.
		 */

		/* 
		 * From a queue you can only retrieve one message at a time; to read at maximum
		 * SO_TP_SIZE messages you have to make a cycle with an index counting the number
		 * of messages read (to check until it is minor than SO_TP_SIZE) and in the 
		 * condition you have to use msgrcv with IPC_NOWAIT (the system call msgrcv block
		 * if there are no messages to be read, so with IPC_NOWAIT the system call returns
		 * -1 that indicated the queue is empty, and we use it as an exit condition from loop).
		 * In this cycle we create the transaction pool of the new node process.
		 */
				
		MsgGlobalQueue msg_from_node;
		int c_msg_read;
		c_msg_read = 0;
		int SO_TP_SIZE = atoi(getenv("SO_TP_SIZE"));
		Transaction transanctions_read[SO_TP_SIZE]; /* array of transactions read from global queue */

		/*
		 * COSE DA FARE:
		 	- creare tutte le strutture di memoria per il nuovo nodo (dopo aver letto SO_TP_SIZE messaggi)
		 	- aggiungere la nuova transaction pool a tpList globale ??? Problema!!!! Abbiamo creato tpList
			 come lista di transaction pools in modo che un processo utente possa scegliere un nodo a cui
			 inviare la transazione; il problema è che l'abbiamo creata statica e non è una zona di memoria 
			 condivisa con i processi utenti, come fanno a vederla ?? Come facciamo ad aggiungere quella 
			 appena creata ????!!!! [NON CREDO SERVA PIÙ, L'AGGIORNAMENTO LO FACCIAMO PRINCIPALMENTE PER IL MASTER]
		 */

		/* messages reading cycle */
		/* MSG_COPY solo LINUX, non va bene..... */
		/* IN CASO DECIDIAMO CHE NON VADA BENE MSG_COPY, DEVO TOGLIERLO E SE NON È IL MESSAGGIO CHE VOGLIO LO DEVO RISCRIVERE SULLA CODA */
		while(msgrcv(globalQueueId, &msg_from_node, sizeof(msg_from_node)-sizeof(long), getpid(), IPC_NOWAIT | MSG_COPY) != -1 && c_msg_read < SO_TP_SIZE) {
			/* come dimensione specifichiamo sizeof(msg_from_node)-sizeof(long) perché bisogna specificare la dimensione del testo, non dell'intera struttura */
			/* come mType prendiamo i messaggi destinati al Master, cioè il suo pid (prende il primo messaggio con quel mType) */
			/* prendiamo il messaggio con flag MSG_COPY perché altrimenti se non è di tipo NEWNODE lo elimineremmo */
			
			/* in questo caso cerchiamo i messaggi con msgContent NEWNODE */
			if(msg_from_node.msgContent == NEWNODE) {
				/* 
				 * per aggiungere la transazione alla transaction pool, devo aggiungere un nuovo messaggio 
				 * alla msgqueue che sarebbe la tp del nuovo nodo 
				 * siccome prima di creare la TP del nuovo nodo devo accertarmi che ci sia un nuovo nodo da creare,
				 * creiamo una lista di TPElement di massimo SO_TP_SIZE transazioni e poi quando abbiamo creato la TP
				 * del nuovo nodo ci inseriamo i messaggi sopra. 
				 */
				memcpy(&transanctions_read[c_msg_read], &msg_from_node.transaction, sizeof(msg_from_node.transaction));
				/* DA CONTROLLARE !!!!!! */
				
				c_msg_read++;

				/* DEVO RICORDARMI DI RIMUOVERE IL MESSAGGIO DALLA QUEUE */
				msgrcv(globalQueueId, &msg_from_node, sizeof(msg_from_node)-sizeof(long), getpid(), IPC_NOWAIT);
				EXIT_ON_ERROR
			}
		}

		/* SHOULD CHECK IF ERRNO is ENOMSG, otherwise an error occurred */
		if(errno == ENOMSG) {
			if(c_msg_read == 0)
				printf("Master: No creation of new node needed\n");
			else {
				printf("Master: No more transactions to read from global queue. Starting creation of new node...\n");
				
				/******* CREATION OF NEW NODE PROCESS *******/
				/********************************************/

				int id_new_friends[SO_FRIENDS_NUM]; /* array to keep track of already chosen new friends */
				int new; /* flag */
				int index, tr_written;

				/* setting every entry of array to -1 (it rappresents "not chosen") */
				for(i = 0; i < SO_FRIENDS_NUM; i++)
					id_new_friends[i] = -1;
				
				switch(fork()) {
					case -1:
						/* Handle error */
						EXIT_ON_ERROR;
					case 0:
						/* NEW NODE */
						/* Creation of list of friends for new node */
						srand(getpid()); /* we put it here so that for every new node we generate a different sequence */
						
						for(i = 0; i < SO_FRIENDS_NUM; i++) {
							if(i == 0) {
								/* first friend in array, no need to check if already chosen */
								index = rand()%SO_NODES_NUM; /* generate new index */
							} else {
								new = 0;
								/* choosing a new friend */
								while(!new) {
									index = rand()%SO_NODES_NUM; /* generate new index */
									/* check if it is already a friend */
									j = 0;
									while(j < SO_FRIENDS_NUM && !new){
										if(id_new_friends[j] == -1)
											new = 1; /* no friend in this position */
										else if(id_new_friends[j] == index)
											break; /* if friend already chosen, change index */
										j++;
									}
								}
							}

							/* adding new index friend to array */
							id_new_friends[i] = index;

							/* CAPIRE COME CREARE LA LISTA DI AMICI */
							/* inviare su coda di messaggi globale con msgContent FRIENDINIT */
							/* send a message on global queue to new node informing it of its new friend */
							
							/* declaration of node to send to new friend */
							MsgGlobalQueue msg_to_node;
							msg_to_node.mType = getpid();
							msg_to_node.msgContent = FRIENDINIT;
							msg_to_node.friend.procId = nodesList[index].procId; /* devo accedervi in mutua esclusione */
							msg_to_node.friend.procState = ACTIVE;
							msgsnd(globalQueueId, &msg_to_node, sizeof(msg_to_node)-sizeof(long), 0);
							EXIT_ON_ERROR

							/* here we notice the friend node of its new friend (the new node created here) */
							msg_to_node.mType = nodesList[index].procId; /* devo accedervi in mutua esclusione */
							msg_to_node.msgContent = NEWFRIEND;
							msg_to_node.friend.procId = getpid();
							msg_to_node.friend.procState = ACTIVE;
							msgsnd(globalQueueId, &msg_to_node, sizeof(msg_to_node)-sizeof(long), 0);
							EXIT_ON_ERROR

						}

#if 0
						/* NON MODIFICHIAMO PIÙ LA LUNGHEZZA DELLA LISTA DI NODI, È SEGMENTO DI MEMORIA CONDIVISA */
						/* usare realloc per aggiungere spazio a vettore nodesList */
						/* add a new entry to the nodeList array */
						nodesList = (ProcListElem *)realloc(nodesList, sizeof(*nodesList) + sizeof(ProcListElem));
						int nl_length = sizeof(*nodesList)/sizeof(ProcListElem); /* get nodesList length */
						/* Save new node process pid and state into nodesList */
						nodesList[nl_length-1].procId = getpid();
						nodesList[nl_length-1].procState = ACTIVE;
#endif

						/* CAPIRE SE DA ULTIME DISPOSIZIONI SI DEVE ANCORA FARE O NO */

						/* stessa cosa per tpList del nuovo nodo */
						/* add a new entry to the tpList array */
						tpList = (TPElement *)realloc(tpList, sizeof(*tpList) + sizeof(TPElement));
						int tpl_length = sizeof(*tpList)/sizeof(TPElement); /* get tpList length */
						/* Initialize messages queue for transactions pools */
						tpList[tpl_length-1].procId = getpid();
						tpList[tpl_length-1].msgQId = msgget(getpid(), IPC_CREAT | IPC_EXCL | 0600);
						EXIT_ON_ERROR;

						int tp_new_node = tpList[tpl_length-1].msgQId;
						/* here we have to insert transactions read from global queue in new node TP*/
						for(tr_written = 0; tr_written < c_msg_read; tr_written++) { /* c_msg_read is the number of transactions actually read*/
							MsgTP new_trans;
							new_trans.mType = getpid();
							memcpy(&new_trans.transaction, &transanctions_read[tr_written], sizeof(new_trans.transaction)); /* CHECK IF IT'S CORRECT */
							msgsnd(tp_new_node, &new_trans, sizeof(new_trans)-sizeof(long), 0);
							EXIT_ON_ERROR /* DA CAMBIARE */
						}

						/* TO COMPLETE.......*/

						/*execve(...);*/ 
						break;
					default:
						/* MASTER */
						break;
				}
			}
		}
		else 
			EXIT_ON_ERROR
		
		/* AGGIUNGERE PARTE IN CUI CONTROLLO SE DEGLI UTENTI SONO TERMINATI PER AGGIORNARE LA LISTA DI UTENTI IN MEMORIA CONDIVISA!!!! */
		
		/* AGGIUNGERE PARTE DI FEDE CON CONTROLLO LIBRO MASTRO PIENO */

		/* now sleep for 1 second */
		nanosleep(&onesec, &tim);
		
		/* condition to exit, just for testing */
		break;
	}

    /* Momentary management for the termination of the simulation */
    while ((child_pid = wait(&status)) != -1)
        printf("PARENT: PID=%d. Got info of child with PID=%d\n", getpid(), child_pid);
    if (errno == ECHILD)
    {
        printf("In PID=%d, no more child processes\n", getpid());
        /* Deallocate messages queues */
        msgctl(globalQueueId, IPC_RMID, NULL);
        for (i = 0; i < SO_NODES_NUM; i++)
            msgctl(tpList[i].msgQId, IPC_RMID, NULL);
        /* Release shared memories */
        shmdt(regPtrs[0]);
        shmdt(regPtrs[1]);
        shmdt(regPtrs[2]);
        shmdt(usersList);
        /* Deallocate the semaphores */
        semctl(fairStartSem, 0, IPC_RMID);
        semctl(rdPartSem, 0, IPC_RMID);
        semctl(rdPartSem, 1, IPC_RMID);
        semctl(rdPartSem, 2, IPC_RMID);
        semctl(wrPartSem, 0, IPC_RMID);
        semctl(wrPartSem, 1, IPC_RMID);
        semctl(wrPartSem, 2, IPC_RMID);
        /* Deallocate shared memory */
        shmctl(regPartsIds[0], 0, IPC_RMID);
        shmctl(regPartsIds[1], 0, IPC_RMID);
        shmctl(regPartsIds[2], 0, IPC_RMID);
        /*shmctl(shm_id_users, 0, IPC_RMID);*/
        /*EXIT_ON_ERROR;*/
        free(regPtrs);
        free(regPartsIds);
        free(usersList);
        free(nodesList);
        free(tpList);

		/* freeing of budget list */
		list_free(bud_list);

        exit(EXIT_SUCCESS);
    }
    else
    {
        fprintf(stderr, "Error #%d: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void list_free(budgetlist p)
{
	if (p == NULL) return;
	
	list_free(p->next);
	free(p);
}


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include "info.h"

/*****  Function to give information about an error on semaphores  *****/
/***********************************************************************/
#define TEST_ERROR                                 \
    if (errno)                                     \
    {                                              \
        fprintf(stderr,                            \
                "%s:%d: PID=%5d: Error %d (%s)\n", \
                __FILE__,                          \
                __LINE__,                          \
                getpid(),                          \
                errno,                             \
                strerror(errno));                  \
    }
/***********************************************************************/
/***********************************************************************/

/*****        Global structures        *****/
/*******************************************/
Register **regPtrs;
int *regPartsIds;
ProcListElem *usersList;
ProcListElem *nodesList;
TPElement *tpList;

int globalQueueId;

int fairStartSem; // Id of the set that contais the three semaphores
                  // used to write on the register's partitions
int wrPartSem;    // Id of the set that contais the three semaphores
                  // used to write on the register's partitions
int rdPartSem;    // Id of the set that contais the three semaphores
                  // used to read from the register's partitions
/*******************************************/
/*******************************************/

/***** IPC ID of global shared memory *****/
/******************************************/
// IN TEORIA NON SERVE PIU'
//int shm_id_users; // Shared memory used to store id and state of users processes
/******************************************/
/******************************************/

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
int SO_USERS_NUM,   // Number of user processes
    SO_NODES_NUM,   // Number of node processes
    SO_SIM_SEC,     // Duration of the simulation
    SO_FRIENDS_NUM; // Number of friends
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
    // Reading line by line, max 128 bytes
    const unsigned MAX_LENGTH = 128;
    // Array that will contain the lines read from the file
    char line[14][MAX_LENGTH];
    // Counter of the number of lines in the file
    int k = 0;

    // Handles any error in opening the file
    if (fp == NULL)
    {
        printf("Error: could not open file %s", filename);
        return -1;
    }

    // Inserts the lines read from the file into the array
    while (fgets(line[k], MAX_LENGTH, fp))
        k++;

    // It inserts the parameters read into environment variables
    for (int i = 0; i < k; i++)
        putenv(line[i]);

    // Assigns the values ​​of the environment
    // variables to the global variables defined above
    assignEnvironmentVariables();

    // Close the file
    fclose(fp);

    return 0;
}
/************************************************************************/
/************************************************************************/

/****   Function that creates the ipc structures used in the project    *****/
/****************************************************************************/
void createIPCFacilties()
{
    regPtrs = (Register **)malloc(REG_PARTITION_COUNT * sizeof(Register *));
    for (int i = 0; i < REG_PARTITION_COUNT; i++)
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
    // Initialization of semaphores
    fairStartSem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    TEST_ERROR;

    wrPartSem = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600);
    TEST_ERROR;

    rdPartSem = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600);
    TEST_ERROR;
    semctl(fairStartSem, 0, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);

    semctl(rdPartSem, 0, SETVAL, 1);
    semctl(rdPartSem, 1, SETVAL, 1);
    semctl(rdPartSem, 2, SETVAL, 1);

    semctl(wrPartSem, 0, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);
    semctl(wrPartSem, 1, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);
    semctl(wrPartSem, 2, SETVAL, SO_USERS_NUM + SO_NODES_NUM + 1);

    /*****  Creates and initialize the messages queues  *****/
    /********************************************************/
    // Creates the global queue
    globalQueueId = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL);
    TEST_ERROR;
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
    TEST_ERROR;

    /*shm_id_users = shmget(IPC_PRIVATE, SO_USERS_NUM * sizeof(ProcListElem), S_IRUSR | S_IWUSR);
    usersList = (ProcListElem *)shmat(shm_id_users, NULL, 0);
    TEST_ERROR;*/
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

    // Set common semaphore options
    sops.sem_num = 0;
    sops.sem_flg = 0;

    // Read configuration parameters from
    // file and save them as environment variables
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
    for (int i = 0; i < SO_USERS_NUM; i++)
    {
        switch (child_pid = fork())
        {
        case -1:
            //Handle error
            TEST_ERROR;
            exit(EXIT_FAILURE);
        case 0:
            // The process tells the father that it is ready to run
            // and that it waits for all processes to be ready
            sops.sem_op = -1;
            semop(fairStartSem, &sops, 1);

            // Save users processes pid and state into usersList
            usersList[i].procId = getpid();
            usersList[i].procState = ACTIVE;

            sops.sem_op = 0;
            semop(fairStartSem, &sops, 1);

            // Temporary part to get the process to do something
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
    for (int i = 0; i < SO_NODES_NUM; i++)
    {
        switch (child_pid = fork())
        {
        case -1:
            // Handle error
            TEST_ERROR;
            exit(EXIT_FAILURE);
        case 0:
            // The process tells the father that it is ready to run
            // and that it waits for all processes to be ready
            sops.sem_op = -1;
            semop(fairStartSem, &sops, 1);

            // Save users processes pid and state into usersList
            nodesList[i].procId = getpid();
            nodesList[i].procState = ACTIVE;

            // Initialize messages queue for transactions pools
            tpList[i].procId = getpid();
            tpList[i].msgQId = msgget(getpid(), IPC_CREAT | IPC_EXCL);
            TEST_ERROR;

            sops.sem_op = 0;
            semop(fairStartSem, &sops, 1);

            // Temporary part to get the process to do something
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

    // The father also waits for all the children
    // to be ready to continue the execution
    sops.sem_op = -1;
    semop(fairStartSem, &sops, 1);
    sops.sem_op = 0;
    semop(fairStartSem, &sops, 1);

    // Momentary management for the termination of the simulation
    while ((child_pid = wait(&status)) != -1)
        printf("PARENT: PID=%d. Got info of child with PID=%d\n", getpid(), child_pid);
    if (errno == ECHILD)
    {
        printf("In PID=%d, no more child processes\n", getpid());
        // Deallocate messages queues
        msgctl(globalQueueId, IPC_RMID, NULL);
        for (int i = 0; i < SO_NODES_NUM; i++)
            msgctl(tpList[i].msgQId, IPC_RMID, NULL);
        // Release shared memories
        shmdt(regPtrs[0]);
        shmdt(regPtrs[1]);
        shmdt(regPtrs[2]);
        shmdt(usersList);
        // Deallocate the semaphores
        semctl(fairStartSem, 0, IPC_RMID);
        semctl(rdPartSem, 0, IPC_RMID);
        semctl(rdPartSem, 1, IPC_RMID);
        semctl(rdPartSem, 2, IPC_RMID);
        semctl(wrPartSem, 0, IPC_RMID);
        semctl(wrPartSem, 1, IPC_RMID);
        semctl(wrPartSem, 2, IPC_RMID);
        // Deallocate shared memory
        shmctl(regPartsIds[0], 0, IPC_RMID);
        shmctl(regPartsIds[1], 0, IPC_RMID);
        shmctl(regPartsIds[2], 0, IPC_RMID);
        //shmctl(shm_id_users, 0, IPC_RMID);
        //TEST_ERROR;
        free(regPtrs);
        free(regPartsIds);
        free(usersList);
        free(nodesList);
        free(tpList);
        exit(EXIT_SUCCESS);
    }
    else
    {
        fprintf(stderr, "Error #%d: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}
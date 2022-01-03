#include "user.h"

struct timespec now;
ProcListElem *usersList = NULL;
ProcListElem *nodesList = NULL;

/***** Definition of global variables that contain *****/
/***** the values ​​of the configuration parameters  *****/
/*******************************************************/
int SO_USERS_NUM, /* Number of user processes NOn è "statico" ???*/
    SO_NODES_NUM, /* Number of node processes ?? NOn è "statico" ???*/
    SO_REWARD,
    SO_MIN_TRANS_GEN_NSEC,
    SO_MAX_TRANS_GEN_NSEC,
    SO_RETRY,
    SO_TP_SIZE,
    SO_MIN_TRANS_PROC_NSEC,
    SO_MAX_TRANS_PROC_NSEC,
    SO_BUDGET_INIT,
    SO_SIM_SEC,     /* Duration of the simulation*/
    SO_FRIENDS_NUM; /* Number of friends*/
/*******************************************************/
/*******************************************************/

int extractReceiver(pid_t pid);
int extractNode();

int main(int argc, char **argv, char *envp[])
{
    key_t key;
    int tId = -1;
    int trans_gen_time;
    int balance = 100;
    int num_failure = 0;
    float amount = 0.0;
    float reward = 0.0;
    pid_t receiver_node;
    int receiver_node_index;
    pid_t receiver_user;
    int receiver_user_index;
    Transaction new_tran;
    MsgTP msgT;
    struct timespec remaining, request;

    SO_MIN_TRANS_GEN_NSEC = 100000000;
    SO_MAX_TRANS_GEN_NSEC = 200000000;

    /* Check that the balance is greater than or equal
    to 2 and create the new transaction */
    if (balance >= 2)
    {
        /* Extracts the receiving user and the node and
        calculates the amount and reward randomly */
        receiver_user_index = extractReceiver(getpid());
        receiver_node_index = extractNode();
        if (receiver_user_index == -1 || receiver_node_index == -1)
            safeErrorPrint("User: failed to extract user and node receiver. Error: ");
        else
        {
            receiver_user = usersList[receiver_node_index].procId;
            receiver_node = nodesList[receiver_node_index].procId;
            srand(getpid());
            amount = rand() % balance + 2;
            reward = (float)(amount / 100) * SO_REWARD;
            amount = amount - reward;

            /* Set the new transaction to send */
            clock_gettime(CLOCK_REALTIME, &now);
            new_tran.timestamp = now;
            new_tran.sender = getpid();
            new_tran.receiver = receiver_user;
            new_tran.amountSend = amount;
            new_tran.reward = reward;

            /* Prepare the message with the transaction to send to the node */
            msgT.mType = receiver_node;
            msgT.transaction = new_tran;

            /* Hang up the message queue and send the transaction to it */
            key = ftok(MSGFILEPATH, receiver_node);
            if (key == -1)
                safeErrorPrint("User: failed to connect to transaction pool. Error: ");
            else
            {
                tId = msgget(key, 0600);
                if (tId == -1)
                    safeErrorPrint("User: failed to connect to transaction pool. Error: ");
                else
                {
                    if (msgsnd(tId, &msgT, sizeof(Transaction), 0600) == -1)
                    {
                        safeErrorPrint("User: failed to dispatch transaction to node. Error: ");
                    }
                    else
                    {
                        write(STDOUT_FILENO,
                              "User: transaction successfully dispatched to node.\n",
                              strlen("User: transaction successfully dispatched to node.\n"));
                        /* Wait a random time in between SO_MIN_TRANS_GEN_NSEC and SO_MAX_TRANS_GEN_NSEC */
                        trans_gen_time = rand() % SO_MAX_TRANS_GEN_NSEC + SO_MIN_TRANS_GEN_NSEC;
                        request.tv_sec = 0;
                        request.tv_nsec = trans_gen_time;
                        printf("User: sending the created transaction to the node...\n");
                        if (nanosleep(&request, &remaining) == -1)
                            safeErrorPrint("User: failed to simulate waiting for processing the transaction. Error: ");
                    }
                }
            }
        }
    }
    else
        num_failure++;

    exit(EXIT_SUCCESS);
}

int extractReceiver(pid_t pid)
{
    int n = -1;
    do
    {
        clock_gettime(CLOCK_REALTIME, &now);
        n = now.tv_nsec % SO_USERS_NUM;
    } while (pid == usersList[n].procId);
    return n;
}

int extractNode()
{
    int n = -1;
    clock_gettime(CLOCK_REALTIME, &now);
    return n = now.tv_nsec % SO_NODES_NUM;
}
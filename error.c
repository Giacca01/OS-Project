/*
    This modules contains all the functions used in the various
    modules of the project.
    The idea is to put these functions in a separate module to
    load them only if necessary.
    We don't call an exit function here, because particular operations
    such as deallocating IPC facilities or notifying the master should
    be done in these cases.
*/
#include "error.h"

/* Vedere come trattare eventuali errori in queste funzioni*/
/* Liberamente ispirato a EXIT_ON_ERROR del prof Radicioni*/
void unsafeErrorPrint(char *msg)
{
    fprintf(stderr, "An error occurred at line %d during the execution of process of PID: %ld. See following descrption.\n",
            __LINE__, (long)getpid());

    if (errno)
    {
        /* If errno was set we dispaly the internal message*/
        perror(msg);
    }
    else
    {
        /* If errno is unset we can only rely on user-defined message*/
        strcat(msg, "\n");
        fputs(msg, stderr);
    }
}

void safeErrorPrint(char *msg)
{
    char *aus = NULL;
    int ret = -1;

    ret = sprintf(aus, "An error occurred at line %d during the execution of process of PID: %ld. See following descrption.\n",
                  __LINE__, (long)getpid());
    write(STDERR_FILENO, aus, ret);

    if (errno)
    {
        /* check if null*/
        strcat(msg, strerror(errno));
        write(STDERR_FILENO, msg, strlen(msg));
    }
    else
    {
        write(STDERR_FILENO, msg, strlen(msg));
    }

    write(STDERR_FILENO, "\n", 1);
}

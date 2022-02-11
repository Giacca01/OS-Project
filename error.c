#include "error.h"
/*
 *    This modules contains all the functions used in the various
 *    modules of the project.
 *    The idea is to put these functions in a separate module to
 *    load them only if necessary.
 *    We don't call an exit function here, because particular operations
 *    such as deallocating IPC facilities or notifying the master should
 *    be done in these cases.
 */

void unsafeErrorPrint(char *msg, int line)
{
    fprintf(stderr, "An error occurred at line %d during the execution of process of PID: %ld. See following description.\n",
            line, (long)getpid());

    if (errno)
    {
        /* If errno was set we dispaly the internal message */
        perror(msg);
        errno = 0;
    }
    else
    {
        /* If errno is unset we can only rely on user-defined message */
        fprintf(stderr, "%s\n", msg);
    }
}

void safeErrorPrint(char *msg, int line)
{
    char *aus = NULL;
    int ret = -1;

    aus = (char *)calloc(500, sizeof(char));

    ret = snprintf(aus, 499, "An error occurred at line %d during the execution of process of PID: %5ld. See following description.\n",
                   line, (long)getpid());
    write(STDERR_FILENO, aus, strlen(aus));

    if (errno)
    {
        dprintf(STDERR_FILENO, "%s%s\n", msg, strerror(errno));
        errno = 0;
    }
    else
    {
        dprintf(STDERR_FILENO, "%s\n", msg);
    }

    free(aus);
}

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <wait.h>
#include <sys/types.h>
#include <stdio.h>

int main(int argc, char * argv[])
{
    FILE * f = NULL;
    char ipc_res;
    char par2[10];
    long id_res;
    int status;

    f = fopen("./IPC_resources.txt", "r");

    if(f == NULL)
    {
        printf("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while(fscanf(f, "%c;%ld\n", &ipc_res, &id_res) != EOF)
    {
        /* The line contains this format: ipc-structure;id-of-structure */

        snprintf(par2, log10(id_res)+2, "%ld", id_res);

        if(!fork())
        {
            if(ipc_res == 'm')
            {
                /*printf("par 1: %c\tpar 2: %s\n", ipc_res, par2);*/
                if(execlp("ipcrm", "ipcrm", "-m", par2, NULL) == -1)
                    printf("%s\n", strerror(errno));
            }
            else if(ipc_res == 's')
            {
                /*printf("par 1: %c\tpar 2: %s\n", ipc_res, par2);*/
                if(execlp("ipcrm", "ipcrm", "-s", par2, NULL) == -1)
                    printf("%s\n", strerror(errno));
            }
            else if(ipc_res == 'q')
            {
                /*printf("par 1: %c\tpar 2: %s\n", ipc_res, par2);*/
                if(execlp("ipcrm", "ipcrm", "-q", par2, NULL) == -1)
                    printf("%s\n", strerror(errno));
            }
            exit(EXIT_SUCCESS);
        }
        else
        {
            par2[0] = 0;
        }
    }

    while(wait(&status) != -1);

    fclose(f);
    remove("./IPC_resources.txt");
    exit(EXIT_SUCCESS);
}
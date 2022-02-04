#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <math.h>

int main(int argc, char * argv[])
{
    FILE * f = NULL;
    pid_t pid;
    int status;
    char par[10];

    f = fopen("./processes_created.txt", "r");

    if(f == NULL)
    {
        printf("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while(fscanf(f, "%d\n", &pid) != EOF)
    {
        if(!fork())
        {
            snprintf(par, log10(pid)+2, "%d", pid);
            
            if(execlp("kill", "kill", "-9", par, NULL) == -1)
                printf("%d: %s\n", errno, strerror(errno));
            exit(EXIT_SUCCESS);
        }
    }

    while(wait(&status) != -1);

    fclose(f);
    remove("./processes_created.txt");
    exit(EXIT_SUCCESS);
}
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char ** argv)
{
    int pid;

    for(;;) 
    {
        pid = fork();
        
        if(pid == -1) 
        {
            printf("fork failed, pid:%d\n", pid);
            exit(1);
        } 
        else if (pid) 
        {
            printf("forked pid:%d\n", pid);
        } 
        else 
        {
            for (;;) 
            {
                sleep(1);
            }
        };
    }

    exit(0);
}
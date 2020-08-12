#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char ** argv)
{
    int id;

    if (argc < 2)
    {
        printf("No Command Was Given To Trace\n");
        exit(0);
    }

    id = fork();
    
    if (id == -1) {
        exit(-1);
    }
    else if (id == 0) {
        /* we are in the child */
        strace_on();
        exec(argv[1], &argv[1]);
        strace_off();
        printf("Child: would run strace system call for %s but failed.\n", argv[1]);
        exit(0);
    } else {
        /* we are in the parent */
        id = wait(&id);
    }
    exit(0);
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int
main(void)
{
    int id;
    int x = 3;

    printf("Parent: x = %d\n", x);

    id = fork();

    if (id == 0) {
        /* we are in the child */
        printf("Child: id = %d\n", id);
        printf("Child PID = %d\n", getpid());
        printf("Child: x = %d\n", x);
        x = 99;
        printf("Child: x = %d\n", x);
        printf("Child: &x = %x\n", &x);
        exit(0);
    } else {
        /* we are in the parent */
        printf("Parent PID = %d\n", getpid());
        printf("Parent: id = %d\n", id);
        printf("Parent: wait for child\n");
        id = wait(&id);
        printf("Parent: child terminated pid = %d\n", id);
        printf("Parent: x = %d\n", x);
        printf("Parent: &x = %x\n", &x);
    }

    exit(0);
}


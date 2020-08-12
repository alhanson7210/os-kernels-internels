#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/spinlock.h"
#include "kernel/riscv.h"
#include "kernel/proc.h"
#include "user/user.h"

int 
main(int argc, char ** argv)
{
    int pid, fd;
    char * filename;
    
    if (argc != 3)
    {
        printf("Suspend implementation: suspend pid filename\n");
        exit(-1);
    }
    pid = atoi(argv[1]);
	filename = argv[2];

	fd = open(filename, O_CREATE | O_WRONLY);
	if (fd < 0)
	{
        printf("Opening file %s has failed and is now unlinked\n", filename);
		unlink(filename);
		exit(-1);
	}
	
    if(suspend(pid, fd) < 0)
    {
        printf("Suspending process %d into %s failed\n", pid, filename);
        close(fd);
		unlink(filename);
        exit(-1);
    }
    printf("Successfully suspended process\n");
    close(fd);
    kill(pid);
    exit(0);
}

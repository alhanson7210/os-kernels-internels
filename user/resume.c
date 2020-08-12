#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char ** argv)
{
    int status, pid;
    char * filename;

    if (argc != 2)
    {
        printf("Resume implementation: resume filename\n");
        exit(-1);
    }
    filename = argv[1];

    pid = fork();
    if (pid < 0)
    {
        printf("Fork failed\n");
    	exit(-1);
    }
    
    if (pid == 0)
    {
		status = resume(filename);
	    if (status < 0) 
	    {
	        printf("Resuming a process failed for %s.\n", filename);
	        exit(-1);
	    }
    }
    exit(0);
}

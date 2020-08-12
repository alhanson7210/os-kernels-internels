#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/spinlock.h"
#include "kernel/riscv.h"
#include "kernel/proc.h"
#include "user/user.h"


// static char *states[] = {
// 	[UNUSED]    "unused",
// 	[SLEEPING]  "sleep ",
// 	[RUNNABLE]  "runble",
// 	[RUNNING]   "run   ",
// 	[ZOMBIE]    "zombie",
// 	[SUSPENDED] "suspended"
// };

int
main(int argc, char **argv)
{
	int status;
	// struct ptable pt; //storage space for ps information

	if (argc > 1)
	{
		printf("failed\n");
		exit(-1);
	}
	// printf("ps has started on the user. argv is %s\n", argv);
	// pt.count = 0;
	
	// pid = fork();
    // if (pid < 0)
    // {
    //     printf("Fork failed\n");
    // 	exit(-1);
    // }
    
    // if (pid == 0)
    // {
	status = psinfo();
	if (status < 0) 
	{
		printf("ps failed.\n");
		exit(-1);
	}
	// 	exit(0);
    // } 

	// wait(&pid);
	// if (pt.count <= 0) 
	// {
	// 	printf("No processes to display\n");
	// 	exit(0);
	// }

	// printf("PID\tMEM\tNAME\tSTATE\tCONTAINER\n");
	// for (i = 0; i < pt.count; i++)
	// 	printf("%3d\t%3dK\t%16s\t%16s\t32%s\n", 
	// 		pt.pinfo_t[i].pid, 
	// 		pt.pinfo_t[i].mem, 
	// 		pt.pinfo_t[i].name,
	// 		states[pt.pinfo_t[i].state],
	// 		pt.pinfo_t[i].cname
	// 	);
	exit(0);
}

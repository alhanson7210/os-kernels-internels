#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/spinlock.h"
#include "kernel/riscv.h"
#include "kernel/proc.h"
#include "kernel/fs.h"
#include "user/user.h"

#define COMMMANDSZ 15
#define TOOLSZ 6
//reference
enum CTOOLS { CCREATE, CINFO, CPAUSE, CRESUME, CSTART, CSTOP };
//six possible ctool commands to run
static char *tools[TOOLSZ] = {
    [CCREATE]    "create\0",
    [CINFO]  "info\0",
    [CPAUSE]  "pause\0",
    [CRESUME]   "resume\0",
    [CSTART]    "start\0",
    [CSTOP] "stop\0"
};
//reference
enum COMMANDS { CAT, COUNTER, ECHO, FORK, GREP, KILL, LN, LS, MKDIR, PS, RESUME, RM, SH, STRACE, SUSPEND };
//fifteen possible commands that can be copied to a containers fill system from root
static char *programs[COMMMANDSZ] = {
    [CAT]     "cat\0",
    [COUNTER] "counter\0",
    [ECHO]    "echo\0",
    [FORK]    "fork\0",
    [GREP]    "grep\0",
    [KILL]    "kill\0",
    [LN]      "ln\0",
    [LS]      "ls\0",
    [MKDIR]   "mkdir\0",
    [PS]      "ps\0",
    [RESUME]  "resume\0",
    [RM]      "rm\0",
    [SH]      "sh\0",
    [STRACE]  "strace\0",
    [SUSPEND] "suspend\0"
};

enum VCS { VC1, VC2, VC3, VC4 };
static char *vcs[NVC] = { 
    [VC1] "vc1\0",
    [VC2] "vc2\0",
    [VC3] "vc3\0",
    [VC4] "vc4\0"
 };

void
init_vc_array()
{
    int i;
    char * vcn = "vc1\0";
    for (i = 0; i < NVC; i++)
    {
        vcn[2] = '1' + i;
        strcpy(vcs[i], vcn);
    }
}

void
error(void)
{
    printf(
    "ctool needs a command and its options/arguments\nctool <cmd> <arg(s)>\nctool <create> <container> <program(s)>\nctool <info>\nctool <pause> <container>\nctool <resume <container>\nctool <start> <vcN> <container> <program>\n"
    );
    exit(-1);
}

int copy(char* source, char* destination)
{
  int src_file, dst_file, n;
  char buf[512];

  if((src_file = open(source, O_RDONLY)) < 0) {
    printf("Can't open the source file!\n");
    return -1;
  }

  if((dst_file = open(destination, O_RDWR|O_CREATE)) < 0) {
    printf("Can't open or create the destination file!\n");
    return -1;
  }
  
  while((n = read(src_file, buf, sizeof(buf))) > 0) {
    write(dst_file, buf, n);
  }

  close(dst_file);
  close(src_file);

  return 0;
}

int 
valid_command(char *command, char ** list, int size)
{
    int i, found;
    found = -1;
    for (i = 0; i < size; i++)
    {
        if(strcmp(list[i], command) != 0)
            continue;
        found = 1;
        break;
    }
    return found;
}

void
tcreate(int argc, char ** argv)
{
    int start = 2, end = 16;
    if (argc < start || argc > end) error();

    int dir = 0;
    if (mkdir(argv[dir]) < 0)
    {
        printf("could not make directory\n");
        unlink(argv[dir]);
        error();
    }

    int i, prog = 1, container_length, root_length, root, slash, null;
    char rootpath[MAXPATH] = { 0 };
    root = 0;
    slash = 1;
    null = 1;
    container_length = strlen(argv[dir]);
    rootpath[root] = '/';
    strcpy(rootpath + slash, argv[dir]);
    rootpath[container_length + slash] = '/';
    rootpath[container_length + slash + null] = '\0';
    root_length = strlen(rootpath);
    printf("rootpath %s length %d\n", rootpath, root_length);
    for (i = prog; i < argc; i++)
    {
        if (valid_command(argv[i], programs, COMMMANDSZ))
        {
            int program_length;
            char src[MAXPATH] = { 0 };
            char dst[MAXPATH] = { 0 };
            char *cmd = argv[i];
            program_length = strlen(cmd);
            src[root] = '/';
            strcpy(src + slash, cmd);
            src[program_length + null] = '\0';
            printf("src %s\n", src);
            strcpy(dst, rootpath);
            strcpy(dst + root_length, cmd);
            dst[root_length + program_length] = '\0';
            printf("dst %s\n", dst);
            if (copy(src, dst) < 0)
            {
                printf("could not copy all programs to the containers<%s> directory", argv[dir]);
                unlink(argv[dir]);
                error();
            }
        }
    }
}

void
tinfo(void)
{
    if (cinfo() < 0)
    {
        printf("failed to print the container info\n");
        error();
    }
}

void
tpause(int argc, char ** argv)
{
    int cname = 0, cont = 1;
    if (argc < cont || argc > cont) error();
    cpause(argv[cname]);
}

void
tresume(int argc, char ** argv)
{
    int cname = 0, cont = 1;
    if (argc < cont || argc > cont) error();
    cresume(argv[cname]);
}

void
tstart(int argc, char ** argv)
{
    int end = 3;
    if (argc < end) error();
    int vcn = 0, container = 1, program = 2, fd, id, pid;

    if (valid_command(argv[vcn], vcs, NVC) < 0)
    {
        printf("invalid vc to use\n");
        error();
    }

    if ((fd = open(argv[vcn], O_RDWR)) < 0)
    {
        printf("could not open the vc<%s>\n", argv[vcn]);
        error();
    }

    int container_length, root_length, program_length, root, slash, null;
    char rootpath[MAXPATH] = { 0 }, command[MAXPATH] = { 0 };
    root = 0;
    slash = 1;
    null = 1;
    container_length = strlen(argv[container]);
    rootpath[root] = '/';
    strcpy(rootpath + slash, argv[container]);
    rootpath[container_length + slash] = '/';
    rootpath[container_length + slash + null] = '\0';
    root_length = strlen(rootpath);
    program_length = strlen(argv[program]);
    strcpy(command, rootpath);
    strcpy(command + root_length, argv[program]);
    command[root_length + program_length] = '\0';

    pid = fork();
    if (pid < 0) error();
    if (pid == 0)
    {
        id = fork();
        if (id < 0) error();
        if (id == 0)
        {
            close(0);
            close(1);
            close(2);
            dup(fd);
            dup(fd);
            dup(fd);
            if (cstart(fd, argv[vcn], argv[container], rootpath, argv[program]) < 0)
            {
                printf("could not start a container\n");
                error();
            }
            printf("program <%s> is running on %s\n", argv[program], argv[vcn]);
            printf("Exec failed with exit status\n", exec(argv[program], &argv[program]));
            exit(-1);
        }
        wait(&pid);
        exit(0);
    }
    exit(0);
}

void
tstop(int argc, char ** argv)
{
    int cname = 0, cont = 1;
    if (argc < cont || argc > cont) error();
    cstop(argv[cname]);
}

int
main(int argc, char ** argv)
{

    if (argc < 2) error();

    int cmd, used_params, arg_start;
    cmd = 1;
    used_params = 2;
    arg_start = used_params;

    if (strcmp(argv[cmd], tools[CCREATE]) == 0)
    {
        tcreate(argc - used_params, &argv[arg_start]);
    }
    else if(strcmp(argv[cmd], tools[CINFO]) == 0)
    {
        tinfo();
    }
    else if(strcmp(argv[cmd], tools[CPAUSE]) == 0)
    {
        tpause(argc - used_params, &argv[arg_start]);
    }
    else if(strcmp(argv[cmd], tools[CRESUME]) == 0)
    {
        tresume(argc - used_params, &argv[arg_start]);
    }
    else if(strcmp(argv[cmd], tools[CSTART]) == 0)
    {
        // init_vc_array();
        tstart(argc - used_params, &argv[arg_start]);
    }
    else if(strcmp(argv[cmd], tools[CSTOP]) == 0)
    {
        tstop(argc - used_params, &argv[arg_start]);
    }
    else
    {
        error();
    }

    exit(0);
}
// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char *argv[] = { "sh", 0 };

void
create_vcs(void)
{
  int i, fd;
  char *dname = "vc0";

  for (i = 0; i < NVC; i++) {
    dname[2] = '1' + i;
    if ((fd = open(dname, O_RDWR)) < 0){
      mknod(dname, 2 + i, 1 + i);
    } else {
      close(fd);
    }
  }
}

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  create_vcs();

  for(;;){
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }
    while((wpid=wait(0)) >= 0 && wpid != pid){
      //printf("zombie!\n");
    }
  }
}

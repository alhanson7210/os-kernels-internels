#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("Hello xv6 counter!\n");

  int i = 0;
  char *label = 0;

  if(argc == 2)
    label = argv[1];
  
  while(1){
    if(label)
      printf("[%s] i = %d\n", label, i);
    else
      printf("i = %d\n", i);
  
    sleep(150);
    i++;
  }

  exit(0);
}

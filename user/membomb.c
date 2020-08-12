#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGESIZE 4096
int
main(int argc, char *argv[])
{
  int pages = 0;
  char *page;

  printf("membomb: started\n");
  while(1) {
    page = (char *) malloc(PAGESIZE);
    if (!page) {
      printf("membomb: malloc() failed, exiting\n");
      cinfo();
      exit(0);
    }
    pages++;
    printf("membomb: total memory allocated: %d KB\n", pages);
  }
  exit(0);
}

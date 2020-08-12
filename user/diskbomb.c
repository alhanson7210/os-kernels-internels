#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

#define BLOCKSIZE 1024
#define NBLOCKS 140

void
createfile(char *filename, char *buf, int blocksize, int count, int *totalblocks)
{
  int fd;
  int i;
  int rv;

  fd = open(filename, O_CREATE | O_WRONLY);
  if(fd < 0) {
    printf("diskbomb: open() failed, exiting.\n");
    exit(0);
  }

  for(i = 0; i < count; i++) {
    rv = write(fd, buf, blocksize);
    if(rv < 0) {
      printf("diskbomb: write() failed, exiting.\n");
      exit(0);
    }

    *totalblocks = *totalblocks + 1;
    printf("diskbomb: total blocks written: %d\n", *totalblocks);
  }
  close(fd);
}

void
setfilename(char *filename, int i)
{
  filename[0] = 'D';
  filename[1] = 'B';
  filename[2] = i + 21;
}

int
main(int argc, char *argv[])
{
  int i = 0;
  int totalblocks = 0;
  int totalfiles = 0;
  char filename[16];
  char buf[BLOCKSIZE];

  /* Initialize buf with 'a' */
  for (i = 0; i < BLOCKSIZE; i++) {
    buf[i] = 'a';
  }

  while(1) {
    setfilename(filename, totalfiles);
    printf("diskbomb: creating %s\n", filename);
    createfile(filename, buf, BLOCKSIZE, NBLOCKS, &totalblocks);
    totalfiles += 1;
    printf("diskbomb: total files created: %d\n", totalfiles);
  }

  exit(0);
}

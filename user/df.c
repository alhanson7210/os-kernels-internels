#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define KB(x) (x/1024)
#define MB(x) (x/1024/1024)

static char * skipelem(char *path, char *name);
uint64 df(char *path, int depth);

int
main(int argc, char *argv[])
{
  uint64 disk_used, disk_available;

  if(argc < 2)
    disk_used = df(".", 1);
  else if(argc > 1)
    disk_used = df(argv[1], 1);
  
  disk_available = (FSSIZE * BSIZE) - disk_used;

  if(disk_used == 0) {
    printf("df: No disk usage must be incorrect?!?!?!\n");
    exit(-1);
  }

 if(root_access()) {
    printf("------------------  DISK AVAILABLE  -----------------\n");
    printf("Disk Available  (B):   %l\n", disk_available);
    printf("Disk Available (KB):   %l\n", KB(disk_available));
    if(2 < MB(disk_available))
      printf("Disk Available (MB):   %l\n", MB(disk_available));
  }
    
  printf("\n------------------  DISK USED  ------------------\n");
  printf("Disk Usage  (B):   %l\n", disk_used);
  printf("Disk Usage (KB):   %l\n", KB(disk_used));
  if(2 < MB(disk_used))
    printf("Disk Usage (MB):   %l\n", MB(disk_used));

  exit(0);
}

// * WARNING a flaw with this is that we can potentially have more FDs opened than allowed.
uint64
df(char *path, int depth)
{
  char buf[512], name[DIRSIZ], *p, *buf_offset;
  int fd;
  struct dirent de;
  struct stat st;
  uint64 disk_used = 0;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "df: cannot open %s\n", path);
    return 0;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "df: cannot stat %s\n", path);
    close(fd);
    return 0;
  }

  switch(st.type){
  case T_FILE:
    disk_used += st.size;
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("df: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("df: cannot stat %s\n", buf);
        continue;
      }
      buf_offset = buf;
      for(int i = 0; i < depth; i++) {
        buf_offset = skipelem(buf_offset, name);
      }

      if(*buf_offset != '.' && *buf_offset + 1 != '.' && T_DIR == st.type) {
        disk_used += df(buf, depth + 1);
      }
      disk_used += st.size;
    }
    break;
  }
  close(fd);

  return disk_used;
}

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

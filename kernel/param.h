#define NPROC        64  // maximum number of processes
#define NCONTAINERS   4  // maximum number of containers
#define PROCLIMIT    16  // maximum number of processes a container may contain
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       0  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       2000  // size of file system in blocks
#define CDISKDEFAULT (FSSIZE/4) // default container disk limit
#define TOTALPAGES   (PHYSTOP/PGSIZE) //2gb
#define CMEMPGS      (TOTALPAGES/4)
#define KILOMEM      1024
#define MAXPATH      128   // maximum file path name
#define CNAME        32
#define NDISK        2
#define NNETIF       2
#define NVC          4   // max number virtual consoles
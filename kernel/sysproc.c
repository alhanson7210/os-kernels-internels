#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  struct proc *p;

  if(argint(0, &n) < 0)
    return -1;

  p = myproc();
  if (p -> tracing)
  	printf(" [%d] sys_exit(%d)\n", p -> pid, n);

  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  struct proc *p;

  p = myproc();
  if (p -> tracing)
  	printf(" [%d] sys_getpid(void)\n", p -> pid);
  
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  struct proc *p;

  p = myproc();
  if (p -> tracing)
  	printf(" [%d] sys_fork(void)\n", p -> pid);

  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  struct proc *pr;

  if(argaddr(0, &p) < 0)
    return -1;

  pr = myproc();
  if (pr -> tracing)
    printf(" [%d] sys_wait(%p)\n", pr -> pid, p);

  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr, n;
  struct proc *p;

  if(argint(0, &n) < 0)
    return -1;

  p = myproc();
  if (p -> tracing)
  	printf(" [%d] sys_sbrk(%d)\n", p -> pid, n);
  addr = p -> sz;
  //addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
  struct proc *p;

  if(argint(0, &n) < 0)
    return -1;

  p = myproc();
  if (p -> tracing)
  	printf(" [%d] sys_sleep(%d)\n", p ->  pid, n);

  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;
  struct proc *p;

  if(argint(0, &pid) < 0)
    return -1;

  p = myproc();
  if (p -> tracing)
  	printf(" [%d] sys_kill(%d)\n", p -> pid, pid);

  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  struct proc *p;

  p = myproc();
  if (p -> tracing)
  	printf(" [%d] sys_uptime(void)\n", p -> pid);
  
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//return zero after turning off the tracing flag for a process
uint64
sys_strace_off(void)
{
  struct proc *p;
  p = myproc();
  acquire(&p -> lock);
  p -> tracing = 0;
  release(&p -> lock);
  return 0;
}

//return zero after turning on the tracing flag for a process
uint64
sys_strace_on(void)
{
  struct proc *p;
  p = myproc();
  acquire(&p -> lock);
  p -> tracing = 1;
  release(&p -> lock);
  return 0;
}

//copying process table info into the user
//do unknown reasons, copying was left out
uint64
sys_psinfo(void)
{
  //local vars
  // int i;
	// uint64 u_pt;
  // struct ptable k_pt;
  struct proc *cp;
  //user struct validation
  // printf("attempt to got address from user\n");
	int status = 1;
	// if(argaddr(0, &u_pt) < 0)
	// // 	return status;
  // printf("got address from user\n");
  //enabled tracing
  cp = myproc();
  if (cp -> tracing)
    printf(" [%d] sys_psinfo()\n", cp -> pid);
  //kernel struct population
  psinfo();
  //get the current process from the running cpu
  //writing kernel_pt to user_pt
	// acquire(&cp -> lock);
	// status = copyout(cp -> pagetable, u_pt, (char *)&k_pt, sizeof(k_pt) + 1);
	// release(&cp -> lock);
  //return exit status
	return status;
}

uint64 
sys_cinfo(void)
{
  return cinfo();
}

uint64 
sys_cpause(void)
{
  int position = 0;
  char cname[CNAME] = { 0 };
  if (argstr(position, cname, CNAME) < 0) return -1;
  return cpause(cname);
}

uint64 
sys_cresume(void)
{
  int position = 0;
  char cname[CNAME] = { 0 };
  if (argstr(position, cname, CNAME) < 0) return -1;
  return cresume(cname);
}

uint64 
sys_cstart(void)
{
  int vcn = 0, vcname = 1, container = 2, rootdir = 3, program = 4, vcfd;
  char vname[CNAME] = { 0 }, cname[CNAME] = { 0 }, rootpath[MAXPATH] = { 0 }, command[MAXPATH] = { 0 };
  if (argint(vcn, &vcfd) < 0) return -1;
  if (argstr(vcname, vname, CNAME) < 0) return -1;
  if (argstr(container, cname, CNAME) < 0) return -1;
  if (argstr(rootdir, rootpath, CNAME) < 0) return -1;
  if (argstr(program, command, CNAME) < 0) return -1;
  return cstart(vcfd, vname, cname, rootpath, command);
}

uint64 
sys_cstop(void)
{
  int position = 0;
  char cname[CNAME] = { 0 };
  if (argstr(position, cname, CNAME) < 0) return -1;
  return cstop(cname);
}

uint64
sys_root_access(void)
{
  return myproc()->container->root_access;
}

uint64
sys_ticks(void)
{
  uint time;
  acquire(&tickslock);
  time = ticks;
  release(&tickslock);
  return time;
}

uint64
sys_freememory(void)
{
  freememory();
  return 1;
}
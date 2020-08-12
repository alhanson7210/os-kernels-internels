#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "defs.h"
#include "resume_header.h"

#define ROOT 0

struct cpu cpus[NCPU];

struct proc proc[NPROC];
struct proc *initproc;

struct container containers[NCONTAINERS]; //array of containers
struct container *active_container; //active container running
static int active_idx; //supposed to be used for switching like the console
static int creation_quantum; //creation quantum(tracker) for containers for cstart
/** Note:
=> container(zero)- C('Q')
=> container(one)- C('W')
=> container(two)- C('E')
=> container(three)- C('R')
*/

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void wakeup1(struct proc *chan);

extern char trampoline[]; // trampoline.S

struct container* 
rootcontainer(void)
{
  return &containers[ROOT];
}

struct container* 
cstartcontainer(void)
{
  return &containers[creation_quantum];
}

struct container*
mycontainer()
{
  struct proc *p;
  if (creation_quantum == 0) return &containers[ROOT];
  p = myproc();
  return p? p->container : &containers[ROOT];
}

void
containerinit(void)
{
  struct container *c;
  //Set and access the active container
  active_idx = ROOT;
  creation_quantum = ROOT;
  //initialize
  for(c = containers; c < &containers[NCONTAINERS]; c++)
  {
    initlock(&c->lock, "container");
    c->state = c != containers? FREE : STARTED;
    c->proc_count = 0;
    c->mem_usage = 0;
    c->disk_usage = 0;
    c->proc_limit = NPROC;
    c->disk_limit = c != containers? CDISKDEFAULT : FSSIZE; // 4th of disk size
    c->mem_limit = c != containers? CMEMPGS : (TOTALPAGES); /*TOTALPAGES CMEMLIMIT/ *DEFAULTPGS * PGSIZE CMEMDEFAULT CMEMLIMIT*/  // 16th of memory
    c->cpu_tokens = 0;
    c->scheduler_tokens = 0;
    c->root_access = c != containers? 0 : 1;
    c->cidx = 0;
    c->current_pid = 0;
    c->next_pid = 0;
    c->vc_name[0] = 0;
    c->rootpath[0] = 0;
    if (c != containers)
      c->name[0] = 0;
    else
      safestrcpy(c->name, "root", CNAME);
  }
  //set root
  active_container = cstartcontainer();
  active_container->rootdir = namei("/");
  active_container->scheduler_tokens++;
  safestrcpy(active_container->rootpath, "/", MAXPATH);
}

void
procinit(void)
{
  struct proc *p;
  //Set process space
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++)
  {
    // Initialize process lock
    initlock(&p->lock, "proc");
    // Allocate a page for the process's kernel stack.
    // Map it high in memory, followed by an invalid
    // guard page.
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    p->kstack = va;
    p->cpu_tokens = 0;
    p->container = 0;
    p->pagetable = 0;
    p->sz = 0;
    p->pid = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->state = UNUSED;
    p->tracing = 0;
    p->assigned = 0;
    p->cpu_tokens = 0;
  }
  kvminithart();
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  //struct proc *cp;
  struct container *c;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  c = mycontainer();
  acquire(&c->lock);
  if(!c->root_access && c->mem_usage + 5 > c->mem_limit)
  {
    release(&c->lock);
    return 0;
  }
  release(&c->lock);

  p->pid = allocpid();
  // Allocate a trapframe page.
  if((p->tf = (struct trapframe *)kalloc()) == 0){
    kfree(p->tf);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  
  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof p->context);
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  //this place from lecture is recommended for initialization
  //mark: set tracing //since fork calls allocproc
  p->tracing = 0; 
  p->container = c;
  p->assigned = 1;
  p->cpu_tokens = 0;
  //increase proc count
  acquire(&c->lock);
  c->proc_count++;
  release(&c->lock);

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  struct proc *mp;
  struct container *c;
  
  mp = myproc();
  c = mp->container;
  mp->container = p->container;

  acquire(&p->container->lock);
  if (p->container->proc_count > 0) p->container->proc_count--;
  release(&p->container->lock);

  if(p->tf)
    kfree((void*)p->tf);
  p->tf = 0;

  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);

  mp->container = c;
  p->container = 0;
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  p->tracing = 0;
  p->assigned = 0;
  p->cpu_tokens = 0;
}

// Create a page table for a given process,
// with no user pages, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0) {
    printf("proc_pagetable failed to allocate\n");
    return 0;
  }
  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  mappages(pagetable, TRAMPOLINE, PGSIZE,
           (uint64)trampoline, PTE_R | PTE_X);

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  mappages(pagetable, TRAPFRAME, PGSIZE,
           (uint64)(p->tf), PTE_R | PTE_W);

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, PGSIZE, 0);
  uvmunmap(pagetable, TRAPFRAME, PGSIZE, 0);
  if(sz > 0)
    uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x05, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x05, 0x02,
  0x9d, 0x48, 0x73, 0x00, 0x00, 0x00, 0x89, 0x48,
  0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0xbf, 0xff,
  0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x01,
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->tf->epc = 0;      // user program counter
  p->tf->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  creation_quantum++;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct container *c;

  c = p->container;
  acquire(&c->lock);
  if (!c->root_access && (c->mem_usage + (int)(p->sz/PGSIZE) > c->mem_limit || c->proc_count + 1 > c->proc_limit))
  {
    release(&c->lock);
    return -1;
  }
  release(&c->lock);
  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  
  np->sz = p->sz;

  np->parent = p;

  // copy saved user registers.
  *(np->tf) = *(p->tf);

  // Cause fork to return 0 in the child.
  np->tf->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  np->state = RUNNABLE;

  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold p->lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    // this code uses pp->parent without holding pp->lock.
    // acquiring the lock first could cause a deadlock
    // if pp or a child of pp were also in exit()
    // and about to try to lock p.
    if(pp->parent == p){
      // pp->parent can't change between the check and the acquire()
      // because only the parent changes it, and we're the parent.
      acquire(&pp->lock);
      pp->parent = initproc;
      // we should wake up init here, but that would require
      // initproc->lock, which would be a deadlock, since we hold
      // the lock on one of init's children (pp). this is why
      // exit() always wakes init (before acquiring any locks).
      release(&pp->lock);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op(ROOTDEV);
  iput(p->cwd);
  end_op(ROOTDEV);
  p->cwd = 0;

  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);
  
  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // hold p->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acquire(&p->lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if(np->parent == p){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 
          && copyout(p->pagetable, addr, (char *)&np->xstate, sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &p->lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  int start;
  uint tokens;
  struct proc* p;
  struct cpu* c = mycpu();
  struct container* current, *search, *smallest;
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by giving devices a chance to interrupt.
    intr_on();
    // Run the for loop with interrupts off to avoid
    // a race between an interrupt and WFI, which would
    // cause a lost wakeup.
    for(p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      smallest = containers;
      for (search = containers; search < &containers[NCONTAINERS]; search++)
      {
        acquire(&search->lock);
        if(search->state != STARTED)
        {
          release(&search->lock);
          continue;
        }
        tokens = search->scheduler_tokens;
        release(&search->lock);
        if(smallest->scheduler_tokens > tokens && tokens != 0) smallest = search;
      }
      current = smallest;
      if (p->container == current && p->state == RUNNABLE && current->state == STARTED)
      {
        current->scheduler_tokens++;
        p->state = RUNNING;
        c->proc = p;
        start = ticks;
        swtch(&c->scheduler, &p->context);
        c->proc = 0;
        current->scheduler_tokens += ticks - start;
        current->current_pid = p->pid;
      }
      c->intena = 0;
      release(&p->lock);
    }
  }
}

/* SCHEDULING CODE
 * general scheduler code with containers
 for (cn = containers; cn < &containers[NCONTAINERS]; cn++)
    {
      acquire(&cn->lock);
      if (cn->state != STARTED)
      {
        release(&cn->lock);
        continue;
      }
      release(&cn->lock);
      for(p = proc; p < &proc[NPROC]; p++)
      {
        acquire(&p->lock);
        if (p->container == cn && p->state == RUNNABLE && cn->state == STARTED)
        {
          p->state = RUNNING;
          c->proc = p;
          swtch(&c->scheduler, &p->context);
          c->proc = 0;
        }
        c->intena = 0;
        release(&p->lock);
      }
    }
 * general scheduler code with containers
 *
 * OLD SCHEDULER CODE
    // intr_off();
    // int found = 0;
  //   for(p = proc; p < &proc[NPROC]; p++) {
  //     acquire(&p->lock);
  //     if(p->state == RUNNABLE) {
  //       // Switch to chosen process.  It is the process's job
  //       // to release its lock and then reacquire it
  //       // before jumping back to us.
  //       p->state = RUNNING;
  //       c->proc = p;
  //       swtch(&c->scheduler, &p->context);

  //       // Process is done running for now.
  //       // It should have changed its p->state before coming back.
  //       c->proc = 0;

  //       found = 1;
  //     }

  //     // ensure that release() doesn't enable interrupts.
  //     // again to avoid a race between interrupt and WFI.
  //     c->intena = 0;

  //     release(&p->lock);
  //   }
  //   // if(found == 0){
  //   //   asm volatile("wfi");
  //   // }
 * OLD SCHEDULER CODE
 */


// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(minor(ROOTDEV));
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  if(lk != &p->lock){  //DOC: sleeplock0
    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}

// Wake up p if it is sleeping in wait(); used by exit().
// Caller must hold p->lock.
static void
wakeup1(struct proc *p)
{
  if(!holding(&p->lock))
    panic("wakeup1");
  if(p->chan == p && p->state == SLEEPING) {
    p->state = RUNNABLE;
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;
  struct proc *mp;

  mp = myproc();
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && (mp->container == p->container || mp->container->root_access)){
      p->killed = 1;
      if(p->state == SLEEPING || p->state == SUSPENDED){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie",
  [SUSPENDED] "suspended"
  };
  
  char *state;
  struct proc *p;
  struct proc *mp;
  struct container *c;

  mp = myproc();
  printf("\n");
  printf("PID\tSTATE\tNAME\tCONTAINER\tPROCESS\n");
  for(p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if(p->state == UNUSED || !p->assigned || (mp && !mp->container->root_access && p->container != mp->container))
    {
      release(&p->lock);
      continue;
    }
    release(&p->lock);
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d\t%s\t%s\t%s\t\t%d\n", p->pid, state, p->name, p->container->name, p->container->proc_count);
  }
  printf("\nNAME\tMEM(KB)\tDISK\tPROCS\tTOKENS\n");
  for (c = containers; c < &containers[NCONTAINERS]; c++)
  {
    acquire(&c->lock);
    if (c->state == STARTED)
    {
      printf("%s\t%d\t%d\t%d\t%d\n", 
        c->name,
        (c->mem_usage * PGSIZE) / KILOMEM, 
        c->disk_usage * KILOMEM,
        c->proc_count,
        c->scheduler_tokens);
      c->cpu_tokens = 1;
    }
    release(&c->lock);
  }
}

void
psinfo(void)
{
  // int found;
  static char *states[] = {
	  [UNUSED]    "unused",
	  [SLEEPING]  "sleep ",
	  [RUNNABLE]  "runble",
	  [RUNNING]   "run   ",
	  [ZOMBIE]    "zombie",
	  [SUSPENDED] "suspended"
  };

	struct proc *p; // process from process list
  struct proc *mp; // check myproc to make sure the containers are the same

  mp = myproc();
  printf("PID\tMEM\tNAME\tSTATE\tCONTAINER\n");
	for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED || !p->assigned || (!mp->container->root_access && p->container != mp->container))
    {
      release(&p->lock);
      continue;
    }
    if (mp->container->root_access || p->container == mp->container)
    {
      printf("%d\t%dK\t%s\t%s\t%s\n", 
        p->pid, 
        (int)(p->sz / KILOMEM), 
        p->name,
        states[p->state],
        p->container->name
      );
    }
    release(&p->lock);
	}
}
/*
 * old ps code that broke whenever the structure was accessed on the kernel side
 * maybe the structure needed everything to be zeroed out but I'm not going to
 * waste anymore time on trying to fix or figure it out
*/
// printf("found a process\n");
//we can add the information to the 
// printf("acquired lock\n");
// pt->pinfo_t[pt->count].pid = p->pid;
// printf("got p pid\n");
// pt->pinfo_t[pt->count].mem = (int) (p->sz / KILOMEM);
// printf("saved p mem\n");
// pt->pinfo_t[pt->count].state = p->state;
// printf("state copy \n");
// safestrcpy(pt->pinfo_t[pt->count].name, p->name, strlen(p->name) + 1);
// printf("p name copy\n");
// safestrcpy(pt->pinfo_t[pt->count].cname, p->container->name, strlen(p->container->name));
// printf("cname copy\n");
// pt->count++;
// printf("pt count\n");
// printf("release\n");

int	
suspend(int pid, struct file *f)
{
	pagetable_t oldpt;
	struct proc *p; // process from process list
	struct proc *mp; //
	struct resumehdr rhdr; //resume header instead of elf header
  mp = myproc();
	printf("Finding suspended process:\n");
	for(p = proc; p < &proc[NPROC]; p++)
	{
     	if(!p->assigned || p->pid != pid || (!p->container->root_access && p->container != mp->container))
     		continue;
		  //suspend process
     	acquire(&p -> lock);
   		printf("Found process and changing it to suspended now.\n");
   		p -> state = SUSPENDED;
   		//release successfully
   		release(&p -> lock);
   		//copy info  to header
   		rhdr.memory_size = p->sz;
   		rhdr.code_size = p->sz - 2*PGSIZE;
   		rhdr.stack_size = PGSIZE;
   		rhdr.tracing = p->tracing;
   		safestrcpy(rhdr.name, p->name, strlen(p->name) + 1);
   		//copy proc info into file
   		oldpt = mp->pagetable;
   		mp->pagetable = p->pagetable;
   		//write from kernel
    	filewritefromkernelspace(f, (uint64) &rhdr, sizeof(rhdr)); // resume header
    	filewritefromkernelspace(f, (uint64) p->tf, sizeof(struct trapframe)); // trapframe
    	//write normally
    	filewrite(f, (uint64) 0, rhdr.code_size); // code + data
    	filewrite(f, (uint64) (rhdr.code_size + PGSIZE), PGSIZE); // stack info
    	//swap back in the old pagetable
   		mp->pagetable = oldpt;
   		return 1;
	}
	printf("Did not find process needed to be suspended.\n");
	return -1;
}

struct container*
createcontainer(void)
{
  struct container *c;
  for(c = &containers[1]; c < &containers[NCONTAINERS]; c++) {
    acquire(&c->lock);
    if(c->state == FREE || c->state == CREATED || c->state == STOPPED) {
      // Reserve the container.
      c->state = STARTED;
      release(&c->lock);
      return c;
    }
    release(&c->lock);
  }
  return 0;
}

struct container*
find(char *cname)
{
  struct container *c;
  for(c = containers; c < &containers[NCONTAINERS]; c++) {
    acquire(&c->lock);
    if(strncmp(c->name, cname, sizeof(c->name)) == 0) {
      release(&c->lock);
      return c;
    }
    release(&c->lock);
  }
  return 0;
}

int cinfo(void)
{
  //process states
  static char *states[] = {
	  [UNUSED]    "unused",
	  [SLEEPING]  "sleep ",
	  [RUNNABLE]  "runble",
	  [RUNNING]   "run   ",
	  [ZOMBIE]    "zombie",
	  [SUSPENDED] "suspended"
  };
  //variables
  uint64 total;
  uint tokens;
  char *state;
  struct proc *p;
  struct container* c;
  total = 0;
  tokens = 0;
  for(p = proc; p < &proc[NPROC]; p++)
  {
    if(p->state == UNUSED || !p->assigned)
      continue;
    total += p->cpu_tokens;
    p->container->cpu_tokens += p->cpu_tokens;
  }
  printf("Ticks: %d\nTotal Tokens: %d\n", ticks, total);
  printf("[Process Statistics]\n");
  printf("\nPID\tCPU %%\tTOKENS\tSTATE\tNAME\tCONTAINER\n");
  for(p = proc; p < &proc[NPROC]; p++)
  {
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    c = p->container;
    tokens = p->cpu_tokens;
    printf("%d\t%d\t%d\t%s\t%s\t'%s'\t\n", p->pid, (tokens*100)/total, tokens, state, p->name, c->name);
  }
  printf("[Process Statistics]\n");
  printf("\n[Container Statistics]\n");
  printf("NAME\tMEM(KB)\tDISK\tPROCS\tCPU %%\tTOKENS\n");
  for (c = containers; c < &containers[NCONTAINERS]; c++)
  {
    acquire(&c->lock);
    if (c->state == STARTED)
    {
      printf("%s\t%d\t%d\t%d\t%d%%\t%d\n", 
        c->name, 
        (c->mem_usage * PGSIZE)/KILOMEM,
        c->disk_usage * KILOMEM,
        c->proc_count,
        (c->cpu_tokens * 100)/total,
        c->cpu_tokens
      );
      c->cpu_tokens = 1;
    }
    release(&c->lock);
  }
  printf("[Container Statistics]\n");
  return 1;
}

int cpause(char* cname)
{
  struct container *c = find(cname);
  if (!c) return -1;
  acquire(&c->lock);
  c->state = PAUSED;
  release(&c->lock);
  return 1;
}

int cresume(char* cname)
{
  struct container *c = find(cname);
  if (!c || c->state != PAUSED) return -1;
  acquire(&c->lock);
  c->state = STARTED;
  release(&c->lock);
  return 1;
}

int cstart(int vcfd, char* vcname, char* cname, char* rootpath, char* program)
{
  struct inode *ip;
  if ((ip = namei(rootpath)) < 0)
  {
    printf("failed to get the rootpath\n");
    return -1;
  }
  struct container *c;
  if (!(c = createcontainer()))
  { // c -> state should be STARTED after this method call
    printf("failed to create a container\n");
    return -1;
  }
  //pages for sh program
  //5 pages for every proc
  //4 pages: where 2 pages are code + data 
  // and the other 2 pages are for the stack
  int pages = 9; 
  acquire(&c->lock);
  strncpy(c->name, cname, CNAME);
  strncpy(c->vc_name, vcname, CNAME);
  safestrcpy(c->rootpath, rootpath, MAXPATH);
  c->proc_count++;
  c->mem_usage += pages;
  c->scheduler_tokens++;
  release(&c->lock);
  //update the parent container and stats
  struct proc *p;
  p = myproc();
  p->container = c;
  strncpy(p->name, program, 16);
  acquire(&p->parent->container->lock); /*parent->*/
  p->parent->container->proc_count--; /*parent->*/
  p->parent->container->mem_usage -= pages; /*parent->*/
  release(&p->parent->container->lock); /*parent->*/
  //correct file pointers
  begin_op(ROOTDEV);
  c->rootdir = idup(ip);
  iput(p->cwd);
  end_op(ROOTDEV);
  p->cwd = ip;
  //return success
  return 1;
}

int cstop(char* cname)
{
  struct proc *p;
  struct container *c = find(cname);
  if (!c) return -1;
  for(p = proc; p < &proc[NPROC]; p++)
  {
    if(p->container == c)
    {
      kill(p->pid);
      yield();
    }
  }
  acquire(&c->lock);
  *c->name = '\0';
  *c->vc_name = '\0';
  *c->rootpath = '\0';
  c->proc_count = 0;
  c->state = STOPPED;
  c->rootdir = 0;
  release(&c->lock);
  return 1;
}

void
freememory(void)
{
  struct proc* p = myproc();
  struct container* c;
  uint mem_usage = 0, mem_limit = 0;
  if (p->container->root_access)
  {
    for(c = containers; c < &containers[NCONTAINERS]; c++)
    {
      acquire(&c->lock);
      mem_usage += c->mem_usage;
      release(&c->lock);
    }
    mem_limit += PHYSTOP / PGSIZE;
  }
  else
  {
    mem_usage = p->container->mem_usage;
    mem_limit = p->container->mem_limit;
  }
  printf("Used memory:  '%d' Pages\n", mem_usage);
  printf("Free memory:  '%d' Pages\n", mem_limit);
}
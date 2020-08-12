// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context scheduler;   // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// the sscratch register points here.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE, SUSPENDED };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  struct container *container; // Process's Container Space
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID
  int tracing;                 // flag for strace
  int assigned;                // Default until process is assigned
  uint cpu_tokens;             // tokens for the amount cpu used
  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // Page table
  struct trapframe *tf;        // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

enum containerstate { FREE, CREATED, STARTED, PAUSED, STOPPED };
//container space ~ manage name space isolation, proess isolation, memory space isolation
struct container {
  int root_access;
  int proc_limit;
  int mem_limit;
  int disk_limit;
  int proc_count;
  int mem_usage;
  int disk_usage;
  int current_pid; // the current point in the proc array
  int cidx; // the index in the proc array to start and search from
  int next_pid; // these is the next point in the proc array
  uint cpu_tokens;
  uint scheduler_tokens;
  enum containerstate state;
  char name[CNAME];
  char vc_name[CNAME];
  char rootpath[MAXPATH];
  struct spinlock lock;
  struct inode *rootdir;
  uint ticks;
};
//Access to containers outside of proc.c
extern struct container containers[NCONTAINERS];

//process info for ps
struct pinfo {
	int pid;
	int mem;
	enum procstate state;
	char name[16];
  char cname[CNAME];
  uint ticks; 
};

//process info table needed for printing out all found pinfo for running processes for ps
struct ptable {
	struct pinfo pinfo_t[NPROC];
	int count;
};

struct cinfo {
  int mem_limit;
  int disk_limit;
  int proc_limit;
  int mem_usage;
  int disk_usage;
  uint ticks;
  enum procstate state;
  char cname[CNAME];
  char rootpath[MAXPATH];
  struct pinfo proc[NPROC];
};

struct ctable {
  struct cinfo cinfo_t[NPROC];
  int count;
};

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
#include "resume_header.h"

static int loadseg(pde_t *pgdir, uint64 addr, struct inode *ip, uint offset, uint sz);

int	
resume(char * filename)
{
  //initializations
  int fileoffset, ksp;
  uint64 sz, oldsz, stackbase, newstacksz, stackspace;
  pagetable_t pagetable, oldpagetable;
  struct inode *ip;
  struct resumehdr rhdr;
  struct trapframe tf;
  struct proc *p;
  //open inode to file
  begin_op(ROOTDEV);
  if((ip = namei(filename)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  //lock inode to file
  ilock(ip);
  //keep track of the offset within the file
  pagetable = 0;
  fileoffset = 0;
  ksp = 0;
  sz = 0;
	//read resume header into kernel space and add file offset
  if(readi(ip, ksp, (uint64)&rhdr, fileoffset, sizeof(rhdr)) != sizeof(rhdr))
    goto bad;
  fileoffset += sizeof(rhdr);
  //getting current process and save old values
  p = myproc();
  oldsz = p->sz; 
  oldpagetable = p->pagetable;
  //read in the trapframe and add file offset
  if(readi(ip, ksp, (uint64)p->tf, fileoffset, sizeof(tf)) != sizeof(tf))
    goto bad;
  fileoffset += sizeof(tf);
  //empty the pagetable
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;
  //malloc space for the code size + data
  if((sz = uvmalloc(pagetable, sz, rhdr.code_size)) == 0)
    goto bad;
  //load the code segment into the processes pagetable and add file offset
  if(loadseg(pagetable, 0, ip, fileoffset, rhdr.code_size) < 0)
      goto bad;
  fileoffset += rhdr.code_size;
  //use the second page as the user stack after allocating two page sizes worth of memory.
  sz = PGROUNDUP(sz);
  stackspace = sz;
  stackbase = stackspace + PGSIZE;
  newstacksz = stackbase + PGSIZE;
  if((sz = uvmalloc(pagetable, sz, newstacksz)) == 0)
    goto bad;
  //clear the stack space
  uvmclear(pagetable, stackspace);
  //load the stack segment into the processes pagetable
  if(loadseg(pagetable, stackbase, ip, fileoffset, rhdr.stack_size) < 0)
      goto bad;
  //don't forget to unlock the inode when done reading and loading segments
  iunlockput(ip);
  end_op(ROOTDEV);
  //copying information into current proc
  acquire(&p->lock);
  safestrcpy(p->name, rhdr.name, strlen(rhdr.name) + 1);
  p->sz = rhdr.memory_size;
  p->tracing = rhdr.tracing;
  p->pagetable = pagetable;
  release(&p->lock);
  //freeing old pagetable before returning
  proc_freepagetable(oldpagetable, oldsz);
	return 1;

bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op(ROOTDEV);
  }
  return -1;
}

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz, sp, ustack[MAXARG+1], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();
  
  begin_op(ROOTDEV);

  if((ip = namei(path)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op(ROOTDEV);
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = uvmalloc(pagetable, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  uvmclear(pagetable, sz-2*PGSIZE);
  sp = sz;
  stackbase = sp - PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->tf->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->tf->epc = elf.entry;  // initial program counter = main
  p->tf->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op(ROOTDEV);
  }
  return -1;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;

  if((va % PGSIZE) != 0)
    panic("loadseg: va must be page aligned");

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  
  return 0;
}

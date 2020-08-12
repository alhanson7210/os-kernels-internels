//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define CONSOLES (NVC + CONSOLE)
#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

//
// send one character to the uart.
//
void
consputc(int c)
{
  extern volatile int panicked; // from printf.c

  if(panicked){
    for(;;)
      ;
  }

  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else {
    uartputc(c);
  }
}

struct console {
  struct spinlock lock;
  
  // input
#define INPUT_BUF 128
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
};

static int active_console;
struct console consoles[CONSOLES];
struct console * cons_a;
//
// user write()s to the console go here.
//
int
cwrite(struct file *f, int user_src, uint64 src, int n, struct console * cons)
{
  int i;

  acquire(&cons->lock);
  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    if(cons_a == cons)
      consputc(c);
  }
  release(&cons->lock);

  return n;
}

int
cread(struct file *f, int user_dst, uint64 dst, int n, struct console * cons)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons->lock);
  while(n > 0){
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while(cons->r == cons->w){
      if(myproc()->killed){
        release(&cons->lock);
        return -1;
      }
      sleep(&cons->r, &cons->lock);
    }

    c = cons->buf[cons->r++ % INPUT_BUF];

    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons->r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  release(&cons->lock);

  return target - n;
}

int
consolewrite(struct file *f, int user_src, uint64 src, int n)
{
  //f->minor corresponds to the index of a given device(console)
  return cwrite(f, user_src, src, n, &consoles[f->minor]);
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(struct file *f, int user_dst, uint64 dst, int n)
{
  //f->minor corresponds to the index of a given device(console)
	return cread(f, user_dst, dst, n, &consoles[f->minor]);
}

//
// the console input interrupt handler.
// uartintr() calls this for input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  int switchedConsole;
  
  acquire(&cons_a->lock);
  switchedConsole = 0;
  switch(c){
  case C('P'):  // Print process list.
    procdump();
    break;
  case C('U'):  // Kill line.
    while(cons_a->e != cons_a->w &&
          cons_a->buf[(cons_a->e-1) % INPUT_BUF] != '\n'){
      cons_a->e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f':
    if(cons_a->e != cons_a->w){
      cons_a->e--;
      consputc(BACKSPACE);
    }
    break;
  case C('B'): // Rotate Backwards a console
    //edge case 0: (4 + 0 - 1) % 4 is 3
    //edge case 3: (4 + 3 - 1) % 4 is 2
    printf("\nActive console was %d\n", active_console);
    active_console = (CONSOLES + active_console - 1) % CONSOLES;
    printf("Active console is %d\n", active_console);
    switchedConsole = 1;
    break;
  case C('F'): // Rotate Forwards a console
    //edge case 0: (0 + 1) % 4 is 1
    //edge case 3: (3 + 1) % 4 is 0
    printf("\nActive console was %d\n", active_console);
    active_console = (active_console + 1) % CONSOLES;
    printf("Active console is %d\n", active_console);
    switchedConsole = 1;
    break;
  case C('T'):
    printf("\nActive console is %d\n", active_console);
    break;
  default:
    if(c != 0 && cons_a->e-cons_a->r < INPUT_BUF){
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      consputc(c);

      // store for consumption by consoleread().
      cons_a->buf[cons_a->e++ % INPUT_BUF] = c;

      if(c == '\n' || c == C('D') || cons_a->e == cons_a->r+INPUT_BUF){
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        cons_a->w = cons_a->e;
        wakeup(&cons_a->r);
      }
    }
    break;
  }
  release(&cons_a->lock);
  if (switchedConsole)
    cons_a = &consoles[active_console];
}

void
consoleinit(void)
{
  int i, device_start, device_end;
  char cbf[6] = {"cons0\0"};
  // initialize all locks for all devices (console and all vc[n])
  for (i = 0; i < CONSOLES; i++)
  {
  	cbf[5] = '1' + i;
  	initlock(&consoles[i].lock, cbf);
  	consoles[i].r = 0;
  	consoles[i].w = 0;
  	consoles[i].e = 0;
  }
  // initialize the console index and set the correct console
  active_console = 0;
  cons_a = &consoles[active_console];
  // run the code to initialize the UART control registers
  uartinit();
  //define the device start and end for consoles
  device_start = CONSOLE;
  device_end = device_start + CONSOLES;
  // connect read and write system calls
  // to consoleread and consolewrite.
  // i representing the major number each 
  // console corresponds to in the devsw
  for(i = device_start; i < device_end; i++)
  {
  	devsw[i].read = consoleread;
  	devsw[i].write = consolewrite;
  }
}

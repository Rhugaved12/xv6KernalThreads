#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#define PGSIZE 4096

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
//Rhugaved Edits
int
tkill_children(int);
// int nexttid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  // Rhugaved changes
  p->is_thread = 0;     //In the beginning of the process, make the is_thread as 0 to show that it's a process

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();
  // Rhugaved changes
  // struct proc *p;

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  //Rhugaved Changes
  // Change the size variable of all children threads
  // depending upon the flags
  // for(p = ptable.proc; p<&ptable.proc[NPROC]; p++){
  //   if(p->pthread == curproc){
  //     p->sz = sz;
  //   }
  // }

  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  //Rhugaved edit 
  np->is_thread = 0;        //As it fork, we need to have the flag show that it's a prcess not a thread
  np->tgid = np->pid;       // Used in thread groups

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

int
clone(void(*fcn)(void*), void* arg, void* stack_add, int flags)
{
  // cprintf("%d:Clone SYSTEM CALL", flags);
  // flags = 24;
  int i, pid;
  struct proc *newthread;
  // curproc is the proc/thread that calls clone
  struct proc *curproc = myproc();
  uint temp; 
  // Allocate process.
  if((newthread = allocproc()) == 0){
    return -1;
  }

   // Checking the flags
  if (flags & (1 << (1 - 1)))
    newthread->CLONE_FILES = 1;
  else
    newthread->CLONE_FILES = 0;
  if (flags & (1 << (2 - 1)))
    newthread->CLONE_FS = 1;
  else
    newthread->CLONE_FS = 0;
  if (flags & (1 << (3 - 1)))
    newthread->CLONE_VM = 1;
  else
    newthread->CLONE_VM = 0;
  if (flags & (1 << (4 - 1)))
    newthread->CLONE_THREAD = 1;
  else
    newthread->CLONE_THREAD = 0;
  if (flags & (1 << (5 - 1)))
    newthread->CLONE_PARENT = 1;
  else
    newthread->CLONE_PARENT = 0;

  // parent here is the group leader aka main process
  // Handling CLONE_PARENT
  if(curproc->is_thread == 0){
    if(newthread->CLONE_PARENT == 1)
      newthread->parent = curproc->parent;
    else
      newthread->parent = curproc;
  }
  else{
    if(newthread->CLONE_PARENT == 1)
      newthread->parent = curproc->parent->parent;
    else
      newthread->parent = curproc->parent;
  }
  // cprintf("Clone end-4");

  newthread->thread_stack = stack_add;

  // Copy process pg dic from proc.
  // Handling CLONE_VM
  if (newthread->CLONE_VM == 1){
    newthread->pgdir = curproc->pgdir;
  }
  else{
    // Copy process state from proc.
    if((newthread->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
      kfree(newthread->kstack);
      newthread->kstack = 0;
      newthread->state = UNUSED;
      return -1;
    }
  }
  // Immediate parent of the thread
  newthread->pthread = curproc;
  // if((newthread->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
  //   kfree(newthread->kstack);
  //   newthread->kstack = 0;
  //   newthread->state = UNUSED;
  //   return -1;
  // }

  // Point to the main thread/main process if the caller is a thread
  // if (curproc->is_thread)
  //   curproc =  curproc->parent;
  
  newthread->sz = curproc->sz;
  // newthread->parent = curproc;
  *newthread->tf = *curproc->tf;
  newthread->is_thread = 1;

  // Clear %eax so that clone returns 0 in the child.
  newthread->tf->eax = 0;
  // cprintf("Clone end-3");

  // Handling CLONE_FILES
  if (newthread->CLONE_FILES == 0){
    for(i = 0; i < NOFILE; i++)
      if(curproc->ofile[i])
        newthread->ofile[i] = filedup(curproc->ofile[i]);
  }
  else{
    for(i = 0; i < NPROC; i++) {
      if(newthread->clone_file_share[i] == 0) {
        newthread->clone_file_share[i] = curproc;
      }
    }
    for(i = 0; i < NPROC; i++) {
      if(curproc->clone_file_share[i] == 0) {
        curproc->clone_file_share[i] = newthread;
      }
    }
    // newthread->ofile = curproc->ofile;
    for(i = 0; i < NOFILE; i++)
      if(curproc->ofile[i])
        newthread->ofile[i] = curproc->ofile[i];
  }

  // Handling CLONE_FS
  if (newthread->CLONE_FS == 0)
    newthread->cwd = idup(curproc->cwd);
  else{
    for(i = 0; i < NPROC; i++) {
      if(newthread->clone_fs_share[i] == 0) {
        newthread->clone_fs_share[i] = curproc;
      }
    }
    for(i = 0; i < NPROC; i++) {
      if(curproc->clone_fs_share[i] == 0) {
        curproc->clone_fs_share[i] = newthread;
      }
    }
    newthread->cwd = curproc->cwd;
  }

  safestrcpy(newthread->name, curproc->name, sizeof(curproc->name));

// Handling IDs
  pid = newthread->pid;
  // newthread->tid = nexttid++;
  // For threads, the pid will work as tid
  if (newthread->CLONE_THREAD == 1){
    newthread->tgid = curproc->tgid;
  }
  else{
    newthread->tgid = newthread->pid;
  }

  // Set temp to stack top first and then sub 8 to push arg and return add(fake ret) above it.
  temp = (uint)stack_add + PGSIZE - 8;
  uint arr[2];
  arr[1] = (uint)arg;
  arr[0] = (uint)0xffffffff;
  copyout(newthread->pgdir, temp, arr, 8);
  // newthread->context->ebp = (uint)temp;
  // newthread->context->eip =  (uint)fcn;
  newthread->tf->ebp = temp;
  newthread->tf->eip = (uint)fcn;
  newthread->tf->esp = temp;
  // cprintf("Clone End-1");

  acquire(&ptable.lock);

  newthread->state = RUNNABLE;

  release(&ptable.lock);
  // cprintf("Clone End");
  // cprintf("EIP: %d", (uint)fcn);

  return pid;
}



// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd, i, j;

  if(curproc == initproc)
    panic("init exiting");

  // Handling filesharing in threads
  for(i = 0; i < NPROC; i++) {
    if(curproc->clone_file_share[i] != 0) {
      p = curproc->clone_file_share[i];
      for(j = 0; j < NPROC; j++) {
        if(p->clone_file_share[j] == curproc) {
          p->clone_file_share[j] = 0;
          break;
        }
      }
      curproc->clone_file_share[i] = 0;
    }
  }

  // Handling file systems in threads
  for(i = 0; i < NPROC; i++) {
    if(curproc->clone_fs_share[i] != 0) {
      p = curproc->clone_fs_share[i];
      for(j = 0; j < NPROC; j++) {
        if(p->clone_fs_share[j] == curproc) {
          p->clone_fs_share[j] = 0;
          break;
        }
      }
      curproc->clone_fs_share[i] = 0;
    }
  }

  // Close all open files.
  // RHugaved Edit begins
  // But only for processes not for threads
  if(curproc->CLONE_FILES == 0){
    for(fd = 0; fd < NOFILE; fd++){
      if(curproc->ofile[fd]){
        fileclose(curproc->ofile[fd]);
        curproc->ofile[fd] = 0;
      }
    }
  } 
  else{
    for(fd = 0; fd < NOFILE; fd++){
      if(curproc->ofile[fd]){
        // fileclose(curproc->ofile[fd]);
        curproc->ofile[fd] = 0;
      }
    }
  }

  if(curproc->CLONE_FS == 0){
    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;
    } 
    else{
    curproc->cwd = 0;
  }
  

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  int flag = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->wait_for == curproc->pid){
      flag = 1;
      // cprintf("Exit: %d", p->wait_for);
      wakeup1(p);
    }
  }
  if(flag == 0){
    if(curproc->is_thread == 0)
      wakeup1(curproc->parent);
    else
      wakeup1(curproc->pthread);
  }

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    // Rhugaved edits
    // Do below only for processes
    if(p->parent == curproc && curproc->is_thread == 0){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  // For threads, don't kill any child threads
    if(curproc->is_thread == 1){
      // wakeup1(curproc->parent);
    }
    // If the process has child threads, then kill all threads
    else{
      // kill all threads children
      tkill_children(curproc->pid);
    }

  // Jump into the scheduler,  never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      // Rhugaved changes
      if(p->is_thread == 1)
        continue;

      // For thread group members forked process
      if(curproc->is_thread == 1)
        if(p->parent->tgid != curproc->tgid)
          continue;

      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->tgid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Rhugaved Edit begins:
// Join for threading
int
join(int pid)
{
  struct proc *p;
  int havekids, return_pid;
  // myproc() returns threads parent process
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

      if (p->pid == pid){
        if(curproc->is_thread == 1) {
          if(p->parent != curproc->parent) {
            release(&ptable.lock);
            return -1;
          }
        } 
        else {
          if(p->parent != curproc){
            release(&ptable.lock);
            return -1;
          }
        }
        if(p->is_thread == 0 ){
          release(&ptable.lock);
          return -1;
        }
        // if (p->is_thread == 0 || p->parent!= curproc || p->tgid != curproc->tgid){
        //   release(&ptable.lock);
        //   return -1;
        // }
      }
      else{
        continue;
      }
      // If a thread has called fork and clone, then for forked: p->parent == curproc,
      // but for fork, we should use wait. So, we skip that p and loop again to
      // try and find the thread.
      // if(p->is_thread == 0) 
      //   continue;

      // if(curproc->is_thread == 1) {
      //   if(p->parent != curproc->parent) 
      //     continue;
      // } 
      // else {
      //   if(p->parent != curproc)
      //     continue;
      // }

      // if(p->pid != pid)
      //   continue;
      
      // if (p->pid == pid){
      //   if (p->parent!= curproc->parent || p->isThread == 0 ){
      //     release(&ptable.lock);
      //     return -1;
      //   }
      
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        return_pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        if (p->CLONE_VM == 0)
          freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return return_pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    curproc->wait_for = pid;
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Rhugaved Edits
      // Kill all the children threads for the process
      if(p->is_thread == 0)
        tkill_children(pid);
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Kill a particular thread given the tid(pid) of the thread
// and the tgid
int
tkill(int tgid, int tid, int sig){
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == tid && p->is_thread == 1 && p->tgid == tgid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
// Kill all the child threads of the parent_process_pid
int
tkill_children(int parent_process_pid)
{
  struct proc *p;

  // acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if((p->is_thread == 1) && (p->parent->pid == parent_process_pid)){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
    }
  }
  // release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

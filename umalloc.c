#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}

void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}

typedef struct THREAD{
    int pid;
    void* stack;
}THREAD;

THREAD create_thread(void (*fcn)(void *), void *arg, int flags);
int join_threads(THREAD t);
int get_pid(void);
int get_ppid(void);
int get_tid(void);
int thread_kill(int, int, int);


THREAD create_thread(void(*fcn)(void *), void *arg, int flags){
    int stack_addr;
    void* stack = malloc(4096);
    stack_addr = (uint)stack;
    int ret = clone(fcn, arg, stack, flags);

    THREAD thread;
    thread.pid = ret;
    thread.stack = (void*)stack_addr;
    return thread;

}

int join_threads(THREAD t){
    if(join(t.pid) != t.pid){
        free(t.stack);
        return -1;
    }
    free(t.stack);
    return t.pid;
}

int get_pid(void){
  return getpid();
}

int get_ppid(void){
  return getppid();
}

int get_tid(void){
  return gettid();
}

int thread_kill(int tgid, int tid, int flags){
  return tkill(tgid, tid, flags);
}
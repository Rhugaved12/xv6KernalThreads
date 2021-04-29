// #include "types.h"
// #include "stat.h"
// #include "user.h"

// typedef struct THREAD{
//     int pid;
//     void* stack;
// }THREAD;

// THREAD create_thread(void (*start)(void *), void *arg){
//     int stack_addr;
//     void* stack = malloc(4096);
//     stack_addr = (uint)stack;
//     printf(1, "hello");
//     printf(1, "%d\n", (int)start);
//     int ret = clone(start, arg, stack);
//     printf(1, "Return: %d", ret);
//     THREAD thread;
//     thread.pid = ret;
//     thread.stack = (void*)stack_addr;
//     return thread;

// }

// int join_threads(THREAD t){
//     int ret = join(t.pid);
//     return ret;
// }


// void test1(void* arg){
//     printf(1,"IN THREAD\n");      
//     exit();

// }

// int main(int argc, char *argv[])
// {
//     printf(1,"In main\n");
//     THREAD th = create_thread(test1, 0);
//     printf(1,"Thread Created\n");
//     //join_threads(th);
//     join(th.pid);
//     printf(1,"Threads Joined\n");
//     printf(1, "RETURN VALUE FOR CLONE : %d\n", th.pid);
//     exit();
// }


#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"

volatile int globalCounter = 0;

void addToCounter(){
  printf(1, "#\n");
  globalCounter++;
  // printf(1, "counter is: %x\n", globalCounter);
  exit();
}

int
main(int argc, char *argv[])
{
//  int pid;
 void * stack[10];
 int pid[10];

//  printf(1, "Running testUserCalls:\n");
 globalCounter++;
//  printf(1, "before cloning counter is: %x\n", globalCounter);

 int x;
 for(x=0; x<10; x++){
   stack[x] = malloc(4096);
   pid[x] = clone((void *) &addToCounter, (void *) &globalCounter, (void *) stack[x], 0);
   printf(1, "user pid[x]: %d\n", pid[x]);
 }

 for(x=0; x<10; x++){
   int p;
   p = join(pid[x]);
   printf(1, "join pid[x] %d\n", p);
 }


 globalCounter++;
//  printf(1, "joined\n");

 printf(1, "The globalCounter should be 12 : and it is = %d\n", globalCounter);
 exit();
 return 0;
}
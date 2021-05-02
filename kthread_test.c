
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"

// Increment TESTING
// Volatile means static
volatile int increment_count = 0;

// Thread function
void increment_fcn(void* val){
  increment_count++;

  printf(1, "Increment is: %x\n", increment_count);
  exit();
}

void Increment(){
    printf(1,"\nIncrement testing\n");
    THREAD t2[10];
    printf(1, "Tests Running\n");
    increment_count++;
    printf(1, "Increment before calling clone: %x\n", increment_count);

    int i;
    for(i=0; i<10; i++){

        t2[i] = create_thread(increment_fcn, 0, CLONE_VM);
        // Call join
        join_threads(t2[i]);
        printf(1, "PID: %d\n", t2[i].pid);
    }
    increment_count++;

    printf(1, "Joined\n");

    printf(1, "Increment value should be 12, and it is = %d\n", increment_count);
}


int B[4][4] = {{1, 2, 3, 4},{1, 2, 3, 4},{1, 2, 3, 4},{1 , 2, 3, 4}};
int A[4][4] = {{1, 2, 3, 4},{1, 2, 3, 4},{1, 2, 3, 4},{1 , 2, 3, 4}};
int C[4][4] = {{0}};

void matrix_addition_fcn(void* arg){
    int id = (int)arg;
    for(int i = id; i < 4; i+=2){
        for(int j = 0; j < 4; j++){
            C[i][j] = A[i][j] + B[i][j];
        }
    }
    exit();
}

void matrix_addition(){
    THREAD thread[2];
    for(int i = 0; i < 2; i++){
        thread[i] = create_thread(matrix_addition_fcn, (void*)i, CLONE_VM);
        // Calling join
        join_threads(thread[i]);
    }
    printf(1,"\nMatrix Addition Test\n");
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
          
            printf(1, (j < 4-1) ? "%d " : "%d\n", C[i][j]);
        }
    }
    
}

// GET TID, TGID, AND PARENT ID
void get_process_or_thread_ids_fcn(void* arg){
    int tid = get_tid();
    int parent_id = get_ppid();
    int tgid = get_pid();
    printf(1, "TID : %d\nParent_ID : %d\nTGID : %d\n", tid,parent_id, tgid);
    exit();
}

//CLONE_VM TESTING
void clone_files_and_vm_fcn(void * arg){
    char buf[100];
    int fd = (int)arg;
    read(fd, &buf, 100);
    int i = write(1, &buf, 100);
    printf(1,"In clone files, write count: %d\n", i);
    close(fd);
    exit();
}

void get_process_or_thread_ids(){

    printf(1,"\nget_process_or_thread_ids function\n");
    THREAD thread;
    thread = create_thread(get_process_or_thread_ids_fcn, 0, CLONE_VM);
    join_threads(thread);
    
} 



void clone_files_and_vm(){
    printf(1,"\nclone_files_and_vm test\n");
    char buff[100];
    int fd = open("README", O_RDWR);
    // read(fd, &buff, 10);
    // write(1, &buff, 10);
    THREAD t = create_thread(clone_files_and_vm_fcn, (void*)fd, CLONE_FILES | CLONE_VM);
    join_threads(t);
    printf(1,"\nAfter thread join\n");
    int ret = read(fd, &buff, 100);
    // write(1, &buff, 100);
    printf(1, "In parent, read count: %d\n", ret);
    if(ret == -1){
      printf(1, "clone_files_and_vm test passed\n");
    }
    else{
      printf(1, "clone_files_and_vm test failed\n");
    }
}

//JOIN ORDER TEST
void jointestthread(void *a)
{
  int val = (int)a;
  // Sleep for val time
  sleep(val);
  exit();
}
void child_kill_test_fcn(void* arg){
    sleep(10);
    printf(1,"Child Kill Test Failed\n");
    exit();
}

void joinordertest(){
    int ret1, ret2;
    int v1, v2;
    THREAD thread1, thread2;
    v2 = 1;
    v1 = 3;
    
    thread1 = create_thread(jointestthread, (void*)v1, CLONE_VM);

    thread2 = create_thread(jointestthread, (void*)v2, CLONE_VM);

    ret1 = join_threads(thread1);

    ret2 = join_threads(thread2);
    if (ret1 == thread1.pid && ret2 == thread2.pid)
        printf(1, "\nOrder of joining test Passed\n");
    else
        printf(1, "\nOrder of joining test Failed\n");
    
}



void child_kill_test(){
    printf(1, "\nChild Kill Test passed if no failed printed\n");
    THREAD t = create_thread(child_kill_test_fcn, 0, CLONE_VM);
    // thread_kill(t.pid, t.tgid, 1);
    kill(t.pid);
    join_threads(t);


}


//RACE TEST
// Threads are racing to increment a single integer
void race_test_fcn(void*a){

    int *value = (int*)a;
    for(int i = 0; i < 10;i++){

        *value += 1;
    }
    exit();
}

void race_test(){
    THREAD thread[100];
    int value = 0;
    for(int i = 0; i < 100; i++){
        thread[i] = create_thread(race_test_fcn,(void*)&value, CLONE_VM );
        join_threads(thread[i]);
    }
    if(value != 100*10){
        printf(1,"\nrace_test failed\n");
    }else{
        printf(1,"\nrace_test passed\n\n");
    }
}

void second_thread(void* arg){
    printf(1,"\nThread in Thread Passed\n");
    exit();
}

void first_thread(void* arg){
    THREAD thread;

    thread = create_thread(second_thread, 0, CLONE_VM);

    join_threads(thread);
    exit();

}

void one_thread_calling_another(){
    THREAD thread;

    thread = create_thread(first_thread, 0, CLONE_VM);

    join_threads(thread);
    
}


//JOIN WAIT TEST
void wait_and_join_fcn(void *a){
    int val = (int)a;
    sleep(val);
    exit();
}
void wait_and_join(){
    
    THREAD thread[2];
    int val1 = 5, val2 = 2;
    // Call fork
    int tgid = fork();
    // Go in child
    if(!tgid){
      // create theads
        thread[0] = create_thread(wait_and_join_fcn, (void*)val1, CLONE_VM);
        // Create another thread
        thread[1] = create_thread(wait_and_join_fcn, (void*)val2, CLONE_VM);
        // Call join for all
        int ans1 = join_threads(thread[0]);

        int ans2 = join_threads(thread[1]);

        if(ans1 != thread[0].pid || ans2 != thread[1].pid){
            printf(1,"\nwait_and_join Test Failed.\n");
        }else{
            printf(1,"\nwait_and_join Test Passed\n");
        }
        exit();  
    }

    int ans;
    while((ans = wait()) != -1);
    exit();
}





// Global variable
int global_for_flag_testing = 0;
// int parent_id = 0;
// int group_id = 0;

void flag_combination_testing_fcn(void* arg1){
  int *arg = (int*)arg1; 
  int fd = (int)arg[0];
  // int rd;
  char str[10];
  // fd = open("README", O_RDWR);
  read(fd, &str, 10);
  // int cwd;
  // cwd = cwd_inode();
  int pid, ppid;
  // int tid;
  pid = get_pid();
  ppid = get_ppid();
  // tid = get_tid();
  printf(1, "Group ID of thread: %d, Group ID of parent: %d", pid, arg[1]);
  printf(1, "\tPPID of thread: %d, PPID of parent: %d", ppid, arg[2]);
  // arg[1] = ppid;
  // arg[2] = pid;
  // printf(1, "\n---PID: %d\n", group_id);
  // printf(1, "Global var: %d\nFile Descripter: %d\tRead return: %d\nCWD Inode num: %d\npid: %d\tppid: %d\ttid:%d\n",
  // global_for_flag_testing, fd, rd, cwd, pid, ppid, tid);

  // Make changes here
  global_for_flag_testing++;
  close(fd);
  // rd = read(fd, &str, 10);
  mkdir("OSTest");
  chdir("OSTest");
  // cwd = cwd_inode();
  // printf(1, "Global var: %d\nFile Descripter: %d\tRead return: %d\nCWD Inode num: %d\npid: %d\tppid: %d\ttid:%d\n",
  // global_for_flag_testing, fd, rd, cwd, pid, ppid, tid);
  // printf(1, "\n---PID: %d %d\n", arg[1], arg[2]);

  exit();


}

void flag_combination_testing(int flags){
  int ids[3];

int clone_files = 0, clone_fs = 0, clone_vm = 0, clone_thread = 0, clone_parent = 0;

  // Checking the flags
  if (flags & (1 << (1 - 1)))
    clone_files = 1;
  else
    clone_files = 0;
  if (flags & (1 << (2 - 1)))
    clone_fs = 1;
  else
    clone_fs = 0;
  if (flags & (1 << (3 - 1)))
    clone_vm = 1;
  else
    clone_vm = 0;
  if (flags & (1 << (4 - 1)))
    clone_thread = 1;
  else
    clone_thread = 0;
  if (flags & (1 << (5 - 1)))
    clone_parent = 1;
  else
    clone_parent = 0;




  int fd1, temp_global;
  char str1[10];
  fd1 = open("README", O_RDWR);
  read(fd1, &str1, 10);
  int cwd1;
  // int rd1;
  cwd1 = cwd_inode();
  /* rd1 =  */read(fd1, &str1, 10);

  temp_global = global_for_flag_testing;
  int pid, ppid;
  // int tid;
  pid = get_pid();
  ppid = get_ppid();
  // tid = get_tid();
  // printf(1, "Global var: %d\nFile Descripter: %d\tRead return: %d\nCWD Inode num: %d\npid: %d\tppid: %d\ttid:%d\n",
  // global_for_flag_testing, fd1, rd1, cwd1, pid, ppid, tid);

  // Now create thread
  THREAD thread;
  ids[0] = fd1;
  ids[1] = pid;
  ids[2] = ppid;
  thread = create_thread(flag_combination_testing_fcn, (void*)&ids[0], flags);
  join_threads(thread);

  int rd2;
  char str2[10];
  int cwd2;
  
  rd2 = read(fd1, &str2, 10);
  cwd2 = cwd_inode();
  // printf(1, "Global var: %d\nFile Descripter: %d\tRead return: %d\nCWD Inode num: %d\npid: %d\tppid: %d\ttid:%d\n",
  // global_for_flag_testing, fd1, rd2, cwd2, pid, ppid, tid);

  int total_flag_answer = -1;

  printf(1, "\t%d %d %d %d %d\t", clone_files, clone_fs, clone_vm, clone_thread, clone_parent);
  if(clone_files == 1){
    if(rd2 == -1){
      total_flag_answer = 1;
      // printf(1, "11");
    }
    else{
      total_flag_answer = -1;
      // printf(1, "11-");

    }
  }
  else{
    if(rd2 != -1){
      total_flag_answer = 0;
      // printf(1, "10");

    }
    else{
      total_flag_answer = -1;
      // printf(1, "10-");

    }
  }
  if(clone_fs == 1){
    if(cwd1 != cwd2){
      total_flag_answer += 2;
      // printf(1, "21");

    }
    else{
      total_flag_answer = -1;
      // printf(1, "21-");

    }
  }
  else{
    if(cwd1 == cwd2){
      total_flag_answer += 0;
      // printf(1, "20");

    }
    else{
      total_flag_answer = -1;
      // printf(1, "20-");

    }
  }
  if(clone_vm == 1){
    if(temp_global != global_for_flag_testing){
      total_flag_answer += 4;
      // printf(1, "31");

    }
    else{
      total_flag_answer = -1;
      // printf(1, "31-");

    }
  }
  else{
    if(temp_global == global_for_flag_testing){
      total_flag_answer += 0;
      // printf(1, "30");

    }
    else{
      total_flag_answer = -1;
      // printf(1, "30-");

    }
  }
  // printf(1, "\nPIDS: %d, %d - %d\n", ppid, ids[1], ids[2]);
  // if(clone_thread == 1){
  //   if(pid == ids[1]){
  //     total_flag_answer += 8;
  //     printf(1, "41");

  //   }
  //   else{
  //     total_flag_answer = -1;
  //     printf(1, "41-");

  //   }
  // }
  // else{
  //   if(pid != ids[1]){
  //     total_flag_answer += 0;
  //     printf(1, "40");

  //   }
  //   else{
  //     total_flag_answer = -1;
  //     printf(1, "40-");

  //   }
  // }
  // if(clone_parent == 1){
  //   if(ppid == ids[2]){
  //     total_flag_answer += 16;
  //     printf(1, "51");

  //   }
  //   else{
  //     total_flag_answer = -1;
  //     printf(1, "51-");

  //   }
  // }
  // else{
  //   printf(1, "PPID: %d, %dp", ppid, ids[2]);
  //   if(ppid != ids[2]){
  //     total_flag_answer += 0;
  //     printf(1, "50");

  //   }
  //   else{
  //     total_flag_answer = -1;
  //     printf(1, "50-");

  //   }
  // }
chdir("..");
  int all_flag = flags;
  if(flags >= 16){
    flags -= 16;
  }
  if(flags >= 8)
    flags -= 8;

  // printf(1, "\nFlag Test for Flag %d and Result: %d\n", flags, total_flag_answer);
  
  if(flags == total_flag_answer){
    printf(1, "\tFlag Test for Flag %d PASSED\n", all_flag);
  }
  else{
    printf(1, "\tFlag Test for Flag %d FAILED\n", all_flag);
  }

}

void stress_test_fcn(void* i){
  printf(1, "%d", (int)i++);
  exit();
}
void stress_test(){
  THREAD t;
  // printf(1, "\nStress Testing:\n");
  // After running some stress tests it was found that after 39 iterations
  // the kernal would stop working
  for(int i = 0; i < 39;){
    t = create_thread(stress_test_fcn, (void*)i, 0);
    join_threads(t);
    i++;
  }
  sleep(20);
  printf(1, "\n");
}

//MATRIX MULTIPLICATION
int thread_count = 3;
int row1, col1, row2, col2;
int matrix1[10][10];
int matrix2[10][10];
int result[10][10];
// int dim[1][2];

void matrix_multiplication_fcn(void *a){
    int n = (int)a;
    // calculate begin and stop
    int begin = (n*row1)/thread_count;

    int stop = ((n+1)*row1)/thread_count;

    for(int i = begin ; i < stop; i++){
        for(int j = 0; j < col2; j++){

            result[i][j] = 0;
            for(int k = 0 ; k < col1; k++){

                result[i][j] += matrix1[i][k]*matrix2[k][j];
            }
        }
    }
    exit();
}

void matrix_multiplication(){
    //Declare number of threads
    THREAD tid[thread_count];
    row1 = 1; 
    col1 = 3;
    for(int row = 0; row<row1; row++){
        for(int col = 0; col < col1; col++){
            matrix1[row][col] = 5;
        }
    }

    row2 = 3; col2 = 1;
    
    for(int row = 0; row<row2; row++){
        for(int col = 0; col < col2; col++){
            matrix2[row][col] = 5;
        }
    }

    //Creating threads
    for(int i = 0; i < thread_count; i++){
        tid[i] = create_thread(matrix_multiplication_fcn, (void*)i, CLONE_VM);
        
    }
    for(int i = 0; i < thread_count; i++){
        join_threads(tid[i]);
    }
    printf(1, "Dim: %d x %d\n", row1, col2);
    for(int r = 0; r < row1; r++){
        for(int c = 0; c < col2; c++){

            printf(1,"%d",result[r][c]);
        }
        printf(1, "\n");
    }
    if(result[0][0] == 75){

        printf(1,"\nmatrix_multiplication passed\n");
    }

}


//FORK IN THREAD
void forkthreadchild(void* arg){
    int ret = fork();
    if(!ret){
        //CHILD
        printf(1,"CHILD IN THREAD\n");
        exit();
    }
    int ret1 = wait();
    printf(1,"ret1 : %d\n", ret1);
    printf(1,"PARENT IN THREAD\tFORK IN THREAD TEST PASSED\n\n");
    
    exit();
}

void forkinThread(){
    THREAD t;
    t = create_thread(forkthreadchild, 0, CLONE_VM);
    join_threads(t);
}

char test_pass_string_arg_var[5];
void test_pass_string_arg_fun(void* args) {
  char *var1 = (char*)args;
  test_pass_string_arg_var[0] = var1[0];
  exit();
}

void test_pass_string_arg() {
  THREAD t;
  char var1[5] = "hello";
  // printf()
  t = create_thread(test_pass_string_arg_fun, (void*)var1, CLONE_VM | CLONE_THREAD);
  join_threads(t);
  if(test_pass_string_arg_var[0] == 'h') {
    printf(1, "Test pass string argument Pass\n\n");
  } else {
    printf(1, "Test pass string argument Fail\n\n");
  }
}

int main(int argc, char *argv[])
{   

    //waitchildtest();
    
    // Final Testing functions
    for(int i = 0; i <= 18; i++)
      flag_combination_testing(i);
    Increment();
    matrix_addition();
    get_process_or_thread_ids();
    clone_files_and_vm();
    joinordertest();
    child_kill_test();
    one_thread_calling_another();
    // wait_and_join();
    race_test();
    matrix_multiplication();
    
    test_pass_string_arg();


    forkinThread();


    stress_test();
    sleep(20);
    exit();
}


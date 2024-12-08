#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  sem_t *mutex; // mutual exlusion for the buffer pool
  sem_t *empty; // number of empty buffers, initially 30
  sem_t *full;  // number of full buffers, initially 0
  
  int *start; // start and end pointers
  int *end;   // of the bounded buffer
  
  int* buf[30];
  int i,j,p,pid,id;
  sh_key_t key=malloc(sizeof(*key));

  // use the shared page 30 for the semaphores
  for (i=0; i<16; i++) key->key[i]=30;
  mutex=(sem_t*)shmget(key);
  if (mutex==0)
    exit();
  empty=mutex+1;
  full=empty+1;
  
  // initialize the semaphores and pointers
  sem_init(mutex,1);
  sem_init(empty,30);
  sem_init(full,0);
  
  start=(int*)(((char*)full)+sizeof(sem_t));
  end=start+1;
  *start=0;
  *end=0;
  
  for (p=1; p<=15; p++) {
    pid=fork();
    if (pid==-1)
      exit();
    if (pid==0) {
      // get all the 30 shared pages (keys 0-29) 
      for (i=0; i<30; i++) {
        for (j=0; j<16; j++) key->key[j]=i;
        buf[i]=(int*)shmget(key);
        if (buf[i]==0)
          exit();
      }

      id=getpid();
      if (id%2==0) {
        // even pids are consumers
        for (;;) {
          sem_down(full);
          sem_down(mutex);
          printf(1,"%d consumed %d from position %d\n",id,buf[*start][0],*start); 
          *start=(*start+1)%30;
          sem_up(mutex);
          sem_up(empty);
        }
      }
      else {
        // odd pids are producers
        for (;;) {
          sem_down(empty);
          sem_down(mutex);
          // each producer writes the whole buffer with its pid 
          for (i=0; i<1024; i++)
            buf[*end][i]=id;
          printf(1,"%d produced to position %d\n",id,*end); 
          *end=(*end+1)%30;
          sem_up(mutex);
          sem_up(full);
        }
      }
      
      exit();
    }
  }
  
  for (p=1; p<=15; p++)
    wait();

  exit();
}


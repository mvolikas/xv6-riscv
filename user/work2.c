#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  sem_t *sem;
  sh_key_t key=malloc(sizeof(*key));
  int* buf[32];
  int i,j,p,pid,total;

  for (i=0; i<32; i++) {
    for (j=0; j<16; j++) key->key[j]=i;
    sem=(sem_t*)shmget(key);
    if (sem==0)
      exit();
    // the semaphore for each of the 32 shared pages lies in the beginning 
    buf[i]=(int*)((char*)sem+sizeof(sem_t));
    sem_init(sem,1);
  }

  // master forks 15 children
  for (p=1; p<=15; p++) {
    pid=fork();
    if (pid==-1)
      exit();
    if (pid==0) {
      // each child increases each of the 32000 integers 1 time 
      for (i=0; i<32; i++) {
        sem=(sem_t*)((char*)buf[i]-sizeof(sem_t));
        sem_down(sem); // comment this line to see the race
        for (j=0; j<1000; j++) {
          int tmp=buf[i][j];
          tmp=tmp+1;
          for (int p=0; p<10; p++); // waste some time here
          buf[i][j]=tmp;
        }
        sem_up(sem); // comment this line to see the race
        }
      exit();
    }
  }
  
  for (p=1; p<=15; p++)
    wait();
  
  // the total sum should be 15*32000=480000
  total=0;
  for (i=0; i<32; i++)
    for (j=0; j<1000; j++) total+=buf[i][j];
  printf(1,"Master sums: %d\n",total);

  exit();
}


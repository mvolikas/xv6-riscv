#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  int i,j,pid;
  char *buf[33];
  sh_key_t key=malloc(sizeof(*key));
  
  for (i=0; i<33; i++) {
    for (j=0; j<16; j++) key->key[j]=i;
    buf[i]=shmget(key);
  }
  if (buf[32]==0)
    printf(1,"max number of pages check: OK\n");
  
  for (j=0; j<16; j++) key->key[j]=31;
  shmrem(key);
  
  for (j=0; j<16; j++) key->key[j]=32;
  buf[32]=shmget(key);
  if (buf[32]!=0)
    printf(1,"free page position check: OK\n");
  
  for (i=0; i<33; i++) {
    for (j=0; j<16; j++) key->key[j]=i;
    if (shmrem(key)<0 && i==31)
      printf(1,"removing unexisting page check: OK\n");
  }

  for (i=0; i<31; i++) {
    for (j=0; j<16; j++) key->key[j]=i;
    buf[i]=shmget(key);
  }
  
  buf[31]=shmget(key);
  if (buf[31]==0)
    printf(1,"trying to get a mapped page check: OK\n");
  for (j=0; j<16; j++) key->key[j]=31;
  buf[31]=shmget(key);
  
  pid=fork();
  if (pid==0) {
    for (j=0; j<16; j++) key->key[j]=32;
    buf[0]=shmget(key);
    if (buf[0]==0)
      printf(1,"child trying to get with maximum number of pages check: OK\n");
    exit();
  }
  wait();

  for (j=0; j<16; j++) key->key[j]=10;
  shmrem(key);
  for (j=0; j<16; j++) key->key[j]=32;
  if (shmget(key)!=0)
    printf(1,"finding empty page position check: OK\n");
  shmrem(key);
  
  for (i=0; i<16; i++) {
    pid=fork();
    if (pid==-1) {
      printf(1,"maximum number of processes per page check: OK\n");
      exit();
    }
    if (pid==0) {
      sleep(100);
      exit();
    }
  }
  for (i=0; i<16; i++)
    wait();

  exit();
}


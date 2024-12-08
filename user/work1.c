#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  sh_key_t key=malloc(sizeof(*key));
  for (int i=0; i<16; i++) key->key[i]=1;
  char *buf=shmget(key);
  
  if (buf==0)
    exit();

  int pid=fork();
  if (pid==-1)
    exit();
    
  if (pid==0) {
    int child=getpid();
    // uncomment to see the trap 14
    // shmrem(key);
    strcpy(buf,"i am your child ");
    itoa(child,buf+16);
  }
  else {
    sleep(100);
    printf(1,"Parent %d reads: \'%s\'\n",getpid(),buf);
  }

  exit();
}


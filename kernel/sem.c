#include "types.h"
#include "defs.h"

// the user is responsible for calling sem_init in a synchronized manner
void
sem_init(sem_t *sem,int value)
{
  sem->value=value;
  initlock(&sem->lock,"sem");
}

void
sem_up(sem_t *sem)
{
  acquire(&sem->lock);
  sem->value++; // increase the value
  wakeup(sem);  // and wake up processes sleeping on sem
  release(&sem->lock);
}

void
sem_down(sem_t *sem)
{
  acquire(&sem->lock);
  // sleep on sem while the value is <=0
  while (sem->value<=0) {
    sleep(sem,&sem->lock);
  }
  sem->value--; // decrease value on wake up
  release(&sem->lock);
}


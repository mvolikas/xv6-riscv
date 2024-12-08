#include "spinlock.h"

typedef struct {
  int value;            // the semaphore value
  struct spinlock lock; // for synchronization
} sem_t;


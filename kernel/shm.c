#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct shpage {
  char used;          // 1 if this page is being used by at least one process
  char *addr;         // virtual kernel address of this page
  sh_key_tt key;      // the 128 bit key of this page
  int psharing;       // number of processes sharing this page
  int maps[NSHPROC];  // pid of a process which maps this page
  char mpos[NSHPROC]; // position in process's shared space
};

struct {
  struct spinlock lock;         // for synchronization
  struct shpage pages[NSHPAGE]; // the page table
} shmtable;

// called in main.c with only one cpu running
void
shinit(void)
{
  initlock(&shmtable.lock,"shtable");
  for (int i=0; i<NSHPAGE; i++) {
    shmtable.pages[i].used=0;
    shmtable.pages[i].psharing=0;
    for (int j=0; j<16; j++)
      shmtable.pages[i].key.key[j]=0; // initialize key with 0
    for (int j=0; j<NSHPROC; j++)
      shmtable.pages[i].maps[j]=-1;   // -1 means empty position
  }
}

// returns 0 if key1==key2 and 1 otherwise
int
keycmp(sh_key_tt key1,sh_key_tt key2)
{
  for (int i=0; i<16; i++)
    if (key1.key[i]!=key2.key[i])
      return 0;
  return 1;
}

void*
shmget(sh_key_tt key)
{
  int i,j,k,found=0;
  struct proc *p=myproc(); // get current process

  acquire(&shmtable.lock);
  // try to find this page's location i
  for (i=0; i<NSHPAGE; i++)
    if (shmtable.pages[i].used && keycmp(shmtable.pages[i].key,key))
      break;

  if (i==NSHPAGE) {
    // this page does not exist
    // try to find an empty position
    for (i=0; i<NSHPAGE; i++)
      if (!shmtable.pages[i].used) {
        shmtable.pages[i].used=1;
        break;
      }
    
    // page table is full
    if (i==NSHPAGE)
      goto bad;

    found=1;

    // allocate a new page and store its address
    if ((shmtable.pages[i].addr=kalloc())==0)
      goto bad;
    memset(shmtable.pages[i].addr,0,PGSIZE);
    shmtable.pages[i].key=key;
  }

  // check if this process is already using this page
  for (j=0; j<NSHPROC; j++)
    if (shmtable.pages[i].maps[j]==p->pid)
      goto bad;
  
  // find an empty position for this process
  for (j=0; j<NSHPROC; j++)
    if (shmtable.pages[i].maps[j]==-1)
      break;
  
  // this page is being used by the maximum number of processes
  if (j==NSHPROC)
    goto bad;
  
  // find an empty position in process's shmaps bitmap  
  for (k=0; k<NSHPAGE; k++)
      if (!(p->shmaps[k/8]&(1<<(k%8)))) { // found a 0 bit
        p->shmaps[k/8]|=1<<(k%8);         // set this bit to 1
        break;
      }
  
  // map PGSIZE virtual addresses starting at KERNBASE-(k+1)*PGSIZE to physical starting at this page's address
  if (mappages(p->pgdir,(char*)(KERNBASE-(k+1)*PGSIZE),PGSIZE,V2P(shmtable.pages[i].addr),PTE_W|PTE_U)<0) {
    p->shmaps[k/8]^=1<<(k%8); // clear this bit, since the mapping failed
    goto bad;
  }
  
  shmtable.pages[i].maps[j]=p->pid; // store this process's pid
  shmtable.pages[i].mpos[j]=k;      // what is the bitmap position
  shmtable.pages[i].psharing++;     // and increment the counter for this page
  
  release(&shmtable.lock);
  return (void*)(KERNBASE-(k+1)*PGSIZE); // return the virtual user address
  
  bad:
    // something went wrong
    if (found)
      shmtable.pages[i].used=0;
    release(&shmtable.lock);
    return 0;
}

int
shmrem(sh_key_tt key)
{
  int i,j,k;
  struct proc *p=myproc(); // get current process

  acquire(&shmtable.lock);
  // try to find this page's location i
  for (i=0; i<NSHPAGE; i++)
    if (shmtable.pages[i].used && keycmp(shmtable.pages[i].key,key))
      break;
  
  // this page does not exist
  if (i==NSHPAGE)
    goto bad;
  
  // find the position of this process
  for (j=0; j<NSHPROC; j++)
    if (shmtable.pages[i].maps[j]==p->pid)
      break;
  
  // this process does not share this page
  if (j==NSHPROC)
    goto bad;

  // find the page table entry for this page's address
  pte_t *pte=walkpgdir(p->pgdir,(char*)(KERNBASE-(shmtable.pages[i].mpos[j]+1)*PGSIZE),0);
  
  // this PTE is not valid
  if (!pte || (*pte & PTE_P)==0)
    goto bad;

  // unmap this page from this process by setting its bitmap's bit to 0 
  p->shmaps[shmtable.pages[i].mpos[j]/8]^=1<<(shmtable.pages[i].mpos[j]%8);
  shmtable.pages[i].maps[j]=-1;   // make this position empty
  k=--shmtable.pages[i].psharing; // and decrement the counter for this page

  if (k==0) {                      // if no process shares this page anymore
    shmtable.pages[i].used=0;      // set used to 0
    kfree(shmtable.pages[i].addr); // and free the space
    }
  *pte=0; // unmap this page from process's page table

  release(&shmtable.lock);
  return k; // return the number of processes sharing this page

  bad:
    // something went wrong
    release(&shmtable.lock);
    return -1;
}

// release all shared pages being used by this process
// called inside wait before freevm(p->pgdir)
void
shmfree(struct proc *p)
{
  acquire(&shmtable.lock);
  int i,j;
  for (i=0; i<NSHPAGE; i++) {
    if (shmtable.pages[i].used)
      for (j=0; j<NSHPROC; j++)
        // if this process shares this page
        if (shmtable.pages[i].maps[j]==p->pid) {
          shmtable.pages[i].maps[j]=-1;      // make this position empty
          shmtable.pages[i].psharing--;      // decrement the counter for this page 
          if (!shmtable.pages[i].psharing) { // if no process shares this page anymore
            shmtable.pages[i].used=0;        // set used to 0
            kfree(shmtable.pages[i].addr);   // and free the space
            // no need to update the page table because it will be deleted after shmfree
          }
        }
  }
  release(&shmtable.lock);
}

// used by fork to copy shared pages mappings from parent to child
int
shmcopy(struct proc *p,struct proc *np)
{
  int i,j,k,pos,ppos,mapped;

  acquire(&shmtable.lock);
  for (i=0; i<NSHPAGE; i++)
    if (shmtable.pages[i].used) { // for every used page
      pos=-1;   // empty position for child's mapping
      mapped=0; // check if this page is mapped by the parent
      for (j=0; j<NSHPROC; j++) {
        if (shmtable.pages[i].maps[j]==-1)
          pos=j;
        else if (shmtable.pages[i].maps[j]==p->pid) {
          ppos=j;
          mapped=1;
          }
        }

      // this page is being used by the maximum number of processes
      if (pos==-1)
        goto bad;
      
      // go to next page position if no mapping for the parent exists
      if (!mapped)
        continue;
      
      k=shmtable.pages[i].mpos[ppos]; // map at the same positions with parent
      
      // map PGSIZE virtual addresses starting at KERNBASE-(k+1)*PGSIZE to physical starting at parent page's address
      if (mappages(np->pgdir,(char*)(KERNBASE-(k+1)*PGSIZE),PGSIZE,V2P(shmtable.pages[i].addr),PTE_W|PTE_U)<0)
        goto bad;
      
      shmtable.pages[i].maps[pos]=np->pid; // save child's pid
      shmtable.pages[i].mpos[pos]=k;       // what is the bitmap position
      shmtable.pages[i].psharing++;        // increment the counter for this page
      np->shmaps[k/8]|=1<<(k%8);           // and set the corresponding bit to 1
    }
  release(&shmtable.lock);
  return 0;
  
  bad:
    // something went wrong
    release(&shmtable.lock);
    return -1;
}


A fork of the xv6 kernel with shared memory components.

#### Key points of modification:
- Semaphores: `sem.c` `sem.h`
- Shared memory management: `shm.c` `shm.h`
- In `argptr` (`syscall.c`), a check was added to handle the case where the address is in the shared memory space. `argptr` is used in `sysproc.c` for both `shmget` and `shmrem`, as `sh_key_t` is simply a pointer to `sh_key_tt` used by the kernel.
- In `allocproc` (`proc.c`), the bitmap for free shared pages positions is initialized to 0. In `fork`, the `shmcopy` function is called to map the parent's shared pages to the same positions in the child. In `wait`, when an exited child is found, `shmfree` is called to free any shared pages that only this process had.

#### Kernel structures:
The key for each page is a `char[16]`.

There is a `shmtable` structure protected by a spinlock that keeps track of the following:
- the page key
- whether the page is in use
- the address returned by `kalloc`
- processes that share the page, and where in the shared space.

For each process we keep a bitmap (`char[4]`) with its mappings.

#### Workloads:
- **work1**:
  Forks a child that writes its PID to a shared buffer. The parent then reads the correct data from the buffer. If the child performs `shmrem` and then tries to write, a trap 14 occurs as expected since the mapping for the child no longer exists.
- **work2**:
  Forks 15 processes that each increment one of the 1024 values in each of the 32 shared pages. The master (the one that did the fork) calculates the sum. Using semaphores, the sum is correct, whereas without them, a race condition is observed.
- **work3**:
  Implements the bounded buffer problem with 3 semaphores. The program runs continuously, with consumers trying to write and producers trying to read from a buffer with 30 shared pages. Without semaphores, a race condition is observed here as well.
- **work4**:
  Performs various `shmget`, `shmrem`, and `fork` operations to test extreme conditions (e.g., attempting to `shmrem` a page that does not exist).

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

uint64
sys_co_yield(void)
{
  int pid, val;
  argint(0, &pid);
  argint(1, &val);
  return co_yield(pid, val);
}

uint64
sys_memsize(void)
{
  return myproc()->sz;
}

// --- Task 1: process group identity ---

uint64
sys_setgid(void)
{
  int gid;
  argint(0, &gid);
  myproc()->gid = gid;
  return 0;
}

uint64
sys_getgid(void)
{
  return (uint64)myproc()->gid;
}

// --- Task 1: Israeli lock syscall wrappers ---

uint64
sys_israeli_create(void)
{
  int favoritism;
  argint(0, &favoritism);
  return (uint64)israeli_create(favoritism);
}

uint64
sys_israeli_acquire(void)
{
  int lock_id;
  argint(0, &lock_id);
  return (uint64)israeli_acquire(lock_id);
}

uint64
sys_israeli_release(void)
{
  int lock_id;
  argint(0, &lock_id);
  return (uint64)israeli_release(lock_id);
}

uint64
sys_israeli_destroy(void)
{
  int lock_id;
  argint(0, &lock_id);
  return (uint64)israeli_destroy(lock_id);
}

// --- Task 2: relay-race score syscall wrappers ---

uint64
sys_israeli_score_inc(void)
{
  int lock_id, team_id;
  argint(0, &lock_id);
  argint(1, &team_id);
  return (uint64)israeli_score_inc(lock_id, team_id);
}

uint64
sys_israeli_score_get(void)
{
  int lock_id, team_id;
  argint(0, &lock_id);
  argint(1, &team_id);
  return (uint64)israeli_score_get(lock_id, team_id);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sys_flip_display: zero-copy page flip.
//
// Syscall argument 0: user virtual address of a page-aligned buffer
// that is exactly GPU_FB_PAGES (300) * PGSIZE bytes (i.e. 640x480x4 =
// 1,228,800 bytes).  The buffer must already be fully mapped in the
// calling process's address space.
//
uint64
sys_flip_display(void)
{
  uint64 buf;
  argaddr(0, &buf);
  struct proc *p = myproc();

  // The buffer must be page-aligned (the device backing list is in pages).
  if(buf == 0 || (buf % PGSIZE) != 0)
    return -1;

  // Collect the physical address of each back-buffer page, validating that
  // all GPU_FB_PAGES pages are mapped with user permission.  A kalloc'd page
  // (4096 bytes) holds up to 512 uint64 entries, comfortably more than the
  // 300 we need, and avoids a large kernel-stack array.
  uint64 *pas = (uint64 *)kalloc();
  if(pas == 0)
    return -1;

  for(int i = 0; i < GPU_FB_PAGES; i++){
    uint64 pa = walkaddr(p->pagetable, buf + (uint64)i * PGSIZE);
    if(pa == 0){            // not mapped, or no user permission
      kfree(pas);
      return -1;
    }
    pas[i] = pa;
  }

  if(virtio_gpu_flip(pas, GPU_FB_PAGES, p->pid) < 0){
    kfree(pas);
    return -1;
  }

  kfree(pas);
  p->fb_flipped = 1;
  return 0;
}

// sys_map_display: map the GPU's kernel framebuffer pages (fb[]) directly
// into the calling process's address space with PTE_U|PTE_R|PTE_W.
//
// Syscall argument 0: desired user virtual address (must be page-aligned).
//   Pass 0 to let the kernel auto-select the next available VA above p->sz.
//
// Returns the mapped virtual address on success, (uint64)-1 on failure.
//
uint64
sys_map_display(void)
{
  uint64 addr;
  argaddr(0, &addr);
  struct proc *p = myproc();

  uint64 va;
  if(addr == 0){
    // Auto-select the fixed framebuffer window below the trapframe.  It sits
    // far above the heap, so a later sbrk() can never grow into it.
    va = GPU_FB_BASE;
  } else {
    // Caller-supplied VA must be page-aligned.
    if((addr % PGSIZE) != 0)
      return -1;
    va = addr;
  }

  // The framebuffer region must fit below the trapframe/trampoline and must
  // not overlap the current heap -- otherwise a later sbrk() could try to
  // remap these pages and panic the kernel.
  uint64 size = (uint64)GPU_FB_PAGES * PGSIZE;
  if(va < p->sz || va >= TRAPFRAME || va + size > TRAPFRAME)
    return -1;

  // Only one framebuffer mapping per process; drop a previous one first so
  // its kernel-owned pages are never leaked or double-mapped.
  if(p->fbva){
    vm_unmap_fb(p->pagetable, p->fbva);
    p->fbva = 0;
  }

  if(vm_map_fb(p->pagetable, va) < 0)
    return -1;

  p->fbva = va;
  return va;
}

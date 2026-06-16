#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define MAX_ISRAELI_LOCKS 16
#define MAX_IL_WAITERS    16
#define MAX_IL_TEAMS      8

struct israeli_lock {
  struct spinlock lk;
  int active;
  int locked;
  int holder_gid;
  int favoritism;
  struct proc *queue[MAX_IL_WAITERS];
  int count;
  int scores[MAX_IL_TEAMS]; // Task 2 helper
};

static struct israeli_lock ilocks[MAX_ISRAELI_LOCKS];

void israeli_init(void) {
  for(int i = 0; i < MAX_ISRAELI_LOCKS; i++) {
    initlock(&ilocks[i].lk, "israeli");
    ilocks[i].active = 0;
  }
}

int israeli_create(int favoritism) {
  if(favoritism < 0 || favoritism > 100) return -1;
  for(int i = 0; i < MAX_ISRAELI_LOCKS; i++) {
    acquire(&ilocks[i].lk);
    if(!ilocks[i].active) {
      ilocks[i].active = 1;
      ilocks[i].locked = 0;
      ilocks[i].holder_gid = -1;
      ilocks[i].favoritism = favoritism;
      ilocks[i].count = 0;
      for(int t = 0; t < MAX_IL_TEAMS; t++) ilocks[i].scores[t] = 0;
      release(&ilocks[i].lk);
      return i;
    }
    release(&ilocks[i].lk);
  }
  return -1;
}

int israeli_acquire(int lock_id) {
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS) return -1;
  struct israeli_lock *il = &ilocks[lock_id];
  struct proc *p = myproc();

  acquire(&il->lk);
  if(!il->active) {
    release(&il->lk);
    return -1;
  }

  // Fast path: lock is free
  if(!il->locked) {
    il->locked = 1;
    il->holder_gid = p->gid;
    release(&il->lk);
    return 0;
  }

  // Slow path: enqueue and sleep
  if(il->count >= MAX_IL_WAITERS) {
    release(&il->lk);
    return -1; 
  }
  il->queue[il->count++] = p;

  while(1) {
    sleep(p, &il->lk);
    
    // Handle destroyed lock or killed process
    if(!il->active || killed(p)) {
      for(int i = 0; i < il->count; i++) {
        if(il->queue[i] == p) {
          for(int j = i; j < il->count - 1; j++) il->queue[j] = il->queue[j+1];
          il->count--;
          break;
        }
      }
      release(&il->lk);
      return -1;
    }

    // Check if we are still in the queue
    int queued = 0;
    for(int i = 0; i < il->count; i++) {
      if(il->queue[i] == p) queued = 1;
    }
    if(!queued) break; // We own the lock
  }

  release(&il->lk);
  return 0;
}

int israeli_release(int lock_id) {
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS) return -1;
  struct israeli_lock *il = &ilocks[lock_id];

  acquire(&il->lk);
  if(!il->active || !il->locked) {
    release(&il->lk);
    return -1;
  }

  if(il->count == 0) {
    il->locked = 0;
    il->holder_gid = -1;
    release(&il->lk);
    return 0;
  }

  // Apply favoritism logic
  int chosen = 0;
  if(il->favoritism > 0 && (lcg_rand() % 100) < il->favoritism) {
    for(int i = 0; i < il->count; i++) {
      if(il->queue[i]->gid == il->holder_gid) {
        chosen = i;
        break;
      }
    }
  }

  struct proc *next = il->queue[chosen];
  for(int i = chosen; i < il->count - 1; i++) il->queue[i] = il->queue[i+1];
  il->count--;

  il->holder_gid = next->gid;
  wakeup(next);
  release(&il->lk);
  return 0;
}

int israeli_destroy(int lock_id) {
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS) return -1;
  struct israeli_lock *il = &ilocks[lock_id];
  
  acquire(&il->lk);
  if(!il->active) {
    release(&il->lk);
    return -1;
  }
  
  il->active = 0;
  for(int i = 0; i < il->count; i++) wakeup(il->queue[i]);
  il->count = 0;
  
  release(&il->lk);
  return 0;
}

// Task 2 Helpers (Scores)
int israeli_score_inc(int lock_id, int team_id) {
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS || team_id < 0 || team_id >= MAX_IL_TEAMS) return -1;
  struct israeli_lock *il = &ilocks[lock_id];
  acquire(&il->lk);
  int v = ++il->scores[team_id];
  release(&il->lk);
  return v;
}

int israeli_score_get(int lock_id, int team_id) {
  if(lock_id < 0 || lock_id >= MAX_ISRAELI_LOCKS || team_id < 0 || team_id >= MAX_IL_TEAMS) return -1;
  struct israeli_lock *il = &ilocks[lock_id];
  acquire(&il->lk);
  int v = il->scores[team_id];
  release(&il->lk);
  return v;
}
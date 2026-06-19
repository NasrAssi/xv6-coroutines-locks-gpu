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

// Remove the queue entry at index idx, shifting the remaining waiters down.
// Caller must hold il->lk.
static void
il_remove_at(struct israeli_lock *il, int idx) {
  for(int j = idx; j < il->count - 1; j++) il->queue[j] = il->queue[j+1];
  il->count--;
}

// Remove proc p from the wait queue if present.  Returns 1 if it was found.
// Caller must hold il->lk.
static int
il_dequeue(struct israeli_lock *il, struct proc *p) {
  for(int i = 0; i < il->count; i++) {
    if(il->queue[i] == p) {
      il_remove_at(il, i);
      return 1;
    }
  }
  return 0;
}

// Hand the currently-held lock to the next waiter (honoring favoritism), or
// mark it free if no one is waiting.  Caller must hold il->lk with the lock
// held (il->locked == 1); il->holder_gid identifies the releasing team.
static void
il_handoff(struct israeli_lock *il) {
  if(il->count == 0) {
    il->locked = 0;
    il->holder_gid = -1;
    return;
  }

  // FIFO by default, but with probability `favoritism` percent give the baton
  // to the first waiter from the releasing holder's team.
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
  il_remove_at(il, chosen);
  il->holder_gid = next->gid;
  wakeup(next);
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

    // The lock was destroyed out from under us.  Nothing to release: even if
    // we had just been granted it, the lock no longer exists.
    if(!il->active) {
      il_dequeue(il, p);
      release(&il->lk);
      return -1;
    }

    // We are granted the lock once israeli_release() has removed us from the
    // queue (it leaves locked==1 and holder_gid set).
    int queued = 0;
    for(int i = 0; i < il->count; i++) {
      if(il->queue[i] == p) { queued = 1; break; }
    }

    if(!queued) {
      if(killed(p)) {
        // We were handed the lock but we are dying.  Pass it on instead of
        // leaking it -- otherwise locked stays 1 with no holder and every
        // other waiter blocks forever.
        il_handoff(il);
        release(&il->lk);
        return -1;
      }
      break; // We own the lock.
    }

    // Still queued: give up if killed, otherwise it was a spurious wakeup so
    // go back to sleep.
    if(killed(p)) {
      il_dequeue(il, p);
      release(&il->lk);
      return -1;
    }
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

  il_handoff(il);
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
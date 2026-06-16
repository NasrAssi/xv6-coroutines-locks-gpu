// Linear Congruential Generator (LCG) pseudo-random number generator.
//
//   x_{n+1} = (a * x_n + b) mod m
//
// with a = 1664525, b = 1013904223, m = 2^32. The modulus is implicit:
// `uint` is 32 bits, so unsigned overflow wraps mod 2^32 for free.
//
// A short-lived spinlock protects the shared state from concurrent updates.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

#define LCG_A 1664525u
#define LCG_B 1013904223u

static struct spinlock lcg_lock;
static uint lcg_state = 1;

void lcg_init(void)
{
  initlock(&lcg_lock, "lcg");
}

void lcg_srand(uint seed)
{
  acquire(&lcg_lock);
  lcg_state = seed;
  release(&lcg_lock);
}

uint lcg_rand(void)
{
  uint r;
  acquire(&lcg_lock);
  lcg_state = LCG_A * lcg_state + LCG_B;
  r = lcg_state;
  release(&lcg_lock);
  return r;
}

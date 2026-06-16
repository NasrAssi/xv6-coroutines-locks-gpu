#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static int passes = 0;
static int fails = 0;

static void
expect(const char *name, int got, int expected)
{
  if(got == expected){
    printf("PASS: %s (got %d)\n", name, got);
    passes++;
  } else {
    printf("FAIL: %s (got %d, expected %d)\n", name, got, expected);
    fails++;
  }
}

static void
test_nonexistent_pid(void)
{
  int r = co_yield(99999, 1);
  expect("A: yield to non-existent pid 99999", r, -1);
}

static void
test_zombie_target(void)
{
  int pid = fork();
  if(pid < 0){
    printf("FAIL: B: fork failed\n");
    fails++;
    return;
  }
  if(pid == 0){
    exit(0);
  }
  // Give the child time to run and become a ZOMBIE before we yield.
  sleep(5);
  int r = co_yield(pid, 1);
  expect("B: yield to zombie child", r, -1);
  wait(0);
}

static void
test_self_yield(void)
{
  int r = co_yield(getpid(), 1);
  expect("C: yield to self", r, -1);
}

static void
test_pid_zero(void)
{
  int r = co_yield(0, 1);
  expect("D: yield to pid 0", r, -1);
}

static void
test_negative_pid(void)
{
  int r = co_yield(-5, 1);
  expect("E: yield to negative pid -5", r, -1);
}

int
main(int argc, char *argv[])
{
  printf("co_error_test: starting\n");

  test_nonexistent_pid();
  test_zombie_target();
  test_self_yield();
  test_pid_zero();
  test_negative_pid();

  printf("co_error_test: %d passed, %d failed\n", passes, fails);
  exit(fails == 0 ? 0 : 1);
}

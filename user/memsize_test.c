#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  void *p;

  printf("memsize before allocation: %d bytes\n", memsize());

  p = malloc(20000);

  printf("memsize after malloc(20000): %d bytes\n", memsize());

  free(p);
  
  printf("memsize after free: %d bytes\n", memsize());

  exit(0);
}

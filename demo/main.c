#include "csdsa.h"
#include <stdio.h>

void tester() {
  void *x = stpush(2000);
  stpush(200);
  stpop();
  printf("%p\n", x);
}

int main() {
  unsigned int num = 0xFFFFFFF9;

  printf("Header: 0x%08X\n", num);
  printf("Block Size: 0x%06X\n", ((uint32_t)(num >> 4)));
  printf("Block Free: %d\n", ((num & 1) == 1));

  stalloc *alloc = stalloc_create(STALLOC_DEFAULT);

  GFRAME(alloc, tester());

  stalloc_free(alloc);

  return 0;
}
#include "csdsa.h"

void memswap(void *a, void *b, size_t size) {
  unsigned char  temp;
  unsigned char *p = a;
  unsigned char *q = b;

  for (size_t i = 0; i < size; i++) {
    temp = p[i];
    p[i] = q[i];
    q[i] = temp;
  }
}

void *recalloc(void *a, size_t size) {
  void *new = realloc(a, size);
  memset(new, 0, size);
  return new;
}
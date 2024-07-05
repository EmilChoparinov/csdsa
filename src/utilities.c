#include "csdsa.h"

uint64_t hash_bytes(void *ptr, size_t size) {
  /* djb2 by Dan Bernstein
   * http://www.cse.yorku.ca/~oz/hash.html */
  uint64_t       hash = 5381;
  unsigned char *b_ptr = (unsigned char *)ptr;

  for (size_t i = 0; i < size; i++) {
    hash = ((hash << 5) + hash) + b_ptr[i];
  }
  // log_info("hash %s (%ld) -> %ld\n", (char *)ptr, size, hash);
  return hash;
}

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
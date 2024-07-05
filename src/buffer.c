#include "csdsa.h"

cbuff *buff_init(cbuff *b, void *region) {
  b->region = b->top = region;
  b->len = 0;
  return b;
}

void buff_push(cbuff *b, void *item, size_t item_size) {
  assert(b);
  assert(item);
  assert(item_size > 0);

  b->len++;
  memmove(b->top, item, item_size);
  b->top += item_size;
}

void *buff_skip(cbuff *b, size_t to_skip) {
  assert(b);
  if (to_skip == 0) return b->top;
  b->top += to_skip;
  return b->top;
}

void *buff_at(cbuff *b) { return b->top; }

void *buff_pop(cbuff *b, size_t item_size) {
  assert(b);
  assert(item_size > 0);

  b->len--;
  b->top -= item_size;
  return b->top;
}
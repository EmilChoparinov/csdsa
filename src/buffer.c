#include "csdsa.h"

buffapi *buff_init(buffapi *b, void *region) {
  b->region = b->top = region;
  b->len = 0;
  return b;
}
buffapi *buff_hinit(void *region) {
  buffapi *b = malloc(sizeof(buffapi));
  assert(b);

  buff_init(b, region);
  return b;
}

void buff_hfree(buffapi *b) { free(b); }

void buff_push(buffapi *b, void *item, size_t item_size) {
  assert(b);
  assert(item);
  assert(item_size > 0);

  b->len++;
  memmove(b->top, item, item_size);
  b->top += item_size;
}

void *buff_skip(buffapi *b, size_t to_skip) {
  assert(b);
  if (to_skip == 0) return b->top;
  b->top += to_skip;
  return b->top;
}

void *buff_at(buffapi *b) { return b->top; }

void *buff_pop(buffapi *b, size_t item_size) {
  assert(b);
  assert(item_size > 0);

  b->len--;
  b->top -= item_size;
  return b->top;
}

#include "csdsa.h"

/* Since the set is built on the map structure, we must have some data for a
   value to be stored. */
#define VALUE_SIZE sizeof(int8_t)
int8_t dead_value = 0;

/*-------------------------------------------------------
 * Static Functions
 *-------------------------------------------------------*/
static void init_asserts(set *s);

static void init_asserts(set *s) {
  assert(s);
  assert(s->internals.allocator);
  assert(s->internals.elements.elements);
}

set *__set_init(set *s, int64_t el_size, stalloc *alloc, int32_t flags,
                int64_t initial_size) {
  assert(el_size > 0);
  assert(alloc);

  assert(__map_init(&s->internals, VALUE_SIZE, el_size, alloc, flags,
                    initial_size) != NULL);
  return s;
}

void set_free(set *s) { map_free(&s->internals); }
void set_clear(set *s) { map_clear(&s->internals); }
set *set_copy(set *dest, set *src) {
  map_copy(&dest->internals, &src->internals);
  return dest;
}
int64_t set_length(set *s) { return map_load(&s->internals); }

feach(intersects, kvpair, item, {
  void *key = item.key;
  set  *out = ((void **)args)[0];
  set  *b = ((void **)args)[1];
  if (set_has(b, key)) set_put(out, key);
});
set *set_intersect(set *a, set *b, set *out) {
  assert(a->internals.__key_size == b->internals.__key_size);
  init_asserts(a);
  init_asserts(b);

  /* Out can only be as big as the smallest of a and b */
  int64_t min = set_length(a);
  int64_t other = set_length(b);
  if (other < min) min = other;

  __set_init(out, a->internals.__key_size, a->internals.allocator,
             a->internals.flags, min);

  void *args[2];
  args[0] = out;
  args[1] = b;
  map_foreach(&a->internals, intersects, args);
  return out;
}

feach(add_to_set, kvpair, item, {
  void *key = item.key;
  set  *out = args;
  set_put(out, key);
});
set *set_union(set *a, set *b, set *out) {
  init_asserts(a);
  init_asserts(b);
  assert(a->internals.__key_size == b->internals.__key_size);

  __set_init(out, a->internals.__key_size, a->internals.allocator,
             a->internals.flags,
             map_load(&a->internals) + map_load(&b->internals));

  map_foreach(&a->internals, add_to_set, out);
  map_foreach(&b->internals, add_to_set, out);
  return out;
}

/* Element Operations */
void set_put(set *s, void *key) { map_put(&s->internals, key, &dead_value); }
bool set_has(set *s, void *key) { return map_has(&s->internals, key); }
void set_del(set *s, void *key) { map_del(&s->internals, key); }
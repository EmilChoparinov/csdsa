#include "csdsa.h"

static vec  *vec_construct(vec *v);
static void  init_asserts(vec *v);
static void  bound_asserts(vec *v, int64_t pos);
static void *vec_alloc(vec *v, int64_t bytes);
static void *lookup_el(vec *v, int64_t pos);

vec *__vec_init(vec *v, int64_t el_size, stalloc *alloc, int32_t flags) {
  vec_construct(v);
  v->allocator = alloc;
  v->__el_size = el_size;
  v->flags = flags;
  v->elements = vec_alloc(v, v->__el_size * v->__size);
  assert(v->elements);
  return v;
}

void vec_free(vec *v) {
  if (v->flags == TO_HEAP) {
    hfree(v->allocator, v->elements);
  }
;}

void vec_resize(vec *v, int64_t size) {
  init_asserts(v);
  v->length = size;
  if (size < v->__size) return;
  int64_t old_size = v->__size;
  while (size >= v->__size) v->__size *= 2;
  void *new_addr = vec_alloc(v, v->__size * v->__el_size);
  assert(new_addr);
  memcpy(new_addr, v->elements, old_size * v->__el_size);
  /* If old address was heap alloc'd then it needs to be cleared */
  if (v->flags == TO_HEAP) free(v->elements);
  v->elements = new_addr;
}

void vec_copy(vec *dest, vec *src) {
  init_asserts(dest);
  init_asserts(src);
  vec_free(dest);

  /* Copy metadata */
  memmove(dest, src, sizeof(*src));
  dest->elements = vec_alloc(dest, src->__size * src->__el_size);

  /* Copy elements */
  memmove(dest->elements, src->elements, src->__size * src->__el_size);
}

void vec_clear(vec *v) {
  init_asserts(v);
  v->length = 0;
  v->__top = 0;
  memset(v->elements, 0, v->__size * v->__el_size);
}

/* Element operations */
void *vec_at(vec *v, int64_t pos) {
  bound_asserts(v, pos);
  return lookup_el(v, pos);
}

void vec_put(vec *v, int64_t pos, void *el) {
  bound_asserts(v, pos);
  void *cur_el = vec_at(v, pos);
  memmove(cur_el, el, v->__el_size);
}

void vec_delete_at(vec *v, int64_t pos) {
  bound_asserts(v, pos);
  if (v->length - 1 == pos) {
    vec_pop(v);
    return;
  }

  void *loc = vec_at(v, pos);
  void *loc_next = vec_at(v, pos + 1);
  assert(loc);
  assert(loc_next);

  memmove(loc, loc_next, (v->length - pos - 1) * v->__el_size);
  v->length--;
  v->__top--;
}

bool vec_has(vec *v, void *el) {
  init_asserts(v);
  assert(el);
  return vec_find(v, el) != -1;
}
int64_t vec_find(vec *v, void *el) {
  init_asserts(v);

  for (int64_t i = 0; i < v->length; i++)
    if (memcmp(el, vec_at(v, i), v->__el_size) == 0) return i;
  return -1;
}
void vec_push(vec *v, void *el) {
  init_asserts(v);
  assert(el);
  vec_resize(v, v->length);
  memmove(lookup_el(v, v->__top), el, v->__el_size);
  v->__top++;
  if (v->__top >= v->length) v->length = v->__top;
}
void vec_pop(vec *v) {
  init_asserts(v);
  assert(v->__top);
  if (v->__top == v->length) v->length--;
  v->__top--;
}
void *vec_top(vec *v) {
  init_asserts(v);
  return lookup_el(v, v->__top - 1);
}
void vec_swap(vec *v, int64_t idx1, int64_t idx2) {
  void *a = vec_at(v, idx1);
  void *b = vec_at(v, idx2);
  memswap(a, b, v->__el_size);
}

/* Functional operations */
void vec_sort(vec *v, _compare cmp, void *args) {
  /* Just simple selection sort (I think) for now. */

  int64_t i, j;
  for (i = 0; i < v->length; i++) {
    for (j = 0; j < v->length - i - 1; j++) {
      if (!cmp(vec_at(v, j), vec_at(v, j + 1), args)) vec_swap(v, j, j + 1);
    }
  }
}

int64_t vec_count_if(vec *v, _pred p, void *args) {
  init_asserts(v);
  int64_t count = 0;
  for (int64_t i = 0; i < v->length; i++)
    if (p(vec_at(v, i), args)) count++;
  return count;
}

vec *vec_filter(vec *v, _pred p, void *args) {
  init_asserts(v);
  vec filter;
  __vec_init(&filter, v->__el_size, v->allocator, TO_STACK);

  for (int64_t i = 0; i < v->length; i++)
    if (p(vec_at(v, i), args)) vec_push(&filter, vec_at(v, i));

  /* Free the internals of the *v vector and copy over the local one. */
  vec_free(v);
  memmove(v, &filter, sizeof(*v));

  return v;
}

void vec_foreach(vec *v, _nullary n, void *args) {
  init_asserts(v);
  for (int64_t i = 0; i < v->length; i++) n(vec_at(v, i), args);
}

vec *vec_map(vec *v, _unary u, void *args) {
  init_asserts(v);
  void *mapped_el = stpusha(v->allocator, v->__el_size);
  for (int64_t i = 0; i < v->length; i++) {
    void *el = vec_at(v, i);
    u(mapped_el, el, args);
    memmove(el, mapped_el, v->__el_size);
  }
  stpopa(v->allocator);
  return v;
}

void *vec_foldl(vec *v, _binary b, void *result, void *args) {
  init_asserts(v);
  for (int64_t i = 0; i < v->length; i++) {
    b(result, result, vec_at(v, i), args);
  }
  return result;
}

/*-------------------------------------------------------
 * Statics Below
 *-------------------------------------------------------*/
static void *vec_alloc(vec *v, int64_t bytes) {
  if (v->flags == TO_STACK)
    return stpusha(v->allocator, v->__size * v->__el_size);
  return halloc(v->allocator, v->__size * v->__el_size);
}
static vec *vec_construct(vec *v) {
  v->length = 0;
  v->__size = 1;
  v->__top = 0;
  return v;
}

static void init_asserts(vec *v) {
  assert(v);
  assert(v->elements);
}
static void bound_asserts(vec *v, int64_t i) {
  init_asserts(v);
  assert(i >= 0 && i < v->length);
}
static void *lookup_el(vec *v, int64_t pos) {
  return v->elements + pos * v->__el_size;
}
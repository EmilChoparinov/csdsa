#include "csdsa.h"
#include <stdio.h>

/*-------------------------------------------------------
 * Static Functions
 *-------------------------------------------------------*/
static void     init_asserts(map *m);
static int64_t  key_pos(map *m, void *key);
static int64_t  linear_search_open_pos(map *m, int64_t from);
static void     maintain_load_factor(map *m);
static void    *__get_key(map *m, void *el);
static void    *__get_value(map *m, void *el);
static int32_t *__get_state(map *m, void *el);

static void init_asserts(map *m) {
  assert(m);
  assert(m->allocator);
  assert(m->elements.elements);
}

static int64_t key_pos(map *m, void *key) {
  int64_t start_idx = hash_bytes(key, m->__key_size) % m->elements.length;

  /* Probe forward */
  for (int64_t idx = start_idx; idx < m->elements.length; idx++) {
    void *el = vec_at(&m->elements, idx);
    if (*__get_state(m, el) != m->in_use_id) continue;

    /* Check if keys match */
    kvpair kv = read_kvpair(m, el);
    if (memcmp(kv.key, key, m->__key_size) == 0) return idx;
  }

  /* Probe failed, start from zero and go until start_idx */
  for (int64_t idx = 0; idx < start_idx; idx++) {
    void *el = vec_at(&m->elements, idx);
    if (*__get_state(m, el) != m->in_use_id) continue;

    kvpair kv = read_kvpair(m, el);
    if (memcmp(kv.key, key, m->__key_size) == 0) return idx;
  }

  /* Search failed */
  return -1;
}

static int64_t linear_search_open_pos(map *m, int64_t from) {
  /* Linear probe forward */
  for (int64_t idx = from; idx < m->elements.length; idx++) {
    void *el = vec_at(&m->elements, idx);
    /* Check if slot free */
    if (*__get_state(m, el) != m->in_use_id) return idx;
  }

  /* Prove failed, start at zero and go to from */
  for (int64_t idx = 0; idx < from; idx++) {
    void *el = vec_at(&m->elements, idx);
    if (*__get_state(m, el) != m->in_use_id) return idx;
  }

  /* Map Full!!! */
  assert(false && "Map is full, this should never happen.");
  return -1;
}

static void maintain_load_factor(map *m) {
  float lf = (float)m->slots_in_use / m->__size;
  if (lf < MAP_LOAD_FACTOR) return;

  /* The cache invalidation counter persists across resizes */
  int32_t cache_counter = m->cache_counter;
  cache_counter++;

  /* If the load factor constraint is reached, create a map double the
     current size. */
  map new_map;
  __map_init(&new_map, m->__el_size, m->__key_size, m->allocator, m->flags,
             m->__size * 2);
  map_clear(&new_map); /* Reset map to be 2x bigger */

  /* Reinsertion step */
  for (int64_t i = 0; i < m->elements.length; i++) {
    void *el = vec_at(&m->elements, i);
    if (*__get_state(m, el) != m->in_use_id) continue;

    kvpair kv = read_kvpair(m, el);
    map_put(&new_map, kv.key, kv.value);
  }

  map_free(m);
  memmove(m, &new_map, sizeof(map));
  m->cache_counter = cache_counter;
}

static void *__get_key(map *m, void *el) {
  return el; /* Key is in first pos */
}
static void *__get_value(map *m, void *el) {
  return (char *)el + m->__key_size;
}
static int32_t *__get_state(map *m, void *el) {
  return (int32_t *)((char *)el + m->__key_size + m->__el_size);
}
/*-------------------------------------------------------
 * Map Implementation
 *-------------------------------------------------------*/

/* Container Operations */
map *__map_init(map *m, int64_t el_size, int64_t key_size, stalloc *alloc,
                int32_t flags, int64_t initial_size) {
  assert(key_size > 0);
  assert(el_size > 0);
  assert(alloc);
  m->slots_in_use = 0;
  m->flags = flags;
  m->allocator = alloc;
  m->in_use_id = 1;
  m->cache_counter = 0;

  m->__size = initial_size;
  m->__el_size = el_size;
  m->__key_size = key_size;

  /* We lay out the KV pair in memory as such. The kvpair struct is just
     smoke and mirrors. */
  assert(__vec_init(&m->elements, key_size + el_size + sizeof(m->in_use_id),
                    alloc, flags, initial_size) != NULL);
  vec_resize(&m->elements, m->__size);

  return m;
}
void map_free(map *m) {
  init_asserts(m);
  vec_free(&m->elements);
}
void map_clear(map *m) {
  init_asserts(m);
  m->in_use_id++;
  m->slots_in_use = 0;
  vec_clear(&m->elements);
  vec_resize(&m->elements, m->__size);
}

pred(select_in_use, int *, el, {
  map   *m = (map *)args;
  kvpair kv = read_kvpair(m, el);
  printf("el state in SEARCHL: %d\n", kv.state);
  return kv.state == m->in_use_id;
});
vec *map_to_vec(map *m, vec *out) {
  init_asserts(m);

  __vec_init(out, m->__el_size + m->__key_size + sizeof(m->in_use_id),
             m->allocator, m->flags, m->slots_in_use);
  vec_resize(out, m->slots_in_use);

  for (int64_t i = 0; i < m->elements.length; i++) {
    void *el = vec_at(&m->elements, i);
    if (*__get_state(m, el) != m->in_use_id) continue;
    vec_push(out, el);
  }

  return out;
}

map *map_copy(map *dest, map *src) {
  init_asserts(src);
  memmove(dest, src, sizeof(*src));
  vec_copy(&dest->elements, &src->elements);
  return dest;
}

int64_t map_load(map *m) {
  init_asserts(m);
  return m->slots_in_use;
}

/* Element Operations */
kvpair map_get(map *m, void *key) {
  init_asserts(m);
  assert(key);
  kvpair kv = {0};

  int64_t idx = key_pos(m, key);
  if (idx == -1) return kv;

  /* Load element and check element validity */
  void *el = vec_at(&m->elements, idx);

  if (*__get_state(m, el) != m->in_use_id) return kv;

  return read_kvpair(m, el);
}

void map_put(map *m, void *key, void *value) {
  init_asserts(m);
  assert(key);
  assert(value);
  maintain_load_factor(m);

  if (map_has(m, key)) map_del(m, key);

  int64_t idx = hash_bytes(key, m->__key_size) % m->elements.length;
  idx = linear_search_open_pos(m, idx);

  void *el = vec_at(&m->elements, idx);

  cbuff buff;
  buff_init(&buff, el);
  buff_push(&buff, key, m->__key_size);
  buff_push(&buff, value, m->__el_size);
  buff_push(&buff, &m->in_use_id, sizeof(m->in_use_id));

  m->slots_in_use++;
}
bool map_has(map *m, void *key) {
  init_asserts(m);
  assert(key);

  return map_get(m, key).key != NULL;
}

void map_del(map *m, void *key) {
  init_asserts(m);
  assert(key);

  kvpair kv = map_get(m, key);
  if (!kv.key) return;

  /* Hack! Since map_get's kvpair return contains the state as a copied value
     and we know that right after the value is the REAL state, so we edit
     the 8 bytes after value. */
  cbuff buff;
  buff_init(&buff, kv.value);
  buff_skip(&buff, m->__el_size);

  /* setting to 0 is explicit delete. setting to any value other than
     in_use_id is an implicit delete. */
  int32_t new_state = 0;
  buff_push(&buff, &new_state, sizeof(new_state));

  m->slots_in_use--;
}

kvpair read_kvpair(map *m, void *el) {
  kvpair kv = {0};
  kv.key = __get_key(m, el);
  kv.value = __get_value(m, el);
  kv.state = *__get_state(m, el);
  return kv;
}

/* Functional Operations */
int64_t map_count_if(map *m, _pred p, void *args) {
  init_asserts(m);

  int64_t counter = 0;
  for (int64_t i = 0; i < m->elements.length; i++) {
    void *el = vec_at(&m->elements, i);
    if (*__get_state(m, el) != m->in_use_id) continue;

    kvpair kv = read_kvpair(m, el);
    if (p(&kv, args)) counter++;
  }
  return counter;
}

void map_foreach(map *m, _each n, void *args) {
  init_asserts(m);
  for (int64_t i = 0; i < m->elements.length; i++) {
    void *el = vec_at(&m->elements, i);
    if (*__get_state(m, el) != m->in_use_id) continue;

    kvpair kv = read_kvpair(m, el);
    n(&kv, args);
  }
}
map *map_filter(map *m, _pred p, void *args) {
  init_asserts(m);
  map filter;
  __map_init(&filter, m->__el_size, m->__key_size, m->allocator, TO_STACK,
             m->__size);

  for (int64_t i = 0; i < m->elements.length; i++) {
    void *el = vec_at(&m->elements, i);
    if (*__get_state(m, el) == m->in_use_id) {

      kvpair kv = read_kvpair(m, el);
      if (p(&kv, args)) map_put(&filter, kv.key, kv.value);
    }
  }

  memmove(m, &filter, sizeof(map));
  return m;
}
kvpair map_find_one(map *m, _pred p, void *args) {
  init_asserts(m);
  kvpair kv = {0};
  for (int64_t i = 0; i < m->elements.length; i++) {
    void *el = vec_at(&m->elements, i);

    if (*__get_state(m, el) != m->in_use_id) continue;

    kv = read_kvpair(m, el);
    if (p(&kv, args)) return kv;
  }
  return kv;
}

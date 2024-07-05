
#include "csdsa.h"
#include "unity.h"
#include <stdbool.h>

typedef struct complex_struct cs;
struct complex_struct {
  int  x, y, z;
  bool active;
};

typedef struct id id;
struct id {
  int  uid;
  bool is_active;
} __attribute__((packed));

MAP_TYPEDEC(id_cs_map, id, cs);
typedef id_cs_map id_cs_map_t;

/*-------------------------------------------------------
 * REGISTER
 *-------------------------------------------------------*/

id_cs_map_t tmap;
stalloc    *allocator = NULL;

void setUp(void) {
  start_frame(allocator);
  id_cs_map_inita(&tmap, allocator, TO_STACK);
}
void tearDown(void) {
  id_cs_map_free(&tmap);
  end_frame(allocator);
}

void test_put_and_find(void);
void test_has(void);
void test_remove(void);
void test_push_loadfactor(void);
void test_duplicate_handling(void);
void test_foreach(void);
void test_count_if(void);
void test_filter(void);
void test_to_vec(void);
void test_map_copy(void);
void test_map_clear(void);

void tests(void) {
  FRAME(allocator, RUN_TEST(test_count_if));
  FRAME(allocator, RUN_TEST(test_put_and_find));
  FRAME(allocator, RUN_TEST(test_has));
  FRAME(allocator, RUN_TEST(test_remove));
  FRAME(allocator, RUN_TEST(test_push_loadfactor));
  FRAME(allocator, RUN_TEST(test_duplicate_handling));
  FRAME(allocator, RUN_TEST(test_foreach));
  FRAME(allocator, RUN_TEST(test_filter));
  FRAME(allocator, RUN_TEST(test_to_vec));
  FRAME(allocator, RUN_TEST(test_map_copy));
  FRAME(allocator, RUN_TEST(test_map_clear));
}

int main(void) {
  UNITY_BEGIN();

  allocator = stalloc_create(STALLOC_DEFAULT);

  GFRAME(allocator, tests());

  stalloc_free(allocator);
  UNITY_END();
}

/*-------------------------------------------------------
 * TESTS
 *-------------------------------------------------------*/
void test_put_and_find(void) {
  id key = {.is_active = true, .uid = 69};
  id fake_key = {.is_active = true, .uid = 99};
  cs value = {.x = 99};
  id_cs_map_put(&tmap, &key, &value);

  TEST_ASSERT_MESSAGE(
      memcmp(id_cs_map_get(&tmap, &key).value, &value, sizeof(value)) == 0,
      "Did not retrieve from map value currently put in.");

  TEST_ASSERT_MESSAGE(id_cs_map_get(&tmap, &fake_key).key == NULL,
                      "Key should not have returned");
}
void test_has(void) {
  id key = {.is_active = true, .uid = 69};
  id fake_key = {.is_active = true, .uid = 99};
  cs value = {.x = 99};
  id_cs_map_put(&tmap, &key, &value);

  TEST_ASSERT_MESSAGE(id_cs_map_has(&tmap, &key),
                      "Map should contain this keypair.");

  TEST_ASSERT_MESSAGE(!id_cs_map_has(&tmap, &fake_key),
                      "Map should NOT contain this keypair.");
}
void test_remove(void) {
  id key = {.is_active = true, .uid = 69};
  cs value = {.x = 99};
  id_cs_map_put(&tmap, &key, &value);

  TEST_ASSERT_MESSAGE(id_cs_map_has(&tmap, &key),
                      "Map should contain this keypair.");

  id_cs_map_del(&tmap, &key);
  TEST_ASSERT_MESSAGE(!id_cs_map_has(&tmap, &key),
                      "Map should NOT contain this keypair.");
}

void test_push_loadfactor(void) {
  for (int i = 0; i < 25; i++) {
    id_cs_map_put(&tmap, &(id){.is_active = true, .uid = i},
                  &(cs){.x = 1, .y = 2, .z = 3, .active = true});
    TEST_ASSERT(id_cs_map_has(&tmap, &(id){.is_active = true, .uid = i}));
  }
  printf("USED: %ld", 500 * (sizeof(id) + sizeof(cs) + sizeof(int32_t)));

  /* Check hash algo */
  for (int i = 0; i < 500; i++) {
    TEST_ASSERT(!id_cs_map_has(&tmap, &(id){.is_active = false, .uid = i}));
  }

  for (int i = 0; i < 500; i++) {
    id_cs_map_del(&tmap, &(id){.is_active = true, .uid = i});
    TEST_ASSERT(!id_cs_map_has(&tmap, &(id){.is_active = true, .uid = i}));
    if (i == 499) {
      id_cs_map_put(&tmap, &(id){.is_active = true, .uid = i},
                    &(cs){.x = 1, .y = 2, .z = 3, .active = true});
      TEST_ASSERT(id_cs_map_has(&tmap, &(id){.is_active = true, .uid = i}));
      id_cs_map_del(&tmap, &(id){.is_active = true, .uid = i});
      TEST_ASSERT(!id_cs_map_has(&tmap, &(id){.is_active = true, .uid = i}));
    }
  }

  TEST_ASSERT(id_cs_map_load(&tmap) == 0);
}

void test_duplicate_handling(void) {

  id_cs_map_put(&tmap, &(id){.is_active = true, .uid = 69},
                &(cs){.x = 1, .y = 2, .z = 3, .active = true});
  for (int i = 0; i < 500; i++) {
    id_cs_map_put(&tmap, &(id){.is_active = true, .uid = 69},
                  &(cs){.x = 1, .y = 2, .z = 3, .active = true});
    TEST_ASSERT(id_cs_map_load(&tmap) == 1);
  }

  id_cs_map_del(&tmap, &(id){.is_active = true, .uid = 69});
  for (int i = 0; i < 500; i++) {
    id_cs_map_del(&tmap, &(id){.is_active = true, .uid = 69});
    TEST_ASSERT(id_cs_map_load(&tmap) == 0);
  }
}

nullary(kvpair, item, require_active, {
  cs *value = item.value;
  TEST_ASSERT(value->active);
});

void test_foreach(void) {
  for (int i = 0; i < 500; i++) {
    id_cs_map_put(&tmap, &(id){.is_active = true, .uid = i},
                  &(cs){.x = 1, .y = 2, .z = 3, .active = true});
  }

  id_cs_map_foreach(&tmap, require_active, NULL);
}

pred(kvpair, item, is_active, {
  cs *value = item.value;
  return value->active;
});
void test_count_if(void) {
  for (int i = 0; i < 500; i++) {
    id_cs_map_put(&tmap, &(id){.is_active = true, .uid = i},
                  &(cs){.x = 1, .y = 2, .z = 3, .active = true});
  }

  TEST_ASSERT(id_cs_map_count_if(&tmap, is_active, NULL) == 500);
}

MAP_TYPEDEC(int_int_map, int, int);
typedef int_int_map int_int_map_t;

pred(kvpair, item, select_filter, {
  int *t = item.value;
  return *t == 1 || *t == 10;
});

void test_filter(void) {
  int_int_map_t f;
  int_int_map_sinit(&f);

  for (int i = 0; i < 500; i++) {
    int_int_map_put(&f, &i, &i);
  }

  // filter selects those where x = 1 and x = 10

  int_int_map_t *filter = int_int_map_filter(&f, select_filter, NULL);

  int i = 1;
  TEST_ASSERT(int_int_map_has(filter, &i));
  i = 10;
  TEST_ASSERT(int_int_map_has(filter, &i));
  i = 501;
  TEST_ASSERT(!int_int_map_has(filter, &i));

  TEST_ASSERT(int_int_map_load(filter) == 2);
}

void test_to_vec(void) {
  int_int_map_t f;
  int_int_map_sinit(&f);

  for (int i = 0; i < 500; i++) {
    int_int_map_put(&f, &i, &i);
  }

  vec elements;
  int_int_map_to_vec(&f, &elements);

  int sum = 0;
  for (int i = 0; i < 500; i++) {
    kvpair kv = read_kvpair(&f, vec_at(&elements, i));
    sum += *(int *)kv.value;
  }
  TEST_ASSERT(sum == 124750);
}

void test_map_copy(void) {
  int_int_map_t orig, copy;
  int_int_map_sinit(&orig);

  for (int i = 0; i < 50; i++) {
    int_int_map_put(&orig, &i, &i);
  }

  int_int_map_copy(&copy, &orig);

  /* Assert pointers of memory go to different segments */
  TEST_ASSERT(orig.elements.elements != copy.elements.elements);

  /* Assert maps are diverging  */
  int x = 51;
  int_int_map_put(&orig, &x, &x);

  TEST_ASSERT(orig.slots_in_use == copy.slots_in_use + 1);
}

void test_map_clear(void) {
  int_int_map_t orig;
  int_int_map_sinit(&orig);

  for (int i = 0; i < 50; i++) {
    int_int_map_put(&orig, &i, &i);
  }

  int_int_map_clear(&orig);

  for (int i = 0; i < 50; i++) {
    assert(!int_int_map_has(&orig, &i));
  }
}
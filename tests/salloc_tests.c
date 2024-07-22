#include "csdsa.h"
#include "unity.h"
#include <pthread.h>

#define ALLOCATED 0
#define FREE      1

#define GUARD_SIZE        4
#define HEADER_SIZE       (GUARD_SIZE)
#define FOOTER_SIZE       (GUARD_SIZE)
#define BLOCK_SIZE(guard) ((uint32_t)((guard) >> 4))

#define IS_FREE(guard)          (((guard) & 1) == 1)
#define MAKE_GUARD(size, state) (((size) << 4) | (state))

void setUp() {}
void tearDown() {}

VEC_TYPE_IMPL(int_vec, int);

static void function_using_heap(void) {
  int_vec v;
  int_vec_sinit(&v, 16);

  for (int i = 0; i < 24; i++) {
    int_vec_push(&v, &i);
  }

  TEST_ASSERT(v.length == 24);
  TEST_ASSERT(v.__size == 32);
}

static void *thread_alloc(void *arg) {
  stalloc *alloc = stalloc_create(128);

  GFRAME(alloc, function_using_heap());

  stalloc_free(alloc);
  return NULL;
}

void test_concurrent_globals(void) {

  // thread_alloc(NULL);

  size_t thread_cnt = 8;

  pthread_t threads[thread_cnt];

  for (int i = 0; i < thread_cnt; i++) {
    pthread_create(&threads[i], NULL, thread_alloc, NULL);
  }

  for (int i = 0; i < thread_cnt; i++) {
    pthread_join(threads[i], NULL);
  }
}

void test_block_bit_arithmatic(void) {

  uint32_t bytes = 16;

  uint32_t guard = MAKE_GUARD(bytes, FREE);

  TEST_ASSERT(BLOCK_SIZE(guard) == bytes);
  TEST_ASSERT(IS_FREE(guard));

  guard = MAKE_GUARD(bytes, ALLOCATED);

  TEST_ASSERT(BLOCK_SIZE(guard) == bytes);
  TEST_ASSERT(!IS_FREE(guard));
}

void stack_frame_tests(void) {
  stalloc *alloc = stalloc_create(128);
  start_frame(alloc);
  int *x = stpusha(alloc, sizeof(int));
  *x = 1;
  start_frame(alloc);
  int *a = stpusha(alloc, sizeof(int));
  *a = 2;
  stpop();
  stpusha(alloc, sizeof(int));
  end_frame(alloc);
  TEST_ASSERT(*x == 1);
  end_frame(alloc);
}

int main() {

  UNITY_BEGIN();

  RUN_TEST(test_block_bit_arithmatic);
  RUN_TEST(test_concurrent_globals);
  RUN_TEST(stack_frame_tests);

  UNITY_END();
}
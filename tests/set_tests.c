#include "csdsa.h"
#include "unity.h"

SET_TYPEDEC(int_set, int);

stalloc *allocator = NULL;
int_set  iset;

void setUp(void) {
  start_frame(allocator);
  int_set_inita(&iset, allocator, TO_STACK, STACK_DEFAULT_SIZE);
}
void tearDown(void) {
  end_frame(allocator);
  int_set_free(&iset);
}

void test_simple_put_has(void) {
  int i = 1;
  int_set_put(&iset, &i);
  i = 12;
  int_set_put(&iset, &i);

  TEST_ASSERT(int_set_length(&iset) == 2);

  i = 1;
  TEST_ASSERT(int_set_has(&iset, &i));

  i = 12;
  TEST_ASSERT(int_set_has(&iset, &i));

  int_set_del(&iset, &i);

  TEST_ASSERT(!int_set_has(&iset, &i));
}

void test_resize(void) {
  for (int i = 0; i < 500; i++) {
    int_set_put(&iset, &i);
  }

  TEST_ASSERT(set_length(&iset) == 500);

  for (int i = 0; i < 500; i++) {
    TEST_ASSERT(int_set_has(&iset, &i));
    int_set_del(&iset, &i);
    TEST_ASSERT(!int_set_has(&iset, &i));
  }

  TEST_ASSERT(set_length(&iset) == 0);
}

void tests(void) {
  FRAME(allocator, RUN_TEST(test_simple_put_has));
  FRAME(allocator, RUN_TEST(test_resize));
}

int main(void) {
  UNITY_BEGIN();

  allocator = stalloc_create(STALLOC_DEFAULT);
  GFRAME(allocator, tests());
  stalloc_free(allocator);

  UNITY_END();
}
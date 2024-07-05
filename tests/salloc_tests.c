#include "csdsa.h"
#include "unity.h"

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


void test_block_bit_arithmatic(void) {

  uint32_t bytes = 16;

  uint32_t guard = MAKE_GUARD(bytes, FREE);

  TEST_ASSERT(BLOCK_SIZE(guard) == bytes);
  TEST_ASSERT(IS_FREE(guard));

  guard = MAKE_GUARD(bytes, ALLOCATED);

  TEST_ASSERT(BLOCK_SIZE(guard) == bytes);
  TEST_ASSERT(!IS_FREE(guard));
}

int main() {

  UNITY_BEGIN();

  RUN_TEST(test_block_bit_arithmatic);

  UNITY_END();
}
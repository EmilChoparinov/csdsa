#include "csdsa.h"
#include "unity.h"

typedef struct group group;
struct group {
  int  x, y, z;
  bool active;
};

VEC_TYPEDEC(gvec, group);

gvec     vector;
stalloc *allocator = NULL;

void setUp(void) {
  start_frame(allocator);
  gvec_inita(&vector, allocator, TO_STACK);
}
void tearDown(void) {
  gvec_free(&vector);
  end_frame(allocator);
}

void push_pop_clear_256(void);
void count_5_circles(void);
void has_random_element(void);

void count_multiples_of_10_to_100(void);
void filter_multiples_of_10_and_sum(void);
void divide_all_even_numbers_by_2_under_100(void);
void add_one_to_odds_and_set_all_evens_to_active(void);

void sort(void);
void delete_by_idx(void);

void tests(void) {
  RUN_TEST(push_pop_clear_256);
  RUN_TEST(count_5_circles);
  RUN_TEST(has_random_element);

  RUN_TEST(count_multiples_of_10_to_100);
  RUN_TEST(filter_multiples_of_10_and_sum);
  RUN_TEST(divide_all_even_numbers_by_2_under_100);
  RUN_TEST(add_one_to_odds_and_set_all_evens_to_active);
  RUN_TEST(sort);
  RUN_TEST(delete_by_idx);
}

int main(void) {
  UNITY_BEGIN();

  allocator = stalloc_create(STALLOC_DEFAULT);
  GFRAME(allocator, tests());

  stalloc_free(allocator);

  UNITY_END();
}

void push_pop_clear_256(void) {
  for (int i = 0; i < 256; i++) {
    gvec_push(&vector, &(group){.x = i});
    TEST_ASSERT(gvec_top(&vector)->x == i);
  }

  for (int i = 255; i >= 0; i--) {
    TEST_ASSERT(gvec_top(&vector)->x == i);
    gvec_pop(&vector);
  }

  TEST_ASSERT(vector.length == 0);
  gvec_push(&vector, &(group){.y = 99});
  gvec_clear(&vector);
  gvec_push(&vector, &(group){.y = 999});
  TEST_ASSERT(vector.length == 1);
  TEST_ASSERT(gvec_at(&vector, 0)->y == 999);
}

void count_5_circles(void) {
  gvec_resize(&vector, 10);
  for (int circle_run = 1; circle_run <= 5; circle_run++) {
    for (int i = 0; i < 10; i++) {
      gvec_put(&vector, i, &(group){.x = circle_run * 10 + i});
      TEST_ASSERT(gvec_at(&vector, i)->x = circle_run * 10 + i);
    }
  }
}

pred(group, el, find_5, return el.x == 5);
pred(group, el, find_100, return el.x == 100);

void has_random_element(void) {
  for (int i = 0; i < 100; i++) {
    gvec_push(&vector, &(group){.x = i});
  }

  gvec copy;
  gvec_copy(gvec_sinit(&copy), &vector);

  gvec_filter(&vector, find_5, NULL);
  gvec_filter(&copy, find_100, NULL);

  TEST_ASSERT(vector.length == 1);
  TEST_ASSERT(copy.length == 0);

  gvec_free(&copy);
}

pred(group, el, is_mult_10, return el.x % 10 == 0;);
void count_multiples_of_10_to_100(void) {
  for (int i = 0; i < 100; i++) {
    gvec_push(&vector, &(group){.x = i, .y = i, .z = i, .active = false});
  }

  gvec_filter(&vector, is_mult_10, NULL);
  TEST_ASSERT(vector.length == 10);
}

// fold(int, group, residual, el, adder, return residual + el.x;);

int adder_bin(void *_res, void *_a, void *args) {
  int   residual = *(int *)_res;
  group el = *(group *)_a;
  return residual + el.x;
  ;
}
void adder(void *out, void *a, void *b, void *args) {
  int prop = adder_bin(a, b, args);
  memcpy(out, &prop, sizeof(int));
}
void filter_multiples_of_10_and_sum(void) {
  for (int i = 0; i < 21; i++) {
    gvec_push(&vector, &(group){.x = i, .y = i, .z = i, .active = false});
  }

  int sum = 0;
  sum = *(int *)gvec_foldl(gvec_filter(&vector, is_mult_10, NULL), adder, &sum,
                           NULL);
  TEST_ASSERT(sum == 30);
}

pred(group, el, is_mult_2, return el.x % 2 == 0);
unary(group, el, divide_2, if (el.x == 0) return el; el.x /= 2; return el;);

void divide_all_even_numbers_by_2_under_100(void) {
  gvec vector;
  gvec_sinit(&vector);
  for (int i = 0; i < 100; i++) {
    gvec_push(&vector, &(group){.x = i, .y = i, .z = i, .active = false});
  }

  gvec_map(gvec_filter(&vector, is_mult_2, NULL), divide_2, NULL);

  int indexer = 0;
  int counter = 0;
  for (int i = 0; i < 100; i++) {
    if (i % 2 == 0) {
      TEST_ASSERT(gvec_at(&vector, indexer)->x == i / 2);
      indexer++;
      counter++;
    }
  }
  TEST_ASSERT(vector.length == counter);
}

unary(group, el, to_odd_map, el.x = (el.x % 2 == 0) ? el.x : el.x + 1;
      return el;);
unary(group, el, activate, el.active = true; return el;);
pred(group, el, is_active, return el.active;);

void add_one_to_odds_and_set_all_evens_to_active(void) {
  for (int i = 0; i < 100; i++) {
    gvec_push(&vector, &(group){.x = i, .y = i, .z = i, .active = false});
  }

  gvec all_active;
  gvec_copy(gvec_sinit(&all_active), &vector);

  gvec_filter(gvec_map(gvec_map(&all_active, to_odd_map, NULL), activate, NULL),
              is_active, NULL);

  TEST_ASSERT(all_active.length == vector.length);
  gvec_free(&all_active);
}

compare(group, a, b, sort_asc, return a.x < b.x;);
void sort(void) {
  for (int i = 25; i >= 0; i--) {
    gvec_push(&vector,
              (void *)&(group){.x = i, .y = i, .z = i, .active = false});
  }

  gvec_sort(&vector, sort_asc, NULL);

  for (int i = 1; i < 25; i++) {
    group *last = vec_at(&vector, i - 1);
    group *cur = vec_at(&vector, i);
    TEST_ASSERT(last->x < cur->x);
  }
}

void delete_by_idx(void) {

  for (int i = 25; i >= 0; i--) {
    gvec_push(&vector,
              (void *)&(group){.x = i, .y = i, .z = i, .active = false});
  }

  int itr_count = 0;
  int original_length = vector.length;
  while (vector.length != 0) {
    gvec_delete_at(&vector, 0);
    itr_count++;
  }

  TEST_ASSERT(itr_count == original_length);
}
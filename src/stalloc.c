/*------------------------------------------------------------------------------
 * Block Format:
 *                     Header
 *             31..............3 2 1 0
 * Memory     +----------------+-+-+-+  a=0 : Allocated
 * write -+-->|Block Size      |0|0|a|  a=1 : Free
 * guards |   +----------------+-+-+-+
 *        |   |Allocated User        |
 *        |   |Memory                |
 *        |   |                      |
 *        |   |                      |
 *        |   +----------------------+
 *        |   |Padding               |
 *        |   |                      |
 *        |   +----------------+-+-+-+
 *        +-->|Block Size      |0|0|a|
 *            +----------------+-+-+-+
 *
 *
 *
 *                              +---- Heap division
 * Region:                      v     ptr
 * +-----+-----+-----+----------+------+----+-----+--+-----+
 * |Block|Block|Block|          |Block|     |Block|  |Block|
 * +-----+-----+-----+----------+------+----+-----+--+-----+
 * +------->         ^                             <-------+
 * Stack             |  Stack                           Heap
 * Allocator         +- ptr                        Allocator
 *----------------------------------------------------------------------------*/
#include "csdsa.h"

#define ALLOCATED 0
#define FREE      1

#define GUARD_SIZE        4
#define THREAD_ID_SIZE    4
#define HEADER_SIZE       (GUARD_SIZE + THREAD_ID_SIZE)
#define FOOTER_SIZE       (GUARD_SIZE)
#define BLOCK_SIZE(guard) ((uint32_t)((guard) >> 4))

#define IS_FREE(guard)          (((guard) & 1) == 1)
#define MAKE_GUARD(size, state) (((size) << 4) | (state))

/* Each thread has their own local id */
pthread_key_t alloc_ids;

typedef struct stack_frame stack_frame;
struct stack_frame {
  int64_t   stack_allocs; /* The count of allocations done on this frame */
  pthread_t key;          /* Key to what position the stack id is in */
};

typedef struct alloc alloc;
struct alloc {
  void            *region;
  atomic_uintptr_t stack_ptr;
  atomic_uintptr_t heap_div_ptr;
  int64_t          region_size;
  alloc           *next;
};

struct stalloc {
  atomic_uintptr_t *top;
  stack_frame      *frames;
  int64_t           allocator_count; /* The count in `allocators` */
};

/*-------------------------------------------------------
 * Implementation
 *-------------------------------------------------------*/
static void append_new_alloc(stalloc *allocs, int64_t region_size);
static void alloc_make(alloc *alloc, int64_t region_size);
static void attempt_alloc_merge(stalloc *allocs);

stalloc *stalloc_create(int64_t bytes) {
  stalloc *alloc = calloc(1, sizeof(*alloc));

  alloc->allocator_count = 0;
  append_new_alloc(alloc, bytes);

  return alloc;
}

void stalloc_free(stalloc *a) {
  alloc *cur = (alloc *)atomic_load(a->top);
  while (cur != NULL) {
    free(cur->region);
    void *temp = cur->next;
    free(cur);
    cur = temp;
  }
  free(a);
}

void *stpush(stalloc *a, int64_t bytes) {
  alloc *alloc_to_use = (alloc *)atomic_load(a->top);

  void *stack_ptr = (void *)atomic_load(&alloc_to_use->stack_ptr);
  void *heap_div_ptr = (void *)alloc_to_use->heap_div_ptr;

  if (stack_ptr + bytes > heap_div_ptr) {
    append_new_alloc(a, bytes);
    alloc_to_use = (alloc *)atomic_load(a->top);
  }

  /* TODO: make sure blocks are aligned! */
  /* The guard stores the TOTAL size of the block so the block can be skipped
     while doing linear reads. Therefore, header/footer are included. */
  uint32_t memory_guard =
      MAKE_GUARD(bytes + HEADER_SIZE + FOOTER_SIZE, ALLOCATED);

  buffapi buff;
  buff_init(&buff, &alloc_to_use->stack_ptr);
  buff_push(&buff, &memory_guard, GUARD_SIZE);
  // buff_skip(&buff, )
  buff_skip(&buff, bytes);
  buff_push(&buff, &memory_guard, sizeof(memory_guard));

  void *mem_to_return = stack_ptr + GUARD_SIZE;
  alloc_to_use->stack_ptr += bytes + GUARD_SIZE * 2; /* Skip both guards */

  /* zero out for the users convenience */
  memset(mem_to_return, 0, bytes);

  return mem_to_return;
}

void stpop(stalloc *a) {
  /* Cascade down from the top of the allocators and pop first found. */
  alloc *cur = (alloc *)atomic_load(a->top);
  while (cur != NULL) {
    if (cur->stack_ptr == cur->region) { /* Stack empty case. */
      cur = cur->next;
      continue;
    }

    buffapi buff;
    buff_init(&buff, cur->stack_ptr);
    buff_skip(&buff, -HEADER_SIZE); /* Load header behind the stack ptr */
    uint32_t size = BLOCK_SIZE(*(uint32_t *)buff_at(&buff));
    assert(!IS_FREE(*(uint32_t *)buff_at(&buff)));
    cur->stack_ptr -= size;
    attempt_alloc_merge(a);
    break;
  }
}

/*-------------------------------------------------------
 * Heap Allocation
 *-------------------------------------------------------*/
/* Note that the cache locality part of the allocator is not implemented, so
   random heap allocs are just interfaced with stdlib.  */
void *halloc(stalloc *alloc, int64_t bytes) { return calloc(1, bytes); }
void *hrealloc(stalloc *alloc, void *ptr, int64_t bytes) {
  return realloc(ptr, bytes);
}
void hfree(stalloc *alloc, void *ptr) { free(ptr); }

/*-------------------------------------------------------
 * Stack Frame Interface
 *-------------------------------------------------------*/
__stack_frame start_frame(stalloc *alloc) {
  return (__stack_frame){.stack_allocs = 0};
}

/*-------------------------------------------------------
 * Private Sections
 *-------------------------------------------------------*/
static void alloc_make(alloc *a, int64_t region_size) {
  assert(a);
  a->region = calloc(1, region_size);
  a->region_size = region_size;
  a->stack_ptr = a->region;
  a->heap_div_ptr = a->stack_ptr + region_size;
  a->next = NULL;
}

static void append_new_alloc(stalloc *a, int64_t region_size) {
  if (a->top == NULL) {
    a->top = calloc(1, sizeof(*a->top));
    a->allocator_count++;

    alloc_make(a->top, region_size);
    return;
  }

  /* Each next allocator must be at least double the region size of the last,
     and at least double the requested region size.  */
  // alloc *last_top = alloc->top;
  alloc *last_top = (alloc *)atomic_load(a->top);
  size_t next_size = last_top->region_size;
  next_size *= 2;
  while (next_size < region_size * 2) next_size *= 2;

  a->allocator_count++;
  a->top = calloc(1, sizeof(*a->top));
  alloc_make(a->top, next_size);

  /* CAS operation */
  ((alloc *)a->top)->next = last_top;
}

void attempt_alloc_merge(stalloc *allocs) {
  if (allocs->top == NULL) return;
  alloc *top, *toptop;
  top = allocs->top;
  toptop = top->next;

  /* If toptop is NULL we only have one allocator. */
  if (toptop == NULL) return;

  /* Merge requirement check */
  if (!(top->stack_ptr == top->region && toptop->stack_ptr == toptop->region))
    return;

  /* Prepare merge block */
  alloc *merged = calloc(1, top->region_size + toptop->region_size);
  alloc_make(merged, top->region_size + toptop->region_size);
  merged->next = toptop->next;

  /* CAS operation */
  allocs->top = merged;
}
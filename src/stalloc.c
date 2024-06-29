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

#define GUARD_SIZE              4
#define BLOCK_SIZE(guard)       ((uint32_t)((guard) >> 4))
#define IS_FREE(guard)          (((guard) & 1) == 1)
#define MAKE_GUARD(size, state) (((size) << 4) | (state))

typedef struct allocator allocator;
struct allocator {
  void   *region;
  void   *stack_ptr;
  void   *heap_div_ptr;
  int64_t region_size;
  int64_t total_allocations;
};

struct stalloc {
  allocator *allocators;      /* The allocators being managed */
  int64_t    allocator_count; /* The count in `allocators` */
};

/*-------------------------------------------------------
 * Implementation
 *-------------------------------------------------------*/
static void append_new_alloc(stalloc *allocs, int64_t region_size);
static void alloc_make(allocator *alloc, int64_t region_size);

stalloc *stalloc_create(int64_t bytes) {
  stalloc *alloc = calloc(1, sizeof(*alloc));

  alloc->allocator_count = 0;
  append_new_alloc(alloc, bytes);

  return alloc;
}

void stalloc_free(stalloc *alloc) {
  for (int64_t i = 0; i < alloc->allocator_count; i++) {
    free(alloc->allocators[i].region);
  }

  free(alloc->allocators);
  free(alloc);
}

void *stpush(stalloc *alloc, int64_t bytes) {
  allocator *alloc_to_use = &alloc->allocators[alloc->allocator_count - 1];

  if (alloc_to_use->stack_ptr + bytes > alloc_to_use->heap_div_ptr) {
    append_new_alloc(alloc, bytes);
    alloc_to_use = &alloc->allocators[alloc->allocator_count - 1];
  }

  /* TODO: make sure blocks are aligned! */
  /* The guard stores the TOTAL size of the block so the block can be skipped
     while doing linear reads. Therefore, both guards are included. */
  uint32_t memory_guard = MAKE_GUARD(bytes + GUARD_SIZE * 2, ALLOCATED);

  buffapi buff;
  buff_init(&buff, alloc_to_use->stack_ptr);
  buff_push(&buff, &memory_guard, sizeof(uint32_t));
  buff_skip(&buff, bytes);
  buff_push(&buff, &memory_guard, sizeof(memory_guard));

  void *mem_to_return = alloc_to_use->stack_ptr + GUARD_SIZE;
  alloc_to_use->stack_ptr += bytes + GUARD_SIZE * 2; /* Skip both guards */
  alloc_to_use->total_allocations++;

  /* zero out for the users convenience */
  memset(mem_to_return, 0, bytes);

  return mem_to_return;
}

void stpop(stalloc *allocs) {
  /* Cascade down from the top of the allocators and pop first found. */
  for (int64_t i = allocs->allocator_count - 1; i >= 0; i--) {
    allocator *alloc = &allocs->allocators[i];
    if (alloc->stack_ptr == alloc->region) continue; /* Stack empty */

    buffapi buff;
    buff_init(&buff, alloc->stack_ptr);
    buff_skip(&buff, -GUARD_SIZE); /* Load guard behind the stack ptr*/
    uint32_t size = BLOCK_SIZE(*(uint32_t *)buff_at(&buff));
    assert(!IS_FREE(*(uint32_t *)buff_at(&buff)));
    alloc->stack_ptr -= size;
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
 * Private Sections
 *-------------------------------------------------------*/
static void alloc_make(allocator *alloc, int64_t region_size) {
  assert(alloc);
  alloc->region = calloc(1, region_size);
  alloc->region_size = region_size;
  alloc->stack_ptr = alloc->region;
  alloc->heap_div_ptr = alloc->stack_ptr + region_size;
  alloc->total_allocations = 0;
}

static void append_new_alloc(stalloc *alloc, int64_t region_size) {
  if (alloc->allocators == NULL) {
    alloc->allocators = calloc(1, sizeof(*alloc->allocators));
    alloc->allocator_count++;

    alloc_make(alloc->allocators, region_size);
    return;
  }

  /* Each next allocator must be at least double the region size of the last,
     and at least double the requested region size.  */
  size_t next_size = alloc->allocators[alloc->allocator_count - 1].region_size;
  next_size *= 2;
  while (next_size < region_size * 2) next_size *= 2;

  alloc->allocator_count++;
  alloc->allocators =
      realloc(alloc->allocators, sizeof(allocator) * alloc->allocator_count);
  allocator *new_allocator = &alloc->allocators[alloc->allocator_count - 1];
  alloc_make(new_allocator, next_size);
}
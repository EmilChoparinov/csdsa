/*------------------------------------------------------------------------------
 * Concurrent Stack Allocator memory layout strategy.
 *                     Header
 *             31..............3 2 1 0
 * Memory     +----------------+-+-+-+  a=0 : Allocated
 * write -+-->|Block Size      |0|0|a|  a=1 : Free
 * guards |   +----------------+-+-+-+
 *        |   |Thead ID              |
 *        |   +----------------------+
 *        |   |Allocated User        |
 *        |   |Memory                |
 *        |   |                      |
 *        |   +----------------------+    Always 0 for
 *        |   |Alignment Padding     |    protection
 *        |   +----------------+-+-+-+         |
 *        +-->|Block Size      |0|0|a|         |
 *            +----------------+-+-+-+         |
 *                               ^             |
 *                               +-------------+
 * Region:
 * +-----+-----+-----+--------------------------------+
 * |Block|Block|Block|                                |
 * +-----+-----+-----+--------------------------------+
 * +------->         ^                                ^
 * Stack             |  Atomic               Heap Div |
 * Allocator         +- Stack Ptr            Ptr    --+
 * ---------------------------------------------------------------------------*/

#include "csdsa.h"
#include <execinfo.h>

#define ALLOCATED 0
#define FREE      1

#define GUARD_SIZE        4
#define HEADER_SIZE       (GUARD_SIZE)
#define FOOTER_SIZE       (GUARD_SIZE)
#define BLOCK_SIZE(guard) ((uint32_t)((guard) >> 4))

#define IS_FREE(guard)          (((guard) & 1) == 1)
#define MAKE_GUARD(size, state) (((size) << 4) | (state))

typedef struct stack_frame stack_frame;
struct stack_frame {
  int64_t stack_allocs; /* The count of allocations done on this frame */
};

typedef struct alloc alloc;
struct alloc {
  void   *region;
  void   *stack_ptr;
  void   *heap_div_ptr;
  int64_t region_size;
  alloc  *next;
};

struct stalloc {
  alloc       *top;
  stack_frame *frames;
  int64_t      frame_count;
  int64_t      __frame_arr_len;
  int64_t      allocator_count; /* The count in `allocators` */
};

/*-------------------------------------------------------
 * Implementation
 *-------------------------------------------------------*/
static void append_new_alloc(stalloc *allocs, int64_t region_size);
static void alloc_make(alloc *alloc, int64_t region_size);
static void attempt_alloc_merge(stalloc *allocs);
static void _stpop(stalloc *a, int64_t to_pop);

stalloc *framed_alloc = NULL;

stalloc *stalloc_create(int64_t bytes) {
  stalloc *alloc = calloc(1, sizeof(*alloc));
  append_new_alloc(alloc, bytes);

  alloc->allocator_count = 0;

  alloc->frames = NULL;
  alloc->frame_count = 0;
  alloc->__frame_arr_len = 0;

  return alloc;
}

void stalloc_free(stalloc *a) {
  alloc *cur = a->top;
  while (cur != NULL) {
    free(cur->region);
    void *temp = cur->next;
    free(cur);
    cur = temp;
  }
  free(a->frames);
  free(a);
}

void *__stpush(stalloc *a, int64_t bytes) {

  alloc  *alloc_to_use = a->top;
  int64_t block_size = bytes;
  
  // TODO: implement stack alignment!
  // addr = (addr + (8 - 1)) & -8;
  // /* Ensure the stack pointer is aligned */
  // uintptr_t aligned_stack_ptr = (uintptr_t)alloc_to_use->stack_ptr;
  // if (aligned_stack_ptr % 8 != 0) {
  //   aligned_stack_ptr = (aligned_stack_ptr + 7) & ~((uintptr_t)7);
  // }

  // /* Add the difference to the user block as padding */
  // block_size += aligned_stack_ptr - (uintptr_t)alloc_to_use->stack_ptr;

  if (alloc_to_use->stack_ptr + block_size + HEADER_SIZE + FOOTER_SIZE >
      alloc_to_use->heap_div_ptr) {
    append_new_alloc(a, bytes);
    alloc_to_use = a->top;
  }

  /* The guard stores the TOTAL size of the block so the block can be skipped
   while doing linear reads. Therefore, header/footer are included. */
  uint32_t memory_guard =
      MAKE_GUARD(block_size + HEADER_SIZE + FOOTER_SIZE, ALLOCATED);

  /* Write the header guard */
  memcpy(alloc_to_use->stack_ptr, &memory_guard, HEADER_SIZE);
  void *mem_to_return = alloc_to_use->stack_ptr + HEADER_SIZE;

  /* Zero out the user memory area for convenience */
  memset(mem_to_return, 0, bytes);

  /* Write the footer guard */
  memcpy(alloc_to_use->stack_ptr + HEADER_SIZE + bytes, &memory_guard,
         FOOTER_SIZE);

  /* Move the stack pointer past the allocated block */
  alloc_to_use->stack_ptr += HEADER_SIZE + bytes + FOOTER_SIZE;

  /* Add 1 to the top frame */
  a->frames[a->frame_count - 1].stack_allocs++;

  return mem_to_return;
}

void *__stpushframe(int64_t bytes) { return __stpush(framed_alloc, bytes); }

void __stpop(stalloc *a) {
  _stpop(a, 1);
  a->frames[a->frame_count - 1].stack_allocs--;
}

void __stpopframe(void) {
  _stpop(framed_alloc,
         framed_alloc->frames[framed_alloc->frame_count - 1].stack_allocs);
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
void start_frame(stalloc *alloc) {
  if (alloc->__frame_arr_len == 0) {
    alloc->frame_count++;
    alloc->__frame_arr_len = 1;
    alloc->frames =
        calloc(STACK_FRAME_GUESS, alloc->__frame_arr_len * sizeof(stack_frame));
  }

  alloc->frame_count++;
  int64_t new_len = alloc->__frame_arr_len;
  while (new_len < alloc->frame_count) {
    new_len *= 2;
  }

  /* No resizing needed */
  if (new_len == alloc->__frame_arr_len) return;

  alloc->__frame_arr_len = new_len;
  alloc->frames = recalloc(alloc->frames, new_len * sizeof(stack_frame));
}

void end_frame(stalloc *alloc) {
  assert(framed_alloc != NULL);
  assert(alloc->frame_count > 0);
  _stpop(alloc, alloc->frames[alloc->frame_count - 1].stack_allocs);
  alloc->frame_count--;
}

/*-------------------------------------------------------
 * Private Sections
 *-------------------------------------------------------*/
static void alloc_make(alloc *a, int64_t region_size) {
  assert(a);
  assert(region_size > 0);
  a->region = calloc(1, region_size + HEADER_SIZE + FOOTER_SIZE);
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
  alloc *last_top = a->top;
  size_t next_size = last_top->region_size;
  next_size *= 2;
  while (next_size < region_size * 2) next_size *= 2;

  a->allocator_count++;

  a->top = calloc(1, sizeof(*a->top));
  alloc_make(a->top, next_size);

  a->top->next = last_top;
}

static void _stpop(stalloc *a, int64_t to_pop) {
  /* Cascade down from the top of the allocators and pop first found. */
  alloc  *cur = a->top;
  int64_t popped_so_far = 0;
  while (cur != NULL && popped_so_far < to_pop) {
    if (cur->stack_ptr == cur->region) { /* Stack empty case. */
      cur = cur->next;
      continue;
    }

    cbuff buff;
    buff_init(&buff, cur->stack_ptr);
    buff_skip(&buff, -HEADER_SIZE); /* Load header behind the stack ptr */
    uint32_t size = BLOCK_SIZE(*(uint32_t *)buff_at(&buff));
    assert(!IS_FREE(*(uint32_t *)buff_at(&buff)));
    cur->stack_ptr -= size;
    attempt_alloc_merge(a);
    cur = a->top;
    popped_so_far++;
  }
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

  free(top->region);
  free(toptop->region);
  free(top);
  free(toptop);
}
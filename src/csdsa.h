/* =========================================================================
    Author: E.D Choparinov, Amsterdam
    Related Files: csdsa.h
    Created On: June 28 2024
    Purpose:
        The purpose of this library is to provide various data structures
        and algorithms for solving general computing tasks in C. The library
        leans towards being as functional as possible, so most calls to this
        library require passing in functions.

        This library also features an allocator that provides stack or random
        heap allocation modes. The user can specify either directly which
        allocator instance to use or to use a globally existing allocator.

    The following are the contents and descriptions of each part of this
    library:

    === Allocator ===
    The allocator built into this library is called stalloc (as in for stack
    allocator) and supports stack allocation and random heap allocation. The
    allocator utilizes the same contiguous memory segment for both heap and
    stack allocation for best performance.

    Only random heap allocations may use the libraries free function. Stack
    context allocations use pop() or the built in global stack pop function.

    === Data Structures ===
    vec_*   : Wrapper for a contiguous segment of memory. Can be
                      used to represent a fixed or resizable array.

    map_*   : Open address hashing table with resize capabilities.

    set_*   : Implemented as a resizeable sparse set wrapped with
                      the libraries hashing function.

    stack_* : Wrapper over vec_* and constrains the vector.

    queue_* : Wrapper over vec_* again and constrains the vector.

    graph_* : Implemented as a map of sets. Each vertex stores a pointer to
              some address you define.

    === Algorithms ===
    hash_bytes() : Modified version of djb2 to consume N bytes. Originally from:
                   http://www.cse.yorku.ca/~oz/hash.html

    bfs()        : Breadth first search over a graph
    dfs()        : Depth first search over a graph
    a_star()     : The classic a* graph search algorithm.

========================================================================= */
#ifndef __HEADER_CSDSA_H__
#define __HEADER_CSDSA_H__

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------
 * libcsdsa Configurations
 *-------------------------------------------------------*/
// #define DISABLE_ALLOCATOR

/*-------------------------------------------------------
 * Type Definitions
 *-------------------------------------------------------*/
typedef struct stalloc stalloc;
typedef struct buffapi buffapi;

/*-------------------------------------------------------
 * Stack Allocator stalloc
 *-------------------------------------------------------*/
#define STALLOC_DEFAULT 1 /* The default size to pass to stalloc_create */
typedef struct __stack_frame __stack_frame;
struct __stack_frame {
  int64_t   stack_allocs; /* The count of allocations done on this frame */
  pthread_t allocatee;    /* The thread that owns this allocation */
};
#define FRAME(alloc, call)                                                     \
  __stack_frame __csdsa_frame_ = start_frame(alloc);                           \
  call;                                                                        \
  end_frame(alloc);

__stack_frame start_frame(stalloc *alloc);
void          end_frame(stalloc *alloc);

stalloc *stalloc_create(int64_t bytes);
void     stalloc_free(stalloc *alloc);

/* Stack allocation */
void *stpush(stalloc *alloc, int64_t bytes);
void  stpop(stalloc *alloc);

/* Heap allocation */
void *halloc(stalloc *alloc, int64_t bytes);
void *hrealloc(stalloc *alloc, void *ptr, int64_t bytes);
void  hfree(stalloc *alloc, void *ptr);

/*-------------------------------------------------------
 * Buffer API
 *-------------------------------------------------------*/
struct buffapi {
  void   *region; /* Pointer to base memory. */
  void   *top;    /* Pointer to the end of base memory operations. */
  int64_t len;    /* Length of objects pushed/pop'd. */
};

/* Container Operations */
buffapi *buff_init(buffapi *b, void *region);
buffapi *buff_hinit(void *region);
void     buff_hfree(buffapi *b);

/* Element Operations */
void  buff_push(buffapi *b, void *item, size_t item_size);
void *buff_pop(buffapi *b, size_t item_size);
void *buff_at(buffapi *b);
void *buff_skip(buffapi *b, size_t to_skip);

#endif
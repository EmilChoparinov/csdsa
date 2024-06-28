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

#include <stdint.h>

/*-------------------------------------------------------
 * libcsdsa Configurations
 *-------------------------------------------------------*/
// #define DISABLE_ALLOCATOR

/*-------------------------------------------------------
 * Type Definitions
 *-------------------------------------------------------*/
typedef struct stallocator stallocator;

/*-------------------------------------------------------
 * Stack Allocator stalloc
 *-------------------------------------------------------*/
#define STALLOC_DEFAULT 1024 /* The default size to pass to stalloc_create */

stallocator *stalloc_create(int64_t bytes);
void         stalloc_free(stallocator *alloc);

/* Stack allocation */
void *stalloc(stallocator *alloc, int64_t bytes);
void *stcalloc(stallocator *alloc, int64_t times, int64_t bytes);

/* Heap allocation */
void *sthalloc(stallocator *alloc, int64_t bytes);

/* Works with both stack and heap allocated pointers */
void *strealloc(stallocator *alloc, void *ptr, int64_t bytes);

#endif
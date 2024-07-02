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

    buff_*  : Buffer API implementation that takes a region of memory and allows
              for various manipulations

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
// #include <pthread.h>
// #include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------
 * libcsdsa Configurations
 *-------------------------------------------------------*/
// #define DISABLE_ALLOCATOR

#define TO_STACK 0
#define TO_HEAP  1

/*-------------------------------------------------------
 * Type Definitions
 *-------------------------------------------------------*/
typedef struct stalloc stalloc;
typedef struct cbuff   cbuff;

/* Typedefs for a functional programming style. */
typedef void (*_nullary)(void *el, void *args);
typedef bool (*_pred)(void *el, void *args);
typedef void (*_unary)(void *out, void *el, void *args);
typedef void (*_binary)(void *out, void *a, void *b, void *args);
typedef bool (*_compare)(void *a, void *b, void *args);

#define nullary(ty, prop, name, body)                                          \
  void name(void *_el, void *args) {                                           \
    ty prop = *(ty *)_el;                                                      \
    body                                                                       \
  }

#define pred(ty, prop, name, body)                                             \
  bool name(void *_el, void *args) {                                           \
    ty prop = *(ty *)_el;                                                      \
    body;                                                                      \
  }

#define unary(ty, prop, name, body)                                            \
  ty name##_un(void *_el, void *args) {                                        \
    ty prop = *(ty *)_el;                                                      \
    body;                                                                      \
  }                                                                            \
  void name(void *out, void *_el, void *args) {                                 \
    ty prop = name##_un(_el, args);                                             \
    memcpy(out, &prop, sizeof(ty));                                            \
  }

#define binary(ty, prop_a, prop_b, name, body)                                 \
  ty name##_bin(void *a, void *b, void *args) {                                \
    ty prop_a = *(ty *)_a;                                                     \
    ty prop_b = *(ty *)_b;                                                     \
    body;                                                                      \
  }                                                                            \
  void name(void *out, void *a, void *b, void *args) {                         \
    ty prop = name##_bin(a, b, args);                                          \
    memcpy(out, &prop, sizeof(ty));                                            \
  }

#define fold(ret_ty, ty, result_prop, prop_a, name, body)                      \
  ret_ty name##_bin(void *_res, void *_a, void *args) {                        \
    ret_ty result_prop = *(ret_ty *)_res;                                      \
    ty     prop_a = *(ty *)_a;                                                 \
    body;                                                                      \
  }                                                                            \
  void name(void *out, void *a, void *b, void *args) {                         \
    ret_ty prop = name##_bin(a, b, args);                                      \
    memcpy(out, &prop, sizeof(ret_ty));                                        \
  }

#define compare(ty, prop_a, prop_b, name, body)                                \
  bool name(void *_a, void *_b, void *args) {                                  \
    ty prop_a = *(ty *)_a;                                                     \
    ty prop_b = *(ty *)_b;                                                     \
    body;                                                                      \
  }

/* =========================================================================
   Section: Allocator
========================================================================= */
#define STALLOC_DEFAULT 1024 /* The default size to pass to stalloc_create */

extern stalloc *framed_alloc;

#define GFRAME(alloc, call)                                                    \
  framed_alloc = alloc;                                                        \
  start_frame(alloc);                                                          \
  call;                                                                        \
  end_frame(alloc);

#define FRAME(alloc, call)                                                     \
  start_frame(alloc);                                                          \
  call;                                                                        \
  end_frame(alloc);

void start_frame(stalloc *alloc);
void end_frame(stalloc *alloc);

stalloc *stalloc_create(int64_t bytes);
void     stalloc_free(stalloc *alloc);

/* Stack allocation */
#define stpusha(alloc, bytes) __stpush(alloc, bytes)
#define stpush(bytes)         __stpushframe(bytes)

void *__stpush(stalloc *alloc, int64_t bytes);
void *__stpushframe(int64_t bytes);

#define stpopa(alloc) __stpop(alloc)
#define stpop()       __stpopframe()
void __stpop(stalloc *alloc);
void __stpopframe(void);

/* Heap allocation */
void *halloc(stalloc *alloc, int64_t bytes);
void *hrealloc(stalloc *alloc, void *ptr, int64_t bytes);
void  hfree(stalloc *alloc, void *ptr);

/* =========================================================================
   Section: Data Structures
========================================================================= */

/*-------------------------------------------------------
 * Vector (vec_*)
 *-------------------------------------------------------*/
typedef struct vec vec;
struct vec {
  int64_t  __top, __size, __el_size;
  int64_t  length;
  int32_t  flags;
  stalloc *allocator;
  void    *elements;
};

/* Container operations */
vec *__vec_init(vec *v, int64_t el_size, stalloc *alloc, int32_t flags);
void vec_free(vec *v);
void vec_resize(vec *v, int64_t size);
void vec_copy(vec *dest, vec *src);
void vec_clear(vec *v);

/* Element operations */
void   *vec_at(vec *v, int64_t pos);
void    vec_delete_at(vec *v, int64_t pos);
void    vec_put(vec *v, int64_t pos, void *el);
bool    vec_has(vec *v, void *el);
int64_t vec_find(vec *v, void *el);
void    vec_push(vec *v, void *el);
void    vec_pop(vec *v);
void   *vec_top(vec *v);
void    vec_swap(vec *v, int64_t idx1, int64_t idx2);

/* Functional operations */
void    vec_sort(vec *v, _compare cmp, void *args);
int64_t vec_count_if(vec *v, _pred p, void *args);
vec    *vec_filter(vec *v, _pred p, void *args);
void    vec_foreach(vec *v, _nullary n, void *args);
vec    *vec_map(vec *v, _unary u, void *args);
void   *vec_foldl(vec *v, _binary b, void *result, void *args);

#define VEC_TYPEDEC(cn, ty)                                                      \
  typedef vec cn;                                                                \
                                                                                 \
  cn *cn##_sinit(cn *v) {                                                        \
    return __vec_init((vec *)v, sizeof(ty), framed_alloc, TO_STACK);             \
  }                                                                              \
  cn *cn##_hinit(cn *v) {                                                        \
    return __vec_init((vec *)v, sizeof(ty), framed_alloc, TO_HEAP);              \
  }                                                                              \
  cn *cn##_inita(cn *v, stalloc *alloc, int8_t flags) {                          \
    return __vec_init((vec *)v, sizeof(ty), alloc, flags);                       \
  }                                                                              \
  void cn##_free(cn *v) { vec_free((vec *)v); }                                  \
  void cn##_resize(cn *v, int64_t size) { vec_resize((vec *)v, size); }          \
  void cn##_copy(cn *dest, cn *src) { vec_copy((vec *)dest, (vec *)src); }       \
                                                                                 \
  void cn##_clear(cn *v) { vec_clear((vec *)v); }                                \
                                                                                 \
  ty     *cn##_at(cn *v, int64_t pos) { return vec_at((vec *)v, pos); }          \
  void    cn##_put(cn *v, int64_t pos, void *el) { vec_put((vec *)v, pos, el); } \
  void    cn##_delete_at(cn *v, int64_t pos) { vec_delete_at((vec *)v, pos); }   \
  bool    cn##_has(cn *v, ty *el) { return vec_has((vec *)v, el); }              \
  int64_t cn##_find(cn *v, ty *el) { return vec_find((vec *)v, el); }            \
  void    cn##_push(cn *v, ty *el) { vec_push((vec *)v, el); }                   \
  void    cn##_pop(cn *v) { vec_pop((vec *)v); }                                 \
  ty     *cn##_top(cn *v) { return vec_top((vec *)v); }                          \
  void    cn##_swap(cn *v, int64_t idx1, int64_t idx2) {                         \
    vec_swap((vec *)v, idx1, idx2);                                           \
  }                                                                              \
                                                                                 \
  void cn##_sort(cn *v, _compare cmp, void *args) {                              \
    vec_sort((vec *)v, cmp, args);                                               \
  }                                                                              \
  int64_t cn##_count_if(cn *v, _pred p, void *args) {                            \
    return vec_count_if((vec *)v, p, args);                                      \
  }                                                                              \
  cn *cn##_filter(cn *v, _pred p, void *args) {                                  \
    return vec_filter((vec *)v, p, args);                                        \
  }                                                                              \
  void cn##_foreach(cn *v, _nullary n, void *args) {                             \
    vec_foreach((vec *)v, n, args);                                              \
  }                                                                              \
  cn *cn##_map(cn *v, _unary u, void *args) {                                    \
    return vec_map((vec *)v, u, args);                                           \
  }                                                                              \
  void *cn##_foldl(cn *v, _binary b, void *result, void *args) {                 \
    return vec_foldl((vec *)v, b, result, args);                                 \
  }

/*-------------------------------------------------------
 * Buffer API
 *-------------------------------------------------------*/
struct cbuff {
  void   *region; /* Pointer to base memory. */
  void   *top;    /* Pointer to the end of base memory operations. */
  int64_t len;    /* Length of objects pushed/pop'd. */
};

/* Container Operations */
cbuff *buff_init(cbuff *b, void *region);
cbuff *buff_hinit(void *region);
void   buff_hfree(cbuff *b);

/* Element Operations */
void  buff_push(cbuff *b, void *item, size_t item_size);
void *buff_pop(cbuff *b, size_t item_size);
void *buff_at(cbuff *b);
void *buff_skip(cbuff *b, size_t to_skip);

/*-------------------------------------------------------
 * Utilities
 *-------------------------------------------------------*/
void  memswap(void *a, void *b, size_t size);
void *recalloc(void *a, size_t size);

#endif
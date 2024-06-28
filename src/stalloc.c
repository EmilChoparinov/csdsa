#include "csdsa.h"

typedef struct chunk_metadata chunk_metadata;
struct chunk_metadata {
  int16_t header;
};

typedef struct allocator allocator;
struct allocator {
  void *region;
  void *stack_ptr;
};

struct stallocator {
  allocator *allocators;      /* The allocators being managed */
  int64_t    allocator_count; /* The count in `allocators` */
};
#include "csdsa.h"
#include <stdio.h>

int main() {
    unsigned int num = 0xFFFFFFF9;

    printf("Header: 0x%08X\n", num);
    printf("Block Size: 0x%06X\n", ((uint32_t)(num >> 4)));
    printf("Block Free: %d\n", ((num & 1) == 1));

    stalloc *alloc = stalloc_create(STALLOC_DEFAULT);

    void *x = stpush(alloc, 2000);
    stpush(alloc, 200);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    stpop(alloc);
    printf("%p\n", x);

    stalloc_free(alloc);

    return 0;
}
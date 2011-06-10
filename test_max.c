#include <stdio.h>
#include "proposed-api.h"


/*
 *  Expected output (64bit):
-------------------
get_all(1):	0x6027a0
   	0x6027c8
   	6424
get_all(1):	0x6027a0
   	0x6027c8
   	3232
get_all(1):	0x6027a0
   	0x6027c8
   	3232
dont freeze(2):	0x603440
   	0x603470
   	152
0x6027a0: val:  404 (get_all)
0x603440: val:   19 (dont freeze)
-------------------
struct buflib_callbacks ops;
int main(void)
{
    buflib_core_init();
    size_t size;
    int handle = core_alloc_maximum("get_all", &size, &ops);

    if (handle <= 0)
        printf("core_alloc_maximum error\n");
    int handle2;

    core_print_allocs();

    /* this should freeze */
    // core_alloc("freeze", 100);
    
    core_shrink(handle, core_get_data(handle), size/2);

    core_print_allocs();

    /* this should not freeze anymore */
    handle2 = core_alloc("dont freeze", 100);
    if (handle2 <= 0)
        printf("handle 2 failed!\n");

    core_print_allocs();
    core_print_blocks();

    core_free(handle);
    core_free(handle2);
    return 0;
}
    

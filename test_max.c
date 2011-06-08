#include <stdio.h>
#include "proposed-api.h"

struct buflib_callbacks ops;
int main(void)
{
    buflib_core_init();
    size_t size;
    int handle = core_alloc_maximum("get_all", &size, &ops);
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

    core_free(handle);
    core_free(handle2);
    return 0;
}
    

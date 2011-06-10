#include <stdio.h>
#include "proposed-api.h"

/*
 * Expected output:
-------------------
0x602520: val: 1285 (first)
0x604d48: val:  -32 (<unallocated>)
0x604e48: val: 1221 (second)
0x607470: val:  -32 (<unallocated>)
0x607570: val: 1285 (third)
-------------------
*/
struct buflib_callbacks ops;

int main(void)
{
    buflib_core_init();

    int first  = core_alloc("first", 10<<10);
    int second = core_alloc("second", 10<<10);
    int third  = core_alloc("third", 10<<10);

    strcpy((char*)core_get_data(second)+0x100, "foobar");
    core_shrink(second, (char*)core_get_data(second)+0x100, (10<<10)-0x200);
    core_print_blocks();
    
    int ret = strcmp(core_get_data(second), "foobar");

    core_free(first);
    core_free(second);
    core_free(third);

    return ret;
}


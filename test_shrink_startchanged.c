#include <stdio.h>
#include "proposed-api.h"

/*
 * Expected output:
-------------------
0x602620: val: 1285 (first)
0x604e48: val:  -32 (<unallocated>)
0x604f48: val: 1253 (second)
0x607670: val: 1285 (third)
-------------------
*/
struct buflib_callbacks ops;

int main(void)
{
    buflib_core_init();

    int first  = core_alloc("first", 10<<10);
    int second = core_alloc("second", 10<<10);
    int third  = core_alloc("third", 10<<10);

    strcpy(core_get_data(third), "baz");

    strcpy((char*)core_get_data(second)+0x102, "foobar");
    core_shrink(second, (char*)core_get_data(second)+0x102, (10<<10)-0x102);
    memset(core_get_data(second), 0, (10<<10)-0x102);
    core_print_blocks();
    
    int ret =  strcmp(core_get_data(second), "foobar")
            || strcmp(core_get_data(third), "baz");

    core_free(first);
    core_free(second);
    core_free(third);

    return ret;
}

#include <stdio.h>
#include "proposed-api.h"
/*
 * Expected output (64bit)
--------------------
after freeing first: available: 9072
after freeing forth: available: 9072
buflib_compact(): Compacting!
A hole is created due to MUST_NOT_MOVE (0x6024c0, 2565)
Trying to fill hole. 2565 left
move_block(): moving "third"(id=3) by -3210(-25680)
Move! third, 0x608938, 0x6024e8, 25680
Filled hole: 640 left
fifth handle: 0
sixth(1):	0x6060e8
   	0x606110
   	2088
second(2):	0x6074e8
   	0x607510
   	5160
third(3):	0x6024c0
   	0x6024e8
   	15400
seventh(4):	0x606910
   	0x606938
   	552
--------------------
*/
static int move_size;
int move_callback(int handle, void* old, void* new)
{
    move_size = (char*)old-(char*)new;
    printf("Move! %s, %p, %p, %d\n", core_get_alloc_name(handle), old, new,
        move_size);

    return BUFLIB_CB_OK;
}

struct buflib_callbacks ops = {
    .move_callback = move_callback,
    .shrink_callback = NULL,
};

struct buflib_callbacks ops_no_move = {
    .move_callback = NULL,
    .shrink_callback = NULL,
};

int main(void)
{
    buflib_core_init();

    int first = core_alloc("first", 20<<10);
    int second= core_alloc_ex("second", 5<<10, &ops_no_move);
    int third = core_alloc_ex("third", 15<<10, &ops);
    strcpy(core_get_data(second), "Here's data");

    core_free(first);
    printf("after freeing first: available: %zu\n", core_available());
    /* should not trigger compaction, but replace the just freed one */
    int fourth = core_alloc("forth", 20<<10);
    core_free(fourth);
    printf("after freeing forth: available: %zu\n", core_available());
    /* should trigger compaction since it's a bit bigger than the just freed one */
    int fifth = core_alloc("fifth", 21<<10);
    printf("fifth handle: %d\n", fifth);
    int sixth = core_alloc("sixth", 2<<10);
    int seventh = core_alloc("seventh", 512);

    core_print_allocs();
    int ret = !(!strcmp(core_get_data(second), "Here's data") && move_size >= 20<<10);

    core_free(second);
    core_free(third);

    return ret;
}
    

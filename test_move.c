#include <stdio.h>
#include "proposed-api.h"

static int move_size;
int move_callback(int handle, void* old, void* new)
{
    move_size = (char*)old-(char*)new;
    printf("Move! %s, %p, %p, %zu\n", core_get_alloc_name(handle), old, new,
        move_size);

    return BUFLIB_CB_OK;
}

struct buflib_callbacks ops = {
    .move_callback = move_callback,
    .shrink_callback = NULL,
};

int main(void)
{
    buflib_core_init();

    int first = core_alloc("first", 20<<10);
    int second= core_alloc_ex("second", 20<<10, &ops);
    strcpy(core_get_data(second), "Here's data");

    core_free(first);
    /* should not trigger compaction, but replace the just freed one */
    int third = core_alloc("third", 20<<10);
    core_free(third);
    /* should trigger compaction since it's a bit bigger than the just freed one */
    int fourth = core_alloc("fourth", 21<<10);

    int ret = !(!strcmp(core_get_data(second), "Here's data") && move_size >= 20<<10);

    core_print_blocks();

    core_free(second);
    core_free(third);

    return ret;
}
    

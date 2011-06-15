
#include "buflib.h"
#include "new_apis.h"
#include "proposed-api.h"

static struct buflib_context core_ctx;
static char buf[50<<10];

void buflib_core_init(void)
{
    buflib_init(&core_ctx, buf, sizeof(buf));
}

int core_alloc(const char* name, size_t size)
{
    return buflib_alloc_ex(&core_ctx, size, name, NULL);
}

int core_alloc_ex(const char* name, size_t size, struct buflib_callbacks *ops)
{
    return buflib_alloc_ex(&core_ctx, size, name, ops);
}

size_t core_available(void)
{
    return buflib_available(&core_ctx);
}

void* core_get_data(int handle)
{
    return buflib_get_data(&core_ctx, handle);
}

void core_free(int handle)
{
    buflib_free(&core_ctx, handle);
}

int core_alloc_maximum(const char* name, size_t *size, struct buflib_callbacks *ops)
{
    return buflib_alloc_maximum(&core_ctx, name, size, ops);
}

bool core_shrink(int handle, void* new_start, size_t new_size)
{
    return buflib_shrink(&core_ctx, handle, new_start, new_size);
}

void core_print_allocs(void)
{
    buflib_print_allocs(&core_ctx);
}
void core_print_blocks(void)
{
    buflib_print_blocks(&core_ctx);
}

const char* core_get_alloc_name(int handle)
{
    return buflib_get_name(&core_ctx, handle);
}

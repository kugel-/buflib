
#include "buflib.h"
#include "new_apis.h"
#include "proposed-api.h"

static struct buflib_context core_ctx;

void buflib_core_init(void)
{
    buflib_init(&core_ctx, 50<<10);
}

int core_alloc(const char* name, size_t size)
{
    return buflib_alloc_ex(&core_ctx, size, name, NULL);
}

int core_alloc_ex(const char*name, size_t size, struct buflib_callbacks *ops)
{
    return buflib_alloc_ex(&core_ctx, size, name, ops);
}

int core_available(void)
{
    return buflib_available(&core_ctx);
}

int core_realloc(int handle, size_t new_size)
{
    return buflib_realloc(handle, new_size);
}

void core_free(int handle)
{
    buflib_free(&core_ctx, handle);
}

void core_shrink(int handle, void* new_start, void* new_end)
{
    buflib_shrink(&core_ctx, handle, new_start, new_end);
}

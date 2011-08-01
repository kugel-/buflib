/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* This is a memory allocator designed to provide reasonable management of free
* space and fast access to allocated data. More than one allocator can be used
* at a time by initializing multiple contexts.
*
* Copyright (C) 2009 Andrew Mahone
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
****************************************************************************/

#include <stdlib.h> /* for abs() */
#include "buflib.h"
#include "new_apis.h"
#include "strlcpy.h"
/* The main goal of this design is fast fetching of the pointer for a handle.
 * For that reason, the handles are stored in a table at the end of the buffer
 * with a fixed address, so that returning the pointer for a handle is a simple
 * table lookup. To reduce the frequency with which allocated blocks will need
 * to be moved to free space, allocations grow up in address from the start of
 * the buffer. The buffer is treated as an array of union buflib_data. Blocks
 * start with a length marker, which is included in their length. Free blocks
 * are marked by negative length, allocated ones use the a buflib_data in
 * the block to store a pointer to their handle table entry, so that it can be
 * quickly found and updated during compaction. Followed by that, there's
 * the pointer to the corresponding struct buflib. That pointer follows a
 * character array containing the string identifier of the allocation. After the
 * array there is another buflib_data containing the length of that string +
 * the sizeo of this buflib_data.
 * The allocator functions are passed a context struct so that two allocators
 * can be run, for example, one per core may be used, with convenience wrappers
 * for the single-allocator case that use a predefined context.
 */

/* Use this for the default callbacks.
 *
 * The default callbacks do nothing, therefore the address of this
 * acts as a magic as to not even call the default callbacks
 */
static struct buflib_callbacks default_callbacks;

#if defined(ROCKBOX)
#define YIELD() yield()
#elif defined(__unix) && (__unix == 1)
#include <sched.h>
#define YIELD() sched_yield()
#else
#warning YIELD not defined. Will busy-wait
#define  YIELD()
#endif

/* from "system.h"
/* align up or down to nearest integer multiple of a */
#define _ALIGN_DOWN(n, a)     ((typeof(n))((((intptr_t)(n)))&~((a)-1L)))
#define _ALIGN_UP(n, a)       _ALIGN_DOWN((n)+((a)-1),a)

#define B_ALIGN_DOWN(x) \
    _ALIGN_DOWN(x, sizeof(union buflib_data))

#define B_ALIGN_UP(x) \
    _ALIGN_UP(x, sizeof(union buflib_data))

/* Initialize buffer manager */
void
buflib_init(struct buflib_context *ctx, void *buf, size_t size)
{
    union buflib_data *bd_buf = buf;

    /* Align on sizeof(buflib_data), to prevent unaligned access */
    ALIGN_BUFFER(bd_buf, size, sizeof(union buflib_data));
    size /= sizeof(union buflib_data);
    /* The handle table is initialized with no entries */
    ctx->handle_table = bd_buf + size;
    ctx->last_handle = bd_buf + size;
    ctx->first_free_handle = bd_buf + size - 1;
    ctx->first_free_block = bd_buf;
    ctx->buf_start = bd_buf;
    /* A marker is needed for the end of allocated data, to make sure that it
     * does not collide with the handle table, and to detect end-of-buffer.
     */
    ctx->alloc_end = bd_buf;
    ctx->compact = true;
}

/* Allocate a new handle, returning 0 on failure */
static inline
union buflib_data* handle_alloc(struct buflib_context *ctx)
{
    union buflib_data *handle;
    /* first_free_handle is a lower bound on free handles, work through the
     * table from there until a handle containing NULL is found, or the end
     * of the table is reached.
     */
    for (handle = ctx->first_free_handle; handle >= ctx->last_handle; handle--)
        if (!handle->alloc)
            break;
    /* If the search went past the end of the table, it means we need to extend
     * the table to get a new handle.
     */
    if (handle < ctx->last_handle)
    {
        if (handle >= ctx->alloc_end)
            ctx->last_handle--;
        else
            return NULL;
    }
    handle->val = -1;
    return handle;
}

/* Free one handle, shrinking the handle table if it's the last one */
static inline
void handle_free(struct buflib_context *ctx, union buflib_data *handle)
{
    handle->alloc = 0;
    /* Update free handle lower bound if this handle has a lower index than the
     * old one.
     */
    if (handle > ctx->first_free_handle)
        ctx->first_free_handle = handle;
    if (handle == ctx->last_handle)
        ctx->last_handle++;
    else
        ctx->compact = false;
}

/* Get the start block of an allocation */
static union buflib_data* handle_to_block(struct buflib_context* ctx, int handle)
{    
    union buflib_data* name_field =
                (union buflib_data*)buflib_get_name(ctx, handle);

    return name_field - 3;
}

/* Shrink the handle table, returning true if its size was reduced, false if
 * not
 */
static inline
bool
handle_table_shrink(struct buflib_context *ctx)
{
    bool rv;
    union buflib_data *handle;
    for (handle = ctx->last_handle; !(handle->alloc); handle++);
    if (handle > ctx->first_free_handle)
        ctx->first_free_handle = handle - 1;
    rv = handle == ctx->last_handle;
    ctx->last_handle = handle;
    return rv;
}


/* If shift is non-zero, it represents the number of places to move
 * blocks in memory. Calculate the new address for this block,
 * update its entry in the handle table, and then move its contents.
 *
 * Returns false if moving was unsucessful
 * (NULL callback or BUFLIB_CB_CANNOT_MOVE was returned)
 */
static bool
move_block(struct buflib_context* ctx, union buflib_data* block, int shift)
{
    char* new_start;
    union buflib_data *new_block, *tmp = block[1].handle;
    struct buflib_callbacks *ops = block[2].ops;
    if (ops && !ops->move_callback)
        return false;
        
    int handle = ctx->handle_table - tmp;
    BDEBUGF("%s(): moving \"%s\"(id=%d) by %d(%d)\n", __func__, block[3].name,
            handle, shift, shift*sizeof(union buflib_data));
    new_block = block + shift;
    new_start = tmp->alloc + shift*sizeof(union buflib_data);
    /* call the callback before moving, the default one needn't be called */
    if (ops)
    {
        if (ops->move_callback(handle, tmp->alloc, new_start)
                == BUFLIB_CB_CANNOT_MOVE)
            return false;
    }
    tmp->alloc = new_start; /* update handle table */
    memmove(new_block, block, block->val * sizeof(union buflib_data));

    return true;
}

/* Compact allocations and handle table, adjusting handle pointers as needed.
 * Return true if any space was freed or consolidated, false otherwise.
 */
static bool
buflib_compact(struct buflib_context *ctx)
{
    BDEBUGF("%s(): Compacting!\n", __func__);
    union buflib_data *first_free = ctx->first_free_block, *block;
    int shift = 0, len;
    /* Store the results of attempting to shrink the handle table */
    bool ret = handle_table_shrink(ctx);
    for(block = first_free; block != ctx->alloc_end; block += len)
    {
        len = block->val;
        /* This block is free, add its length to the shift value */
        if (len < 0)
        {
            shift += len;
            len = -len;
            continue;
        }
        /* attempt to fill any hole */
        if (abs(ctx->first_free_block->val) > block->val)
        {
            intptr_t size = first_free->val;
            if (move_block(ctx, block, first_free - block))
            {
                block->val *= -1;
                block = ctx->first_free_block;
                ctx->first_free_block += block->val;
                ctx->first_free_block->val = size + block->val;
                continue;
            }
        }
        /* attempt move the allocation by shift */
        if (shift)
        {
            /* failing to move creates a hole, therefore mark this
             * block as not allocated anymore and move first_free_block up */
            if (!move_block(ctx, block, shift))
            {
                union buflib_data* hole = block + shift;
                hole->val = shift;
                if (ctx->first_free_block > hole)
                    ctx->first_free_block = hole;
                shift = 0;
            }
        }
    }
    /* Move the end-of-allocation mark, and return true if any new space has
     * been freed.
     */
    ctx->alloc_end += shift;
    /* only move first_free_block up if it wasn't already by a hole */
    if (ctx->first_free_block > ctx->alloc_end)
        ctx->first_free_block = ctx->alloc_end;
    ctx->compact = true;
    return ret || shift;
}

/* Compact the buffer by trying both shrinking and moving.
 *
 * Try to move first. If unsuccesfull, try to shrink. If that was successful
 * try to move once more as there might be more room now.
 */
static bool
buflib_compact_and_shrink(struct buflib_context *ctx, unsigned shrink_hints)
{
    bool result = false;
    /* if something compacted before already there will be no further gain */
    if (!ctx->compact)
        result = buflib_compact(ctx);
    if (!result)
    {
        union buflib_data* this;
        for(this = ctx->buf_start; this < ctx->alloc_end; this += abs(this->val))
        {
            if (this->val > 0 && this[2].ops
                              && this[2].ops->shrink_callback)
            {
                int ret;
                int handle = ctx->handle_table - this[1].handle;
                char* data = this[1].handle->alloc;
                ret = this[2].ops->shrink_callback(handle, shrink_hints,
                                            data, (char*)(this+this->val)-data);
                result |= (ret == BUFLIB_CB_OK);
                /* this might have changed in the callback (if
                 * it shrinked from the top), get it again */
                this = handle_to_block(ctx, handle);
            }
        }
        /* shrinking was successful at least once, try compaction again */
        if (result)
            result |= buflib_compact(ctx);
    }

    return result;
}

/* Shift buffered items by size units, and update handle pointers. The shift
 * value must be determined to be safe *before* calling.
 */
static void
buflib_buffer_shift(struct buflib_context *ctx, int shift)
{
    memmove(ctx->buf_start + shift, ctx->buf_start,
        (ctx->alloc_end - ctx->buf_start) * sizeof(union buflib_data));
    union buflib_data *handle;
    for (handle = ctx->last_handle; handle < ctx->handle_table; handle++)
        if (handle->alloc)
            handle->alloc += shift;
    ctx->first_free_block += shift;
    ctx->buf_start += shift;
    ctx->alloc_end += shift;
}

/* Shift buffered items up by size bytes, or as many as possible if size == 0.
 * Set size to the number of bytes freed.
 */
void*
buflib_buffer_out(struct buflib_context *ctx, size_t *size)
{
    if (!ctx->compact)
        buflib_compact(ctx);
    size_t avail = ctx->last_handle - ctx->alloc_end;
    size_t avail_b = avail * sizeof(union buflib_data);
    if (*size && *size < avail_b)
    {
        avail = (*size + sizeof(union buflib_data) - 1)
            / sizeof(union buflib_data);
        avail_b = avail * sizeof(union buflib_data);
    }
    *size = avail_b;
    void *ret = ctx->buf_start;
    buflib_buffer_shift(ctx, avail);
    return ret;
}

/* Shift buffered items down by size bytes */
void
buflib_buffer_in(struct buflib_context *ctx, int size)
{
    size /= sizeof(union buflib_data);
    buflib_buffer_shift(ctx, -size);
}

/* Allocate a buffer of size bytes, returning a handle for it */
int
buflib_alloc(struct buflib_context *ctx, size_t size)
{
    return buflib_alloc_ex(ctx, size, "<anonymous>", &default_callbacks);
}

/* Allocate a buffer of size bytes, returning a handle for it.
 *
 * The additional name parameter gives the allocation a human-readable name,
 * the ops parameter points to caller-implemented callbacks for moving and
 * shrinking. NULL for default callbacks
 */

int
buflib_alloc_ex(struct buflib_context *ctx, size_t size, const char *name,
                struct buflib_callbacks *ops)
{
    /* busy wait if there's a thread owning the lock */
    while (ctx->handle_lock != 0) YIELD();

    union buflib_data *handle, *block;
    size_t name_len = name ? B_ALIGN_UP(strlen(name)+1) : 0;
    bool last;
    /* This really is assigned a value before use */
    int block_len;
    size += name_len;
    size = (size + sizeof(union buflib_data) - 1) /
           sizeof(union buflib_data)
           /* add 4 objects for alloc len, pointer to handle table entry and
            * name length, and the ops pointer */
           + 4;
handle_alloc:
    handle = handle_alloc(ctx);
    if (!handle)
    {
        /* If allocation has failed, and compaction has succeded, it may be
         * possible to get a handle by trying again.
         */
        if (!ctx->compact && buflib_compact(ctx))
            goto handle_alloc;
        else
        {   /* first try to shrink the alloc before the handle table
             * to make room for new handles */
            int handle = ctx->handle_table - ctx->last_handle;
            union buflib_data* last_block = handle_to_block(ctx, handle);
            struct buflib_callbacks* ops = last_block[2].ops;
            if (ops && ops->shrink_callback)
            {
                char *data = buflib_get_data(ctx, handle);
                unsigned hint = BUFLIB_SHRINK_POS_BACK | 10*sizeof(union buflib_data);
                if (ops->shrink_callback(handle, hint, data, 
                        (char*)(last_block+last_block->val)-data) == BUFLIB_CB_OK)
                {   /* retry one more time */
                    goto handle_alloc;
                }
            }
            return 0;
        }
    }

buffer_alloc:
    /* need to re-evaluate last before the loop because the last allocation
     * possibly made room in its front to fit this, so last would be wrong */
    last = false;
    for (block = ctx->first_free_block;;block += block_len)
    {
        /* If the last used block extends all the way to the handle table, the
         * block "after" it doesn't have a header. Because of this, it's easier
         * to always find the end of allocation by saving a pointer, and always
         * calculate the free space at the end by comparing it to the
         * last_handle pointer.
         */
        if(block == ctx->alloc_end)
        {
            last = true;
            block_len = ctx->last_handle - block;
            if ((size_t)block_len < size)
                block = NULL;
            break;
        }
        block_len = block->val;
        /* blocks with positive length are already allocated. */
        if(block_len > 0)
            continue;
        block_len = -block_len;
        /* The search is first-fit, any fragmentation this causes will be 
         * handled at compaction.
         */
        if ((size_t)block_len >= size)
            break;
    }
    if (!block)
    {
        /* Try compacting if allocation failed */
        if (buflib_compact_and_shrink(ctx,
                    (size*sizeof(union buflib_data))&BUFLIB_SHRINK_SIZE_MASK))
        {
            goto buffer_alloc;
        } else {
            handle->val=1;
            handle_free(ctx, handle);
            return 0;
        }
    }

    /* Set up the allocated block, by marking the size allocated, and storing
     * a pointer to the handle.
     */
    union buflib_data *name_len_slot;
    block->val = size;
    block[1].handle = handle;
    block[2].ops = ops ?: &default_callbacks;
    strcpy(block[3].name, name);
    name_len_slot = (union buflib_data*)B_ALIGN_UP(block[3].name + name_len);
    name_len_slot->val = 1 + name_len/sizeof(union buflib_data);
    handle->alloc = (char*)(name_len_slot + 1);
    /* If we have just taken the first free block, the next allocation search
     * can save some time by starting after this block.
     */
    if (block == ctx->first_free_block)
        ctx->first_free_block += size;
    block += size;
    /* alloc_end must be kept current if we're taking the last block. */
    if (last)
        ctx->alloc_end = block;
    /* Only free blocks *before* alloc_end have tagged length. */
    else if ((size_t)block_len > size)
        block->val = size - block_len;
    /* Return the handle index as a positive integer. */
    return ctx->handle_table - handle;
}

/* Free the buffer associated with handle_num. */
void
buflib_free(struct buflib_context *ctx, int handle_num)
{
    union buflib_data *handle = ctx->handle_table - handle_num,
                      *freed_block = handle_to_block(ctx, handle_num),
                      *block = ctx->first_free_block,
                      *next_block = block;
    /* We need to find the block before the current one, to see if it is free
     * and can be merged with this one.
     */
    while (next_block < freed_block)
    {
        block = next_block;
        next_block += abs(block->val);
    }
    /* If next_block == block, the above loop didn't go anywhere. If it did,
     * and the block before this one is empty, we can combine them.
     */
    if (next_block == freed_block && next_block != block && block->val < 0)
        block->val -= freed_block->val;
    /* Otherwise, set block to the newly-freed block, and mark it free, before
     * continuing on, since the code below exects block to point to a free
     * block which may have free space after it.
     */
    else
    {
        block = freed_block;
        block->val = -block->val;
    }
    next_block = block - block->val;
    /* Check if we are merging with the free space at alloc_end. */
    if (next_block == ctx->alloc_end)
        ctx->alloc_end = block;
    /* Otherwise, the next block might still be a "normal" free block, and the
     * mid-allocation free means that the buffer is no longer compact.
     */
    else {
        ctx->compact = false;
        if (next_block->val < 0)
            block->val += next_block->val;
    }
    handle_free(ctx, handle);
    handle->alloc = NULL;
    /* If this block is before first_free_block, it becomes the new starting
     * point for free-block search.
     */
    if (block < ctx->first_free_block)
        ctx->first_free_block = block;

    /* if the handle is the one aquired with buflib_alloc_maximum()
     * unlock buflib_alloc() as part of the shrink */
    if (ctx->handle_lock == handle_num)
        ctx->handle_lock = 0;
}

/* Return the maximum allocatable memory in bytes */
size_t
buflib_available(struct buflib_context* ctx)
{
    /* subtract 5 elements for
     * val, handle, name_len, ops and the handle table entry*/
    size_t diff = (ctx->last_handle - ctx->alloc_end - 5);
    diff *= sizeof(union buflib_data); /* make it bytes */
    diff -= 16; /* reserve 16 for the name */

    if (diff > 0)
        return diff;
    else
        return 0;
}

/*
 * Allocate all available (as returned by buflib_available()) memory and return
 * a handle to it
 *
 * This grabs a lock which can only be unlocked by buflib_free() or
 * buflib_shrink(), to protect from further allocations (which couldn't be
 * serviced anyway).
 */
int
buflib_alloc_maximum(struct buflib_context* ctx, const char* name, size_t *size, struct buflib_callbacks *ops)
{
    int handle;

    /* limit name to 16 since that's what buflib_available() accounts for it */
    char buf[16];
    *size = buflib_available(ctx);
    strlcpy(buf, name, sizeof(buf));
    handle = buflib_alloc_ex(ctx, *size, buf, ops);

    if (handle > 0) /* shouldn't happen ?? */
        ctx->handle_lock = handle;
    
    return handle;
}

/* Shrink the allocation indicated by the handle according to new_start and
 * new_size. Grow is not possible, therefore new_start and new_start + new_size
 * must be within the original allocation
 */
bool
buflib_shrink(struct buflib_context* ctx, int handle, void* new_start, size_t new_size)
{
    char* oldstart = buflib_get_data(ctx, handle);
    char* newstart = new_start;
    char* newend = newstart + new_size;

    /* newstart must be higher and new_size not "negative" */
    if (newstart < oldstart || newend < newstart)
        return false;
    union buflib_data *block = handle_to_block(ctx, handle),
                      *old_next_block = block + block->val,
                /* newstart isn't necessarily properly aligned but it
                 * needn't be since it's only dereferenced by the user code */
                      *aligned_newstart = (union buflib_data*)B_ALIGN_DOWN(newstart),
                      *aligned_oldstart = (union buflib_data*)B_ALIGN_DOWN(oldstart),
                      *new_next_block =   (union buflib_data*)B_ALIGN_UP(newend),
                      *new_block, metadata_size;

    /* growing is not supported */
    if (new_next_block > old_next_block)
        return false;

    metadata_size.val = aligned_oldstart - block;
    /* update val and the handle table entry */
    new_block = aligned_newstart - metadata_size.val;
    block[0].val = new_next_block - new_block;

    block[1].handle->alloc = newstart;
    if (block != new_block)
    {
        /* move metadata over, i.e. pointer to handle table entry and name
         * This is actually the point of no return. Data in the allocation is
         * being modified, and therefore we must successfully finish the shrink
         * operation */
        memmove(new_block, block, metadata_size.val*sizeof(metadata_size));
        /* mark the old block unallocated */
        block->val = block - new_block;
        union buflib_data *freed_block = block,
                          *free_before = ctx->first_free_block,
                          *next_block = free_before;
        /* We need to find the block before the current one, to see if it is free
         * and can be merged with this one.
         */
        while (next_block < freed_block)
        {
            free_before = next_block;
            next_block += abs(block->val);
        }
        /* If next_block == free_before, the above loop didn't go anywhere.
         * If it did, and the block before this one is empty, we can combine them.
         */
        if (next_block == freed_block && next_block != free_before && free_before->val < 0)
            free_before->val += freed_block->val;
        else if (next_block == free_before)
            ctx->first_free_block = freed_block;
            
        /* We didn't handle size changes yet, assign block to the new one
         * the code below the wants block whether it changed or not */
        block = new_block;
    }

    /* Now deal with size changes that create free blocks after the allocation */
    if (old_next_block != new_next_block)
    {
        if (ctx->alloc_end == old_next_block)
            ctx->alloc_end = new_next_block;
        else if (old_next_block->val < 0)
        {   /* enlarge next block by moving it up */
            new_next_block->val = old_next_block->val - (old_next_block - new_next_block);
        }
        else if (old_next_block != new_next_block)
        {   /* creating a hole */
            /* must be negative to indicate being unallocated */
            new_next_block->val = new_next_block - old_next_block;
        }
        /* update first_free_block for the newly created free space */
        if (ctx->first_free_block > new_next_block)
            ctx->first_free_block = new_next_block;
    }

    /* if the handle is the one aquired with buflib_alloc_maximum()
     * unlock buflib_alloc() as part of the shrink */
    if (ctx->handle_lock == handle)
        ctx->handle_lock = 0;

    return true;
}

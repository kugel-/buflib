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

#ifndef _BUFLIB_H_
#define _BUFLIB_H_
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* from "debug.h" */
#ifdef DEBUG
    #include <stdio.h>
    #define BDEBUGF printf
#else
    #define BDEBUGF(...) do { } while(0)
#endif

/* from "system.h"
/* align up or down to nearest integer multiple of a */
#define ALIGN_DOWN(n, a)     ((n)/(a)*(a))
#define ALIGN_UP(n, a)       ALIGN_DOWN((n)+((a)-1),a)

/* align start and end of buffer to nearest integer multiple of a */
#define ALIGN_BUFFER(ptr,len,align) \
{\
    uintptr_t tmp_ptr1 = (uintptr_t)ptr; \
    uintptr_t tmp_ptr2 = tmp_ptr1 + len;\
    tmp_ptr1 = ALIGN_UP(tmp_ptr1,align); \
    tmp_ptr2 = ALIGN_DOWN(tmp_ptr2,align); \
    len = tmp_ptr2 - tmp_ptr1; \
    ptr = (typeof(ptr))tmp_ptr1; \
}

union buflib_data
{
    intptr_t val;
    char name;
    union buflib_data *ptr;
};

struct buflib_context
{
    union buflib_data *handle_table;
    union buflib_data *first_free_handle;
    union buflib_data *last_handle;
    union buflib_data *first_free_block;
    union buflib_data *buf_start;
    union buflib_data *alloc_end;
    bool compact;
};

void buflib_init(struct buflib_context *context, void *buf, size_t size);
int buflib_alloc(struct buflib_context *context, size_t size);
void buflib_free(struct buflib_context *context, int handle);
void* buflib_buffer_out(struct buflib_context *ctx, size_t *size);
void buflib_buffer_in(struct buflib_context *ctx, int size);



static inline void* buflib_get_data(struct buflib_context *context, int handle)
{
    return (void*)(context->handle_table[-handle].ptr);
}
#endif

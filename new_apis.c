/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Copyright (C) 2011 Thomas Martitz
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

#include "buflib.h"
#include <stdio.h>

const char* buflib_get_name(struct buflib_context *ctx, int handle)
{
    union buflib_data *data = buflib_get_data(ctx, handle);
    size_t len = data[-1].val;
    if (len <= 1)
        return NULL;
    const char *ret = &data[-len].name;
}

void buflib_print_allocs(struct buflib_context *ctx)
{
    union buflib_data *this, *end = ctx->handle_table;
    for(this = end - 1; this >= ctx->last_handle; this--)
    {
        if (!this->ptr) continue;

        int handle_num;
        const char *name;
        union buflib_data *block_start, *alloc_start;
        intptr_t alloc_len;

        handle_num = end - this;
        name = buflib_get_name(ctx, handle_num);
        block_start = (union buflib_data*)name - 3;
        alloc_start = buflib_get_data(ctx, handle_num);
        alloc_len = block_start->val * sizeof(union buflib_data);

        printf("%s(%d):\t%0p\n"
               "   \t%0p\n"
               "   \t%ld\n",
               name?:"(null)", handle_num, block_start, alloc_start, alloc_len);
    }
}

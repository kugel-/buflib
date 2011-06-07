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

#ifndef __NEW_APIS_H__
#define __NEW_APIS_H__

#include <string.h>
#include "buflib.h"
#include "proposed-api.h"

const char* buflib_get_name(struct buflib_context *ctx, int handle);
int buflib_alloc_ex(struct buflib_context *ctx, size_t size, const char *name,
                    struct buflib_callbacks *ops);
void buflib_print_allocs(struct buflib_context *ctx);
size_t buflib_available(struct buflib_context *ctx);
void buflib_shrink(struct buflib_context *ctx, int handle, void* newstart, void* newend);
struct buflib_callbacks* buflib_default_callbacks(void);
#endif /* __NEW_APIS_H__ */

/**
 * backend-module.c - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Generic filesystem backend module wrapper.
 *
 * This translation unit is compiled once per filesystem backend into
 * libpartclone_<name>.so and is configured entirely through the following
 * preprocessor symbols:
 *
 *   PTC_MODULE_NAME         canonical backend name (string)
 *   PTC_MODULE_MAGIC        filesystem magic (see partclone.h)
 *   PTC_MODULE_DESCRIPTION  human readable description (string)
 *   PTC_MODULE_READ_SUPER   read_super_blocks() implementation symbol
 *   PTC_MODULE_READ_BITMAP  read_bitmap() implementation symbol
 *   PTC_MODULE_POST_FIXUP   optional post_clone_fixup() implementation symbol
 *
 * The backend implementation itself is compiled with
 * -Dread_super_blocks=... -Dread_bitmap=... (see lib/meson.build) so that the
 * historical, non-namespaced entry points become unique symbols and can be
 * referenced by name here.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

#include "ptc-fs-backend.h"

#ifndef PTC_MODULE_NAME
# error "PTC_MODULE_NAME must be defined"
#endif

#ifndef PTC_MODULE_MAGIC
# error "PTC_MODULE_MAGIC must be defined"
#endif

#ifndef PTC_MODULE_READ_SUPER
# error "PTC_MODULE_READ_SUPER must be defined"
#endif

#ifndef PTC_MODULE_READ_BITMAP
# error "PTC_MODULE_READ_BITMAP must be defined"
#endif

#ifndef PTC_MODULE_DESCRIPTION
# define PTC_MODULE_DESCRIPTION PTC_MODULE_NAME
#endif

extern void PTC_MODULE_READ_SUPER(char *device, file_system_info *fs_info);
extern void PTC_MODULE_READ_BITMAP(char *device, file_system_info fs_info,
                                   unsigned long *bitmap, int pui);

#ifdef PTC_MODULE_POST_FIXUP
extern int PTC_MODULE_POST_FIXUP(int *dfw, file_system_info fs_info,
                                 cmd_opt opt);
#endif

/**
 * ptc_fs_backend_module:
 *
 * The single symbol exported by a backend module.  The core looks it up with
 * g_module_symbol() using #PTC_FS_BACKEND_MODULE_SYMBOL.  The name below must
 * match that macro.
 */
const ptc_fs_backend ptc_fs_backend_module = {
    .name = PTC_MODULE_NAME,
    .magic = PTC_MODULE_MAGIC,
    .description = PTC_MODULE_DESCRIPTION,
    .read_super_blocks = PTC_MODULE_READ_SUPER,
    .read_bitmap = PTC_MODULE_READ_BITMAP,
#ifdef PTC_MODULE_POST_FIXUP
    .post_clone_fixup = PTC_MODULE_POST_FIXUP,
#else
    .post_clone_fixup = NULL,
#endif
};

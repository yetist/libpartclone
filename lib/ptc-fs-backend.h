/**
 * ptc-fs-backend.h - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Runtime filesystem backend registry.
 *
 * Historically each partclone command line tool was linked against exactly one
 * filesystem backend, selected at compile time with -D<FS>.  libpartclone
 * instead builds the core engine into libpartclone.so and every filesystem
 * backend into its own loadable module (libpartclone_<name>.so).  The core
 * discovers the modules at runtime and selects one through this descriptor
 * table.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PTC_FS_BACKEND_H
#define PTC_FS_BACKEND_H

#include "partclone.h"

/**
 * ptc_fs_backend:
 * @name: canonical backend name, e.g. "extfs", "btrfs".
 * @magic: filesystem magic string stored in a partclone image, or %NULL.
 * @description: human readable description.
 * @read_super_blocks: fill @fs_info from the filesystem super block.
 * @read_bitmap: build the used-block bitmap of the filesystem.
 * @post_clone_fixup: optional post clone/restore fixup, or %NULL.
 *
 * A runtime descriptor for one filesystem implementation.
 */
typedef struct _ptc_fs_backend ptc_fs_backend;

struct _ptc_fs_backend {
    const char *name;
    const char *magic;
    const char *description;
    void (*read_super_blocks)(char *device, file_system_info *fs_info);
    void (*read_bitmap)(char *device, file_system_info fs_info,
                        unsigned long *bitmap, int pui);
    int  (*post_clone_fixup)(int *dfw, file_system_info fs_info, cmd_opt opt);
};

/**
 * PTC_FS_BACKEND_MODULE_SYMBOL:
 *
 * The exported symbol a backend module must define.  It is a
 * #ptc_fs_backend (not a pointer) so that the core can reference it
 * directly after looking it up with g_module_symbol().
 *
 * A module is named libpartclone_&lt;name&gt;.so and is normally installed into
 * $(libdir)/partclone.  The directory can be overridden at runtime with the
 * PARTCLONE_PLUGIN_DIR environment variable.
 */
#define PTC_FS_BACKEND_MODULE_SYMBOL "ptc_fs_backend_module"

/**
 * ptc_fs_backend_lookup:
 * @name: backend name, e.g. "extfs".
 *
 * Returns: (transfer none) (nullable): the backend or %NULL.
 */
const ptc_fs_backend *ptc_fs_backend_lookup(const char *name);

/**
 * ptc_fs_backend_by_magic:
 * @magic: a filesystem magic string (see file_system_info.fs).
 *
 * Returns: (transfer none) (nullable): the backend or %NULL.
 */
const ptc_fs_backend *ptc_fs_backend_by_magic(const char *magic);

/**
 * ptc_fs_backend_list:
 *
 * Returns: (transfer none) (array zero-terminated=1): %NULL terminated array
 * of every backend module that could be loaded.
 */
const ptc_fs_backend *const *ptc_fs_backend_list(void);

/**
 * ptc_fs_backend_from_program_name:
 * @program: a program name such as "partclone.ext4".
 *
 * Maps a partclone command line program name to a backend name, taking the
 * historical aliases (partclone.ext2/3/4, partclone.vfat, partclone.hfsplus,
 * ...) into account.
 *
 * Returns: (transfer none) (nullable): a backend name or %NULL.
 */
const char *ptc_fs_backend_from_program_name(const char *program);

/**
 * ptc_fs_backend_detect:
 * @device: path to a device or image.
 *
 * Best effort filesystem detection (uses libblkid when available).
 *
 * Returns: (transfer none) (nullable): a backend or %NULL.
 */
const ptc_fs_backend *ptc_fs_backend_detect(const char *device);

#endif /* PTC_FS_BACKEND_H */

/**
 * ptc-fs-backend.c - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Runtime filesystem backend registry.
 *
 * Every enabled backend is compiled into its own libtool convenience library
 * with -Dread_super_blocks=<prefix>_read_super_blocks and
 * -Dread_bitmap=<prefix>_read_bitmap so that the historical, non-namespaced
 * entry points become unique symbols inside the single libpartclone.so.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>
#include <string.h>
#include <glib.h>

#include "ptc-fs-backend.h"

#ifdef HAVE_BLKID
#include <blkid/blkid.h>
#endif

/* Declare the (renamed) entry points of every backend. */
#define PTC_DECLARE_BACKEND(prefix)                                         \
    extern void prefix##_read_super_blocks(char *device,                    \
                                           file_system_info *fs_info);      \
    extern void prefix##_read_bitmap(char *device, file_system_info fs_info, \
                                     unsigned long *bitmap, int pui)

#ifdef PTC_HAVE_EXTFS
PTC_DECLARE_BACKEND(extfs);
#endif
#ifdef PTC_HAVE_REISERFS
PTC_DECLARE_BACKEND(reiserfs);
#endif
#ifdef PTC_HAVE_REISER4
PTC_DECLARE_BACKEND(reiser4);
#endif
#ifdef PTC_HAVE_HFSP
PTC_DECLARE_BACKEND(hfsp);
extern int hfsp_post_clone_fixup(int *dfw, file_system_info fs_info, cmd_opt opt);
#endif
#ifdef PTC_HAVE_APFS
PTC_DECLARE_BACKEND(apfs);
#endif
#ifdef PTC_HAVE_FAT
PTC_DECLARE_BACKEND(fat);
#endif
#ifdef PTC_HAVE_EXFAT
PTC_DECLARE_BACKEND(exfat);
#endif
#ifdef PTC_HAVE_F2FS
PTC_DECLARE_BACKEND(f2fs);
#endif
#ifdef PTC_HAVE_NILFS2
PTC_DECLARE_BACKEND(nilfs2);
#endif
#ifdef PTC_HAVE_NTFS
PTC_DECLARE_BACKEND(ntfs);
#endif
#ifdef PTC_HAVE_UFS
PTC_DECLARE_BACKEND(ufs);
#endif
#ifdef PTC_HAVE_VMFS
PTC_DECLARE_BACKEND(vmfs);
#endif
#ifdef PTC_HAVE_VMFS5
PTC_DECLARE_BACKEND(vmfs5);
#endif
#ifdef PTC_HAVE_JFS
PTC_DECLARE_BACKEND(jfs);
#endif
#ifdef PTC_HAVE_BTRFS
PTC_DECLARE_BACKEND(btrfs);
#endif
#ifdef PTC_HAVE_MINIX
PTC_DECLARE_BACKEND(minix);
#endif
#ifdef PTC_HAVE_XFS
PTC_DECLARE_BACKEND(xfs);
#endif
#ifdef PTC_HAVE_DD
PTC_DECLARE_BACKEND(dd);
#endif

#define PTC_BACKEND(prefix, name_, magic_, desc_)                          \
    {                                                                      \
        name_, magic_, desc_, prefix##_read_super_blocks,                  \
        prefix##_read_bitmap, NULL                                         \
    }

static const ptc_fs_backend backends_static[] = {
#ifdef PTC_HAVE_DD
    PTC_BACKEND(dd, "raw", raw_MAGIC, "Raw / device to device (dd)"),
#endif
#ifdef PTC_HAVE_EXTFS
    PTC_BACKEND(extfs, "extfs", extfs_MAGIC, "ext2/ext3/ext4"),
#endif
#ifdef PTC_HAVE_REISERFS
    PTC_BACKEND(reiserfs, "reiserfs", reiserfs_MAGIC, "ReiserFS 3.6"),
#endif
#ifdef PTC_HAVE_REISER4
    PTC_BACKEND(reiser4, "reiser4", reiser4_MAGIC, "Reiser4"),
#endif
#ifdef PTC_HAVE_HFSP
    {
        "hfsp", hfsplus_MAGIC, "HFS+",
        hfsp_read_super_blocks, hfsp_read_bitmap, hfsp_post_clone_fixup
    },
#endif
#ifdef PTC_HAVE_APFS
    PTC_BACKEND(apfs, "apfs", apfs_MAGIC, "APFS"),
#endif
#ifdef PTC_HAVE_FAT
    PTC_BACKEND(fat, "fat", fat_MAGIC, "FAT12/16/32"),
#endif
#ifdef PTC_HAVE_EXFAT
    PTC_BACKEND(exfat, "exfat", exfat_MAGIC, "exFAT"),
#endif
#ifdef PTC_HAVE_F2FS
    PTC_BACKEND(f2fs, "f2fs", f2fs_MAGIC, "F2FS"),
#endif
#ifdef PTC_HAVE_NILFS2
    PTC_BACKEND(nilfs2, "nilfs2", nilfs_MAGIC, "NILFS2"),
#endif
#ifdef PTC_HAVE_NTFS
    PTC_BACKEND(ntfs, "ntfs", ntfs_MAGIC, "NTFS"),
#endif
#ifdef PTC_HAVE_UFS
    PTC_BACKEND(ufs, "ufs", ufs_MAGIC, "UFS1/UFS2"),
#endif
#ifdef PTC_HAVE_VMFS
    PTC_BACKEND(vmfs, "vmfs", vmfs_MAGIC, "VMFS"),
#endif
#ifdef PTC_HAVE_VMFS5
    PTC_BACKEND(vmfs5, "vmfs5", vmfs_MAGIC, "VMFS5"),
#endif
#ifdef PTC_HAVE_JFS
    PTC_BACKEND(jfs, "jfs", jfs_MAGIC, "JFS"),
#endif
#ifdef PTC_HAVE_BTRFS
    PTC_BACKEND(btrfs, "btrfs", btrfs_MAGIC, "BTRFS"),
#endif
#ifdef PTC_HAVE_MINIX
    PTC_BACKEND(minix, "minix", minix_MAGIC, "MINIX"),
#endif
#ifdef PTC_HAVE_XFS
    PTC_BACKEND(xfs, "xfs", xfs_MAGIC, "XFS"),
#endif
};

static const ptc_fs_backend *backends[G_N_ELEMENTS(backends_static) + 1];
static int backends_initialized;

static void
ptc_fs_backends_init(void)
{
    if (backends_initialized)
        return;
    for (unsigned int i = 0; i < G_N_ELEMENTS(backends_static); i++)
        backends[i] = &backends_static[i];
    backends[G_N_ELEMENTS(backends_static)] = NULL;
    backends_initialized = 1;
}

const ptc_fs_backend *const *
ptc_fs_backend_list(void)
{
    ptc_fs_backends_init();
    return backends;
}

const ptc_fs_backend *
ptc_fs_backend_lookup(const char *name)
{
    if (name == NULL)
        return NULL;

    ptc_fs_backends_init();

    for (const ptc_fs_backend *const *it = backends; *it != NULL; it++) {
        if (g_ascii_strcasecmp((*it)->name, name) == 0)
            return *it;
    }

    /* A few historical aliases. */
    if (g_ascii_strcasecmp(name, "ext2") == 0 ||
        g_ascii_strcasecmp(name, "ext3") == 0 ||
        g_ascii_strcasecmp(name, "ext4") == 0)
        return ptc_fs_backend_lookup("extfs");
    if (g_ascii_strcasecmp(name, "hfsplus") == 0 ||
        g_ascii_strcasecmp(name, "hfs+") == 0)
        return ptc_fs_backend_lookup("hfsp");
    if (g_ascii_strcasecmp(name, "vfat") == 0 ||
        g_ascii_strcasecmp(name, "fat12") == 0 ||
        g_ascii_strcasecmp(name, "fat16") == 0 ||
        g_ascii_strcasecmp(name, "fat32") == 0)
        return ptc_fs_backend_lookup("fat");
    if (g_ascii_strcasecmp(name, "dd") == 0 ||
        g_ascii_strcasecmp(name, "none") == 0)
        return ptc_fs_backend_lookup("raw");

    return NULL;
}

const ptc_fs_backend *
ptc_fs_backend_by_magic(const char *magic)
{
    if (magic == NULL)
        return NULL;

    ptc_fs_backends_init();

    for (const ptc_fs_backend *const *it = backends; *it != NULL; it++) {
        if ((*it)->magic != NULL && strcmp((*it)->magic, magic) == 0)
            return *it;
    }
    return NULL;
}

const char *
ptc_fs_backend_from_program_name(const char *program)
{
    if (program == NULL)
        return NULL;

    /* Strip a directory prefix, if any. */
    const char *base = strrchr(program, '/');
    base = base ? base + 1 : program;

    if (strncmp(base, "partclone.", 10) != 0)
        return NULL;

    return base + 10;
}

#ifdef HAVE_BLKID
static const char *
blkid_type_to_backend(const char *type)
{
    if (type == NULL)
        return NULL;
    if (strcmp(type, "ext2") == 0 || strcmp(type, "ext3") == 0 ||
        strcmp(type, "ext4") == 0)
        return "extfs";
    if (strcmp(type, "vfat") == 0 || strcmp(type, "msdos") == 0)
        return "fat";
    if (strcmp(type, "hfsplus") == 0)
        return "hfsp";
    return type;
}
#endif

const ptc_fs_backend *
ptc_fs_backend_detect(const char *device)
{
#ifdef HAVE_BLKID
    if (device == NULL)
        return NULL;

    blkid_probe probe = blkid_new_probe_from_filename(device);
    if (probe == NULL)
        return NULL;

    blkid_probe_enable_superblocks(probe, 1);
    blkid_probe_set_superblocks_flags(probe, BLKID_SUBLKS_TYPE);

    const ptc_fs_backend *backend = NULL;
    if (blkid_do_safeprobe(probe) == 0) {
        const char *type = NULL;
        if (blkid_probe_lookup_value(probe, "TYPE", &type, NULL) == 0)
            backend = ptc_fs_backend_lookup(blkid_type_to_backend(type));
    }
    blkid_free_probe(probe);
    return backend;
#else
    (void)device;
    return NULL;
#endif
}

/* ------------------------------------------------------------------ */
/* Public (installed) helpers -- see libpartclone.h                    */
/* ------------------------------------------------------------------ */

const char *const *
ptc_backend_names(void)
{
    static const char *names[64];
    static gsize initialized = 0;

    if (g_once_init_enter(&initialized)) {
        const ptc_fs_backend *const *it;
        unsigned int i = 0;

        for (it = ptc_fs_backend_list();
             *it != NULL && i < G_N_ELEMENTS(names) - 1;
             it++)
            names[i++] = (*it)->name;
        names[i] = NULL;

        g_once_init_leave(&initialized, 1);
    }

    return names;
}

gboolean
ptc_backend_available(const char *name)
{
    return ptc_fs_backend_lookup(name) != NULL;
}

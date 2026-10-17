/**
 * ptc-fs-backend.c - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Runtime filesystem backend registry.
 *
 * The core library (libpartclone.so) does not contain any filesystem backend.
 * Every backend is a loadable module named libpartclone_<name>.so that exports
 * a single symbol (#PTC_FS_BACKEND_MODULE_SYMBOL).  The modules are discovered
 * at runtime in (in this order):
 *
 *   1. $PARTCLONE_PLUGIN_DIR, when set (exclusive),
 *   2. <dir of libpartclone.so>/partclone (the installed layout),
 *   3. <dir of libpartclone.so> (the uninstalled build tree),
 *   4. $(libdir)/partclone, compiled in as a last resort.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define _GNU_SOURCE

#include <config.h>
#include <string.h>
#include <dlfcn.h>
#include <glib.h>
#include <gmodule.h>

#include "ptc-fs-backend.h"

#ifdef HAVE_BLKID
#include <blkid/blkid.h>
#endif

#ifndef PTC_PLUGIN_DIR
# define PTC_PLUGIN_DIR "/usr/local/lib/partclone"
#endif

#define PTC_PLUGIN_PREFIX   "libpartclone_"
#define PTC_PLUGIN_SUFFIX   ".so"
#define PTC_MAX_BACKENDS    64

static GMutex backend_lock;
static gboolean backends_loaded;

static const ptc_fs_backend *backends[PTC_MAX_BACKENDS + 1];
static guint n_backends;

/* Keep every successfully opened module resident for the lifetime of the
 * process; the descriptors handed out point straight at it. */
static GPtrArray *modules;

/* ------------------------------------------------------------------ */
/* Module loading                                                      */
/* ------------------------------------------------------------------ */

static gboolean
ptc_backend_try_load(const char *dir, const char *name)
{
    gpointer symbol = NULL;
    GModule *module;
    char *path;
    guint i;

    if (dir == NULL || name == NULL)
        return FALSE;

    path = g_strdup_printf("%s/%s%s%s", dir, PTC_PLUGIN_PREFIX, name,
                           PTC_PLUGIN_SUFFIX);
    module = g_module_open(path, G_MODULE_BIND_LAZY);
    g_free(path);

    if (module == NULL)
        return FALSE;

    if (!g_module_symbol(module, PTC_FS_BACKEND_MODULE_SYMBOL, &symbol) ||
        symbol == NULL) {
        g_module_close(module);
        return FALSE;
    }

    const ptc_fs_backend *backend = symbol;

    if (backend->name == NULL || backend->name[0] == '\0' ||
        backend->read_super_blocks == NULL || backend->read_bitmap == NULL) {
        g_module_close(module);
        return FALSE;
    }

    /* Do not register the same backend twice (e.g. found both in the build
     * tree and in the installed directory). */
    for (i = 0; i < n_backends; i++) {
        if (g_ascii_strcasecmp(backends[i]->name, backend->name) == 0) {
            g_module_close(module);
            return TRUE;
        }
    }

    if (n_backends >= PTC_MAX_BACKENDS) {
        g_module_close(module);
        return FALSE;
    }

    if (modules == NULL)
        modules = g_ptr_array_new();
    g_ptr_array_add(modules, module);

    backends[n_backends++] = backend;
    backends[n_backends] = NULL;
    return TRUE;
}

static void
ptc_backend_scan_dir(const char *dir)
{
    const gsize prefix_len = strlen(PTC_PLUGIN_PREFIX);
    const gsize suffix_len = strlen(PTC_PLUGIN_SUFFIX);
    GDir *gdir;
    const char *entry;

    if (dir == NULL || dir[0] == '\0')
        return;

    gdir = g_dir_open(dir, 0, NULL);
    if (gdir == NULL)
        return;

    while ((entry = g_dir_read_name(gdir)) != NULL) {
        gsize entry_len = strlen(entry);
        char *name;

        if (entry_len <= prefix_len + suffix_len)
            continue;
        if (!g_str_has_prefix(entry, PTC_PLUGIN_PREFIX))
            continue;
        if (!g_str_has_suffix(entry, PTC_PLUGIN_SUFFIX))
            continue;

        name = g_strndup(entry + prefix_len, entry_len - prefix_len - suffix_len);
        ptc_backend_try_load(dir, name);
        g_free(name);
    }

    g_dir_close(gdir);
}

/* Stable, alphabetical ordering so that the enumeration is deterministic
 * regardless of the order the filesystem returns directory entries in. */
static void
ptc_backend_sort(void)
{
    for (guint i = 1; i < n_backends; i++) {
        const ptc_fs_backend *key = backends[i];
        guint j = i;

        while (j > 0 && g_ascii_strcasecmp(backends[j - 1]->name, key->name) > 0) {
            backends[j] = backends[j - 1];
            j--;
        }
        backends[j] = key;
    }
}

/* Directory that holds libpartclone.so itself, so that the backend modules can
 * be found both in an installed tree (<dir>/partclone) and in the build tree
 * (<dir>/.libs, where the modules sit next to the library). */
static char *
ptc_backend_self_dir(void)
{
    Dl_info info;

    if (dladdr((void *)ptc_fs_backend_list, &info) != 0 &&
        info.dli_fname != NULL && info.dli_fname[0] != '\0')
        return g_path_get_dirname(info.dli_fname);

    return NULL;
}

static void
ptc_backends_init(void)
{
    const char *env;

    if (backends_loaded)
        return;

    env = g_getenv("PARTCLONE_PLUGIN_DIR");
    if (env != NULL && env[0] != '\0') {
        /* An explicit override is authoritative. */
        ptc_backend_scan_dir(env);
    } else {
        char *self = ptc_backend_self_dir();

        if (self != NULL) {
            char *installed = g_build_filename(self, "partclone", NULL);

            /* Installed layout: $(libdir)/partclone. */
            ptc_backend_scan_dir(installed);
            /* Uninstalled build tree: the modules live next to the core. */
            ptc_backend_scan_dir(self);

            g_free(installed);
            g_free(self);
        }

        /* Last resort when the library location could not be determined. */
        ptc_backend_scan_dir(PTC_PLUGIN_DIR);
    }

    if (n_backends > 1)
        ptc_backend_sort();

    backends_loaded = TRUE;
}

static void
ptc_backends_ensure_loaded(void)
{
    g_mutex_lock(&backend_lock);
    ptc_backends_init();
    g_mutex_unlock(&backend_lock);
}

/* ------------------------------------------------------------------ */
/* Lookup                                                              */
/* ------------------------------------------------------------------ */

const ptc_fs_backend *const *
ptc_fs_backend_list(void)
{
    ptc_backends_ensure_loaded();
    return backends;
}

/* Map historical command line / libblkid names onto canonical backend names. */
static const char *
ptc_fs_backend_canonical(const char *name)
{
    if (g_ascii_strcasecmp(name, "ext2") == 0 ||
        g_ascii_strcasecmp(name, "ext3") == 0 ||
        g_ascii_strcasecmp(name, "ext4") == 0)
        return "extfs";
    if (g_ascii_strcasecmp(name, "hfsplus") == 0 ||
        g_ascii_strcasecmp(name, "hfs+") == 0)
        return "hfsp";
    if (g_ascii_strcasecmp(name, "vfat") == 0 ||
        g_ascii_strcasecmp(name, "fat12") == 0 ||
        g_ascii_strcasecmp(name, "fat16") == 0 ||
        g_ascii_strcasecmp(name, "fat32") == 0)
        return "fat";
    if (g_ascii_strcasecmp(name, "dd") == 0 ||
        g_ascii_strcasecmp(name, "none") == 0)
        return "raw";
    return name;
}

const ptc_fs_backend *
ptc_fs_backend_lookup(const char *name)
{
    const char *canonical;

    if (name == NULL)
        return NULL;

    ptc_backends_ensure_loaded();

    canonical = ptc_fs_backend_canonical(name);
    for (guint i = 0; i < n_backends; i++) {
        if (g_ascii_strcasecmp(backends[i]->name, canonical) == 0)
            return backends[i];
    }

    return NULL;
}

const ptc_fs_backend *
ptc_fs_backend_by_magic(const char *magic)
{
    if (magic == NULL)
        return NULL;

    ptc_backends_ensure_loaded();

    for (guint i = 0; i < n_backends; i++) {
        if (backends[i]->magic != NULL &&
            strcmp(backends[i]->magic, magic) == 0)
            return backends[i];
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
    static const char *names[PTC_MAX_BACKENDS + 1];
    static gsize initialized = 0;

    if (g_once_init_enter(&initialized)) {
        guint i;

        ptc_backends_ensure_loaded();
        for (i = 0; i < n_backends; i++)
            names[i] = backends[i]->name;
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

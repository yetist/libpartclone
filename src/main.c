/**
 * The command line front end of libpartclone.
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * clone/restore partition to/from an image, device or stdout.
 *
 * Historically every partclone.<fs> tool was a complete copy of the clone
 * engine, linked against exactly one filesystem backend selected at compile
 * time.  All of that now lives in libpartclone.so and this small program only
 * maps its own name (argv[0]) to a default mode and a filesystem backend, then
 * hands over to the shared engine.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "partclone.h"
#include "engine.h"
#include "ptc-fs-backend.h"

/**
 * ptc_cli_classify_program:
 * @argv0: the program name as passed in argv[0].
 * @mode: (out): the default command line mode.
 * @backend_name: (out) (transfer none) (nullable): the backend name, or %NULL.
 *
 * Maps a partclone command line program name to a default mode and filesystem
 * backend, taking the historical aliases (partclone.ext2/3/4, partclone.vfat,
 * partclone.hfsplus, ...) into account.
 */
static void
ptc_cli_classify_program(const char *argv0,
                         ptc_cli_mode *mode,
                         const char **backend_name)
{
    const char *base = argv0 ? strrchr(argv0, '/') : NULL;
    const char *suffix;

    *mode = PTC_CLI_MODE_FULL;
    *backend_name = NULL;

    if (base != NULL)
        base++;
    else
        base = argv0;

    if (base == NULL || strncmp(base, "partclone.", 10) != 0)
        return;

    suffix = base + 10;

    if (strcmp(suffix, "restore") == 0) {
        *mode = PTC_CLI_MODE_RESTORE;
        return;
    }
    if (strcmp(suffix, "chkimg") == 0) {
        *mode = PTC_CLI_MODE_CHKIMG;
        return;
    }
    if (strcmp(suffix, "disk") == 0) {
        *mode = PTC_CLI_MODE_DISK;
        return;
    }
    if (strcmp(suffix, "dd") == 0) {
        *mode = PTC_CLI_MODE_DD;
        *backend_name = "raw";
        return;
    }
    if (strcmp(suffix, "imager") == 0) {
        *mode = PTC_CLI_MODE_FULL;
        *backend_name = "raw";
        return;
    }

    *backend_name = suffix;
}

static void
ptc_cli_list_backends(void)
{
    const ptc_fs_backend *const *it;

    fprintf(stderr, "Available filesystem backends:\n");
    for (it = ptc_fs_backend_list(); *it != NULL; it++)
        fprintf(stderr, "    %-10s %s\n", (*it)->name, (*it)->description);
}

/**
 * main - command line entry point shared by every partclone.<fs> tool.
 */
int
main(int argc, char **argv)
{
    ptc_cli_mode mode;
    const char *backend_name;
    const ptc_fs_backend *backend = NULL;

    ptc_cli_classify_program(argv[0], &mode, &backend_name);

    if (backend_name != NULL)
        backend = ptc_fs_backend_lookup(backend_name);

    if (backend_name != NULL && backend == NULL) {
        fprintf(stderr, "%s: unknown filesystem backend '%s'.\n",
                argv[0], backend_name);
        ptc_cli_list_backends();
        return 1;
    }

    /**
     * Parse the options for the selected mode.  This fills in the global
     * #opt (owned by libpartclone) and exits for --help / --version.
     */
    ptc_parse_options(argc, argv, &opt, mode);

    /**
     * partclone.disk handles whole-disk images (format 0003) with its own
     * engine; it never needs a filesystem backend nor auto detection.
     */
    if (mode == PTC_CLI_MODE_DISK)
        return ptc_disk_engine_run(NULL);

    /**
     * Restore and check modes do not need a primary backend: the filesystem
     * is stored in the image and post-clone fixups are dispatched by magic.
     * For clone/dd/domain without an explicit backend, try to auto detect the
     * filesystem on the source device.
     */
    if (backend == NULL && !opt.restore && !opt.chkimg) {
        backend = ptc_fs_backend_detect(opt.source);
        if (backend == NULL) {
            fprintf(stderr,
                    "%s: could not detect the filesystem on '%s'.\n",
                    get_exec_name(), opt.source);
            ptc_cli_list_backends();
            return 1;
        }
    }

    return ptc_engine_run(backend, NULL, NULL, NULL);
}

/**
 * libpartclone.h - public umbrella header of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * libpartclone turns the classic partclone tools into a shared library that
 * can be embedded in other applications.  The library depends on GLib,
 * GObject and GIO.
 *
 * This header is self contained: it only pulls in GLib/GObject/GIO and the
 * public #PtcClient API, never the internal clone-engine headers.  Link with
 * `pkg-config --cflags --libs partclone`.
 *
 * Typical use:
 *
 * |[<!-- language="C" -->
 * PtcClient *client = ptc_client_new ();
 * g_object_set (client,
 *               "mode",    PTC_MODE_CLONE,
 *               "source",  "/dev/sda1",
 *               "target",  "/backup/sda1.pcl",
 *               "backend", "extfs", // or NULL to auto detect
 *               NULL);
 * if (!ptc_client_run (client, NULL, &error))
 *     g_warning ("%s", error->message);
 * g_object_unref (client);
 * ]|
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef LIBPARTCLONE_H
#define LIBPARTCLONE_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "ptc-client.h"

G_BEGIN_DECLS

/**
 * ptc_version_string:
 *
 * Returns: the libpartclone version, e.g. `"0.3.50"`.
 */
const char *ptc_version_string(void);

/**
 * ptc_backend_names:
 *
 * Lists every filesystem backend compiled into this build of libpartclone.
 *
 * Returns: (transfer none) (array zero-terminated=1): a %NULL terminated array
 * of backend names such as `"extfs"` or `"btrfs"`.
 */
const char *const *ptc_backend_names(void);

/**
 * ptc_backend_available:
 * @name: a backend name or one of its historical aliases (e.g. `"ext4"`,
 *   `"vfat"`, `"hfsplus"`).
 *
 * Returns: %TRUE if @name can be used with PtcClient.
 */
gboolean ptc_backend_available(const char *name);

G_END_DECLS

#endif /* LIBPARTCLONE_H */

/**
 * ptc-image-probe.c - part of the partclone test suite
 *
 * A tiny front end used by the format 0003 tests to exercise the public
 * libpartclone GObject API:
 *
 *   ptc-image-probe info IMAGE
 *       Print the metadata of an image (PtcImage) and its volumes.
 *
 *   ptc-image-probe clone SOURCE TARGET LOGFILE [cancel]
 *       Whole-disk clone through PtcClient (PTC_MODE_DISK_CLONE) and report the
 *       ::volume-started / ::volume-finished signals.
 *
 *   ptc-image-probe restore SOURCE TARGET LOGFILE [cancel]
 *       Whole-disk restore through PtcClient (PTC_MODE_DISK_RESTORE).
 *
 *   ptc-image-probe fs-clone SOURCE TARGET LOGFILE [cancel]
 *       Plain (single filesystem/raw) clone through PtcClient (PTC_MODE_CLONE,
 *       raw backend) - used to check GCancellable support.
 *
 *   ptc-image-probe fs-restore SOURCE TARGET LOGFILE [cancel]
 *       Plain restore through PtcClient (PTC_MODE_RESTORE).
 *
 * When the trailing argument is "cancel" the GCancellable is cancelled before
 * the run starts; the probe then only succeeds if the operation was aborted
 * with G_IO_ERROR_CANCELLED.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "libpartclone.h"

static void
on_volume_started(PtcClient *client, guint index, guint count,
                  guint64 offset, guint64 length,
                  const gchar *fs, const gchar *label,
                  gpointer user_data)
{
    (void)client;
    (void)user_data;
    g_print("started %u/%u offset=%" G_GUINT64_FORMAT
            " length=%" G_GUINT64_FORMAT " fs=%s label=%s\n",
            index, count, offset, length, fs, label);
}

static void
on_volume_finished(PtcClient *client, guint index, guint count,
                   gpointer user_data)
{
    (void)client;
    (void)user_data;
    g_print("finished %u/%u\n", index, count);
}

static int
do_info(const char *path)
{
    GError *error = NULL;
    PtcImage *image;
    guint i, n;

    image = ptc_image_new_from_file(path, &error);
    if (image == NULL) {
        g_printerr("error: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    g_print("version=%s\n", ptc_image_get_version(image));
    g_print("is-disk=%s\n", ptc_image_is_disk(image) ? "yes" : "no");
    g_print("disk-size=%" G_GUINT64_FORMAT "\n", ptc_image_get_disk_size(image));
    g_print("sector-size=%u\n", ptc_image_get_sector_size(image));
    if (ptc_image_get_partition_table(image) != NULL)
        g_print("partition-table=%s\n", ptc_image_get_partition_table(image));
    if (ptc_image_get_filesystem(image) != NULL)
        g_print("filesystem=%s\n", ptc_image_get_filesystem(image));
    g_print("volumes=%u\n", ptc_image_get_volume_count(image));

    n = g_list_model_get_n_items(ptc_image_get_volumes(image));
    for (i = 0; i < n; i++) {
        PtcVolume *vol = ptc_image_get_volume(image, i);

        g_print("volume %u kind=%u part=%u offset=%" G_GUINT64_FORMAT
                " length=%" G_GUINT64_FORMAT " payload=%" G_GUINT64_FORMAT
                " fs=%s label=%s\n",
                ptc_volume_get_index(vol), ptc_volume_get_kind(vol),
                ptc_volume_get_part_number(vol),
                ptc_volume_get_disk_offset(vol),
                ptc_volume_get_disk_length(vol),
                ptc_volume_get_payload_length(vol),
                ptc_volume_get_filesystem(vol),
                ptc_volume_get_label(vol));
    }

    g_object_unref(image);
    return 0;
}

static int
do_run(PtcMode mode, const char *backend, const char *source, const char *target,
       const char *logfile, gboolean cancel)
{
    GError *error = NULL;
    GCancellable *cancellable = NULL;
    PtcClient *client;
    gboolean ok;
    int ret;

    client = ptc_client_new();
    g_object_set(client,
                 "mode", mode,
                 "backend", backend,   /* NULL means auto detect / none */
                 "source", source,
                 "target", target,
                 "logfile", logfile,
                 "quiet", TRUE,
                 "check", FALSE,
                 "overwrite", TRUE,
                 NULL);

    g_signal_connect(client, "volume-started", G_CALLBACK(on_volume_started), NULL);
    g_signal_connect(client, "volume-finished", G_CALLBACK(on_volume_finished), NULL);

    if (cancel) {
        /* Cancel before the engine starts so that the very first poll inside
         * the copy loop aborts the operation; this keeps the test deterministic. */
        cancellable = g_cancellable_new();
        g_cancellable_cancel(cancellable);
    }

    ok = ptc_client_run(client, cancellable, &error);
    if (ok) {
        g_print("cancelled=no\n");
        ret = cancel ? 1 : 0;   /* we asked to cancel but it completed */
    } else if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_print("cancelled=yes\n");
        ret = cancel ? 0 : 1;   /* cancelled without being asked: unexpected */
    } else {
        g_printerr("error: %s\n", error->message);
        ret = 1;
    }
    g_clear_error(&error);
    g_clear_object(&cancellable);
    g_object_unref(client);
    return ret;
}

int
main(int argc, char **argv)
{
    gboolean cancel;

    if (argc >= 3 && g_strcmp0(argv[1], "info") == 0)
        return do_info(argv[2]);

    if (argc < 5)
        goto usage;

    cancel = (argc >= 6 && g_strcmp0(argv[5], "cancel") == 0);

    if (g_strcmp0(argv[1], "clone") == 0)
        return do_run(PTC_MODE_DISK_CLONE, NULL, argv[2], argv[3], argv[4], cancel);
    if (g_strcmp0(argv[1], "restore") == 0)
        return do_run(PTC_MODE_DISK_RESTORE, NULL, argv[2], argv[3], argv[4], cancel);
    if (g_strcmp0(argv[1], "fs-clone") == 0)
        return do_run(PTC_MODE_CLONE, "raw", argv[2], argv[3], argv[4], cancel);
    if (g_strcmp0(argv[1], "fs-restore") == 0)
        return do_run(PTC_MODE_RESTORE, NULL, argv[2], argv[3], argv[4], cancel);

usage:
    g_printerr("usage: %s info IMAGE\n"
               "       %s clone SOURCE TARGET LOGFILE [cancel]\n"
               "       %s restore SOURCE TARGET LOGFILE [cancel]\n"
               "       %s fs-clone SOURCE TARGET LOGFILE [cancel]\n"
               "       %s fs-restore SOURCE TARGET LOGFILE [cancel]\n",
               argv[0], argv[0], argv[0], argv[0], argv[0]);
    return 2;
}

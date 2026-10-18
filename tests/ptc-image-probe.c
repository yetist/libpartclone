/**
 * ptc-image-probe.c - part of the partclone test suite
 *
 * A tiny front end used by the format 0003 tests to exercise the public
 * libpartclone GObject API:
 *
 *   ptc-image-probe info IMAGE
 *       Print the metadata of an image (PtcImage) and its volumes.
 *
 *   ptc-image-probe clone SOURCE TARGET LOGFILE
 *       Whole-disk clone through PtcClient (PTC_MODE_DISK_CLONE) and report the
 *       ::volume-started / ::volume-finished signals.
 *
 *   ptc-image-probe restore SOURCE TARGET LOGFILE
 *       Whole-disk restore through PtcClient (PTC_MODE_DISK_RESTORE).
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
do_run(PtcMode mode, const char *source, const char *target, const char *logfile)
{
    GError *error = NULL;
    PtcClient *client;
    gboolean ok;
    int ret;

    client = ptc_client_new();
    g_object_set(client,
                 "mode", mode,
                 "source", source,
                 "target", target,
                 "logfile", logfile,
                 "quiet", TRUE,
                 "check", FALSE,
                 "overwrite", TRUE,
                 NULL);

    g_signal_connect(client, "volume-started", G_CALLBACK(on_volume_started), NULL);
    g_signal_connect(client, "volume-finished", G_CALLBACK(on_volume_finished), NULL);

    ok = ptc_client_run(client, NULL, &error);
    if (!ok) {
        g_printerr("error: %s\n", error->message);
        g_error_free(error);
    }

    ret = ok ? 0 : 1;
    g_object_unref(client);
    return ret;
}

int
main(int argc, char **argv)
{
    if (argc >= 3 && g_strcmp0(argv[1], "info") == 0)
        return do_info(argv[2]);

    if (argc >= 5 && g_strcmp0(argv[1], "clone") == 0)
        return do_run(PTC_MODE_DISK_CLONE, argv[2], argv[3], argv[4]);

    if (argc >= 5 && g_strcmp0(argv[1], "restore") == 0)
        return do_run(PTC_MODE_DISK_RESTORE, argv[2], argv[3], argv[4]);

    g_printerr("usage: %s info IMAGE\n"
               "       %s clone SOURCE TARGET LOGFILE\n"
               "       %s restore SOURCE TARGET LOGFILE\n",
               argv[0], argv[0], argv[0]);
    return 2;
}

/*
 * ptc-demo.c - minimal libpartclone consumer
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Demonstrates the public GObject/GIO API of libpartclone:
 *   - enumerate the compiled in filesystem backends,
 *   - clone a device/image into a partclone image (synchronous),
 *   - report progress through the ::progress signal,
 *   - restore the image (synchronous),
 *   - verify the image (asynchronous, via GTask).
 *
 * Build out of tree:
 *   cc ptc-demo.c $(pkg-config --cflags --libs partclone) -o ptc-demo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <glib.h>
#include <gio/gio.h>

#include <libpartclone.h>

static guint n_progress = 0;

static void
on_progress(PtcClient *client, guint64 copied, guint64 total, gpointer user_data)
{
    (void)client;
    (void)user_data;

    /* The signal is emitted from the engine's progress thread. */
    if (++n_progress == 1)
        g_print("  progress: %" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT "\n",
                copied, total);
}

static gboolean
run_sync(PtcMode mode, const char *source, const char *target,
         const char *backend)
{
    PtcClient *client = ptc_client_new();
    GError *error = NULL;
    gboolean ok;

    g_object_set(client,
                 "mode",    mode,
                 "source",  source,
                 "target",  target,
                 "backend", backend, /* NULL means auto detect */
                 "logfile", "/tmp/ptc-demo.log",
                 "quiet",   TRUE,
                 "check",   FALSE, /* allow restoring onto a new file */
                 NULL);
    g_signal_connect(client, "progress", G_CALLBACK(on_progress), NULL);

    ok = ptc_client_run(client, NULL, &error);
    if (!ok) {
        g_printerr("  %s\n", error->message);
        g_error_free(error);
    }

    g_object_unref(client);
    return ok;
}

typedef struct {
    GMainLoop *loop;
    gboolean ok;
} AsyncState;

static void
on_async_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
    AsyncState *state = user_data;
    GError *error = NULL;

    state->ok = ptc_client_run_finish(PTC_CLIENT(source), result, &error);
    if (!state->ok) {
        g_printerr("  %s\n", error->message);
        g_error_free(error);
    }
    g_main_loop_quit(state->loop);
}

static gboolean
run_async_chkimg(const char *image)
{
    AsyncState state = { .loop = g_main_loop_new(NULL, FALSE), .ok = FALSE };
    PtcClient *client = ptc_client_new();

    g_object_set(client,
                 "mode",    PTC_MODE_CHKIMG,
                 "source",  image,
                 "logfile", "/tmp/ptc-demo.log",
                 "quiet",   TRUE,
                 NULL);

    ptc_client_run_async(client, NULL, on_async_done, &state);
    g_main_loop_run(state.loop);

    g_main_loop_unref(state.loop);
    g_object_unref(client);
    return state.ok;
}

int
main(int argc, char **argv)
{
    gchar *source;
    const char *image = "/tmp/ptc-demo.pcl";
    GPtrArray *target = g_ptr_array_new();
    gboolean ok = TRUE;

    g_print("libpartclone %s\n", ptc_version_string());

    g_print("backends:");
    for (const char *const *name = ptc_backend_names(); *name; name++)
        g_print(" %s", *name);
    g_print("\n");

    /* Build a small in-memory style source file. */
    source = g_strdup("/tmp/ptc-demo.src");
    {
        GBytes *bytes = g_bytes_new_take(g_malloc0(1024 * 1024), 1024 * 1024);
        g_file_set_contents(source, g_bytes_get_data(bytes, NULL),
                            g_bytes_get_size(bytes), NULL);
        g_bytes_unref(bytes);
    }

    g_print("clone %s -> %s\n", source, image);
    /* "raw" takes the whole device; any other backend would auto detect. */
    ok = ok && run_sync(PTC_MODE_CLONE, source, image, "raw");

    g_print("restore %s -> /tmp/ptc-demo.out\n", image);
    /* No backend: it is read back from the image header. */
    ok = ok && run_sync(PTC_MODE_RESTORE, image, "/tmp/ptc-demo.out", NULL);

    g_print("check %s (async)\n", image);
    ok = ok && run_async_chkimg(image);

    g_print("%u progress signal(s), %s\n", n_progress, ok ? "OK" : "FAILED");

    g_ptr_array_free(target, TRUE);
    g_free(source);
    return ok ? 0 : 1;
}

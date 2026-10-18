/*
 * ptc-gtk4.c - libpartclone GTK4 front end
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * A GTK4 front end for the public libpartclone API.  It exercises:
 *   - enumerating the available filesystem backend modules,
 *   - clone / restore / check / dd / domain,
 *   - whole-disk clone / restore / check (format 0003) with the disk options,
 *   - the ::progress signal and, for whole-disk images, the per-volume
 *     ::volume-started / ::volume-progress / ::volume-finished signals,
 *   - inspecting an image with PtcImage / PtcVolume,
 *   - asynchronous execution (ptc_client_run_async) plus cancellation through a
 *     GCancellable, so the UI stays alive and an operation can be stopped.
 *
 * The user interface is fully translated through the shared "partclone"
 * gettext domain (see po/).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libpartclone.h>

typedef struct {
    GtkApplication *app;
    GtkWindow *window;

    GtkDropDown *mode;
    GtkDropDown *backend;
    const char *const *backends;   /* ptc_backend_names(); index 0 is "auto" */

    GtkEntry *source;
    GtkEntry *target;
    GtkEntry *logfile;
    GtkEntry *device_size;         /* bytes, empty or 0 means auto */
    GtkSpinButton *debug;

    GtkWidget *target_label;
    GtkWidget *target_browse;

    GtkCheckButton *opt_check;
    GtkCheckButton *opt_force;
    GtkCheckButton *opt_quiet;
    GtkCheckButton *opt_overwrite;

    /* Whole-disk (format 0003) options; only meaningful for the disk modes. */
    GtkWidget *disk_frame;
    GtkDropDown *opt_gaps;
    GtkDropDown *opt_ptable;
    GtkCheckButton *opt_sparse;
    GtkCheckButton *opt_no_zero_skipped;
    GtkCheckButton *opt_no_fs_volumes;
    GtkCheckButton *opt_grow;

    GtkButton *run;
    GtkButton *cancel;
    GtkButton *clear;
    GtkProgressBar *progress;
    GtkWidget *volume_row;
    GtkProgressBar *volume_progress;
    GtkLabel *volume_status;
    GtkLabel *status;
    GtkTextView *logview;

    GCancellable *cancellable;
    gboolean quit_after_run;
    gboolean cancelling;

    PtcClient *client;
    gboolean running;
    int self_test;                 /* 0 off, 1..4 steps, 5 done */
} PtcGtkApp;

static PtcGtkApp app;

/* --self-test drives the whole flow without user interaction. */
static gboolean self_test_opt = FALSE;
static int self_test_status = 0;

/* Optional values taken from the command line. */
static gchar *opt_source = NULL;
static gchar *opt_target = NULL;
static gchar *opt_backend = NULL;
static gchar *opt_mode = NULL;
static gchar **opt_remaining = NULL;

static char *self_test_source    = NULL;
static char *self_test_image     = NULL;
static char *self_test_disk_src  = NULL;
static char *self_test_disk_img  = NULL;
static char *self_test_log       = NULL;

/* Give every run its own files so parallel invocations cannot collide. */
static void
self_test_init_paths(void)
{
    const char *tmp = g_get_tmp_dir();
    long pid = (long) getpid();

    self_test_source   = g_strdup_printf("%s/ptc-gtk4-selftest-%ld.src", tmp, pid);
    self_test_image    = g_strdup_printf("%s/ptc-gtk4-selftest-%ld.pcl", tmp, pid);
    self_test_disk_src = g_strdup_printf("%s/ptc-gtk4-selftest-%ld.disk.src", tmp, pid);
    self_test_disk_img = g_strdup_printf("%s/ptc-gtk4-selftest-%ld.disk.pcl", tmp, pid);
    self_test_log      = g_strdup_printf("%s/ptc-gtk4-selftest-%ld.log", tmp, pid);
}

static void
self_test_free_paths(void)
{
    g_free(self_test_source);
    g_free(self_test_image);
    g_free(self_test_disk_src);
    g_free(self_test_disk_img);
    g_free(self_test_log);
}

/* Keyboard accelerators. */
static const char *const accel_quit[]    = { "<Control>q", NULL };
static const char *const accel_run[]     = { "F5", NULL };
static const char *const accel_clear[]   = { "<Control>l", NULL };
static const char *const accel_inspect[] = { "<Control>i", NULL };
static const char *const accel_about[]   = { "F1", NULL };

/* Value lists for the whole-disk option drop downs.  These are the literal
 * strings the engine expects, so they are not translated. */
static const char *const gaps_values[]   = { "none", "metadata", "sparse", "full", NULL };
static const char *const ptable_values[] = { "auto", "raw", "regenerate", NULL };

/* The engine emits its signals from its own thread; the values below are
 * handed over to the main loop through a single coalesced idle source. */
typedef struct {
    guint64 copied;
    guint64 total;

    gboolean vol_active;
    guint    vol_index;
    guint    vol_count;
    guint64  vol_copied;
    guint64  vol_total;
    guint64  vol_offset;
    guint64  vol_length;
    char     vol_fs[64];
    char     vol_label[128];

    gboolean vol_done;
    guint    vol_done_index;
} ProgressState;

static GMutex progress_lock;
static gboolean progress_pending;
static ProgressState progress_state;
static guint ui_last_volume;        /* main thread only */

static void on_run_clicked(GtkButton *button, gpointer user_data);
static void on_clear_clicked(GtkButton *button, gpointer user_data);
static gboolean progress_idle(gpointer user_data);
static void on_inspect_chosen(GObject *source, GAsyncResult *result,
                              gpointer user_data);
static gboolean dialog_was_dismissed(const GError *error);
static gboolean self_test_next(gpointer user_data);
static void self_test_start(PtcGtkApp *self);
static GtkWidget *build_window(PtcGtkApp *self);

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void
setup_i18n(void)
{
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
}

static void
log_append(PtcGtkApp *self, const char *format, ...)
{
    GtkTextBuffer *buffer;
    GtkTextIter end;
    va_list args;
    char *message;

    va_start(args, format);
    message = g_strdup_vprintf(format, args);
    va_end(args);

    buffer = gtk_text_view_get_buffer(self->logview);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message, -1);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_view_scroll_to_iter(self->logview, &end, 0.0, FALSE, 0.0, 0.0);
    g_free(message);
}

static void
show_error(PtcGtkApp *self, const char *message)
{
    GtkAlertDialog *alert = gtk_alert_dialog_new("%s", message);

    log_append(self, "[!] %s", message);
    gtk_alert_dialog_show(alert, self->window);
    g_object_unref(alert);
}

static gboolean
mode_is_disk(PtcMode mode)
{
    return mode == PTC_MODE_DISK_CLONE ||
           mode == PTC_MODE_DISK_RESTORE ||
           mode == PTC_MODE_DISK_CHKIMG;
}

static gboolean
mode_needs_target(PtcMode mode)
{
    return mode != PTC_MODE_CHKIMG && mode != PTC_MODE_DISK_CHKIMG;
}

static PtcMode
mode_from_index(guint index)
{
    switch (index) {
    case 0:  return PTC_MODE_CLONE;
    case 1:  return PTC_MODE_RESTORE;
    case 2:  return PTC_MODE_CHKIMG;
    case 3:  return PTC_MODE_DD;
    case 4:  return PTC_MODE_DOMAIN;
    case 5:  return PTC_MODE_DISK_CLONE;
    case 6:  return PTC_MODE_DISK_RESTORE;
    case 7:  return PTC_MODE_DISK_CHKIMG;
    default: return PTC_MODE_CLONE;
    }
}

static const char *
mode_name(PtcMode mode)
{
    switch (mode) {
    case PTC_MODE_CLONE:        return "clone";
    case PTC_MODE_RESTORE:      return "restore";
    case PTC_MODE_CHKIMG:       return "check";
    case PTC_MODE_DD:           return "dd";
    case PTC_MODE_DOMAIN:       return "domain";
    case PTC_MODE_DISK_CLONE:   return "disk-clone";
    case PTC_MODE_DISK_RESTORE: return "disk-restore";
    case PTC_MODE_DISK_CHKIMG:  return "disk-check";
    }
    return "?";
}

/* Human readable labels for the mode combo box, translated at runtime. */
static const char *const *
mode_labels(void)
{
    static const char *labels[9];

    labels[0] = _("Clone");
    labels[1] = _("Restore");
    labels[2] = _("Check image");
    labels[3] = _("Device to device (dd)");
    labels[4] = _("Domain log");
    labels[5] = _("Disk clone (whole disk)");
    labels[6] = _("Disk restore (whole disk)");
    labels[7] = _("Check disk image");
    labels[8] = NULL;
    return labels;
}

static int
mode_index_from_name(const char *name)
{
    static const char *const names[] = {
        "clone", "restore", "check", "dd", "domain",
        "disk-clone", "disk-restore", "disk-check", NULL
    };
    int i;

    if (name == NULL)
        return -1;
    for (i = 0; names[i] != NULL; i++) {
        if (g_ascii_strcasecmp(name, names[i]) == 0)
            return i;
    }
    return -1;
}

/* Index 0 is "auto (detect)"; the rest follow ptc_backend_names(). */
static const char *
backend_from_index(PtcGtkApp *self)
{
    guint index = gtk_drop_down_get_selected(self->backend);

    if (index == GTK_INVALID_LIST_POSITION || index == 0)
        return NULL;
    return self->backends[index - 1];
}

static int
backend_index_from_name(PtcGtkApp *self, const char *name)
{
    guint i;

    if (name == NULL || *name == '\0')
        return 0;
    for (i = 0; self->backends[i] != NULL; i++) {
        if (g_strcmp0(self->backends[i], name) == 0)
            return (int) i + 1;
    }
    return -1;
}

/* Parse a byte count; an empty string means "auto" (0). */
static gboolean
parse_size(const char *text, guint64 *out)
{
    gchar *end = NULL;
    guint64 value;

    if (text == NULL || *text == '\0') {
        *out = 0;
        return TRUE;
    }

    errno = 0;
    value = g_ascii_strtoull(text, &end, 0);
    if (errno != 0 || end == text || (end != NULL && *end != '\0'))
        return FALSE;

    *out = value;
    return TRUE;
}

static void
set_running(PtcGtkApp *self, gboolean running)
{
    GAction *action;

    self->running = running;
    gtk_widget_set_sensitive(GTK_WIDGET(self->run), !running);
    gtk_widget_set_sensitive(GTK_WIDGET(self->cancel), running);

    action = g_action_map_lookup_action(G_ACTION_MAP(self->app), "run");
    if (action != NULL)
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), !running);
}

/* ------------------------------------------------------------------ */
/* Progress                                                            */
/* ------------------------------------------------------------------ */

static void
progress_schedule(PtcGtkApp *self)
{
    gboolean schedule;

    g_mutex_lock(&progress_lock);
    schedule = !progress_pending;
    progress_pending = TRUE;
    g_mutex_unlock(&progress_lock);

    if (schedule)
        g_main_context_invoke_full(NULL, G_PRIORITY_DEFAULT,
                                   progress_idle, self, NULL);
}

static char *
volume_text(const ProgressState *s)
{
    const char *fs = s->vol_fs[0] != '\0' ? s->vol_fs : _("raw");

    if (s->vol_label[0] != '\0')
        return g_strdup_printf(_("Volume %u/%u: %s (%s)"),
                               s->vol_index, s->vol_count, fs, s->vol_label);
    return g_strdup_printf(_("Volume %u/%u: %s"),
                           s->vol_index, s->vol_count, fs);
}

static gboolean
progress_idle(gpointer user_data)
{
    PtcGtkApp *self = user_data;
    ProgressState snapshot;
    char *text;

    g_mutex_lock(&progress_lock);
    snapshot = progress_state;
    progress_pending = FALSE;
    progress_state.vol_done = FALSE;
    g_mutex_unlock(&progress_lock);

    if (snapshot.total > 0) {
        char *copied_text = g_format_size_full(snapshot.copied, G_FORMAT_SIZE_IEC_UNITS);
        char *total_text  = g_format_size_full(snapshot.total, G_FORMAT_SIZE_IEC_UNITS);

        gtk_progress_bar_set_fraction(self->progress,
                                      (double)snapshot.copied / (double)snapshot.total);
        text = g_strdup_printf("%s / %s (%.1f%%)", copied_text, total_text,
                               100.0 * (double)snapshot.copied / (double)snapshot.total);
        g_free(copied_text);
        g_free(total_text);
    } else {
        text = g_strdup_printf("%" G_GUINT64_FORMAT, snapshot.copied);
    }

    gtk_progress_bar_set_text(self->progress, text);
    g_free(text);

    /* A late progress notification after the run finished must not overwrite
     * the final status line. */
    if (!self->running)
        return G_SOURCE_REMOVE;

    /* Per-volume progress of a whole-disk operation. */
    if (snapshot.vol_active) {
        char *vol_text = volume_text(&snapshot);

        gtk_widget_set_visible(self->volume_row, TRUE);
        gtk_label_set_text(self->volume_status, vol_text);
        gtk_progress_bar_set_fraction(self->volume_progress,
            snapshot.vol_total > 0
                ? (double)snapshot.vol_copied / (double)snapshot.vol_total
                : 0.0);

        if (snapshot.vol_index != ui_last_volume) {
            log_append(self, "%s", vol_text);
            ui_last_volume = snapshot.vol_index;
        }
        g_free(vol_text);
    }

    if (snapshot.vol_done && snapshot.vol_done_index == ui_last_volume)
        log_append(self, _("Volume %u/%u done."),
                   snapshot.vol_done_index, snapshot.vol_count);

    gtk_label_set_text(self->status,
                       self->cancelling ? _("Cancelling...") : _("Running..."));
    return G_SOURCE_REMOVE;
}

static void
on_progress(PtcClient *client, guint64 copied, guint64 total, gpointer user_data)
{
    PtcGtkApp *self = user_data;

    (void)client;

    g_mutex_lock(&progress_lock);
    progress_state.copied = copied;
    progress_state.total = total;
    g_mutex_unlock(&progress_lock);

    progress_schedule(self);
}

static void
on_volume_started(PtcClient *client, guint index, guint count,
                  guint64 offset, guint64 length,
                  const char *filesystem, const char *label,
                  gpointer user_data)
{
    PtcGtkApp *self = user_data;

    (void)client;

    g_mutex_lock(&progress_lock);
    progress_state.vol_active = TRUE;
    progress_state.vol_index = index;
    progress_state.vol_count = count;
    progress_state.vol_offset = offset;
    progress_state.vol_length = length;
    progress_state.vol_copied = 0;
    progress_state.vol_total = 0;
    g_strlcpy(progress_state.vol_fs, filesystem != NULL ? filesystem : "",
              sizeof(progress_state.vol_fs));
    g_strlcpy(progress_state.vol_label, label != NULL ? label : "",
              sizeof(progress_state.vol_label));
    g_mutex_unlock(&progress_lock);

    progress_schedule(self);
}

static void
on_volume_progress(PtcClient *client, guint index, guint count,
                   guint64 copied, guint64 total, gpointer user_data)
{
    PtcGtkApp *self = user_data;

    (void)client;

    g_mutex_lock(&progress_lock);
    progress_state.vol_index = index;
    progress_state.vol_count = count;
    progress_state.vol_copied = copied;
    progress_state.vol_total = total;
    g_mutex_unlock(&progress_lock);

    progress_schedule(self);
}

static void
on_volume_finished(PtcClient *client, guint index, guint count,
                   gpointer user_data)
{
    PtcGtkApp *self = user_data;

    (void)client;

    g_mutex_lock(&progress_lock);
    progress_state.vol_index = index;
    progress_state.vol_count = count;
    progress_state.vol_done = TRUE;
    progress_state.vol_done_index = index;
    g_mutex_unlock(&progress_lock);

    progress_schedule(self);
}

/* ------------------------------------------------------------------ */
/* Running                                                             */
/* ------------------------------------------------------------------ */

static void
on_run_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GError *error = NULL;
    gboolean ok = ptc_client_run_finish(PTC_CLIENT(source), result, &error);
    gboolean cancelled = (!ok && error != NULL &&
                          g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED));

    if (ok) {
        gtk_progress_bar_set_fraction(self->progress, 1.0);
        gtk_label_set_text(self->status, _("Finished"));
        log_append(self, _("--- finished ---"));
    } else if (cancelled) {
        gtk_label_set_text(self->status, _("Cancelled"));
        log_append(self, _("--- cancelled ---"));
    } else {
        gtk_label_set_text(self->status, _("Failed"));
        log_append(self, _("--- error: %s ---"),
                   error != NULL ? error->message : _("unknown error"));
    }
    g_clear_error(&error);

    self->cancelling = FALSE;
    g_clear_object(&self->cancellable);
    g_clear_object(&self->client);
    set_running(self, FALSE);

    /* A quit requested while running waits for the cancellation to settle. */
    if (self->quit_after_run) {
        self->quit_after_run = FALSE;
        g_application_quit(G_APPLICATION(self->app));
        return;
    }

    if (!ok && self->self_test != 0) {
        self->self_test = 0;
        self_test_status = 1;
        g_application_quit(G_APPLICATION(self->app));
        return;
    }

    switch (self->self_test) {
    case 1:
        /* The classic clone finished; verify the image next. */
        self->self_test = 2;
        log_append(self, _("self-test: verifying the image"));
        gtk_editable_set_text(GTK_EDITABLE(self->source), self_test_image);
        gtk_drop_down_set_selected(self->mode, 2);   /* Check image */
        g_idle_add(self_test_next, self);
        break;
    case 2:
        /* The classic round-trip works; do a whole-disk round-trip next. */
        self->self_test = 3;
        log_append(self, _("self-test: cloning a whole disk"));
        g_remove(self_test_disk_img);
        gtk_drop_down_set_selected(self->backend, 0);        /* auto */
        gtk_editable_set_text(GTK_EDITABLE(self->source), self_test_disk_src);
        gtk_editable_set_text(GTK_EDITABLE(self->target), self_test_disk_img);
        gtk_drop_down_set_selected(self->mode, 5);           /* Disk clone */
        g_idle_add(self_test_next, self);
        break;
    case 3:
        /* The whole-disk clone finished; check the format 0003 image. */
        self->self_test = 4;
        log_append(self, _("self-test: checking the whole-disk image"));
        gtk_editable_set_text(GTK_EDITABLE(self->source), self_test_disk_img);
        gtk_drop_down_set_selected(self->mode, 7);           /* Check disk image */
        g_idle_add(self_test_next, self);
        break;
    case 4:
        self->self_test = 5;
        self_test_status = 0;
        g_print("self-test OK\n");
        g_application_quit(G_APPLICATION(self->app));
        break;
    default:
        break;
    }
}

static gboolean
self_test_next(gpointer user_data)
{
    on_run_clicked(NULL, user_data);
    return G_SOURCE_REMOVE;
}

static void
on_run_clicked(GtkButton *button, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    const char *source, *target, *logfile, *backend;
    const char *gaps, *ptable;
    guint64 device_size;
    gboolean skip_check, disk;
    PtcMode mode;
    FILE *log;

    (void)button;

    if (self->running)
        return;

    source = gtk_editable_get_text(GTK_EDITABLE(self->source));
    target = gtk_editable_get_text(GTK_EDITABLE(self->target));
    logfile = gtk_editable_get_text(GTK_EDITABLE(self->logfile));
    mode = mode_from_index(gtk_drop_down_get_selected(self->mode));
    disk = mode_is_disk(mode);
    backend = disk ? NULL : backend_from_index(self);

    if (source[0] == '\0') {
        show_error(self, _("Please choose a source device or image."));
        return;
    }
    if (mode_needs_target(mode) && target[0] == '\0') {
        show_error(self, _("Please choose a target."));
        return;
    }
    if (logfile[0] == '\0') {
        show_error(self, _("Please choose a log file."));
        return;
    }
    if (!parse_size(gtk_editable_get_text(GTK_EDITABLE(self->device_size)),
                    &device_size)) {
        show_error(self, _("The device size must be a number of bytes."));
        return;
    }

    /* The engine exits the process when it cannot open its log file, so fail
     * early with a proper message instead. */
    log = fopen(logfile, "a");
    if (log == NULL) {
        show_error(self, _("Cannot open the log file for writing."));
        return;
    }
    fclose(log);

    gaps = gaps_values[gtk_drop_down_get_selected(self->opt_gaps)];
    ptable = ptable_values[gtk_drop_down_get_selected(self->opt_ptable)];

    self->client = ptc_client_new();
    self->cancellable = g_cancellable_new();
    skip_check = gtk_check_button_get_active(self->opt_check);

    g_object_set(self->client,
                 "mode",      mode,
                 "source",    source,
                 "target",    target,
                 "backend",   backend,        /* NULL means auto detect */
                 "logfile",   logfile,
                 "quiet",     gtk_check_button_get_active(self->opt_quiet),
                 "check",     !skip_check,    /* -C clears the size checks */
                 "force",     gtk_check_button_get_active(self->opt_force),
                 "overwrite", gtk_check_button_get_active(self->opt_overwrite),
                 "debug",     (guint) gtk_spin_button_get_value_as_int(self->debug),
                 "device-size", device_size,
                 "sparse",          gtk_check_button_get_active(self->opt_sparse),
                 "no-zero-skipped", gtk_check_button_get_active(self->opt_no_zero_skipped),
                 "no-fs-volumes",   gtk_check_button_get_active(self->opt_no_fs_volumes),
                 "grow",            gtk_check_button_get_active(self->opt_grow),
                 "gaps",            gaps,
                 "partition-table", ptable,
                 NULL);

    g_signal_connect(self->client, "progress", G_CALLBACK(on_progress), self);
    g_signal_connect(self->client, "volume-started", G_CALLBACK(on_volume_started), self);
    g_signal_connect(self->client, "volume-progress", G_CALLBACK(on_volume_progress), self);
    g_signal_connect(self->client, "volume-finished", G_CALLBACK(on_volume_finished), self);

    log_append(self, _("=== %s ==="), mode_name(mode));
    log_append(self, _("source : %s"), source);
    if (mode_needs_target(mode))
        log_append(self, _("target : %s"), target);
    if (disk) {
        log_append(self, _("backend: %s"), _("(whole-disk image, none)"));
        log_append(self, _("gaps   : %s"), gaps);
        log_append(self, _("table  : %s"), ptable);
    } else {
        log_append(self, _("backend: %s"),
                   backend != NULL ? backend : _("auto (detect)"));
    }

    gtk_progress_bar_set_fraction(self->progress, 0.0);
    gtk_progress_bar_set_text(self->progress, _("starting"));
    gtk_label_set_text(self->status, _("Running..."));

    ui_last_volume = 0;
    gtk_progress_bar_set_fraction(self->volume_progress, 0.0);
    gtk_label_set_text(self->volume_status, _("Waiting for the first volume..."));
    gtk_widget_set_visible(self->volume_row, disk);

    set_running(self, TRUE);
    self->cancelling = FALSE;
    ptc_client_run_async(self->client, self->cancellable, on_run_done, self);
}

static void
on_cancel_clicked(GtkButton *button, gpointer user_data)
{
    PtcGtkApp *self = user_data;

    (void)button;

    if (!self->running || self->cancellable == NULL)
        return;

    log_append(self, _("Cancelling..."));
    gtk_label_set_text(self->status, _("Cancelling..."));
    self->cancelling = TRUE;
    g_cancellable_cancel(self->cancellable);
}

static void
on_clear_clicked(GtkButton *button, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(self->logview);

    (void)button;
    gtk_text_buffer_set_text(buffer, "", -1);
}

/* ------------------------------------------------------------------ */
/* Image inspection (PtcImage / PtcVolume)                             */
/* ------------------------------------------------------------------ */

static const char *
volume_kind_name(PtcVolumeKind kind)
{
    switch (kind) {
    case PTC_VOLUME_KIND_FILESYSTEM: return "filesystem";
    case PTC_VOLUME_KIND_RAW_SPARSE: return "raw-sparse";
    case PTC_VOLUME_KIND_RAW_FULL:   return "raw-full";
    case PTC_VOLUME_KIND_ZERO:       return "zero";
    default:                         return "none";
    }
}

static char *
image_description(PtcImage *image)
{
    GString *text = g_string_new(NULL);
    char *size = g_format_size_full(ptc_image_get_disk_size(image),
                                    G_FORMAT_SIZE_IEC_UNITS);

    g_string_append_printf(text, "%s: %s\n", _("Format version"),
                           ptc_image_get_version(image));
    g_string_append_printf(text, "%s: %s\n", _("Size"), size);
    g_string_append_printf(text, "%s: %u %s\n", _("Sector size"),
                           ptc_image_get_sector_size(image), _("bytes"));
    g_free(size);

    if (ptc_image_is_disk(image)) {
        const char *table = ptc_image_get_partition_table(image);
        guint count = ptc_image_get_volume_count(image);
        guint i;

        g_string_append_printf(text, "%s: %s\n", _("Partition table"),
                               table != NULL ? table : _("none"));
        g_string_append_printf(text, "%s: %u\n", _("Volumes"), count);

        if (count > 0)
            g_string_append_c(text, '\n');

        for (i = 0; i < count; i++) {
            PtcVolume *volume = ptc_image_get_volume(image, i);
            const char *fs, *label;
            char *offset, *length;

            if (volume == NULL)
                continue;

            offset = g_format_size_full(ptc_volume_get_disk_offset(volume),
                                        G_FORMAT_SIZE_IEC_UNITS);
            length = g_format_size_full(ptc_volume_get_disk_length(volume),
                                        G_FORMAT_SIZE_IEC_UNITS);
            fs = ptc_volume_get_filesystem(volume);
            label = ptc_volume_get_label(volume);

            g_string_append_printf(text,
                _("Volume %u: %s (%s) at %s, length %s, %s%s\n"),
                ptc_volume_get_index(volume),
                volume_kind_name(ptc_volume_get_kind(volume)),
                ptc_volume_is_sparse(volume) ? _("sparse") : _("full"),
                offset, length,
                fs != NULL ? fs : (label != NULL ? label : _("none")),
                ptc_volume_is_bootable(volume) ? _(", bootable") : "");
            g_free(offset);
            g_free(length);
        }
    } else {
        const char *fs = ptc_image_get_filesystem(image);

        g_string_append_printf(text, "%s: %s\n", _("File system"),
                               fs != NULL ? fs : _("raw"));
    }

    return g_string_free(text, FALSE);
}

static void
show_text_dialog(PtcGtkApp *self, const char *title, const char *text)
{
    GtkWidget *window = gtk_window_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *view = gtk_text_view_new();
    GtkWidget *close = gtk_button_new_with_label(_("Close"));
    GtkTextBuffer *buffer;

    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), 720, 480);
    gtk_window_set_transient_for(GTK_WINDOW(window), self->window);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_set_text(buffer, text, -1);

    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
    gtk_box_append(GTK_BOX(box), scroll);

    gtk_widget_set_halign(close, GTK_ALIGN_END);
    gtk_widget_set_margin_end(close, 4);
    gtk_box_append(GTK_BOX(box), close);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);

    gtk_window_set_child(GTK_WINDOW(window), box);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), window);
    gtk_window_present(GTK_WINDOW(window));
}

static void
show_image_info(PtcGtkApp *self, const char *path)
{
    GError *error = NULL;
    PtcImage *image = ptc_image_new_from_file(path, &error);
    char *text;

    if (image == NULL) {
        show_error(self, error != NULL ? error->message : _("Cannot read the image."));
        g_clear_error(&error);
        return;
    }

    text = image_description(image);
    log_append(self, _("=== image: %s ==="), path);
    log_append(self, "%s", text);
    show_text_dialog(self, path, text);

    g_free(text);
    g_object_unref(image);
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */

static void
on_run_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    (void)action;
    (void)parameter;
    on_run_clicked(NULL, user_data);
}

static void
on_clear_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    (void)action;
    (void)parameter;
    on_clear_clicked(NULL, user_data);
}

static void
on_inspect_chosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result,
                                              &error);

    if (file != NULL) {
        char *path = g_file_get_path(file);

        if (path != NULL) {
            show_image_info(self, path);
            g_free(path);
        }
        g_object_unref(file);
    } else if (error != NULL && !dialog_was_dismissed(error)) {
        log_append(self, "[!] %s", error->message);
    }
    g_clear_error(&error);
}

static void
on_inspect_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(self->source));

    (void)action;
    (void)parameter;

    if (path[0] != '\0') {
        show_image_info(self, path);
        return;
    }

    {
        GtkFileDialog *dialog = gtk_file_dialog_new();

        gtk_file_dialog_set_title(dialog, _("Select an image to inspect"));
        gtk_file_dialog_open(dialog, self->window, NULL, on_inspect_chosen, self);
        g_object_unref(dialog);
    }
}

static void
on_about_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    static const char *authors[] = {
        "Thomas Tsai <thomas@nchc.org.tw>",
        NULL
    };
    GtkWidget *about;

    (void)action;
    (void)parameter;

    about = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), "partclone");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), ptc_version_string());
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about),
        _("GTK4 front end for the libpartclone partition cloning library."));
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(about),
                                      GTK_LICENSE_GPL_2_0);
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about),
                                        "drive-harddisk");
    gtk_window_set_transient_for(GTK_WINDOW(about), self->window);
    gtk_window_set_modal(GTK_WINDOW(about), TRUE);
    gtk_window_present(GTK_WINDOW(about));
}

static void
on_exit_confirmed(GObject *source, GAsyncResult *result, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GError *error = NULL;
    int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source),
                                                result, &error);

    g_clear_error(&error);
    if (choice != 1)
        return;

    if (self->running && self->cancellable != NULL) {
        /* Stop the operation cleanly, then quit once it has settled. */
        self->quit_after_run = TRUE;
        self->cancelling = TRUE;
        g_cancellable_cancel(self->cancellable);
    } else {
        g_application_quit(G_APPLICATION(self->app));
    }
}

static void
on_exit_action(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GtkAlertDialog *alert;

    (void)action;
    (void)parameter;

    if (!self->running) {
        g_application_quit(G_APPLICATION(self->app));
        return;
    }

    alert = gtk_alert_dialog_new("%s", _("An operation is still running."));
    gtk_alert_dialog_set_detail(alert,
        _("Quitting now will cancel the operation and leave the target incomplete."));
    gtk_alert_dialog_set_buttons(alert,
        (const char *[]) { _("Cancel"), _("Quit"), NULL });
    gtk_alert_dialog_set_cancel_button(alert, 0);
    gtk_alert_dialog_set_default_button(alert, 0);
    gtk_alert_dialog_choose(alert, self->window, NULL, on_exit_confirmed, self);
    g_object_unref(alert);
}

static gboolean
on_close_request(GtkWindow *window, gpointer user_data)
{
    PtcGtkApp *self = user_data;

    (void)window;

    if (!self->running)
        return FALSE;

    on_exit_action(NULL, NULL, self);
    return TRUE;   /* keep the window alive until the user decides */
}

static const GActionEntry app_actions[] = {
    { "run",     on_run_action,     NULL, NULL, NULL },
    { "clear",   on_clear_action,   NULL, NULL, NULL },
    { "inspect", on_inspect_action, NULL, NULL, NULL },
    { "about",   on_about_action,   NULL, NULL, NULL },
    { "exit",    on_exit_action,    NULL, NULL, NULL },
};

/* ------------------------------------------------------------------ */
/* File chooser                                                        */
/* ------------------------------------------------------------------ */

static gboolean
dialog_was_dismissed(const GError *error)
{
    return g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) ||
           g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED);
}

static void
on_source_chosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result,
                                              &error);

    if (file != NULL) {
        char *path = g_file_get_path(file);

        if (path != NULL) {
            gtk_editable_set_text(GTK_EDITABLE(self->source), path);
            g_free(path);
        }
        g_object_unref(file);
    } else if (error != NULL && !dialog_was_dismissed(error)) {
        log_append(self, "[!] %s", error->message);
    }
    g_clear_error(&error);
}

static void
on_browse_source(GtkButton *button, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();

    (void)button;

    gtk_file_dialog_set_title(dialog, _("Select source device or image"));
    gtk_file_dialog_open(dialog, self->window, NULL, on_source_chosen, self);
    g_object_unref(dialog);
}

static void
on_target_chosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result,
                                              &error);

    if (file != NULL) {
        char *path = g_file_get_path(file);

        if (path != NULL) {
            gtk_editable_set_text(GTK_EDITABLE(self->target), path);
            g_free(path);
        }
        g_object_unref(file);
    } else if (error != NULL && !dialog_was_dismissed(error)) {
        log_append(self, "[!] %s", error->message);
    }
    g_clear_error(&error);
}

static void
on_browse_target(GtkButton *button, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();

    (void)button;

    gtk_file_dialog_set_title(dialog, _("Select target device or image"));
    gtk_file_dialog_save(dialog, self->window, NULL, on_target_chosen, self);
    g_object_unref(dialog);
}

static void
on_log_chosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result,
                                              &error);

    if (file != NULL) {
        char *path = g_file_get_path(file);

        if (path != NULL) {
            gtk_editable_set_text(GTK_EDITABLE(self->logfile), path);
            g_free(path);
        }
        g_object_unref(file);
    } else if (error != NULL && !dialog_was_dismissed(error)) {
        log_append(self, "[!] %s", error->message);
    }
    g_clear_error(&error);
}

static void
on_browse_log(GtkButton *button, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();

    (void)button;

    gtk_file_dialog_set_title(dialog, _("Select log file"));
    gtk_file_dialog_save(dialog, self->window, NULL, on_log_chosen, self);
    g_object_unref(dialog);
}

/* ------------------------------------------------------------------ */
/* Self test (headless, used by --self-test and CI)                    */
/* ------------------------------------------------------------------ */

/* A 2 MiB disk image source: mostly zero with one pseudo-random region, so a
 * sparse clone has something to skip and something to keep. */
static gboolean
self_test_write_disk_source(void)
{
    const gsize size = 2 * 1024 * 1024;
    const gsize chunk = 64 * 1024;
    const gsize chunk_at = 1024 * 1024;
    guint8 *data = g_malloc0(size);
    GRand *rand = g_rand_new();
    GError *error = NULL;
    gboolean ok;
    gsize i;

    for (i = 0; i < chunk; i++)
        data[chunk_at + i] = (guint8) g_rand_int_range(rand, 0, 256);
    g_rand_free(rand);

    ok = g_file_set_contents(self_test_disk_src, (const gchar *) data, size,
                             &error);
    if (!ok) {
        g_printerr("self-test: cannot create %s: %s\n", self_test_disk_src,
                   error->message);
        g_clear_error(&error);
    }
    g_free(data);
    return ok;
}

static void
self_test_start(PtcGtkApp *self)
{
    gchar *data = g_malloc0(1024 * 1024);
    GError *error = NULL;
    guint i, raw_index = 0;

    self_test_init_paths();
    g_remove(self_test_image);
    g_remove(self_test_disk_img);

    if (!g_file_set_contents(self_test_source, data, 1024 * 1024, &error)) {
        g_printerr("self-test: cannot create %s: %s\n", self_test_source,
                   error->message);
        g_error_free(error);
        g_free(data);
        self_test_status = 1;
        g_application_quit(G_APPLICATION(self->app));
        return;
    }
    g_free(data);

    if (!self_test_write_disk_source()) {
        self_test_status = 1;
        g_application_quit(G_APPLICATION(self->app));
        return;
    }

    for (i = 0; self->backends[i] != NULL; i++) {
        if (g_strcmp0(self->backends[i], "raw") == 0) {
            raw_index = i + 1;
            break;
        }
    }

    gtk_editable_set_text(GTK_EDITABLE(self->source), self_test_source);
    gtk_editable_set_text(GTK_EDITABLE(self->target), self_test_image);
    gtk_editable_set_text(GTK_EDITABLE(self->logfile), self_test_log);
    gtk_drop_down_set_selected(self->mode, 0);        /* Clone */
    gtk_drop_down_set_selected(self->backend, raw_index);

    self->self_test = 1;
    log_append(self, _("self-test: cloning %s"), self_test_source);
    on_run_clicked(NULL, self);
}

/* ------------------------------------------------------------------ */
/* UI                                                                  */
/* ------------------------------------------------------------------ */

static GtkWidget *
grid_label(const char *text)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    return label;
}

static char *
backend_list_string(void)
{
    GString *string = g_string_new(NULL);
    const char *const *name;

    for (name = ptc_backend_names(); *name != NULL; name++) {
        if (string->len > 0)
            g_string_append_c(string, ' ');
        g_string_append(string, *name);
    }
    if (string->len == 0)
        g_string_append(string, "(none)");

    return g_string_free(string, FALSE);
}

/* Enable or disable fields depending on the selected mode. */
static void
update_mode_sensitivity(PtcGtkApp *self)
{
    PtcMode mode = mode_from_index(gtk_drop_down_get_selected(self->mode));
    gboolean disk = mode_is_disk(mode);
    gboolean needs_target = mode_needs_target(mode);
    gboolean clone = (mode == PTC_MODE_DISK_CLONE);
    gboolean restore = (mode == PTC_MODE_DISK_RESTORE);

    gtk_widget_set_sensitive(self->target_label, needs_target);
    gtk_widget_set_sensitive(GTK_WIDGET(self->target), needs_target);
    gtk_widget_set_sensitive(self->target_browse, needs_target);

    /* The filesystem backend is irrelevant for a whole-disk image. */
    gtk_widget_set_sensitive(GTK_WIDGET(self->backend), !disk);

    gtk_widget_set_sensitive(self->disk_frame, disk);
    gtk_widget_set_sensitive(GTK_WIDGET(self->opt_sparse), disk && clone);
    gtk_widget_set_sensitive(GTK_WIDGET(self->opt_no_fs_volumes), disk && clone);
    gtk_widget_set_sensitive(GTK_WIDGET(self->opt_grow), disk && restore);
    gtk_widget_set_sensitive(GTK_WIDGET(self->opt_no_zero_skipped), disk && restore);
}

static void
on_mode_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_mode_sensitivity(user_data);
}

static GtkWidget *
build_disk_options(PtcGtkApp *self)
{
    GtkWidget *frame = gtk_frame_new(_("Whole-disk image options (format 0003)"));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    self->opt_gaps = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(gaps_values));
    gtk_drop_down_set_selected(self->opt_gaps, 1);   /* metadata */
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_gaps),
        _("How the gaps between partitions (boot loaders, padding, backup GPT) "
          "are stored."));
    gtk_grid_attach(GTK_GRID(grid), grid_label(_("Gaps")), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->opt_gaps), 1, 0, 1, 1);

    self->opt_ptable = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(ptable_values));
    gtk_grid_attach(GTK_GRID(grid), grid_label(_("Partition table")), 0, 1, 1, 1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_ptable),
        _("How the partition table is written on restore: verbatim, or rebuilt "
          "for a target of a different size."));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->opt_ptable), 1, 1, 1, 1);
    gtk_box_append(GTK_BOX(box), grid);

    self->opt_sparse = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Store raw volumes sparsely")));
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_sparse),
        _("Use a zero-detecting bitmap for raw volumes (gaps and unsupported "
          "partitions)."));
    self->opt_no_fs_volumes = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Store every partition as raw")));
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_no_fs_volumes),
        _("Do not use the per-filesystem backend, so every partition is a raw "
          "volume."));
    self->opt_no_zero_skipped = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Do not zero skipped blocks")));
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_no_zero_skipped),
        _("On restore, leave blocks absent from the image untouched instead of "
          "writing zeros."));
    self->opt_grow = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Grow the last partition")));
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_grow),
        _("On a larger target, extend the last partition entry (the filesystem "
          "itself is not resized)."));

    gtk_box_append(GTK_BOX(row), GTK_WIDGET(self->opt_sparse));
    gtk_box_append(GTK_BOX(row), GTK_WIDGET(self->opt_no_fs_volumes));
    gtk_box_append(GTK_BOX(row), GTK_WIDGET(self->opt_no_zero_skipped));
    gtk_box_append(GTK_BOX(row), GTK_WIDGET(self->opt_grow));
    gtk_box_append(GTK_BOX(box), row);

    gtk_frame_set_child(GTK_FRAME(frame), box);
    return frame;
}

static GtkWidget *
build_header_bar(void)
{
    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *menu_button = gtk_menu_button_new();
    GMenu *menu = g_menu_new();
    GMenu *section = g_menu_new();

    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

    g_menu_append(menu, _("_Run"), "app.run");
    g_menu_append(menu, _("_Inspect image..."), "app.inspect");
    g_menu_append(menu, _("_Clear log"), "app.clear");
    g_menu_append(section, _("_About"), "app.about");
    g_menu_append(section, _("_Quit"), "app.exit");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button),
                                  "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button),
                                   G_MENU_MODEL(menu));
    gtk_widget_set_tooltip_text(menu_button, _("Main menu"));
    g_object_unref(menu);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menu_button);
    return header;
}

static GtkWidget *
build_window(PtcGtkApp *self)
{
    GtkWidget *window = gtk_application_window_new(self->app);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *options = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *browse, *clear;
    GtkWidget *scroll;
    char *info, *backend_list;
    guint n_backends = 0, i;
    const char **backend_strings;
    int row = 0;

    gtk_window_set_title(GTK_WINDOW(window), _("partclone"));
    gtk_window_set_default_size(GTK_WINDOW(window), 860, 760);
    gtk_window_set_titlebar(GTK_WINDOW(window), build_header_bar());
    gtk_widget_set_margin_start(root, 12);
    gtk_widget_set_margin_end(root, 12);
    gtk_widget_set_margin_top(root, 12);
    gtk_widget_set_margin_bottom(root, 12);
    gtk_window_set_child(GTK_WINDOW(window), root);

    /* Header: version and available backends. */
    backend_list = backend_list_string();
    info = g_strdup_printf(_("libpartclone %s\nbackends: %s"),
                           ptc_version_string(), backend_list);
    {
        GtkWidget *label = gtk_label_new(info);

        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_label_set_selectable(GTK_LABEL(label), TRUE);
        gtk_box_append(GTK_BOX(root), label);
    }
    g_free(info);
    g_free(backend_list);

    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_append(GTK_BOX(root), grid);

    /* Mode. */
    gtk_grid_attach(GTK_GRID(grid), grid_label(_("Mode")), 0, row, 1, 1);
    self->mode = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(mode_labels()));
    gtk_widget_set_hexpand(GTK_WIDGET(self->mode), TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->mode),
        _("Operation to perform. The available fields change with the "
          "selected mode."));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->mode), 1, row, 2, 1);
    row++;

    /* Backend. */
    for (i = 0; self->backends[i] != NULL; i++)
        n_backends++;
    backend_strings = g_new0(const char *, n_backends + 2);
    backend_strings[0] = _("auto (detect)");
    for (i = 0; i < n_backends; i++)
        backend_strings[i + 1] = self->backends[i];

    gtk_grid_attach(GTK_GRID(grid), grid_label(_("Backend")), 0, row, 1, 1);
    self->backend = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(backend_strings));
    gtk_widget_set_hexpand(GTK_WIDGET(self->backend), TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->backend),
        _("Filesystem backend. Use \"auto (detect)\" to let partclone pick one. "
          "Not used for whole-disk images."));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->backend), 1, row, 2, 1);
    g_free(backend_strings);
    row++;

    /* Source. */
    gtk_grid_attach(GTK_GRID(grid), grid_label(_("Source")), 0, row, 1, 1);
    self->source = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->source), TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->source),
        _("Device or image the data is read from."));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->source), 1, row, 1, 1);
    browse = gtk_button_new_with_label(_("Browse..."));
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_source), self);
    gtk_grid_attach(GTK_GRID(grid), browse, 2, row, 1, 1);
    row++;

    /* Target. */
    self->target_label = grid_label(_("Target"));
    gtk_grid_attach(GTK_GRID(grid), self->target_label, 0, row, 1, 1);
    self->target = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->target), TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->target),
        _("Device or image the data is written to."));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->target), 1, row, 1, 1);
    browse = gtk_button_new_with_label(_("Browse..."));
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_target), self);
    gtk_grid_attach(GTK_GRID(grid), browse, 2, row, 1, 1);
    self->target_browse = browse;
    row++;

    /* Log file. */
    gtk_grid_attach(GTK_GRID(grid), grid_label(_("Log file")), 0, row, 1, 1);
    self->logfile = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_text(GTK_EDITABLE(self->logfile), "/tmp/ptc-gtk.log");
    gtk_widget_set_hexpand(GTK_WIDGET(self->logfile), TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->logfile),
        _("The engine writes its detailed report to this file."));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->logfile), 1, row, 1, 1);
    browse = gtk_button_new_with_label(_("Browse..."));
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_log), self);
    gtk_grid_attach(GTK_GRID(grid), browse, 2, row, 1, 1);
    row++;

    /* Device size. */
    gtk_grid_attach(GTK_GRID(grid), grid_label(_("Device size")), 0, row, 1, 1);
    self->device_size = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(self->device_size, _("0 = auto (bytes)"));
    gtk_widget_set_hexpand(GTK_WIDGET(self->device_size), TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->device_size),
        _("Override the device size in bytes when it cannot be detected "
          "(clone only)."));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->device_size), 1, row, 2, 1);
    row++;

    /* Options. */
    self->opt_check = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Skip target size check (-C)")));
    gtk_check_button_set_active(self->opt_check, TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_check),
        _("Do not check that the target is large enough before writing."));
    self->opt_force = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Force (-F)")));
    gtk_check_button_set_active(self->opt_force, TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_force),
        _("Keep the process alive on recoverable errors and report them "
          "in the log instead of aborting."));
    self->opt_quiet = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Quiet engine output")));
    gtk_check_button_set_active(self->opt_quiet, TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_quiet),
        _("Suppress the progress messages the engine would print on stdout."));
    self->opt_overwrite = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label(_("Overwrite target (-O)")));
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_overwrite),
        _("Replace the target if it already exists (required to update an "
          "existing image)."));

    gtk_box_append(GTK_BOX(options), GTK_WIDGET(self->opt_check));
    gtk_box_append(GTK_BOX(options), GTK_WIDGET(self->opt_force));
    gtk_box_append(GTK_BOX(options), GTK_WIDGET(self->opt_quiet));
    gtk_box_append(GTK_BOX(options), GTK_WIDGET(self->opt_overwrite));

    {
        GtkWidget *debug_label = gtk_label_new(_("Debug level"));
        GtkWidget *debug = gtk_spin_button_new_with_range(0, 2, 1);

        gtk_widget_set_tooltip_text(debug,
            _("Engine verbosity: 0 quiet, 1 or 2 increasingly detailed."));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(debug), 0);
        self->debug = GTK_SPIN_BUTTON(debug);
        gtk_box_append(GTK_BOX(options), debug_label);
        gtk_box_append(GTK_BOX(options), debug);
    }
    gtk_box_append(GTK_BOX(root), options);

    /* Whole-disk options. */
    self->disk_frame = build_disk_options(self);
    gtk_box_append(GTK_BOX(root), self->disk_frame);

    /* Actions. */
    self->run = GTK_BUTTON(gtk_button_new_with_label(_("Run")));
    gtk_widget_add_css_class(GTK_WIDGET(self->run), "suggested-action");
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->run),
        _("Start the operation (F5)."));
    g_signal_connect(self->run, "clicked", G_CALLBACK(on_run_clicked), self);
    self->cancel = GTK_BUTTON(gtk_button_new_with_label(_("Cancel")));
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->cancel),
        _("Stop the running operation at the next block boundary."));
    gtk_widget_set_sensitive(GTK_WIDGET(self->cancel), FALSE);
    g_signal_connect(self->cancel, "clicked", G_CALLBACK(on_cancel_clicked), self);
    clear = gtk_button_new_with_label(_("Clear log"));
    gtk_widget_set_tooltip_text(clear, _("Clear the log view (Ctrl+L)."));
    g_signal_connect(clear, "clicked", G_CALLBACK(on_clear_clicked), self);
    gtk_box_append(GTK_BOX(actions), GTK_WIDGET(self->run));
    gtk_box_append(GTK_BOX(actions), GTK_WIDGET(self->cancel));
    gtk_box_append(GTK_BOX(actions), clear);
    gtk_box_append(GTK_BOX(root), actions);

    /* Progress. */
    self->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(self->progress, TRUE);
    gtk_progress_bar_set_text(self->progress, _("idle"));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(self->progress));

    /* Per-volume progress, only shown for whole-disk operations. */
    self->volume_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    self->volume_status = GTK_LABEL(gtk_label_new(_("Waiting for the first volume...")));
    gtk_label_set_xalign(self->volume_status, 0.0);
    gtk_widget_set_size_request(GTK_WIDGET(self->volume_status), 240, -1);
    self->volume_progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(self->volume_progress, TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(self->volume_progress), TRUE);
    gtk_box_append(GTK_BOX(self->volume_row), GTK_WIDGET(self->volume_status));
    gtk_box_append(GTK_BOX(self->volume_row), GTK_WIDGET(self->volume_progress));
    gtk_widget_set_visible(self->volume_row, FALSE);
    gtk_box_append(GTK_BOX(root), self->volume_row);

    self->status = GTK_LABEL(gtk_label_new(_("Ready")));
    gtk_label_set_xalign(self->status, 0.0);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(self->status));

    /* Log. */
    self->logview = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(self->logview, FALSE);
    gtk_text_view_set_cursor_visible(self->logview, FALSE);
    gtk_text_view_set_monospace(self->logview, TRUE);
    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_WIDGET(self->logview));
    gtk_box_append(GTK_BOX(root), scroll);

    g_signal_connect(self->mode, "notify::selected",
                     G_CALLBACK(on_mode_changed), self);
    g_signal_connect(window, "close-request",
                     G_CALLBACK(on_close_request), self);
    update_mode_sensitivity(self);

    return window;
}

static void
on_activate(GtkApplication *gtk_app, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    int index;

    (void)gtk_app;

    /* A second activation (e.g. launching the binary again) just raises the
     * existing window. */
    if (self->window != NULL) {
        gtk_window_present(self->window);
        return;
    }

    self->window = GTK_WINDOW(build_window(self));
    log_append(self, _("libpartclone %s ready."), ptc_version_string());

    /* Pre-fill the form from the command line. */
    if (opt_source != NULL) {
        gtk_editable_set_text(GTK_EDITABLE(self->source), opt_source);
    } else if (opt_remaining != NULL && opt_remaining[0] != NULL) {
        gtk_editable_set_text(GTK_EDITABLE(self->source), opt_remaining[0]);
    }
    if (opt_target != NULL)
        gtk_editable_set_text(GTK_EDITABLE(self->target), opt_target);

    if (opt_backend != NULL) {
        index = backend_index_from_name(self, opt_backend);
        if (index >= 0)
            gtk_drop_down_set_selected(self->backend, index);
        else
            log_append(self, _("[!] Unknown backend: %s"), opt_backend);
    }

    if (opt_mode != NULL) {
        index = mode_index_from_name(opt_mode);
        if (index >= 0)
            gtk_drop_down_set_selected(self->mode, index);
        else
            log_append(self, _("[!] Unknown mode: %s"), opt_mode);
    }

    gtk_window_present(self->window);

    if (self_test_opt)
        self_test_start(self);
}

int
main(int argc, char **argv)
{
    const GOptionEntry option_entries[] = {
        { "self-test", 0, 0, G_OPTION_ARG_NONE, &self_test_opt,
          _("Clone, check, then do a whole-disk round-trip and quit (headless test)"),
          NULL },
        { "source", 0, 0, G_OPTION_ARG_FILENAME, &opt_source,
          _("Source device or image"), _("PATH") },
        { "target", 0, 0, G_OPTION_ARG_FILENAME, &opt_target,
          _("Target device or image"), _("PATH") },
        { "backend", 0, 0, G_OPTION_ARG_STRING, &opt_backend,
          _("Backend name (default: autodetect)"), _("NAME") },
        { "mode", 0, 0, G_OPTION_ARG_STRING, &opt_mode,
          _("Operation: clone, restore, check, dd, domain, disk-clone, "
            "disk-restore or disk-check"), _("MODE") },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &opt_remaining,
          NULL, _("SOURCE") },
        { NULL }
    };
    GtkApplication *gtk_app;
    int status;

    setup_i18n();
    g_mutex_init(&progress_lock);

    app.backends = ptc_backend_names();
    app.app = gtk_app = gtk_application_new("org.partclone.Gtk4",
                                            G_APPLICATION_DEFAULT_FLAGS);

    g_action_map_add_action_entries(G_ACTION_MAP(gtk_app), app_actions,
                                    G_N_ELEMENTS(app_actions), &app);
    gtk_application_set_accels_for_action(gtk_app, "app.exit", accel_quit);
    gtk_application_set_accels_for_action(gtk_app, "app.run", accel_run);
    gtk_application_set_accels_for_action(gtk_app, "app.clear", accel_clear);
    gtk_application_set_accels_for_action(gtk_app, "app.inspect", accel_inspect);
    gtk_application_set_accels_for_action(gtk_app, "app.about", accel_about);

    g_application_add_main_option_entries(G_APPLICATION(gtk_app),
                                          option_entries);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), &app);

    status = g_application_run(G_APPLICATION(gtk_app), argc, argv);

    if (self_test_opt) {
        status = self_test_status;
        if (self_test_source != NULL) {
            g_remove(self_test_source);
            g_remove(self_test_image);
            g_remove(self_test_disk_src);
            g_remove(self_test_disk_img);
            g_remove(self_test_log);
            self_test_free_paths();
        }
    }

    g_clear_object(&app.cancellable);
    g_clear_object(&app.client);
    g_object_unref(gtk_app);

    g_strfreev(opt_remaining);
    g_free(opt_source);
    g_free(opt_target);
    g_free(opt_backend);
    g_free(opt_mode);

    return status;
}

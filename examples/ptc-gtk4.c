/*
 * ptc-gtk4.c - libpartclone GTK4 test application
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * A small GTK4 front end for the public libpartclone API.  It exercises:
 *   - enumerating the available filesystem backend modules,
 *   - clone / restore / check / dd / domain,
 *   - the ::progress signal (marshalled from the engine thread to the
 *     GTK main loop),
 *   - asynchronous execution (ptc_client_run_async) so the UI stays alive.
 *
 * Build out of tree:
 *   cc ptc-gtk4.c $(pkg-config --cflags --libs partclone gtk4) -o ptc-gtk4
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdarg.h>
#include <stdio.h>

#include <glib/gstdio.h>
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

    GtkCheckButton *opt_check;
    GtkCheckButton *opt_force;
    GtkCheckButton *opt_quiet;

    GtkButton *run;
    GtkProgressBar *progress;
    GtkLabel *status;
    GtkTextView *logview;

    PtcClient *client;
    gboolean running;
    int self_test;                 /* 0 off, 1 clone, 2 check, 3 done */
} PtcGtkApp;

static PtcGtkApp app;

/* --self-test drives the whole flow without user interaction. */
static gboolean self_test_opt = FALSE;
static int self_test_status = 0;

static const char *const self_test_source = "/tmp/ptc-gtk4-selftest.src";
static const char *const self_test_image  = "/tmp/ptc-gtk4-selftest.pcl";
static const char *const self_test_log    = "/tmp/ptc-gtk4-selftest.log";

/* The engine emits ::progress from its own thread; the values below are
 * handed over to the main loop through a single coalesced idle source. */
static GMutex progress_lock;
static gboolean progress_pending;
static guint64 progress_copied;
static guint64 progress_total;

static const char *const mode_labels[] = {
    "Clone",
    "Restore",
    "Check image",
    "Device to device (dd)",
    "Domain log",
    NULL
};

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

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

static PtcMode
mode_from_index(guint index)
{
    switch (index) {
    case 0:  return PTC_MODE_CLONE;
    case 1:  return PTC_MODE_RESTORE;
    case 2:  return PTC_MODE_CHKIMG;
    case 3:  return PTC_MODE_DD;
    case 4:  return PTC_MODE_DOMAIN;
    default: return PTC_MODE_CLONE;
    }
}

static const char *
mode_name(PtcMode mode)
{
    switch (mode) {
    case PTC_MODE_CLONE:   return "clone";
    case PTC_MODE_RESTORE: return "restore";
    case PTC_MODE_CHKIMG:  return "check";
    case PTC_MODE_DD:      return "dd";
    case PTC_MODE_DOMAIN:  return "domain";
    }
    return "?";
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

static void
set_running(PtcGtkApp *self, gboolean running)
{
    self->running = running;
    gtk_widget_set_sensitive(GTK_WIDGET(self->run), !running);
}

static void on_run_clicked(GtkButton *button, gpointer user_data);
static gboolean self_test_next(gpointer user_data);

/* ------------------------------------------------------------------ */
/* Progress                                                            */
/* ------------------------------------------------------------------ */

static gboolean
progress_idle(gpointer user_data)
{
    PtcGtkApp *self = user_data;
    guint64 copied, total;
    char *text;

    g_mutex_lock(&progress_lock);
    copied = progress_copied;
    total = progress_total;
    progress_pending = FALSE;
    g_mutex_unlock(&progress_lock);

    if (total > 0)
        gtk_progress_bar_set_fraction(self->progress,
                                      (double)copied / (double)total);

    text = g_strdup_printf("%" G_GUINT64_FORMAT " / %" G_GUINT64_FORMAT,
                           copied, total);
    gtk_progress_bar_set_text(self->progress, text);
    gtk_label_set_text(self->status, "Running...");
    g_free(text);

    return G_SOURCE_REMOVE;
}

static void
on_progress(PtcClient *client, guint64 copied, guint64 total, gpointer user_data)
{
    PtcGtkApp *self = user_data;
    gboolean schedule;

    (void)client;

    g_mutex_lock(&progress_lock);
    progress_copied = copied;
    progress_total = total;
    schedule = !progress_pending;
    progress_pending = TRUE;
    g_mutex_unlock(&progress_lock);

    if (schedule)
        g_main_context_invoke_full(NULL, G_PRIORITY_DEFAULT,
                                   progress_idle, self, NULL);
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

    if (ok) {
        gtk_progress_bar_set_fraction(self->progress, 1.0);
        gtk_label_set_text(self->status, "Finished");
        log_append(self, "--- finished ---");
    } else {
        gtk_label_set_text(self->status, "Failed");
        log_append(self, "--- error: %s ---",
                   error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
    }

    g_clear_object(&self->client);
    set_running(self, FALSE);

    if (self->self_test == 1) {
        /* The clone finished; verify the image next. */
        self->self_test = 2;
        if (ok) {
            log_append(self, "self-test: verifying the image");
            gtk_editable_set_text(GTK_EDITABLE(self->source), self_test_image);
            gtk_drop_down_set_selected(self->mode, 2);   /* Check image */
            g_idle_add(self_test_next, self);
        } else {
            self_test_status = 1;
            g_application_quit(G_APPLICATION(self->app));
        }
    } else if (self->self_test == 2) {
        self->self_test = 3;
        self_test_status = ok ? 0 : 1;
        g_print("self-test %s\n", ok ? "OK" : "FAILED");
        g_application_quit(G_APPLICATION(self->app));
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
    gboolean skip_check;
    PtcMode mode;
    FILE *log;

    (void)button;

    if (self->running)
        return;

    source = gtk_editable_get_text(GTK_EDITABLE(self->source));
    target = gtk_editable_get_text(GTK_EDITABLE(self->target));
    logfile = gtk_editable_get_text(GTK_EDITABLE(self->logfile));
    mode = mode_from_index(gtk_drop_down_get_selected(self->mode));
    backend = backend_from_index(self);

    if (source[0] == '\0') {
        show_error(self, "Please choose a source device or image.");
        return;
    }
    if (mode != PTC_MODE_CHKIMG && target[0] == '\0') {
        show_error(self, "Please choose a target.");
        return;
    }
    if (logfile[0] == '\0') {
        show_error(self, "Please choose a log file.");
        return;
    }

    /* The engine exits the process when it cannot open its log file, so fail
     * early with a proper message instead. */
    log = fopen(logfile, "a");
    if (log == NULL) {
        show_error(self, "Cannot open the log file for writing.");
        return;
    }
    fclose(log);

    self->client = ptc_client_new();
    skip_check = gtk_check_button_get_active(self->opt_check);

    g_object_set(self->client,
                 "mode",    mode,
                 "source",  source,
                 "target",  target,
                 "backend", backend,          /* NULL means auto detect */
                 "logfile", logfile,
                 "quiet",   gtk_check_button_get_active(self->opt_quiet),
                 "check",   !skip_check,      /* -C clears the size checks */
                 "force",   gtk_check_button_get_active(self->opt_force),
                 NULL);

    g_signal_connect(self->client, "progress", G_CALLBACK(on_progress), self);

    log_append(self, "=== %s ===", mode_name(mode));
    log_append(self, "source : %s", source);
    if (mode != PTC_MODE_CHKIMG)
        log_append(self, "target : %s", target);
    log_append(self, "backend: %s", backend ? backend : "auto (detect)");

    gtk_progress_bar_set_fraction(self->progress, 0.0);
    gtk_progress_bar_set_text(self->progress, "starting");
    gtk_label_set_text(self->status, "Running...");
    set_running(self, TRUE);

    ptc_client_run_async(self->client, NULL, on_run_done, self);
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

    gtk_file_dialog_set_title(dialog, "Select source device or image");
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

    gtk_file_dialog_set_title(dialog, "Select target device or image");
    gtk_file_dialog_save(dialog, self->window, NULL, on_target_chosen, self);
    g_object_unref(dialog);
}

/* ------------------------------------------------------------------ */
/* Self test (headless, used by --self-test and CI)                    */
/* ------------------------------------------------------------------ */

static void
self_test_start(PtcGtkApp *self)
{
    gchar *data = g_malloc0(1024 * 1024);
    GError *error = NULL;
    guint i, raw_index = 0;

    g_remove(self_test_image);

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
    log_append(self, "self-test: cloning %s", self_test_source);
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

static GtkWidget *
build_window(PtcGtkApp *self)
{
    GtkWidget *window = gtk_application_window_new(self->app);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *options = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *browse, *clear, *scroll;
    char *info, *backend_list;
    guint n_backends = 0, i;
    const char **backend_strings;
    int row = 0;

    gtk_window_set_title(GTK_WINDOW(window), "libpartclone GTK4 test");
    gtk_window_set_default_size(GTK_WINDOW(window), 780, 640);
    gtk_widget_set_margin_start(root, 12);
    gtk_widget_set_margin_end(root, 12);
    gtk_widget_set_margin_top(root, 12);
    gtk_widget_set_margin_bottom(root, 12);
    gtk_window_set_child(GTK_WINDOW(window), root);

    /* Header: version and available backends. */
    backend_list = backend_list_string();
    info = g_strdup_printf("libpartclone %s\nbackends: %s",
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
    gtk_grid_attach(GTK_GRID(grid), grid_label("Mode"), 0, row, 1, 1);
    self->mode = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(mode_labels));
    gtk_widget_set_hexpand(GTK_WIDGET(self->mode), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->mode), 1, row, 2, 1);
    row++;

    /* Backend. */
    for (i = 0; self->backends[i] != NULL; i++)
        n_backends++;
    backend_strings = g_new0(const char *, n_backends + 2);
    backend_strings[0] = "auto (detect)";
    for (i = 0; i < n_backends; i++)
        backend_strings[i + 1] = self->backends[i];

    gtk_grid_attach(GTK_GRID(grid), grid_label("Backend"), 0, row, 1, 1);
    self->backend = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(backend_strings));
    gtk_widget_set_hexpand(GTK_WIDGET(self->backend), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->backend), 1, row, 2, 1);
    g_free(backend_strings);
    row++;

    /* Source. */
    gtk_grid_attach(GTK_GRID(grid), grid_label("Source"), 0, row, 1, 1);
    self->source = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->source), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->source), 1, row, 1, 1);
    browse = gtk_button_new_with_label("Browse...");
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_source), self);
    gtk_grid_attach(GTK_GRID(grid), browse, 2, row, 1, 1);
    row++;

    /* Target. */
    gtk_grid_attach(GTK_GRID(grid), grid_label("Target"), 0, row, 1, 1);
    self->target = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(self->target), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->target), 1, row, 1, 1);
    browse = gtk_button_new_with_label("Browse...");
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_target), self);
    gtk_grid_attach(GTK_GRID(grid), browse, 2, row, 1, 1);
    row++;

    /* Log file. */
    gtk_grid_attach(GTK_GRID(grid), grid_label("Log file"), 0, row, 1, 1);
    self->logfile = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_text(GTK_EDITABLE(self->logfile), "/tmp/ptc-gtk.log");
    gtk_widget_set_hexpand(GTK_WIDGET(self->logfile), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(self->logfile), 1, row, 2, 1);
    row++;

    /* Options. */
    self->opt_check = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Skip target size check (-C)"));
    gtk_check_button_set_active(self->opt_check, TRUE);
    self->opt_force = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Force (-F)"));
    gtk_check_button_set_active(self->opt_force, TRUE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->opt_force),
        "Keep the process alive on recoverable errors and report them "
        "in the log instead of aborting.");
    self->opt_quiet = GTK_CHECK_BUTTON(
        gtk_check_button_new_with_label("Quiet engine output"));
    gtk_check_button_set_active(self->opt_quiet, TRUE);

    gtk_box_append(GTK_BOX(options), GTK_WIDGET(self->opt_check));
    gtk_box_append(GTK_BOX(options), GTK_WIDGET(self->opt_force));
    gtk_box_append(GTK_BOX(options), GTK_WIDGET(self->opt_quiet));
    gtk_box_append(GTK_BOX(root), options);

    /* Actions. */
    self->run = GTK_BUTTON(gtk_button_new_with_label("Run"));
    gtk_widget_add_css_class(GTK_WIDGET(self->run), "suggested-action");
    g_signal_connect(self->run, "clicked", G_CALLBACK(on_run_clicked), self);
    clear = gtk_button_new_with_label("Clear log");
    g_signal_connect(clear, "clicked", G_CALLBACK(on_clear_clicked), self);
    gtk_box_append(GTK_BOX(actions), GTK_WIDGET(self->run));
    gtk_box_append(GTK_BOX(actions), clear);
    gtk_box_append(GTK_BOX(root), actions);

    /* Progress. */
    self->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(self->progress, TRUE);
    gtk_progress_bar_set_text(self->progress, "idle");
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(self->progress));

    self->status = GTK_LABEL(gtk_label_new("Ready"));
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

    return window;
}

static void
on_activate(GtkApplication *gtk_app, gpointer user_data)
{
    PtcGtkApp *self = user_data;

    /* A second activation (e.g. launching the binary again) just raises the
     * existing window. */
    if (self->window != NULL) {
        gtk_window_present(self->window);
        return;
    }

    self->window = GTK_WINDOW(build_window(self));
    log_append(self, "libpartclone %s ready.", ptc_version_string());
    gtk_window_present(self->window);

    if (self_test_opt)
        self_test_start(self);
}

int
main(int argc, char **argv)
{
    static const GOptionEntry option_entries[] = {
        { "self-test", 0, 0, G_OPTION_ARG_NONE, &self_test_opt,
          "Clone a temporary file, verify it, then quit (headless test)",
          NULL },
        { NULL }
    };
    GtkApplication *gtk_app;
    int status;

    g_mutex_init(&progress_lock);

    app.backends = ptc_backend_names();
    app.app = gtk_app = gtk_application_new("org.partclone.PtcGtk4Test",
                                            G_APPLICATION_DEFAULT_FLAGS);
    g_application_add_main_option_entries(G_APPLICATION(gtk_app),
                                          option_entries);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), &app);

    status = g_application_run(G_APPLICATION(gtk_app), argc, argv);

    if (self_test_opt)
        status = self_test_status;

    g_clear_object(&app.client);
    g_object_unref(gtk_app);

    return status;
}

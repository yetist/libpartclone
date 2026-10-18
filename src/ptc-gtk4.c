/*
 * ptc-gtk4.c - libpartclone GTK4 front end
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * A GTK4 front end for the public libpartclone API.  It exercises:
 *   - enumerating the available filesystem backend modules,
 *   - clone / restore / check / dd / domain,
 *   - the ::progress signal (marshalled from the engine thread to the
 *     GTK main loop),
 *   - asynchronous execution (ptc_client_run_async) so the UI stays alive.
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

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>

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

    GtkWidget *target_label;
    GtkWidget *target_browse;

    GtkCheckButton *opt_check;
    GtkCheckButton *opt_force;
    GtkCheckButton *opt_quiet;
    GtkCheckButton *opt_overwrite;

    GtkButton *run;
    GtkButton *clear;
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

/* Optional values taken from the command line. */
static gchar *opt_source = NULL;
static gchar *opt_target = NULL;
static gchar *opt_backend = NULL;
static gchar *opt_mode = NULL;
static gchar **opt_remaining = NULL;

static const char *const self_test_source = "/tmp/ptc-gtk4-selftest.src";
static const char *const self_test_image  = "/tmp/ptc-gtk4-selftest.pcl";
static const char *const self_test_log    = "/tmp/ptc-gtk4-selftest.log";

/* Keyboard accelerators. */
static const char *const accel_quit[]  = { "<Control>q", NULL };
static const char *const accel_run[]   = { "F5", NULL };
static const char *const accel_clear[] = { "<Control>l", NULL };
static const char *const accel_about[] = { "F1", NULL };

/* The engine emits ::progress from its own thread; the values below are
 * handed over to the main loop through a single coalesced idle source. */
static GMutex progress_lock;
static gboolean progress_pending;
static guint64 progress_copied;
static guint64 progress_total;

static void on_run_clicked(GtkButton *button, gpointer user_data);
static void on_clear_clicked(GtkButton *button, gpointer user_data);
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
    static const char *labels[6];

    labels[0] = _("Clone");
    labels[1] = _("Restore");
    labels[2] = _("Check image");
    labels[3] = _("Device to device (dd)");
    labels[4] = _("Domain log");
    labels[5] = NULL;
    return labels;
}

static int
mode_index_from_name(const char *name)
{
    static const char *const names[] = {
        "clone", "restore", "check", "dd", "domain", NULL
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

static void
set_running(PtcGtkApp *self, gboolean running)
{
    GAction *action;

    self->running = running;
    gtk_widget_set_sensitive(GTK_WIDGET(self->run), !running);

    action = g_action_map_lookup_action(G_ACTION_MAP(self->app), "run");
    if (action != NULL)
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), !running);
}

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

    if (total > 0) {
        char *copied_text = g_format_size_full(copied, G_FORMAT_SIZE_IEC_UNITS);
        char *total_text  = g_format_size_full(total, G_FORMAT_SIZE_IEC_UNITS);

        gtk_progress_bar_set_fraction(self->progress,
                                      (double)copied / (double)total);
        text = g_strdup_printf("%s / %s (%.1f%%)", copied_text, total_text,
                               100.0 * (double)copied / (double)total);
        g_free(copied_text);
        g_free(total_text);
    } else {
        text = g_strdup_printf("%" G_GUINT64_FORMAT, copied);
    }

    gtk_progress_bar_set_text(self->progress, text);
    gtk_label_set_text(self->status, _("Running..."));
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
        gtk_label_set_text(self->status, _("Finished"));
        log_append(self, _("--- finished ---"));
    } else {
        gtk_label_set_text(self->status, _("Failed"));
        log_append(self, _("--- error: %s ---"),
                   error != NULL ? error->message : _("unknown error"));
        g_clear_error(&error);
    }

    g_clear_object(&self->client);
    set_running(self, FALSE);

    if (self->self_test == 1) {
        /* The clone finished; verify the image next. */
        self->self_test = 2;
        if (ok) {
            log_append(self, _("self-test: verifying the image"));
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
        show_error(self, _("Please choose a source device or image."));
        return;
    }
    if (mode != PTC_MODE_CHKIMG && mode != PTC_MODE_DISK_CHKIMG && target[0] == '\0') {
        show_error(self, _("Please choose a target."));
        return;
    }
    if (logfile[0] == '\0') {
        show_error(self, _("Please choose a log file."));
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

    self->client = ptc_client_new();
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
                 NULL);

    g_signal_connect(self->client, "progress", G_CALLBACK(on_progress), self);

    log_append(self, _("=== %s ==="), mode_name(mode));
    log_append(self, _("source : %s"), source);
    if (mode != PTC_MODE_CHKIMG && mode != PTC_MODE_DISK_CHKIMG)
        log_append(self, _("target : %s"), target);
    log_append(self, _("backend: %s"),
               backend != NULL ? backend : _("auto (detect)"));

    gtk_progress_bar_set_fraction(self->progress, 0.0);
    gtk_progress_bar_set_text(self->progress, _("starting"));
    gtk_label_set_text(self->status, _("Running..."));
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
    if (choice == 1)
        g_application_quit(G_APPLICATION(self->app));
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
        _("Quitting now will abort the operation."));
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
    { "run",   on_run_action,   NULL, NULL, NULL },
    { "clear", on_clear_action, NULL, NULL, NULL },
    { "about", on_about_action, NULL, NULL, NULL },
    { "exit",  on_exit_action,  NULL, NULL, NULL },
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

/* Enable or disable the target widgets depending on the selected mode. */
static void
update_mode_sensitivity(PtcGtkApp *self)
{
    guint index = gtk_drop_down_get_selected(self->mode);
    gboolean needs_target = (index != 2);   /* "Check image" needs no target */

    gtk_widget_set_sensitive(self->target_label, needs_target);
    gtk_widget_set_sensitive(GTK_WIDGET(self->target), needs_target);
    gtk_widget_set_sensitive(self->target_browse, needs_target);
}

static void
on_mode_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_mode_sensitivity(user_data);
}

static GtkWidget *
build_header_bar(PtcGtkApp *self)
{
    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *menu_button = gtk_menu_button_new();
    GMenu *menu = g_menu_new();
    GMenu *section = g_menu_new();

    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

    g_menu_append(menu, _("_Run"), "app.run");
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
    GtkWidget *browse, *clear, *scroll;
    char *info, *backend_list;
    guint n_backends = 0, i;
    const char **backend_strings;
    int row = 0;

    gtk_window_set_title(GTK_WINDOW(window), _("partclone"));
    gtk_window_set_default_size(GTK_WINDOW(window), 820, 680);
    gtk_window_set_titlebar(GTK_WINDOW(window), build_header_bar(self));
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
        _("Operation to perform. The available target fields change with the "
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
        _("Filesystem backend. Use \"auto (detect)\" to let partclone pick one."));
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
    gtk_box_append(GTK_BOX(root), options);

    /* Actions. */
    self->run = GTK_BUTTON(gtk_button_new_with_label(_("Run")));
    gtk_widget_add_css_class(GTK_WIDGET(self->run), "suggested-action");
    gtk_widget_set_tooltip_text(GTK_WIDGET(self->run),
        _("Start the operation (F5)."));
    g_signal_connect(self->run, "clicked", G_CALLBACK(on_run_clicked), self);
    clear = gtk_button_new_with_label(_("Clear log"));
    gtk_widget_set_tooltip_text(clear, _("Clear the log view (Ctrl+L)."));
    g_signal_connect(clear, "clicked", G_CALLBACK(on_clear_clicked), self);
    gtk_box_append(GTK_BOX(actions), GTK_WIDGET(self->run));
    gtk_box_append(GTK_BOX(actions), clear);
    gtk_box_append(GTK_BOX(root), actions);

    /* Progress. */
    self->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_show_text(self->progress, TRUE);
    gtk_progress_bar_set_text(self->progress, _("idle"));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(self->progress));

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
          _("Clone a temporary file, verify it, then quit (headless test)"),
          NULL },
        { "source", 0, 0, G_OPTION_ARG_FILENAME, &opt_source,
          _("Source device or image"), _("PATH") },
        { "target", 0, 0, G_OPTION_ARG_FILENAME, &opt_target,
          _("Target device or image"), _("PATH") },
        { "backend", 0, 0, G_OPTION_ARG_STRING, &opt_backend,
          _("Backend name (default: autodetect)"), _("NAME") },
        { "mode", 0, 0, G_OPTION_ARG_STRING, &opt_mode,
          _("Operation: clone, restore, check, dd or domain"), _("MODE") },
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
    gtk_application_set_accels_for_action(gtk_app, "app.about", accel_about);

    g_application_add_main_option_entries(G_APPLICATION(gtk_app),
                                          option_entries);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), &app);

    status = g_application_run(G_APPLICATION(gtk_app), argc, argv);

    if (self_test_opt)
        status = self_test_status;

    g_clear_object(&app.client);
    g_object_unref(gtk_app);

    g_strfreev(opt_remaining);
    g_free(opt_source);
    g_free(opt_target);
    g_free(opt_backend);
    g_free(opt_mode);

    return status;
}

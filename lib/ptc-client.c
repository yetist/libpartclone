/**
 * ptc-client.c - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * A high level GObject/GIO wrapper around the partclone engine.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

#include <string.h>

#include "ptc-client.h"
#include "engine.h"
#include "checksum.h"

/**
 * PTC_CLIENT_ERROR:
 *
 * Error domain for #PtcClient operations.
 */
#define PTC_CLIENT_ERROR (ptc_client_error_quark())
GQuark ptc_client_error_quark(void);

typedef enum {
    PTC_CLIENT_ERROR_FAILED,
    PTC_CLIENT_ERROR_UNKNOWN_BACKEND,
    PTC_CLIENT_ERROR_NO_BACKEND
} PtcClientError;

GQuark
ptc_client_error_quark(void)
{
    return g_quark_from_static_string("ptc-client-error-quark");
}

struct _PtcClient {
    GObject parent_instance;

    PtcMode mode;
    char *source;
    char *target;
    char *backend;
    char *logfile;
    guint64 device_size;
    guint debug;
    gboolean quiet;
    gboolean overwrite;
    gboolean check;
    gboolean force;
};

G_DEFINE_TYPE(PtcClient, ptc_client, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_MODE,
    PROP_SOURCE,
    PROP_TARGET,
    PROP_BACKEND,
    PROP_DEVICE_SIZE,
    PROP_DEBUG,
    PROP_LOGFILE,
    PROP_QUIET,
    PROP_OVERWRITE,
    PROP_CHECK,
    PROP_FORCE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

enum {
    SIGNAL_PROGRESS,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

/* The engine keeps its state in globals, so only one run may happen at a time. */
static GMutex ptc_client_lock;

/* ------------------------------------------------------------------ */
/* PtcMode enum                                                        */
/* ------------------------------------------------------------------ */

GType
ptc_mode_get_type(void)
{
    static gsize id = 0;

    if (g_once_init_enter(&id)) {
        static const GEnumValue values[] = {
            { PTC_MODE_CLONE,   "PTC_MODE_CLONE",   "clone"   },
            { PTC_MODE_RESTORE, "PTC_MODE_RESTORE", "restore" },
            { PTC_MODE_CHKIMG,  "PTC_MODE_CHKIMG",  "chkimg"  },
            { PTC_MODE_DD,      "PTC_MODE_DD",      "dd"      },
            { PTC_MODE_DOMAIN,  "PTC_MODE_DOMAIN",  "domain"  },
            { 0, NULL, NULL }
        };
        GType type = g_enum_register_static("PtcMode", values);
        g_once_init_leave(&id, type);
    }

    return id;
}

/* ------------------------------------------------------------------ */
/* GObject plumbing                                                    */
/* ------------------------------------------------------------------ */

static void
ptc_client_set_property(GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec)
{
    PtcClient *self = PTC_CLIENT(object);

    switch (prop_id) {
    case PROP_MODE:
        ptc_client_set_mode(self, g_value_get_enum(value));
        break;
    case PROP_SOURCE:
        ptc_client_set_source(self, g_value_get_string(value));
        break;
    case PROP_TARGET:
        ptc_client_set_target(self, g_value_get_string(value));
        break;
    case PROP_BACKEND:
        ptc_client_set_backend(self, g_value_get_string(value));
        break;
    case PROP_DEVICE_SIZE:
        self->device_size = g_value_get_uint64(value);
        break;
    case PROP_DEBUG:
        self->debug = g_value_get_uint(value);
        break;
    case PROP_LOGFILE:
        ptc_client_set_logfile(self, g_value_get_string(value));
        break;
    case PROP_QUIET:
        self->quiet = g_value_get_boolean(value);
        break;
    case PROP_OVERWRITE:
        self->overwrite = g_value_get_boolean(value);
        break;
    case PROP_CHECK:
        self->check = g_value_get_boolean(value);
        break;
    case PROP_FORCE:
        self->force = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
ptc_client_get_property(GObject *object, guint prop_id,
                        GValue *value, GParamSpec *pspec)
{
    PtcClient *self = PTC_CLIENT(object);

    switch (prop_id) {
    case PROP_MODE:
        g_value_set_enum(value, self->mode);
        break;
    case PROP_SOURCE:
        g_value_set_string(value, self->source);
        break;
    case PROP_TARGET:
        g_value_set_string(value, self->target);
        break;
    case PROP_BACKEND:
        g_value_set_string(value, self->backend);
        break;
    case PROP_DEVICE_SIZE:
        g_value_set_uint64(value, self->device_size);
        break;
    case PROP_DEBUG:
        g_value_set_uint(value, self->debug);
        break;
    case PROP_LOGFILE:
        g_value_set_string(value, self->logfile);
        break;
    case PROP_QUIET:
        g_value_set_boolean(value, self->quiet);
        break;
    case PROP_OVERWRITE:
        g_value_set_boolean(value, self->overwrite);
        break;
    case PROP_CHECK:
        g_value_set_boolean(value, self->check);
        break;
    case PROP_FORCE:
        g_value_set_boolean(value, self->force);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
ptc_client_finalize(GObject *object)
{
    PtcClient *self = PTC_CLIENT(object);

    g_free(self->source);
    g_free(self->target);
    g_free(self->backend);
    g_free(self->logfile);

    G_OBJECT_CLASS(ptc_client_parent_class)->finalize(object);
}

static void
ptc_client_class_init(PtcClientClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = ptc_client_set_property;
    object_class->get_property = ptc_client_get_property;
    object_class->finalize = ptc_client_finalize;

    properties[PROP_MODE] =
        g_param_spec_enum("mode", "Mode", "Operation mode",
                          PTC_TYPE_MODE, PTC_MODE_CLONE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SOURCE] =
        g_param_spec_string("source", "Source", "Source device or image",
                            NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_TARGET] =
        g_param_spec_string("target", "Target", "Target image or device",
                            NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_BACKEND] =
        g_param_spec_string("backend", "Backend", "Filesystem backend name",
                            NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DEVICE_SIZE] =
        g_param_spec_uint64("device-size", "Device size",
                            "Override the device size in bytes",
                            0, G_MAXUINT64, 0,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DEBUG] =
        g_param_spec_uint("debug", "Debug", "Debug level",
                          0, 2, 0,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_LOGFILE] =
        g_param_spec_string("logfile", "Log file", "Log file path",
                            "/var/log/partclone.log",
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_QUIET] =
        g_param_spec_boolean("quiet", "Quiet", "Disable progress messages",
                             FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_OVERWRITE] =
        g_param_spec_boolean("overwrite", "Overwrite", "Overwrite the target",
                             FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_CHECK] =
        g_param_spec_boolean("check", "Check",
                             "Check the target size and free space",
                             TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_FORCE] =
        g_param_spec_boolean("force", "Force",
                             "Force the operation and ignore metadata errors",
                             FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(object_class, N_PROPERTIES, properties);

    /**
     * PtcClient::progress:
     * @self: the #PtcClient.
     * @copied: number of blocks copied so far.
     * @total: total number of blocks.
     *
     * Emitted periodically while an operation is running.  The signal is
     * emitted from the engine's progress thread, so handlers must be
     * thread-safe.
     */
    signals[SIGNAL_PROGRESS] =
        g_signal_new("progress",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL, NULL,
                     G_TYPE_NONE, 2, G_TYPE_UINT64, G_TYPE_UINT64);
}

static void
ptc_client_init(PtcClient *self)
{
    self->mode = PTC_MODE_CLONE;
    self->source = NULL;
    self->target = NULL;
    self->backend = NULL;
    self->logfile = g_strdup("/var/log/partclone.log");
    self->device_size = 0;
    self->debug = 0;
    self->quiet = FALSE;
    self->overwrite = FALSE;
    self->check = TRUE;
    self->force = FALSE;
}

/* ------------------------------------------------------------------ */
/* Setters and getters                                                 */
/* ------------------------------------------------------------------ */

PtcClient *
ptc_client_new(void)
{
    return g_object_new(PTC_TYPE_CLIENT, NULL);
}

void
ptc_client_set_mode(PtcClient *self, PtcMode mode)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    if (self->mode == mode)
        return;
    self->mode = mode;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_MODE]);
}

PtcMode
ptc_client_get_mode(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), PTC_MODE_CLONE);
    return self->mode;
}

void
ptc_client_set_source(PtcClient *self, const char *source)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    g_free(self->source);
    self->source = g_strdup(source);
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_SOURCE]);
}

const char *
ptc_client_get_source(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), NULL);
    return self->source;
}

void
ptc_client_set_target(PtcClient *self, const char *target)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    g_free(self->target);
    self->target = g_strdup(target);
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_TARGET]);
}

const char *
ptc_client_get_target(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), NULL);
    return self->target;
}

void
ptc_client_set_backend(PtcClient *self, const char *backend)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    g_free(self->backend);
    self->backend = g_strdup(backend);
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_BACKEND]);
}

const char *
ptc_client_get_backend(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), NULL);
    return self->backend;
}

void
ptc_client_set_device_size(PtcClient *self, guint64 device_size)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    self->device_size = device_size;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DEVICE_SIZE]);
}

guint64
ptc_client_get_device_size(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), 0);
    return self->device_size;
}

void
ptc_client_set_debug(PtcClient *self, guint debug)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    self->debug = debug;
}

guint
ptc_client_get_debug(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), 0);
    return self->debug;
}

void
ptc_client_set_logfile(PtcClient *self, const char *logfile)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    g_free(self->logfile);
    self->logfile = g_strdup(logfile ? logfile : "/var/log/partclone.log");
}

const char *
ptc_client_get_logfile(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), NULL);
    return self->logfile;
}

void
ptc_client_set_quiet(PtcClient *self, gboolean quiet)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    self->quiet = quiet;
}

gboolean
ptc_client_get_quiet(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), FALSE);
    return self->quiet;
}

void
ptc_client_set_overwrite(PtcClient *self, gboolean overwrite)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    self->overwrite = overwrite;
}

gboolean
ptc_client_get_overwrite(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), FALSE);
    return self->overwrite;
}

void
ptc_client_set_check(PtcClient *self, gboolean check)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    self->check = check;
}

gboolean
ptc_client_get_check(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), TRUE);
    return self->check;
}

void
ptc_client_set_force(PtcClient *self, gboolean force)
{
    g_return_if_fail(PTC_IS_CLIENT(self));
    self->force = force;
}

gboolean
ptc_client_get_force(PtcClient *self)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), FALSE);
    return self->force;
}

/* ------------------------------------------------------------------ */
/* Running                                                             */
/* ------------------------------------------------------------------ */

static void
ptc_client_progress_cb(unsigned long long copied, unsigned long long total,
                       gpointer user_data)
{
    PtcClient *self = PTC_CLIENT(user_data);

    g_signal_emit(self, signals[SIGNAL_PROGRESS], 0,
                  (guint64)copied, (guint64)total);
}

static int
ptc_client_run_internal(PtcClient *self, GError **error)
{
    const ptc_fs_backend *backend = NULL;
    int ret;

    ptc_opt_init(&opt);

    switch (self->mode) {
    case PTC_MODE_CLONE:
        opt.clone = 1;
        break;
    case PTC_MODE_RESTORE:
        opt.restore = 1;
        break;
    case PTC_MODE_CHKIMG:
        opt.restore = 1;
        opt.chkimg = 1;
        break;
    case PTC_MODE_DD:
        opt.ddd = 1;
        opt.checksum_mode = CSM_NONE;
        break;
    case PTC_MODE_DOMAIN:
        opt.domain = 1;
        break;
    default:
        g_set_error(error, PTC_CLIENT_ERROR, PTC_CLIENT_ERROR_FAILED,
                    "Unknown mode %d", self->mode);
        return -1;
    }

    opt.debug = self->debug;
    opt.quiet = self->quiet;
    opt.overwrite = self->overwrite;
    opt.check = self->check;
    opt.force = self->force;
    opt.device_size = self->device_size;
    if (self->logfile != NULL)
        opt.logfile = self->logfile;
    opt.source = (char *)self->source;
    opt.target = (char *)self->target;

    if (self->backend != NULL) {
        backend = ptc_fs_backend_lookup(self->backend);
        if (backend == NULL) {
            g_set_error(error, PTC_CLIENT_ERROR, PTC_CLIENT_ERROR_UNKNOWN_BACKEND,
                        "Unknown filesystem backend '%s'", self->backend);
            return -1;
        }
    }

    if (backend == NULL && !opt.restore && !opt.chkimg) {
        backend = ptc_fs_backend_detect(opt.source);
        if (backend == NULL) {
            g_set_error(error, PTC_CLIENT_ERROR, PTC_CLIENT_ERROR_NO_BACKEND,
                        "Could not detect the filesystem on '%s'", opt.source);
            return -1;
        }
    }

    ret = ptc_engine_run(backend, ptc_client_progress_cb, self);
    if (ret != 0)
        g_set_error(error, PTC_CLIENT_ERROR, PTC_CLIENT_ERROR_FAILED,
                    "Operation failed (code %d)", ret);

    return ret;
}

gboolean
ptc_client_run(PtcClient *self, GCancellable *cancellable, GError **error)
{
    gboolean ok;
    int ret;

    g_return_val_if_fail(PTC_IS_CLIENT(self), FALSE);

    (void)cancellable;

    g_mutex_lock(&ptc_client_lock);
    ret = ptc_client_run_internal(self, error);
    g_mutex_unlock(&ptc_client_lock);

    ok = (ret == 0);
    return ok;
}

static void
ptc_client_task_thread(GTask *task, gpointer source_object,
                       gpointer task_data, GCancellable *cancellable)
{
    PtcClient *self = PTC_CLIENT(source_object);
    GError *error = NULL;
    int ret;

    (void)task_data;
    (void)cancellable;

    g_mutex_lock(&ptc_client_lock);
    ret = ptc_client_run_internal(self, &error);
    g_mutex_unlock(&ptc_client_lock);

    if (ret != 0) {
        if (error == NULL)
            error = g_error_new_literal(PTC_CLIENT_ERROR, PTC_CLIENT_ERROR_FAILED,
                                        "Operation failed");
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

void
ptc_client_run_async(PtcClient *self, GCancellable *cancellable,
                     GAsyncReadyCallback callback, gpointer user_data)
{
    GTask *task;

    g_return_if_fail(PTC_IS_CLIENT(self));

    task = g_task_new(self, cancellable, callback, user_data);
    g_task_set_source_tag(task, ptc_client_run_async);
    g_task_run_in_thread(task, ptc_client_task_thread);
    g_object_unref(task);
}

gboolean
ptc_client_run_finish(PtcClient *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail(PTC_IS_CLIENT(self), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, self), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

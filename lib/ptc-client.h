/**
 * ptc-client.h - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * A high level GObject/GIO wrapper around the partclone engine.
 *
 * PtcClient drives a single clone, restore, check, device-to-device or domain
 * operation.  It is a thin, cancellable-friendly facade over the classic
 * command line engine and reports progress through the ::progress signal.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PTC_CLIENT_H
#define PTC_CLIENT_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define PTC_TYPE_CLIENT (ptc_client_get_type())
G_DECLARE_FINAL_TYPE(PtcClient, ptc_client, PTC, CLIENT, GObject)

/**
 * PtcMode:
 * @PTC_MODE_CLONE: clone a filesystem into a partclone image.
 * @PTC_MODE_RESTORE: restore a partclone image onto a device.
 * @PTC_MODE_CHKIMG: verify a partclone image.
 * @PTC_MODE_DD: raw device-to-device copy.
 * @PTC_MODE_DOMAIN: create a ddrescue domain log from a source device.
 * @PTC_MODE_DISK_CLONE: clone a whole disk into a format 0003 image.
 * @PTC_MODE_DISK_RESTORE: restore a format 0003 whole-disk image.
 * @PTC_MODE_DISK_CHKIMG: verify a format 0003 whole-disk image.
 *
 * The operation performed by a #PtcClient.
 */
typedef enum {
    PTC_MODE_CLONE   = 0,
    PTC_MODE_RESTORE,
    PTC_MODE_CHKIMG,
    PTC_MODE_DD,
    PTC_MODE_DOMAIN,
    PTC_MODE_DISK_CLONE,
    PTC_MODE_DISK_RESTORE,
    PTC_MODE_DISK_CHKIMG
} PtcMode;

#define PTC_TYPE_MODE (ptc_mode_get_type())
GType ptc_mode_get_type(void);

/**
 * ptc_client_new:
 *
 * Creates a new client with default options (clone mode, CRC32 checksum, the
 * default buffer size, ...).
 *
 * Returns: (transfer full): a new #PtcClient.
 */
PtcClient *ptc_client_new(void);

void     ptc_client_set_mode(PtcClient *self, PtcMode mode);
PtcMode  ptc_client_get_mode(PtcClient *self);

void     ptc_client_set_source(PtcClient *self, const char *source);
const char *ptc_client_get_source(PtcClient *self);

void     ptc_client_set_target(PtcClient *self, const char *target);
const char *ptc_client_get_target(PtcClient *self);

/**
 * ptc_client_set_backend:
 * @self: a #PtcClient.
 * @backend: (nullable): a backend name such as "extfs" or "btrfs".  When %NULL
 *   the backend is auto detected from @source (clone/dd/domain) or read from
 *   the image (restore/check).
 */
void     ptc_client_set_backend(PtcClient *self, const char *backend);
const char *ptc_client_get_backend(PtcClient *self);

void     ptc_client_set_device_size(PtcClient *self, guint64 device_size);
guint64  ptc_client_get_device_size(PtcClient *self);

void     ptc_client_set_debug(PtcClient *self, guint debug);
guint    ptc_client_get_debug(PtcClient *self);

void     ptc_client_set_logfile(PtcClient *self, const char *logfile);
const char *ptc_client_get_logfile(PtcClient *self);

void     ptc_client_set_quiet(PtcClient *self, gboolean quiet);
gboolean ptc_client_get_quiet(PtcClient *self);

void     ptc_client_set_overwrite(PtcClient *self, gboolean overwrite);
gboolean ptc_client_get_overwrite(PtcClient *self);

/**
 * ptc_client_set_check:
 * @self: a #PtcClient.
 * @check: %FALSE to disable the target size / free space checks (the ``-C``
 *   command line option), which also allows restoring onto a not yet existing
 *   regular file.
 */
void     ptc_client_set_check(PtcClient *self, gboolean check);
gboolean ptc_client_get_check(PtcClient *self);

/**
 * ptc_client_set_force:
 * @self: a #PtcClient.
 * @force: %TRUE to ignore recoverable metadata errors (the ``-F`` option).
 */
void     ptc_client_set_force(PtcClient *self, gboolean force);
gboolean ptc_client_get_force(PtcClient *self);

/**
 * ptc_client_set_sparse:
 * @self: a #PtcClient.
 * @sparse: whether raw volumes (whole-disk mode) are stored with a
 *   zero-detecting bitmap (the ``--sparse`` option).
 */
void     ptc_client_set_sparse(PtcClient *self, gboolean sparse);
gboolean ptc_client_get_sparse(PtcClient *self);

/**
 * ptc_client_set_no_zero_skipped:
 * @self: a #PtcClient.
 * @no_zero_skipped: when %TRUE, blocks absent from a child bitmap are left
 *   untouched instead of being zeroed on restore (the ``--no-zero-skipped``
 *   option).
 */
void     ptc_client_set_no_zero_skipped(PtcClient *self, gboolean no_zero_skipped);
gboolean ptc_client_get_no_zero_skipped(PtcClient *self);

/**
 * ptc_client_set_no_fs_volumes:
 * @self: a #PtcClient.
 * @no_fs_volumes: when %TRUE, every partition is stored as a raw volume
 *   instead of a filesystem volume (the ``--no-fs-volumes`` option).
 */
void     ptc_client_set_no_fs_volumes(PtcClient *self, gboolean no_fs_volumes);
gboolean ptc_client_get_no_fs_volumes(PtcClient *self);

/**
 * ptc_client_set_grow:
 * @self: a #PtcClient.
 * @grow: extend the last partition entry to fill a larger restore target
 *   (the ``--grow`` option, whole-disk restore only).
 */
void     ptc_client_set_grow(PtcClient *self, gboolean grow);
gboolean ptc_client_get_grow(PtcClient *self);

/**
 * ptc_client_set_gaps:
 * @self: a #PtcClient.
 * @gaps: (nullable): the gap policy, one of ``"none"``, ``"metadata"``,
 *   ``"sparse"`` or ``"full"``; %NULL selects the default (``"metadata"``).
 */
void     ptc_client_set_gaps(PtcClient *self, const char *gaps);
const char *ptc_client_get_gaps(PtcClient *self);

/**
 * ptc_client_set_partition_table:
 * @self: a #PtcClient.
 * @partition_table: (nullable): the table restore mode, one of ``"auto"``,
 *   ``"raw"`` or ``"regenerate"``; %NULL selects the default (``"auto"``).
 */
void     ptc_client_set_partition_table(PtcClient *self, const char *partition_table);
const char *ptc_client_get_partition_table(PtcClient *self);

/**
 * ptc_client_run:
 * @self: a #PtcClient.
 * @cancellable: (nullable): a #GCancellable, or %NULL.
 * @error: return location for a #GError, or %NULL.
 *
 * Runs the operation synchronously.
 *
 * Because the underlying engine keeps its state in global variables a single
 * process can only run one operation at a time; calls are serialised with an
 * internal lock.
 *
 * Returns: %TRUE on success, %FALSE on error with @error set.
 */
gboolean ptc_client_run(PtcClient *self,
                        GCancellable *cancellable,
                        GError **error);

/**
 * ptc_client_run_async:
 * @self: a #PtcClient.
 * @cancellable: (nullable): a #GCancellable, or %NULL.
 * @callback: (nullable): a #GAsyncReadyCallback.
 * @user_data: (closure): user data for @callback.
 *
 * Runs the operation in a worker thread.
 */
void ptc_client_run_async(PtcClient *self,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data);

/**
 * ptc_client_run_finish:
 * @self: a #PtcClient.
 * @result: a #GAsyncResult.
 * @error: return location for a #GError, or %NULL.
 *
 * Collects the result of ptc_client_run_async().
 *
 * Returns: %TRUE on success, %FALSE on error with @error set.
 */
gboolean ptc_client_run_finish(PtcClient *self,
                               GAsyncResult *result,
                               GError **error);

G_END_DECLS

#endif /* PTC_CLIENT_H */

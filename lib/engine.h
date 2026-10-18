/**
 * engine.h - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * The partclone clone/restore engine.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PTC_ENGINE_H
#define PTC_ENGINE_H

#include <glib.h>

#include "partclone.h"
#include "ptc-fs-backend.h"
#include "diskimg.h"

G_BEGIN_DECLS

/**
 * ptc_progress_func:
 * @copied: number of blocks copied so far.
 * @total: total number of blocks.
 * @user_data: (closure): user data.
 *
 * Progress callback invoked from the progress thread while the engine runs.
 */
typedef void (*ptc_progress_func)(unsigned long long copied,
                                  unsigned long long total,
                                  gpointer user_data);

/**
 * ptc_volume_info:
 * @index: 1-based volume number, in stream order.
 * @count: total number of volumes in the image.
 * @kind: one of the `PTC_VOL_*` kinds.
 * @flags: `PTC_VOL_*` flags.
 * @part_number: partition number, or 0 for a gap.
 * @disk_offset: byte offset of the volume on the disk.
 * @disk_length: byte length of the volume on the disk.
 * @payload_length: length of the child format 0002 image, in bytes.
 * @fs: filesystem magic of the child image (never %NULL).
 * @label: filesystem label or partition name (never %NULL, may be empty).
 *
 * A description of one volume, passed to the whole-disk progress callbacks.
 * The strings are owned by the engine and only valid for the duration of the
 * callback.
 */
typedef struct {
    unsigned int index;
    unsigned int count;
    guint32      kind;
    guint32      flags;
    guint32      part_number;
    guint64      disk_offset;
    guint64      disk_length;
    guint64      payload_length;
    const char  *fs;
    const char  *label;
} ptc_volume_info;

/**
 * ptc_disk_progress:
 * @overall_progress: (nullable): called periodically with the progress of the
 *   current volume (kept for compatibility with the classic `PtcClient::progress`).
 * @volume_started: (nullable): called before a volume is processed.
 * @volume_progress: (nullable): called periodically while a volume is processed.
 * @volume_finished: (nullable): called after a volume has been processed.
 * @cancelled: (nullable): polled between volumes and between block strips; when
 *   it returns %TRUE the operation is aborted.
 * @data: (closure): passed to every callback.
 *
 * The callback set used by ptc_disk_engine_run().
 */
typedef struct {
    void (*overall_progress)(unsigned long long copied, unsigned long long total,
                             gpointer data);
    void (*volume_started)(const ptc_volume_info *info, gpointer data);
    void (*volume_progress)(const ptc_volume_info *info,
                            unsigned long long copied, unsigned long long total,
                            gpointer data);
    void (*volume_finished)(const ptc_volume_info *info, gpointer data);
    gboolean (*cancelled)(gpointer data);
    gpointer data;
} ptc_disk_progress;

/**
 * ptc_engine_run:
 * @backend: (nullable): filesystem backend to use for clone/dd/domain modes.
 * @progress: (nullable): progress callback.
 * @user_data: (closure): data passed to @progress.
 *
 * Runs the clone/restore engine using the global #cmd_opt (see ptc_parse_options()
 * or set the fields directly).  #fs_opt is initialised from #cmd_opt.
 *
 * Returns: 0 on success, non-zero on failure.
 */
int ptc_engine_run(const ptc_fs_backend *backend,
                   ptc_progress_func progress,
                   gpointer user_data);

/**
 * ptc_engine_run_named:
 * @backend_name: (nullable): backend name, or %NULL.
 * @progress: (nullable): progress callback.
 * @user_data: (closure): data passed to @progress.
 *
 * Convenience wrapper around ptc_engine_run() that looks @backend_name up in the
 * backend registry.
 *
 * Returns: 0 on success, non-zero on failure.
 */
int ptc_engine_run_named(const char *backend_name,
                         ptc_progress_func progress,
                         gpointer user_data);

/**
 * ptc_disk_engine_run:
 * @progress: (nullable): the callback set, or %NULL.
 *
 * Runs the whole-disk (format 0003) engine using the global #cmd_opt.  The mode
 * is selected by #opt.clone, #opt.chkimg or #opt.restore.
 *
 * Returns: 0 on success, non-zero on failure (including -2 when cancelled).
 */
int ptc_disk_engine_run(const ptc_disk_progress *progress);

G_END_DECLS

#endif /* PTC_ENGINE_H */

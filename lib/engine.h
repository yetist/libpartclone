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
 * @progress: (nullable): progress callback.
 * @user_data: (closure): data passed to @progress.
 *
 * Runs the whole-disk (format 0003) engine using the global #cmd_opt.  The mode
 * is selected by #opt.clone, #opt.chkimg or #opt.restore.  Phase 1 only handles
 * raw volumes (a whole disk stored as a single raw volume, no partition table).
 *
 * Returns: 0 on success, non-zero on failure.
 */
int ptc_disk_engine_run(ptc_progress_func progress, gpointer user_data);

G_END_DECLS

#endif /* PTC_ENGINE_H */

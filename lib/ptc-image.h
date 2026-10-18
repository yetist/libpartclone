/**
 * ptc-image.h - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * A read-only, introspectable view of a partclone image.
 *
 * #PtcImage reads the metadata of a format 0001, 0002 or 0003 image without
 * touching its data.  For a whole-disk image (format 0003) it also exposes the
 * list of #PtcVolume objects (one per stored region: filesystem, raw or gap),
 * each with its disk offset, length, filesystem magic and label.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PTC_IMAGE_H
#define PTC_IMAGE_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* ------------------------------------------------------------------ */
/* PtcVolume                                                           */
/* ------------------------------------------------------------------ */

#define PTC_TYPE_VOLUME (ptc_volume_get_type())
G_DECLARE_FINAL_TYPE(PtcVolume, ptc_volume, PTC, VOLUME, GObject)

/**
 * PtcVolumeKind:
 * @PTC_VOLUME_KIND_NONE: reserved / unknown.
 * @PTC_VOLUME_KIND_FILESYSTEM: a filesystem volume (only used blocks stored).
 * @PTC_VOLUME_KIND_RAW_SPARSE: a raw volume with a zero-detecting bitmap.
 * @PTC_VOLUME_KIND_RAW_FULL: a raw volume with every block stored.
 * @PTC_VOLUME_KIND_ZERO: a region known to be zero, with no stored data.
 *
 * The kind of a volume inside a format 0003 image.
 */
typedef enum {
    PTC_VOLUME_KIND_NONE       = 0,
    PTC_VOLUME_KIND_FILESYSTEM = 1,
    PTC_VOLUME_KIND_RAW_SPARSE = 2,
    PTC_VOLUME_KIND_RAW_FULL   = 3,
    PTC_VOLUME_KIND_ZERO       = 4
} PtcVolumeKind;

#define PTC_TYPE_VOLUME_KIND (ptc_volume_kind_get_type())
GType ptc_volume_kind_get_type(void);

guint         ptc_volume_get_index(PtcVolume *self);
guint         ptc_volume_get_part_number(PtcVolume *self);
PtcVolumeKind ptc_volume_get_kind(PtcVolume *self);
gboolean      ptc_volume_is_filesystem(PtcVolume *self);
gboolean      ptc_volume_is_sparse(PtcVolume *self);
gboolean      ptc_volume_is_bootable(PtcVolume *self);
const char   *ptc_volume_get_filesystem(PtcVolume *self);
const char   *ptc_volume_get_label(PtcVolume *self);
guint64       ptc_volume_get_disk_offset(PtcVolume *self);
guint64       ptc_volume_get_disk_length(PtcVolume *self);
guint64       ptc_volume_get_payload_length(PtcVolume *self);
guint         ptc_volume_get_flags(PtcVolume *self);

/* ------------------------------------------------------------------ */
/* PtcImage                                                            */
/* ------------------------------------------------------------------ */

#define PTC_TYPE_IMAGE (ptc_image_get_type())
G_DECLARE_FINAL_TYPE(PtcImage, ptc_image, PTC, IMAGE, GObject)

/**
 * ptc_image_new_from_file:
 * @path: path to a partclone image.
 * @error: return location for a #GError, or %NULL.
 *
 * Reads the metadata of the image at @path.
 *
 * Returns: (transfer full) (nullable): a new #PtcImage, or %NULL on error.
 */
PtcImage *ptc_image_new_from_file(const char *path, GError **error);

/**
 * ptc_image_new_from_file_sync:
 * @file: a #GFile for a partclone image.
 * @cancellable: (nullable): a #GCancellable, or %NULL.
 * @error: return location for a #GError, or %NULL.
 *
 * GIO flavoured variant of ptc_image_new_from_file().
 *
 * Returns: (transfer full) (nullable): a new #PtcImage, or %NULL on error.
 */
PtcImage *ptc_image_new_from_file_sync(GFile *file, GCancellable *cancellable,
                                       GError **error);

/**
 * ptc_image_is_disk:
 *
 * Returns: %TRUE when the image is a format 0003 whole-disk container.
 */
gboolean      ptc_image_is_disk(PtcImage *self);

/// Returns: the image version, "0001", "0002" or "0003".
const char   *ptc_image_get_version(PtcImage *self);

/// Returns: (nullable): the partition table type ("MBR", "GPT", ...) or %NULL.
const char   *ptc_image_get_partition_table(PtcImage *self);

/// Returns: the source disk (or device) size in bytes.
guint64       ptc_image_get_disk_size(PtcImage *self);

/// Returns: the logical sector size in bytes.
guint         ptc_image_get_sector_size(PtcImage *self);

/// Returns: (nullable): the filesystem magic of a plain 0001/0002 image.
const char   *ptc_image_get_filesystem(PtcImage *self);

/// Returns: the number of volumes (0 for a plain 0001/0002 image).
guint         ptc_image_get_volume_count(PtcImage *self);

/**
 * ptc_image_get_volume:
 * @index: a 0-based volume index.
 *
 * Returns: (transfer none) (nullable): the volume, or %NULL when out of range.
 */
PtcVolume    *ptc_image_get_volume(PtcImage *self, guint index);

/**
 * ptc_image_get_volumes:
 *
 * Returns: (transfer none): a #GListModel of #PtcVolume.
 */
GListModel   *ptc_image_get_volumes(PtcImage *self);

G_END_DECLS

#endif /* PTC_IMAGE_H */

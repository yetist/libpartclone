/**
 * ptc-image.c - part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * A read-only, introspectable view of a partclone image (see ptc-image.h).
 *
 * The image header is parsed directly here instead of going through the clone
 * engine: reading metadata must never write to the log file, exit the process
 * or touch any data.  For a format 0003 whole-disk image the volume descriptors
 * are read through diskimg.c, which also knows how to skip the child images.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib-object.h>

#include "ptc-image.h"
#include "partclone.h"
#include "diskimg.h"

/* ------------------------------------------------------------------ */
/* Errors                                                              */
/* ------------------------------------------------------------------ */

#define PTC_IMAGE_ERROR (ptc_image_error_quark())

/**
 * PtcImageError:
 * @PTC_IMAGE_ERROR_FAILED: a generic failure.
 * @PTC_IMAGE_ERROR_FORMAT: the file is not a partclone image.
 * @PTC_IMAGE_ERROR_UNSUPPORTED: the image version is not supported.
 */
typedef enum {
    PTC_IMAGE_ERROR_FAILED,
    PTC_IMAGE_ERROR_FORMAT,
    PTC_IMAGE_ERROR_UNSUPPORTED
} PtcImageError;

static GQuark
ptc_image_error_quark(void)
{
    return g_quark_from_static_string("ptc-image-error-quark");
}

/* ------------------------------------------------------------------ */
/* PtcVolumeKind                                                       */
/* ------------------------------------------------------------------ */

GType
ptc_volume_kind_get_type(void)
{
    static gsize id = 0;

    if (g_once_init_enter(&id)) {
        static const GEnumValue values[] = {
            { PTC_VOLUME_KIND_NONE,       "PTC_VOLUME_KIND_NONE",       "none" },
            { PTC_VOLUME_KIND_FILESYSTEM, "PTC_VOLUME_KIND_FILESYSTEM", "filesystem" },
            { PTC_VOLUME_KIND_RAW_SPARSE, "PTC_VOLUME_KIND_RAW_SPARSE", "raw-sparse" },
            { PTC_VOLUME_KIND_RAW_FULL,   "PTC_VOLUME_KIND_RAW_FULL",   "raw-full" },
            { PTC_VOLUME_KIND_ZERO,       "PTC_VOLUME_KIND_ZERO",       "zero" },
            { 0, NULL, NULL }
        };
        GType type = g_enum_register_static("PtcVolumeKind", values);

        g_once_init_leave(&id, type);
    }
    return (GType)id;
}

/* ------------------------------------------------------------------ */
/* PtcVolume                                                           */
/* ------------------------------------------------------------------ */

struct _PtcVolume {
    GObject parent_instance;

    guint         index;
    guint         part_number;
    PtcVolumeKind kind;
    guint         flags;
    gchar        *filesystem;
    gchar        *label;
    guint64       disk_offset;
    guint64       disk_length;
    guint64       payload_length;
};

G_DEFINE_TYPE(PtcVolume, ptc_volume, G_TYPE_OBJECT)

enum {
    VOL_PROP_0,
    VOL_PROP_INDEX,
    VOL_PROP_PART_NUMBER,
    VOL_PROP_KIND,
    VOL_PROP_FLAGS,
    VOL_PROP_FILESYSTEM,
    VOL_PROP_LABEL,
    VOL_PROP_DISK_OFFSET,
    VOL_PROP_DISK_LENGTH,
    VOL_PROP_PAYLOAD_LENGTH,
    VOL_N_PROPS
};

static GParamSpec *vol_props[VOL_N_PROPS];

static void
ptc_volume_set_property(GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec)
{
    PtcVolume *self = PTC_VOLUME(object);

    switch (prop_id) {
    case VOL_PROP_INDEX:
        self->index = g_value_get_uint(value);
        break;
    case VOL_PROP_PART_NUMBER:
        self->part_number = g_value_get_uint(value);
        break;
    case VOL_PROP_KIND:
        self->kind = g_value_get_enum(value);
        break;
    case VOL_PROP_FLAGS:
        self->flags = g_value_get_uint(value);
        break;
    case VOL_PROP_FILESYSTEM:
        g_free(self->filesystem);
        self->filesystem = g_value_dup_string(value);
        break;
    case VOL_PROP_LABEL:
        g_free(self->label);
        self->label = g_value_dup_string(value);
        break;
    case VOL_PROP_DISK_OFFSET:
        self->disk_offset = g_value_get_uint64(value);
        break;
    case VOL_PROP_DISK_LENGTH:
        self->disk_length = g_value_get_uint64(value);
        break;
    case VOL_PROP_PAYLOAD_LENGTH:
        self->payload_length = g_value_get_uint64(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
ptc_volume_get_property(GObject *object, guint prop_id,
                        GValue *value, GParamSpec *pspec)
{
    PtcVolume *self = PTC_VOLUME(object);

    switch (prop_id) {
    case VOL_PROP_INDEX:
        g_value_set_uint(value, self->index);
        break;
    case VOL_PROP_PART_NUMBER:
        g_value_set_uint(value, self->part_number);
        break;
    case VOL_PROP_KIND:
        g_value_set_enum(value, self->kind);
        break;
    case VOL_PROP_FLAGS:
        g_value_set_uint(value, self->flags);
        break;
    case VOL_PROP_FILESYSTEM:
        g_value_set_string(value, self->filesystem);
        break;
    case VOL_PROP_LABEL:
        g_value_set_string(value, self->label);
        break;
    case VOL_PROP_DISK_OFFSET:
        g_value_set_uint64(value, self->disk_offset);
        break;
    case VOL_PROP_DISK_LENGTH:
        g_value_set_uint64(value, self->disk_length);
        break;
    case VOL_PROP_PAYLOAD_LENGTH:
        g_value_set_uint64(value, self->payload_length);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
ptc_volume_finalize(GObject *object)
{
    PtcVolume *self = PTC_VOLUME(object);

    g_free(self->filesystem);
    g_free(self->label);

    G_OBJECT_CLASS(ptc_volume_parent_class)->finalize(object);
}

static void
ptc_volume_class_init(PtcVolumeClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = ptc_volume_set_property;
    object_class->get_property = ptc_volume_get_property;
    object_class->finalize = ptc_volume_finalize;

    vol_props[VOL_PROP_INDEX] =
        g_param_spec_uint("index", "Index", "1-based volume number",
                          0, G_MAXUINT, 0,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_PART_NUMBER] =
        g_param_spec_uint("part-number", "Partition number",
                          "Partition number, or 0 for a gap",
                          0, G_MAXUINT, 0,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_KIND] =
        g_param_spec_enum("kind", "Kind", "Volume kind",
                          PTC_TYPE_VOLUME_KIND, PTC_VOLUME_KIND_NONE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_FLAGS] =
        g_param_spec_uint("flags", "Flags", "PTC_VOL_* flags",
                          0, G_MAXUINT, 0,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_FILESYSTEM] =
        g_param_spec_string("filesystem", "Filesystem",
                            "Filesystem magic of the child image",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_LABEL] =
        g_param_spec_string("label", "Label",
                            "Filesystem label or partition name",
                            NULL,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_DISK_OFFSET] =
        g_param_spec_uint64("disk-offset", "Disk offset",
                            "Byte offset of the volume on the disk",
                            0, G_MAXUINT64, 0,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_DISK_LENGTH] =
        g_param_spec_uint64("disk-length", "Disk length",
                            "Byte length of the volume on the disk",
                            0, G_MAXUINT64, 0,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    vol_props[VOL_PROP_PAYLOAD_LENGTH] =
        g_param_spec_uint64("payload-length", "Payload length",
                            "Length of the child format 0002 image",
                            0, G_MAXUINT64, 0,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(object_class, VOL_N_PROPS, vol_props);
}

static void
ptc_volume_init(PtcVolume *self)
{
    self->kind = PTC_VOLUME_KIND_NONE;
    self->filesystem = g_strdup("");
    self->label = g_strdup("");
}

guint
ptc_volume_get_index(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), 0);
    return self->index;
}

guint
ptc_volume_get_part_number(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), 0);
    return self->part_number;
}

PtcVolumeKind
ptc_volume_get_kind(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), PTC_VOLUME_KIND_NONE);
    return self->kind;
}

gboolean
ptc_volume_is_filesystem(PtcVolume *self)
{
    return ptc_volume_get_kind(self) == PTC_VOLUME_KIND_FILESYSTEM;
}

gboolean
ptc_volume_is_sparse(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), FALSE);
    return (self->flags & PTC_VOL_SPARSE) != 0;
}

gboolean
ptc_volume_is_bootable(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), FALSE);
    return (self->flags & PTC_VOL_BOOTABLE) != 0;
}

const char *
ptc_volume_get_filesystem(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), NULL);
    return self->filesystem;
}

const char *
ptc_volume_get_label(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), NULL);
    return self->label;
}

guint64
ptc_volume_get_disk_offset(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), 0);
    return self->disk_offset;
}

guint64
ptc_volume_get_disk_length(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), 0);
    return self->disk_length;
}

guint64
ptc_volume_get_payload_length(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), 0);
    return self->payload_length;
}

guint
ptc_volume_get_flags(PtcVolume *self)
{
    g_return_val_if_fail(PTC_IS_VOLUME(self), 0);
    return self->flags;
}

static PtcVolumeKind
volume_kind_from_desc(guint16 kind)
{
    switch (kind) {
    case PTC_VOL_FILESYSTEM: return PTC_VOLUME_KIND_FILESYSTEM;
    case PTC_VOL_RAW_SPARSE: return PTC_VOLUME_KIND_RAW_SPARSE;
    case PTC_VOL_RAW_FULL:   return PTC_VOLUME_KIND_RAW_FULL;
    case PTC_VOL_ZERO:       return PTC_VOLUME_KIND_ZERO;
    default:                 return PTC_VOLUME_KIND_NONE;
    }
}

static PtcVolume *
ptc_volume_new_from_desc(guint index, const image_volume_desc_v3 *desc)
{
    return g_object_new(PTC_TYPE_VOLUME,
                        "index", index,
                        "part-number", desc->part_number,
                        "kind", volume_kind_from_desc(desc->kind),
                        "flags", (guint)desc->flags,
                        "filesystem", desc->fs,
                        "label", desc->label,
                        "disk-offset", (guint64)desc->disk_offset,
                        "disk-length", (guint64)desc->disk_length,
                        "payload-length", (guint64)desc->payload_length,
                        NULL);
}

/* ------------------------------------------------------------------ */
/* PtcImage                                                            */
/* ------------------------------------------------------------------ */

struct _PtcImage {
    GObject parent_instance;

    gboolean    is_disk;
    gchar      *version;
    gchar      *partition_table;
    gchar      *filesystem;
    guint64     disk_size;
    guint       sector_size;
    GListStore *volumes;
    GPtrArray  *volume_list;
};

G_DEFINE_TYPE(PtcImage, ptc_image, G_TYPE_OBJECT)

enum {
    IMG_PROP_0,
    IMG_PROP_IS_DISK,
    IMG_PROP_VERSION,
    IMG_PROP_PARTITION_TABLE,
    IMG_PROP_FILESYSTEM,
    IMG_PROP_DISK_SIZE,
    IMG_PROP_SECTOR_SIZE,
    IMG_PROP_VOLUME_COUNT,
    IMG_PROP_VOLUMES,
    IMG_N_PROPS
};

static GParamSpec *img_props[IMG_N_PROPS];

static void
ptc_image_get_property(GObject *object, guint prop_id,
                       GValue *value, GParamSpec *pspec)
{
    PtcImage *self = PTC_IMAGE(object);

    switch (prop_id) {
    case IMG_PROP_IS_DISK:
        g_value_set_boolean(value, self->is_disk);
        break;
    case IMG_PROP_VERSION:
        g_value_set_string(value, self->version);
        break;
    case IMG_PROP_PARTITION_TABLE:
        g_value_set_string(value, self->partition_table);
        break;
    case IMG_PROP_FILESYSTEM:
        g_value_set_string(value, self->filesystem);
        break;
    case IMG_PROP_DISK_SIZE:
        g_value_set_uint64(value, self->disk_size);
        break;
    case IMG_PROP_SECTOR_SIZE:
        g_value_set_uint(value, self->sector_size);
        break;
    case IMG_PROP_VOLUME_COUNT:
        g_value_set_uint(value, g_list_model_get_n_items(G_LIST_MODEL(self->volumes)));
        break;
    case IMG_PROP_VOLUMES:
        g_value_set_object(value, self->volumes);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
ptc_image_finalize(GObject *object)
{
    PtcImage *self = PTC_IMAGE(object);

    g_clear_object(&self->volumes);
    g_clear_pointer(&self->volume_list, g_ptr_array_unref);
    g_free(self->version);
    g_free(self->partition_table);
    g_free(self->filesystem);

    G_OBJECT_CLASS(ptc_image_parent_class)->finalize(object);
}

static void
ptc_image_class_init(PtcImageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = ptc_image_get_property;
    object_class->finalize = ptc_image_finalize;

    img_props[IMG_PROP_IS_DISK] =
        g_param_spec_boolean("is-disk", "Is disk",
                             "Whether this is a format 0003 whole-disk image",
                             FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    img_props[IMG_PROP_VERSION] =
        g_param_spec_string("version", "Version", "Image version",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    img_props[IMG_PROP_PARTITION_TABLE] =
        g_param_spec_string("partition-table", "Partition table",
                            "Partition table type, or NULL",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    img_props[IMG_PROP_FILESYSTEM] =
        g_param_spec_string("filesystem", "Filesystem",
                            "Filesystem magic of a plain image",
                            NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    img_props[IMG_PROP_DISK_SIZE] =
        g_param_spec_uint64("disk-size", "Disk size",
                            "Source size in bytes",
                            0, G_MAXUINT64, 0,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    img_props[IMG_PROP_SECTOR_SIZE] =
        g_param_spec_uint("sector-size", "Sector size",
                          "Logical sector size in bytes",
                          0, G_MAXUINT, 0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    img_props[IMG_PROP_VOLUME_COUNT] =
        g_param_spec_uint("volume-count", "Volume count",
                          "Number of volumes",
                          0, G_MAXUINT, 0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    img_props[IMG_PROP_VOLUMES] =
        g_param_spec_object("volumes", "Volumes", "The list of volumes",
                            G_TYPE_LIST_MODEL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(object_class, IMG_N_PROPS, img_props);
}

static void
ptc_image_init(PtcImage *self)
{
    self->is_disk = FALSE;
    self->version = g_strdup("");
    self->volumes = g_list_store_new(PTC_TYPE_VOLUME);
    self->volume_list = g_ptr_array_new_with_free_func(g_object_unref);
}

/**
 * ptc_image_load_disk - fill @self from a format 0003 disk header.
 */
static gboolean
ptc_image_load_disk(PtcImage *self, image_disk_head_v3 *head, int fd,
                    GError **error)
{
    ptc_disk_image dimg;
    cmd_opt opt;
    uint32_t i;

    ptc_opt_init(&opt);
    opt.debug = 0;
    opt.force = 1;

    if (ptc_disk_image_load(&fd, head, &dimg, &opt) != 0) {
        g_set_error(error, PTC_IMAGE_ERROR, PTC_IMAGE_ERROR_FAILED,
                    "unable to read the whole-disk image");
        return FALSE;
    }

    self->is_disk = TRUE;
    g_free(self->version);
    self->version = g_strndup(dimg.head.version, IMAGE_VERSION_SIZE);
    self->disk_size = dimg.head.disk_size;
    self->sector_size = dimg.head.sector_size;
    if (dimg.has_table && dimg.table.type != PTC_TABLE_NONE) {
        g_free(self->partition_table);
        self->partition_table = g_strdup(ptc_table_type_str(dimg.table.type));
    }

    for (i = 0; i < dimg.n_volumes; i++) {
        PtcVolume *vol = ptc_volume_new_from_desc(i + 1, &dimg.volumes[i]);

        g_list_store_append(self->volumes, vol);
        g_ptr_array_add(self->volume_list, vol);
    }

    ptc_disk_image_free(&dimg);
    return TRUE;
}

/**
 * ptc_image_load_fd - read the metadata of the image on @fd.
 */
static gboolean
ptc_image_load_fd(PtcImage *self, int fd, GError **error)
{
    cmd_opt opt;
    image_desc_v2 pre;
    int r_size;

    ptc_opt_init(&opt);
    opt.debug = 0;
    opt.force = 1;
    /* No open_log(): log_mesg() tolerates a NULL log file and opt.force
     * keeps a malformed image from exiting the process. */

    r_size = read_all(&fd, (char *)&pre, sizeof(pre), &opt);
    if (r_size != (int)sizeof(pre)) {
        g_set_error(error, PTC_IMAGE_ERROR, PTC_IMAGE_ERROR_FORMAT,
                    "unable to read the image header");
        return FALSE;
    }
    if (memcmp(pre.head.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) != 0) {
        g_set_error(error, PTC_IMAGE_ERROR, PTC_IMAGE_ERROR_FORMAT,
                    "not a partclone image");
        return FALSE;
    }

    if (memcmp(pre.head.version, PTC_IMAGE_VERSION_0003, IMAGE_VERSION_SIZE) == 0) {
        image_disk_head_v3 dh;

        memcpy(&dh, &pre, sizeof(pre));
        r_size = read_all(&fd, (char *)&dh + sizeof(pre),
                          sizeof(dh) - sizeof(pre), &opt);
        if (r_size != (int)(sizeof(dh) - sizeof(pre))) {
            g_set_error(error, PTC_IMAGE_ERROR, PTC_IMAGE_ERROR_FORMAT,
                        "unable to read the whole-disk image header");
            return FALSE;
        }
        return ptc_image_load_disk(self, &dh, fd, error);
    }

    if (memcmp(pre.head.version, IMAGE_VERSION_0001, IMAGE_VERSION_SIZE) != 0 &&
        memcmp(pre.head.version, IMAGE_VERSION_0002, IMAGE_VERSION_SIZE) != 0) {
        g_set_error(error, PTC_IMAGE_ERROR, PTC_IMAGE_ERROR_UNSUPPORTED,
                    "unsupported image version");
        return FALSE;
    }

    {
        image_head_v2 head;
        file_system_info fs_info;
        image_options img_opt;

        load_image_desc_ex(&fd, &opt, &head, &fs_info, &img_opt, &pre);

        g_free(self->version);
        self->version = g_strndup(pre.head.version, IMAGE_VERSION_SIZE);
        self->disk_size = fs_info.device_size;
        self->sector_size = fs_info.block_size;
        g_free(self->filesystem);
        self->filesystem = g_strndup(fs_info.fs, FS_MAGIC_SIZE);
    }

    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

PtcImage *
ptc_image_new_from_file(const char *path, GError **error)
{
    PtcImage *self;
    int fd;

    g_return_val_if_fail(path != NULL, NULL);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "unable to open %s: %s", path, g_strerror(errno));
        return NULL;
    }

    self = g_object_new(PTC_TYPE_IMAGE, NULL);
    if (!ptc_image_load_fd(self, fd, error)) {
        close(fd);
        g_object_unref(self);
        return NULL;
    }

    close(fd);
    return self;
}

PtcImage *
ptc_image_new_from_file_sync(GFile *file, GCancellable *cancellable,
                             GError **error)
{
    gchar *path;
    PtcImage *self;

    g_return_val_if_fail(G_IS_FILE(file), NULL);

    if (cancellable != NULL && g_cancellable_is_cancelled(cancellable)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                            "operation was cancelled");
        return NULL;
    }

    path = g_file_get_path(file);
    if (path == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                            "only local image files are supported");
        return NULL;
    }

    self = ptc_image_new_from_file(path, error);
    g_free(path);
    return self;
}

gboolean
ptc_image_is_disk(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), FALSE);
    return self->is_disk;
}

const char *
ptc_image_get_version(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), NULL);
    return self->version;
}

const char *
ptc_image_get_partition_table(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), NULL);
    return self->partition_table;
}

guint64
ptc_image_get_disk_size(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), 0);
    return self->disk_size;
}

guint
ptc_image_get_sector_size(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), 0);
    return self->sector_size;
}

const char *
ptc_image_get_filesystem(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), NULL);
    return self->filesystem;
}

guint
ptc_image_get_volume_count(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), 0);
    return g_list_model_get_n_items(G_LIST_MODEL(self->volumes));
}

PtcVolume *
ptc_image_get_volume(PtcImage *self, guint index)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), NULL);
    if (index >= self->volume_list->len)
        return NULL;
    return g_ptr_array_index(self->volume_list, index);
}

GListModel *
ptc_image_get_volumes(PtcImage *self)
{
    g_return_val_if_fail(PTC_IS_IMAGE(self), NULL);
    return G_LIST_MODEL(self->volumes);
}

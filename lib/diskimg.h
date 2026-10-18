/**
 * diskimg.h - whole-disk image container (format 0003), part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Format 0003 is a *container* for backing up an entire block device: a fixed
 * 512-byte disk header, an optional partition table blob and then a sequence of
 * volumes.  Every volume is a (256-byte descriptor, child format 0002 image)
 * pair, so the existing clone/restore engine and per-filesystem backends are
 * reused unchanged.
 *
 * See IMAGE_FORMATS.md ("Format 0003 - whole-disk images") for the full
 * description of every field.  This header only implements phase 1: the
 * container reader/writer and raw-only volumes (one raw volume covering the
 * whole device, no partition table yet).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PTC_DISKIMG_H
#define PTC_DISKIMG_H

#include <stdint.h>

#include "partclone.h"

#define PTC_DISK_HEADER_SIZE   512
#define PTC_VOLUME_DESC_SIZE   256

#define PTC_IMAGE_VERSION_0003 "0003"
#define PTC_DISK_IMAGE_VERSION 0x0003

/// Disk header flags (see IMAGE_FORMATS.md)
#define PTC_DISK_TABLE_PRESENT (1u << 0)
#define PTC_DISK_HAS_TRAILER   (1u << 1)
#define PTC_DISK_GAPS_INCLUDED (1u << 2)
#define PTC_DISK_SPARSE_RAW    (1u << 3)

/// Partition table type
#define PTC_TABLE_NONE   0
#define PTC_TABLE_MBR    1
#define PTC_TABLE_GPT    2
#define PTC_TABLE_HYBRID 3
#define PTC_TABLE_OTHER  4

/// Volume kind
#define PTC_VOL_NONE       0
#define PTC_VOL_FILESYSTEM 1
#define PTC_VOL_RAW_SPARSE 2
#define PTC_VOL_RAW_FULL   3
#define PTC_VOL_ZERO       4

/// Volume flags
#define PTC_VOL_SPARSE   (1u << 0)
#define PTC_VOL_BOOTABLE (1u << 1)
#define PTC_VOL_SKIP     (1u << 2)
#define PTC_VOL_GROWABLE (1u << 3)

/* Disable fields alignment for structs stored in the image */
#pragma pack(push, 1)

/// Format 0003 disk header (512 bytes)
typedef struct
{
    char     magic[16];             /// physical signature
    char     ptc_version[14];       /// Partclone's version who created the image
    char     version[IMAGE_VERSION_SIZE]; /// Image version, in text: "0003"
    uint16_t endianess;             /// 0xC0DE = little-endian, 0xDEC0 = big-endian
    uint32_t header_size;           /// Size of this header, in bytes (512)
    uint16_t image_version;         /// Image version, in binary: 0x0003
    uint16_t cpu_bits;              /// 32 or 64, the CPU that created the image
    uint64_t disk_size;             /// Source disk size, in bytes
    uint32_t sector_size;           /// Logical sector size of the source disk
    uint32_t physical_sector_size;  /// Physical sector size (informative)
    uint32_t volume_count;          /// Number of volume descriptors
    uint32_t descriptor_size;       /// Size of one volume descriptor (256)
    uint32_t flags;                 /// PTC_DISK_* flags
    uint32_t table_type;            /// PTC_TABLE_* type
    uint32_t table_size;            /// Size of the partition table blob, in bytes
    uint16_t checksum_mode;         /// Disk-level checksum mode (informative)
    uint16_t checksum_size;         /// Disk-level checksum size, in bytes
    uint32_t feature_size;          /// Size of the feature section
    uint64_t image_size;            /// Total image size, or 0 when unknown
    uint8_t  reserved[412];         /// Reserved, zero
    uint32_t crc;                   /// CRC32 of the previous 508 bytes
} image_disk_head_v3;

/// Format 0003 volume descriptor (256 bytes)
typedef struct
{
    uint16_t kind;                  /// PTC_VOL_* kind
    uint16_t flags;                 /// PTC_VOL_* flags
    uint32_t part_number;           /// Partition number, or 0 for a non-partition region
    char     fs[FS_MAGIC_SIZE + 1]; /// Filesystem magic of the child image
    uint64_t disk_offset;           /// Byte offset of this volume on the disk
    uint64_t disk_length;           /// Byte length of this volume on the disk
    uint64_t payload_length;        /// Length of the child image that follows
    uint32_t block_size;            /// Block size used by the child image bitmap
    uint32_t reserved1;             /// Reserved, zero
    uint8_t  part_type[16];         /// Partition type (MBR type byte or GPT type GUID)
    uint8_t  part_guid[16];         /// Partition unique GUID (GPT)
    char     label[72];             /// Filesystem label or partition name, UTF-8
    uint8_t  reserved2[92];         /// Reserved, zero
    uint32_t crc;                   /// CRC32 of the previous 252 bytes
} image_volume_desc_v3;

#pragma pack(pop)

/* The typedefs and the enum constants above are the stable, public part of this
 * header.  The functions below are used by the disk engine (diskimg.c) and are
 * exported for tooling such as partclone.info; they are not part of the
 * libpartclone GObject API. */
extern void ptc_disk_init_head(image_disk_head_v3 *head,
                               const file_system_info *fs_info,
                               uint32_t volume_count,
                               uint32_t flags,
                               uint32_t table_type,
                               uint32_t table_size,
                               cmd_opt *opt);
extern void ptc_disk_init_desc(image_volume_desc_v3 *desc,
                               uint16_t kind,
                               uint16_t flags,
                               uint64_t disk_offset,
                               uint64_t disk_length,
                               uint64_t payload_length,
                               uint32_t block_size,
                               const char *fs);
extern int  ptc_disk_load_head(int *ret, image_disk_head_v3 *head, cmd_opt *opt);
extern void ptc_disk_write_head(int *ret, image_disk_head_v3 *head, cmd_opt *opt);
extern void ptc_disk_load_desc(int *ret, image_volume_desc_v3 *desc, cmd_opt *opt);
extern void ptc_disk_write_desc(int *ret, image_volume_desc_v3 *desc, cmd_opt *opt);
extern const char *ptc_disk_vol_kind_str(uint16_t kind);

#endif /* PTC_DISKIMG_H */

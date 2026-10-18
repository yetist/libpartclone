/**
 * disktable.h - partition table handling for format 0003, part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * This module reads the partition table (MBR or GPT) of a whole block device,
 * turns it into the format 0003 *partition table blob* (raw sectors plus a
 * device-neutral manifest) and, on restore, writes the table back - either
 * verbatim from the raw records or regenerated from the manifest through
 * libfdisk (needed when the target disk has a different size).
 *
 * The blob layout and the meaning of every field are documented in
 * IMAGE_FORMATS.md ("Format 0003 - Partition table blob").
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PTC_DISKTABLE_H
#define PTC_DISKTABLE_H

#include <stdint.h>

#include "partclone.h"

/* Partition table type (also the disk header field) */
#define PTC_TABLE_NONE   0
#define PTC_TABLE_MBR    1
#define PTC_TABLE_GPT    2
#define PTC_TABLE_HYBRID 3
#define PTC_TABLE_OTHER  4

/* Manifest entry flags */
#define PTC_PART_BOOTABLE  (1u << 0)
#define PTC_PART_CONTAINER (1u << 1)
#define PTC_PART_LOGICAL   (1u << 2)
#define PTC_PART_RESERVED  (1u << 3)

/* Gap policy (PTC_GAPS_*) and table restore mode (PTC_PT_*) live in
 * partclone.h because they are stored in cmd_opt. */

#define PTC_BLOB_HEADER_SIZE   32
#define PTC_RAW_RECORD_SIZE    12
#define PTC_MANIFEST_SIZE      128
#define PTC_DEFAULT_GAP_THRESHOLD (1024ULL * 1024ULL)

#define PTC_TABLE_MAX_PARTS    256
#define PTC_TABLE_MAX_RECORDS  64

/// One partition, as stored in the manifest (128 bytes on disk)
typedef struct {
    uint32_t index;         /// 1-based partition number
    uint32_t flags;         /// PTC_PART_* flags
    uint64_t start_lba;     /// Start LBA
    uint64_t size_lba;      /// Size in LBAs
    uint8_t  type[16];      /// MBR type byte in byte 0, or GPT type GUID
    uint8_t  guid[16];      /// GPT unique GUID (zero for MBR)
    uint32_t mbr_type;      /// MBR type byte, duplicated for convenience
    uint32_t gpt_attrs;     /// GPT attributes
    char     name[64];      /// Partition name, UTF-8, NUL padded
} ptc_part_entry;

/// A byte range that must be written back verbatim (one table sector group)
typedef struct {
    uint64_t offset;        /// Byte offset on the disk
    uint32_t length;        /// Length in bytes
    unsigned char *data;    /// @length bytes (owned)
} ptc_raw_record;

/// An in-memory partition table
typedef struct {
    uint32_t type;          /// PTC_TABLE_*
    uint32_t sector_size;   /// Sector size used for LBA arithmetic
    uint64_t disk_size;     /// Source disk size, in bytes
    uint32_t n_records;
    ptc_raw_record *records;
    uint32_t n_parts;
    ptc_part_entry *parts;
} ptc_disk_table;

/// A region of the disk: a partition or a gap between them
typedef struct {
    uint64_t offset;        /// Byte offset on the disk
    uint64_t length;        /// Length in bytes
    int      is_part;       /// 1 for a partition, 0 for a gap
    uint32_t part_index;    /// Partition index for partitions, 0 otherwise
} ptc_region;

extern void ptc_table_init(ptc_disk_table *t);
extern void ptc_table_free(ptc_disk_table *t);
extern const char *ptc_table_type_str(uint32_t type);
extern const char *ptc_table_part_flag_str(unsigned int flags);

/**
 * ptc_table_probe:
 * @fd: source device/file.
 * @disk_size: size of the source, in bytes.
 * @sector_size: logical sector size.
 * @t: (out): parsed table.
 * @opt: options (for logging).
 *
 * Reads the partition table of the source.  Returns 0 on success (including
 * "no table", which yields @t->type == PTC_TABLE_NONE), -1 on a hard error.
 */
extern int ptc_table_probe(int *fd, uint64_t disk_size, uint32_t sector_size,
                           ptc_disk_table *t, cmd_opt *opt);

extern unsigned long long ptc_table_blob_size(const ptc_disk_table *t);
extern void ptc_table_write_blob(int *ret, const ptc_disk_table *t, cmd_opt *opt);
extern int  ptc_table_read_blob(int *ret, ptc_disk_table *t, uint32_t table_size,
                                cmd_opt *opt);

/// Write the raw records back verbatim (same-size target).
extern int ptc_table_write_raw(const ptc_disk_table *t, int *dfw, cmd_opt *opt);

/**
 * ptc_table_grow:
 * @t: parsed/loaded table, modified in place.
 * @target_size: size of the target disk, in bytes.
 * @opt: options (for logging).
 *
 * Extends the last partition entry so that it fills a larger target:
 * - GPT: the partition with the highest start LBA is resized to the last usable
 *   LBA of @target_size (the table must then be regenerated, see
 *   ptc_table_regenerate());
 * - MBR: the last primary or extended entry is resized to the end of the disk
 *   in the raw sector itself (ptc_table_write_raw() then writes it back).
 * The filesystem inside the partition is never resized.
 *
 * Returns: 1 when the table was changed, 0 when it already fitted, -1 when the
 * table cannot be grown (not a MBR/GPT table, or impossible geometry).
 */
extern int ptc_table_grow(ptc_disk_table *t, uint64_t target_size, cmd_opt *opt);

/// Regenerate the table from the manifest through libfdisk.
extern int ptc_table_regenerate(const ptc_disk_table *t, const char *path,
                                uint64_t target_size, cmd_opt *opt);

/**
 * ptc_table_regions:
 * @t: parsed table.
 * @disk_size: disk size in bytes.
 * @gap_threshold: gaps up to this size are kept for %PTC_GAPS_METADATA.
 * @gaps_mode: %PTC_GAPS_* policy.
 * @regions: (out): malloc'd, ascending offset order.
 * @n_regions: (out): number of regions.
 * @opt: options (for logging).
 *
 * Builds the coverage model: one region per partition plus, depending on
 * @gaps_mode, one region per gap (never overlapping the raw table records).
 */
extern int ptc_table_regions(const ptc_disk_table *t, uint64_t disk_size,
                             uint64_t gap_threshold, int gaps_mode,
                             ptc_region **regions, uint32_t *n_regions,
                             cmd_opt *opt);

#endif /* PTC_DISKTABLE_H */

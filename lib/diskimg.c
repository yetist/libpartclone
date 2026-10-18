/**
 * diskimg.c - whole-disk image container (format 0003), part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Phases 1 to 4 of the format 0003 work described in IMAGE_FORMATS.md:
 *
 *   - the container reader/writer (512-byte disk header + 256-byte volume
 *     descriptors, both CRC32 protected);
 *   - the partition table blob (raw records + device-neutral manifest), read
 *     from the source through disktable.c and written back either verbatim or
 *     regenerated from the manifest with libfdisk;
 *   - per-region raw volumes: one volume per partition plus, depending on the
 *     --gaps policy, raw volumes for the gaps in between.
 *
 * Phase 3 adds filesystem volumes: when a partition's kernel partition device
 * exposes a filesystem that libblkid recognises, the matching per-filesystem
 * backend builds the super block and the used-block bitmap, and only the used
 * blocks are stored (the block data itself still comes from the disk at the
 * partition offset).  Unsupported or reserved partitions, gaps and a source
 * without partition devices (a disk image file) are stored as raw volumes, as
 * in phases 1 and 2.
 *
 * Phase 4 adds the introspection used by partclone.info, the richer progress
 * callbacks used by the GObject API (one event per volume plus cancellation)
 * and --grow, which resizes the last partition entry for a larger target.
 *
 * A disk without a (recognizable) partition table is still stored as a single
 * raw volume covering the whole device, as in phase 1.  The child image of a
 * volume is an ordinary format 0002 image, produced and consumed with the same
 * helpers as partclone.<fs>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <features.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif

#include <glib/gi18n-lib.h>

/**
 * progress.h - only for the progress bar
 */
#include "progress.h"

/**
 * Progress state is shared with engine.c (the classic clones engine): the
 * progress thread, the counters and the progress bar itself are global.  Only
 * one engine runs at a time, so reusing them is safe.
 */
extern progress_bar prog;
extern unsigned long long copied;
extern unsigned long long block_id;

#include "partclone.h"
#include "json_status.h"
#include "engine.h"
#include "diskimg.h"
#include "checksum.h"
#include "ptc-fs-backend.h"
#include "fs_common.h"

/// cmd_opt structure defined in partclone.h, instantiated in engine.c
extern cmd_opt opt;

/// fs_cmd_opt is defined in engine.c; the backend modules read it.
extern fs_cmd_opt fs_opt;

static const ptc_disk_progress *disk_cbs;
static int disk_done;
static gboolean disk_cancelled;

/// Description of the volume currently being processed, for the callbacks.
static ptc_volume_info disk_cur_vol;

/// Progress interface selected by ptc_disk_engine_run() and passed to
/// backend->read_bitmap() while building a filesystem volume.
static int disk_pui = 0;

/**
 * disk_poll_cancel - has the caller requested cancellation?
 */
static gboolean disk_poll_cancel(void)
{
    if (!disk_cancelled && disk_cbs != NULL && disk_cbs->cancelled != NULL &&
        disk_cbs->cancelled(disk_cbs->data))
        disk_cancelled = TRUE;
    return disk_cancelled;
}

/**
 * disk_started / disk_finished - fire the per-volume callbacks.
 */
static void disk_emit_volume_started(void)
{
    if (disk_cbs != NULL && disk_cbs->volume_started != NULL)
        disk_cbs->volume_started(&disk_cur_vol, disk_cbs->data);
}

static void disk_emit_volume_finished(void)
{
    if (disk_cbs != NULL && disk_cbs->volume_finished != NULL)
        disk_cbs->volume_finished(&disk_cur_vol, disk_cbs->data);
}

/**
 * disk_fill_volume_info - build the callback description of one volume.
 */
static void disk_fill_volume_info(unsigned int index, unsigned int count,
                                  const image_volume_desc_v3 *desc)
{
    memset(&disk_cur_vol, 0, sizeof(disk_cur_vol));
    disk_cur_vol.index = index;
    disk_cur_vol.count = count;
    disk_cur_vol.kind = desc->kind;
    disk_cur_vol.flags = desc->flags;
    disk_cur_vol.part_number = desc->part_number;
    disk_cur_vol.disk_offset = desc->disk_offset;
    disk_cur_vol.disk_length = desc->disk_length;
    disk_cur_vol.payload_length = desc->payload_length;
    disk_cur_vol.fs = desc->fs;
    disk_cur_vol.label = desc->label;
}

/**
 * disk_progress_thread - periodic progress updates.
 *
 * Mirrors engine.c's thread_update_pui() but also forwards the GObject API
 * progress and per-volume callbacks.
 */
static void *disk_progress_thread(void *arg)
{
    (void)arg;
    while (!disk_done) {
        int fresh = opt.fresh > 0 ? opt.fresh : 1;
        int waited = 0;

        if (!opt.quiet)
            update_pui(&prog, copied, block_id, disk_done);
        json_status_update(&prog, copied, block_id, JSON_STATE_RUNNING);
        if (disk_cbs != NULL) {
            if (disk_cbs->overall_progress != NULL)
                disk_cbs->overall_progress(copied, prog.total, disk_cbs->data);
            if (disk_cbs->volume_progress != NULL)
                disk_cbs->volume_progress(&disk_cur_vol, copied, prog.total, disk_cbs->data);
        }

        /* Sleep in short slices so that finishing a volume does not block the
         * engine for a whole --UI-fresh interval (there is one thread per
         * volume). */
        while (!disk_done && waited < fresh * 10) {
            usleep(100000);
            waited++;
        }
    }
    pthread_exit("exit");
}

/* ------------------------------------------------------------------------- */
/* Container structures                                                      */
/* ------------------------------------------------------------------------- */

static uint32_t disk_crc32(const void *buf, size_t len)
{
    uint32_t crc;

    init_crc32(&crc);
    return pc_crc32(crc, (void *)buf, (long)len);
}

void ptc_disk_init_head(image_disk_head_v3 *head,
                        const file_system_info *fs_info,
                        uint32_t volume_count,
                        uint32_t flags,
                        uint32_t table_type,
                        uint32_t table_size,
                        cmd_opt *opt)
{
    size_t vlen;

    memset(head, 0, sizeof(*head));

    memcpy(head->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);

    vlen = strlen(VERSION);
    if (vlen > sizeof(head->ptc_version))
        vlen = sizeof(head->ptc_version);
    memcpy(head->ptc_version, VERSION, vlen);

    memcpy(head->version, PTC_IMAGE_VERSION_0003, IMAGE_VERSION_SIZE);

    head->endianess = ENDIAN_MAGIC;
    head->header_size = PTC_DISK_HEADER_SIZE;
    head->image_version = PTC_DISK_IMAGE_VERSION;
    head->cpu_bits = (uint16_t)(sizeof(void *) * 8);
    head->disk_size = fs_info->device_size;
    head->sector_size = fs_info->block_size;
    head->physical_sector_size = fs_info->block_size;
    head->volume_count = volume_count;
    head->descriptor_size = PTC_VOLUME_DESC_SIZE;
    head->flags = flags;
    head->table_type = table_type;
    head->table_size = table_size;
    head->checksum_mode = (uint16_t)opt->checksum_mode;
    head->checksum_size = (uint16_t)get_checksum_size(opt->checksum_mode, opt->debug);
    head->feature_size = 0;
    head->image_size = 0;
}

void ptc_disk_init_desc(image_volume_desc_v3 *desc,
                        uint16_t kind,
                        uint16_t flags,
                        uint64_t disk_offset,
                        uint64_t disk_length,
                        uint64_t payload_length,
                        uint32_t block_size,
                        const char *fs)
{
    memset(desc, 0, sizeof(*desc));

    desc->kind = kind;
    desc->flags = flags;
    desc->part_number = 0;
    if (fs != NULL) {
        size_t l = strlen(fs);
        if (l > sizeof(desc->fs))
            l = sizeof(desc->fs);
        memcpy(desc->fs, fs, l);
    }
    desc->disk_offset = disk_offset;
    desc->disk_length = disk_length;
    desc->payload_length = payload_length;
    desc->block_size = block_size;
}

void ptc_disk_write_head(int *ret, image_disk_head_v3 *head, cmd_opt *opt)
{
    head->crc = disk_crc32(head, PTC_DISK_HEADER_SIZE - CRC32_SIZE);
    if (write_all(ret, (char *)head, PTC_DISK_HEADER_SIZE, opt) != PTC_DISK_HEADER_SIZE)
        log_mesg(0, 1, 1, opt->debug, "error writing disk header to image: %s\n", strerror(errno));
}

int ptc_disk_load_head(int *ret, image_disk_head_v3 *head, cmd_opt *opt)
{
    unsigned long long r;
    char ver[IMAGE_VERSION_SIZE + 1] = { 0 };

    r = read_all(ret, (char *)head, PTC_DISK_HEADER_SIZE, opt);
    if (r != PTC_DISK_HEADER_SIZE) {
        log_mesg(0, 0, 1, opt->debug, "unable to read disk header (got %llu bytes).\n", r);
        return -1;
    }

    if (memcmp(head->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) != 0) {
        log_mesg(0, 0, 1, opt->debug, "not a partclone image (bad magic).\n");
        return -1;
    }

    memcpy(ver, head->version, IMAGE_VERSION_SIZE);
    if (memcmp(head->version, PTC_IMAGE_VERSION_0003, IMAGE_VERSION_SIZE) != 0) {
        log_mesg(0, 0, 1, opt->debug,
                 "not a whole-disk image (version %s, expected %s).\n",
                 ver, PTC_IMAGE_VERSION_0003);
        return -1;
    }

    if (disk_crc32(head, PTC_DISK_HEADER_SIZE - CRC32_SIZE) != head->crc) {
        log_mesg(0, 0, 1, opt->debug, "disk header checksum error.\n");
        if (!opt->force)
            return -1;
    }

    if (head->header_size != PTC_DISK_HEADER_SIZE) {
        log_mesg(0, 0, 1, opt->debug, "unsupported disk header size %u.\n", head->header_size);
        return -1;
    }

    if (head->endianess != ENDIAN_MAGIC) {
        log_mesg(0, 0, 1, opt->debug, "unsupported endianness 0x%04X.\n", head->endianess);
        return -1;
    }

    return 0;
}

void ptc_disk_write_desc(int *ret, image_volume_desc_v3 *desc, cmd_opt *opt)
{
    desc->crc = disk_crc32(desc, PTC_VOLUME_DESC_SIZE - CRC32_SIZE);
    if (write_all(ret, (char *)desc, PTC_VOLUME_DESC_SIZE, opt) != PTC_VOLUME_DESC_SIZE)
        log_mesg(0, 1, 1, opt->debug, "error writing volume descriptor to image: %s\n", strerror(errno));
}

void ptc_disk_load_desc(int *ret, image_volume_desc_v3 *desc, cmd_opt *opt)
{
    unsigned long long r;

    r = read_all(ret, (char *)desc, PTC_VOLUME_DESC_SIZE, opt);
    if (r != PTC_VOLUME_DESC_SIZE)
        log_mesg(0, 1, 1, opt->debug, "unable to read volume descriptor (got %llu bytes).\n", r);

    if (disk_crc32(desc, PTC_VOLUME_DESC_SIZE - CRC32_SIZE) != desc->crc)
        log_mesg(0, 1, 1, opt->debug, "volume descriptor checksum error.\n");
}

const char *ptc_disk_vol_kind_str(uint16_t kind)
{
    switch (kind) {
    case PTC_VOL_NONE:       return "none";
    case PTC_VOL_FILESYSTEM: return "filesystem";
    case PTC_VOL_RAW_SPARSE: return "raw-sparse";
    case PTC_VOL_RAW_FULL:   return "raw-full";
    case PTC_VOL_ZERO:       return "zero";
    default:                 return "unknown";
    }
}

/* ------------------------------------------------------------------------- */
/* Image introspection (partclone.info, PtcImage)                            */
/* ------------------------------------------------------------------------- */

/**
 * disk_skip_forward - advance @dfr by @bytes without keeping the data.
 */
static int disk_skip_forward(int *dfr, uint64_t bytes, cmd_opt *opt)
{
    char buf[65536];

    while (bytes > 0) {
        size_t chunk;

        if (lseek(*dfr, (off_t)bytes, SEEK_CUR) != (off_t)-1)
            return 0;
        /* Not seekable (a pipe): read and discard. */
        chunk = bytes < sizeof(buf) ? (size_t)bytes : sizeof(buf);
        if (read_all(dfr, buf, chunk, opt) != (int)chunk)
            return -1;
        bytes -= chunk;
    }
    return 0;
}

int ptc_disk_image_load(int *dfr, const image_disk_head_v3 *pre_head,
                        ptc_disk_image *img, cmd_opt *opt)
{
    uint32_t i;

    memset(img, 0, sizeof(*img));
    ptc_table_init(&img->table);

    if (pre_head != NULL) {
        img->head = *pre_head;
    } else if (ptc_disk_load_head(dfr, &img->head, opt) != 0) {
        return -1;
    }

    if (img->head.volume_count > 65536) {
        log_mesg(0, 0, 1, opt->debug, "unreasonable volume count %u\n",
                 img->head.volume_count);
        return -1;
    }

    if (img->head.table_size > 0) {
        if (ptc_table_read_blob(dfr, &img->table, img->head.table_size, opt) != 0)
            return -1;
        img->table.disk_size = img->head.disk_size;
        if (img->head.sector_size != 0)
            img->table.sector_size = img->head.sector_size;
        img->has_table = 1;
    }

    if (img->head.volume_count > 0) {
        img->volumes = calloc(img->head.volume_count, sizeof(*img->volumes));
        if (img->volumes == NULL) {
            log_mesg(0, 0, 1, opt->debug, "%s, %i, not enough memory\n", __func__, __LINE__);
            return -1;
        }
    }

    for (i = 0; i < img->head.volume_count; i++) {
        ptc_disk_load_desc(dfr, &img->volumes[i], opt);
        img->n_volumes++;
        if (disk_skip_forward(dfr, img->volumes[i].payload_length, opt) != 0) {
            log_mesg(0, 0, 1, opt->debug, "unable to skip volume %u payload\n", i + 1);
            return -1;
        }
    }

    return 0;
}

void ptc_disk_image_free(ptc_disk_image *img)
{
    if (img == NULL)
        return;
    free(img->volumes);
    if (img->has_table)
        ptc_table_free(&img->table);
    memset(img, 0, sizeof(*img));
}

void ptc_disk_image_print(const ptc_disk_image *img, cmd_opt *opt)
{
    const image_disk_head_v3 *h = &img->head;
    char size_str[11];
    uint32_t i;

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    log_mesg(0, 0, 1, opt->debug, _("image format:    %s\n"), PTC_IMAGE_VERSION_0003);
    log_mesg(0, 0, 1, opt->debug, _("created on a:    %u bits platform\n"), h->cpu_bits);
    log_mesg(0, 0, 1, opt->debug, _("with partclone:  v%.14s\n"), h->ptc_version);

    print_readable_size_str(h->disk_size, size_str);
    log_mesg(0, 0, 1, opt->debug, _("Disk size:       %s = %llu Bytes\n"),
             size_str, h->disk_size);
    log_mesg(0, 0, 1, opt->debug, _("Sector size:     %u Byte\n"), h->sector_size);

    if (img->has_table) {
        log_mesg(0, 0, 1, opt->debug, _("Partition table: %s, %u entr(ies)\n"),
                 ptc_table_type_str(img->table.type), img->table.n_parts);
    } else {
        log_mesg(0, 0, 1, opt->debug, _("Partition table: %s\n"), _("none"));
    }

    log_mesg(0, 0, 1, opt->debug, _("Volumes:         %u\n"), img->n_volumes);
    log_mesg(0, 0, 1, opt->debug,
             "  %-3s %-11s %-5s %-12s %-12s %-12s %-10s %s\n",
             "#", "kind", "part", "offset", "length", "payload", "fs", "label");

    for (i = 0; i < img->n_volumes; i++) {
        const image_volume_desc_v3 *v = &img->volumes[i];
        log_mesg(0, 0, 1, opt->debug,
                 "  %-3u %-11s %-5u %-12llu %-12llu %-12llu %-10s %s\n",
                 i + 1, ptc_disk_vol_kind_str(v->kind), v->part_number,
                 v->disk_offset, v->disk_length, v->payload_length,
                 v->fs[0] ? v->fs : "-", v->label[0] ? v->label : "-");
    }
}

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * disk_get_sector_size - logical sector size of a block device, 512 otherwise.
 */
static unsigned int disk_get_sector_size(int *fd)
{
#ifdef BLKSSZGET
    int ssz = 0;
    if (ioctl(*fd, BLKSSZGET, &ssz) == 0 && ssz > 0)
        return (unsigned int)ssz;
#endif
    return PART_SECTOR_SIZE;
}

/**
 * disk_block_is_zero - true when every byte in the block is zero.
 */
static int disk_block_is_zero(const char *buf, unsigned int size)
{
    unsigned int i;
    for (i = 0; i < size; i++)
        if (buf[i])
            return 0;
    return 1;
}

/**
 * disk_scan_bitmap - build a zero-detecting bitmap for a raw volume.
 *
 * A block is marked present (and therefore stored) when it contains at least
 * one non-zero byte.
 */
static void disk_scan_bitmap(int *dfr, uint64_t src_base, unsigned long *bitmap,
                             const file_system_info *fs_info, cmd_opt *opt)
{
    const unsigned int block_size = fs_info->block_size;
    const unsigned int buffer_capacity =
        opt->buffer_size > block_size ? opt->buffer_size / block_size : 1;
    const unsigned long long blocks_total = fs_info->totalblock;
    char *buffer;
    unsigned long long block = 0;

    buffer = malloc((size_t)buffer_capacity * block_size);
    if (buffer == NULL)
        log_mesg(0, 1, 1, opt->debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    if (lseek(*dfr, (off_t)src_base, SEEK_SET) == (off_t)-1)
        log_mesg(0, 1, 1, opt->debug, "source seek ERROR:%s\n", strerror(errno));

    while (block < blocks_total) {
        unsigned long long n = blocks_total - block < buffer_capacity
            ? blocks_total - block : buffer_capacity;
        unsigned long long want = n * block_size;
        unsigned long long i;

        if (read_all(dfr, buffer, want, opt) != (int)want)
            log_mesg(0, 1, 1, opt->debug, "read error while scanning: %s\n", strerror(errno));

        for (i = 0; i < n; i++) {
            if (!disk_block_is_zero(buffer + i * block_size, block_size))
                pc_set_bit(block + i, bitmap, blocks_total);
        }
        block += n;
    }

    free(buffer);
}

/**
 * disk_payload_length - size of the child format 0002 image of a volume.
 */
static unsigned long long disk_payload_length(const file_system_info *fs_info,
                                              const image_options *img_opt,
                                              cmd_opt *opt)
{
    unsigned long long bitmap_on_disk = get_bitmap_size_on_disk(fs_info, img_opt, opt);
    unsigned long long payload;

    if (img_opt->bitmap_mode != BM_NONE && img_opt->image_version == 0x0002)
        bitmap_on_disk += CRC32_SIZE;

    payload = sizeof(image_desc_v2) + bitmap_on_disk;
    payload += fs_info->usedblocks * (unsigned long long)fs_info->block_size;

    if (img_opt->blocks_per_checksum > 0 && img_opt->checksum_size > 0) {
        unsigned long long cs_count =
            (fs_info->usedblocks + img_opt->blocks_per_checksum - 1) / img_opt->blocks_per_checksum;
        payload += cs_count * img_opt->checksum_size;
    }

    return payload;
}

/**
 * disk_write_child - write the format 0002 child image of a raw volume.
 *
 * This is the existing clone path (see engine.c): read the present blocks from
 * the device, interleave one checksum per strip, and write everything to the
 * image.
 */
static void disk_write_child(int *dfr, int *dfw, uint64_t src_base,
                             const file_system_info *fs_info,
                             const image_options *img_opt,
                             unsigned long *bitmap,
                             cmd_opt *opt)
{
    const unsigned long long blocks_total = fs_info->totalblock;
    const unsigned int block_size = fs_info->block_size;
    const unsigned int buffer_capacity =
        opt->buffer_size > block_size ? opt->buffer_size / block_size : 1;
    const unsigned int blocks_per_cs = img_opt->blocks_per_checksum;
    const unsigned int cs_size = img_opt->checksum_size;
    const int cs_reseed = img_opt->reseed_checksum;
    const unsigned int write_size = cnv_blocks_to_bytes(0, buffer_capacity, block_size, img_opt);
    unsigned char checksum[cs_size];
    unsigned int blocks_in_cs;
    char *read_buffer;
    char *write_buffer;

    read_buffer = malloc((size_t)buffer_capacity * block_size);
    write_buffer = malloc((size_t)write_size + cs_size);
    if (read_buffer == NULL || write_buffer == NULL)
        log_mesg(0, 1, 1, opt->debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    write_image_desc(dfw, *fs_info, *img_opt, opt);
    write_image_bitmap(dfw, *fs_info, *img_opt, bitmap, opt);

    if (lseek(*dfr, (off_t)src_base, SEEK_SET) == (off_t)-1)
        log_mesg(0, 1, 1, opt->debug, "source seek ERROR:%s\n", strerror(errno));

    blocks_in_cs = 0;
    init_checksum(img_opt->checksum_mode, checksum, opt->debug);

    block_id = 0;
    for (;;) {
        unsigned long long i, blocks_skip, blocks_read, read_size;
        unsigned int cs_added = 0, write_offset = 0;
        off_t offset;
        int r_size, w_size;

        if (disk_poll_cancel())
            break;

        for (blocks_skip = 0;
             block_id + blocks_skip < blocks_total &&
             !pc_test_bit(block_id + blocks_skip, bitmap, blocks_total);
             blocks_skip++)
            ;
        if (block_id + blocks_skip == blocks_total)
            break;
        if (blocks_skip)
            block_id += blocks_skip;

        for (blocks_read = 0;
             block_id + blocks_read < blocks_total && blocks_read < buffer_capacity &&
             pc_test_bit(block_id + blocks_read, bitmap, blocks_total);
             ++blocks_read)
            ;
        if (!blocks_read)
            break;

        offset = (off_t)(block_id * block_size);
        if (lseek(*dfr, (off_t)src_base + offset, SEEK_SET) == (off_t)-1)
            log_mesg(0, 1, 1, opt->debug, "source seek ERROR:%s\n", strerror(errno));

        /* The last block may be only partially present on the source device;
         * clamp the read at the device boundary and zero-fill the rest. */
        read_size = blocks_read * block_size;
        if (fs_info->device_size > (unsigned long long)offset &&
            fs_info->device_size < (block_id + blocks_read) * block_size) {
            read_size = fs_info->device_size - offset;
            memset(read_buffer + read_size, 0, (blocks_read * block_size) - read_size);
        }

        r_size = read_all(dfr, read_buffer, read_size, opt);
        if (r_size != (int)read_size)
            log_mesg(0, 1, 1, opt->debug, "read error: %s\n", strerror(errno));

        for (i = 0; i < blocks_read; ++i) {
            memcpy(write_buffer + write_offset, read_buffer + i * block_size, block_size);
            write_offset += block_size;

            update_checksum(checksum, read_buffer + i * block_size, block_size);

            if (blocks_per_cs > 0 && ++blocks_in_cs == blocks_per_cs) {
                finalize_checksum(checksum);
                memcpy(write_buffer + write_offset, checksum, cs_size);
                ++cs_added;
                write_offset += cs_size;
                blocks_in_cs = 0;
                if (cs_reseed)
                    init_checksum(img_opt->checksum_mode, checksum, opt->debug);
            }
        }

        w_size = write_all(dfw, write_buffer, write_offset, opt);
        if (w_size != (int)write_offset)
            log_mesg(0, 1, 1, opt->debug, "image write ERROR:%s\n", strerror(errno));

        copied += blocks_read;
        block_id += blocks_read;

        if (r_size + (int)(cs_added * cs_size) != w_size)
            log_mesg(0, 1, 1, opt->debug, "read(%i) and write(%i) different\n", r_size, w_size);
    }

    if (blocks_in_cs > 0) {
        int w_size;
        finalize_checksum(checksum);
        w_size = write_all(dfw, (char *)checksum, cs_size, opt);
        if (w_size != (int)cs_size)
            log_mesg(0, 1, 1, opt->debug, "image write ERROR:%s\n", strerror(errno));
    }

    free(write_buffer);
    free(read_buffer);
}

/**
 * disk_restore_child - read a format 0002 child image and write its blocks to
 * the target at @disk_offset (or just verify it when @chkimg is set).
 */
static void disk_restore_child(int *dfr, int *dfw,
                               const image_volume_desc_v3 *desc,
                               int chkimg, int target_stdout, int pui)
{
    image_head_v2 img_head;
    file_system_info fs_info;
    image_options img_opt;
    unsigned long *bitmap = NULL;
    const unsigned int block_size_hint = desc->block_size ? desc->block_size : PART_SECTOR_SIZE;
    unsigned long long blocks_total, blocks_used, blocks_used_fix, test_block;
    unsigned int block_size, buffer_capacity, blocks_per_cs, cs_size, buffer_size;
    int cs_reseed, ignore_crc;
    unsigned char *checksum = NULL;
    char *read_buffer = NULL, *write_buffer = NULL, *empty_buffer = NULL, *hole_buffer = NULL;
    unsigned int blocks_in_cs;
    pthread_t prog_thread;
    void *p_result;
    int flag, start;
    unsigned long long stop;

    (void)pui;

    init_fs_info(&fs_info);
    init_image_options(&img_opt);

    load_image_desc(dfr, &opt, &img_head, &fs_info, &img_opt);
    opt.checksum_mode = img_opt.checksum_mode;

    check_mem_size(fs_info, img_opt, opt);

    bitmap = pc_alloc_bitmap(fs_info.totalblock);
    if (bitmap == NULL)
        log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    load_image_bitmap(dfr, opt, fs_info, img_opt, bitmap);

    if (!chkimg && !target_stdout && opt.check)
        check_size(dfw, desc->disk_offset + desc->disk_length);

    blocks_total = fs_info.totalblock;
    block_size = fs_info.block_size ? fs_info.block_size : block_size_hint;
    if (block_size == 0)
        block_size = PART_SECTOR_SIZE;
    buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1;
    blocks_per_cs = img_opt.blocks_per_checksum;
    cs_size = img_opt.checksum_size;
    cs_reseed = img_opt.reseed_checksum;
    ignore_crc = opt.ignore_crc;

    checksum = malloc(cs_size ? cs_size : 1);
    if (checksum == NULL)
        log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    log_mesg(1, 0, 0, opt.debug,
             "#\nBuffer capacity = %u, Blocks per cs = %u\n#\n", buffer_capacity, blocks_per_cs);

    blocks_used = fs_info.usedblocks;
    blocks_used_fix = 0;
    for (test_block = 0; test_block < blocks_total; ++test_block)
        if (pc_test_bit(test_block, bitmap, blocks_total))
            blocks_used_fix++;
    if (blocks_used_fix != blocks_used) {
        blocks_used = blocks_used_fix;
        log_mesg(1, 0, 0, opt.debug, "info: fixed used blocks count\n");
    }

    buffer_size = cnv_blocks_to_bytes(0, buffer_capacity, block_size, &img_opt);
    if (img_opt.image_version != 0x0001)
        read_buffer = malloc((size_t)buffer_size + cs_size);
    else
        read_buffer = malloc((size_t)buffer_size + buffer_capacity * cs_size);

    if (posix_memalign((void **)&write_buffer, BSIZE, (size_t)buffer_capacity * block_size) != 0)
        write_buffer = NULL;

    if (read_buffer == NULL || write_buffer == NULL)
        log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    if (!chkimg) {
        empty_buffer = malloc(block_size);
        if (empty_buffer == NULL)
            log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);
        memset(empty_buffer, 0, block_size);

        /* By default disk restore writes zeros for blocks absent from the
         * child bitmap; --no-zero-skipped seeks over them instead. */
        hole_buffer = (opt.no_zero_skipped && !target_stdout) ? NULL : empty_buffer;

        if (lseek(*dfw, (off_t)desc->disk_offset, SEEK_SET) == (off_t)-1)
            log_mesg(0, 1, 1, opt.debug, "target seek ERROR:%s\n", strerror(errno));
    }

    /* progress bar for this volume */
    start = 0;
    stop = fs_info.usedblocks;
    flag = opt.no_block_detail ? NO_BLOCK_DETAIL : IO;
    progress_init(&prog, start, stop, fs_info.totalblock, flag, fs_info.block_size);
    prog.prog_second = opt.prog_second;
    prog.binary_prefix = opt.binary_prefix;
    if (opt.prog_second)
        strncpy(prog.time_unit, "sec", 4);
    copied = 0;
    block_id = 0;
    disk_done = 0;
    json_status_init(opt.json_path, &fs_info, &opt);
    if (pthread_create(&prog_thread, NULL, disk_progress_thread, NULL))
        log_mesg(0, 1, 1, opt.debug, "%s, %i, thread create error\n", __func__, __LINE__);

    log_mesg(1, 0, 0, opt.debug, "start %s data...\n", chkimg ? "check" : "restore");

    blocks_in_cs = 0;
    if (!ignore_crc)
        init_checksum(img_opt.checksum_mode, checksum, opt.debug);

    for (;;) {
        unsigned int i;
        unsigned long long blocks_written, blocks_skip;
        unsigned int read_size, blocks_read;
        unsigned int read_offset;
        int r_size, w_size;

        if (disk_poll_cancel())
            break;

        blocks_read = copied + buffer_capacity < blocks_used
            ? buffer_capacity : (unsigned int)(blocks_used - copied);
        if (!blocks_read)
            break;

        read_size = cnv_blocks_to_bytes(copied, blocks_read, block_size, &img_opt);
        if (blocks_per_cs && blocks_read < buffer_capacity &&
            (blocks_read % blocks_per_cs) && (blocks_used % blocks_per_cs)) {
            read_size += cs_size;
        }

        r_size = read_all(dfr, read_buffer, read_size, &opt);
        if (r_size != (int)read_size)
            log_mesg(0, 1, 1, opt.debug, "read ERROR:%s\n", strerror(errno));

        read_offset = 0;
        for (i = 0; i < blocks_read; ++i) {
            memcpy(write_buffer + i * block_size, read_buffer + read_offset, block_size);

            if (ignore_crc) {
                read_offset += block_size;
                if (++blocks_in_cs == blocks_per_cs) {
                    read_offset += cs_size;
                    blocks_in_cs = 0;
                }
                continue;
            }

            update_checksum(checksum, read_buffer + read_offset, block_size);

            if (++blocks_in_cs == blocks_per_cs) {
                finalize_checksum(checksum);
                if (memcmp(read_buffer + read_offset + block_size, checksum, cs_size))
                    log_mesg(0, 1, 1, opt.debug, "checksum error, block_id=%llu...\n", block_id + i);
                read_offset += cs_size;
                blocks_in_cs = 0;
                if (cs_reseed)
                    init_checksum(img_opt.checksum_mode, checksum, opt.debug);
            }

            read_offset += block_size;
        }

        if (!ignore_crc && blocks_in_cs && blocks_per_cs &&
            blocks_read < buffer_capacity && (blocks_read % blocks_per_cs)) {
            finalize_checksum(checksum);
            if (memcmp(read_buffer + read_offset, checksum, cs_size))
                log_mesg(0, 1, 1, opt.debug, "checksum error, block_id=%llu...\n", block_id + i);
        }

        blocks_written = 0;
        do {
            unsigned int blocks_write;

            for (blocks_skip = 0;
                 block_id + blocks_skip < blocks_total &&
                 !pc_test_bit(block_id + blocks_skip, bitmap, blocks_total);
                 blocks_skip++)
                ;

            if (!chkimg && blocks_skip > 0) {
                if (skip_blocks(dfw, hole_buffer, block_size, blocks_skip, &opt, &block_id) < 0)
                    log_mesg(0, 1, 1, opt.debug, "target seek ERROR:%s\n", strerror(errno));
                blocks_skip = 0;
            }
            if (blocks_skip > 0)
                block_id += blocks_skip;

            for (blocks_write = 0;
                 block_id + blocks_write < blocks_total &&
                 blocks_written + blocks_write < blocks_read &&
                 pc_test_bit(block_id + blocks_write, bitmap, blocks_total);
                 blocks_write++)
                ;

            if (!chkimg && blocks_write > 0) {
                unsigned int write_size = blocks_write * block_size;

                if (fs_info.device_size > block_id * block_size &&
                    fs_info.device_size < (block_id + blocks_write) * block_size)
                    write_size = fs_info.device_size - block_id * block_size;

                w_size = write_all(dfw, write_buffer + blocks_written * block_size,
                                   write_size, &opt);
                if (w_size != (int)write_size)
                    log_mesg(0, 1, 1, opt.debug, "write block %llu ERROR:%s\n",
                             block_id + blocks_written, strerror(errno));
            }

            blocks_written += blocks_write;
            block_id += blocks_write;
            copied += blocks_write;
        } while (blocks_written < blocks_read);
    }

    disk_done = 1;
    pthread_join(prog_thread, &p_result);
    update_pui(&prog, copied, block_id, 1);
    json_status_finish(&prog, copied, block_id);

    /* Zero-fill the remaining whole blocks of the volume (the partial tail is
     * handled by the caller, e.g. by ftruncate() on a regular-file target). */
    if (!chkimg && empty_buffer != NULL && !opt.no_zero_skipped) {
        unsigned long long cur = (unsigned long long)block_id * block_size;
        unsigned long long end = fs_info.device_size;
        if (cur < end) {
            unsigned long long gap = ((end - cur) / block_size) * block_size;
            if (gap > 0 &&
                skip_bytes(dfw, empty_buffer, block_size, gap, &opt) != (long long)gap)
                log_mesg(0, 1, 1, opt.debug, "target seek ERROR:%s\n", strerror(errno));
        }
    }

    free(read_buffer);
    free(write_buffer);
    free(checksum);
    if (empty_buffer)
        free(empty_buffer);
    free(bitmap);
}

/* ------------------------------------------------------------------------- */
/* Operations                                                                */
/* ------------------------------------------------------------------------- */

static int disk_find_part(const ptc_disk_table *tbl, uint32_t index)
{
    uint32_t i;

    for (i = 0; i < tbl->n_parts; i++)
        if (tbl->parts[i].index == index)
            return (int)i;
    return -1;
}

/**
 * disk_partition_path - kernel partition device of a source block device.
 *
 * Linux exposes every partition as its own device node whose name is the disk
 * name followed by the partition number, with a "p" separator when the disk
 * name ends in a digit (/dev/sda1, /dev/nvme0n1p1, /dev/loop0p1,
 * /dev/mapper/vg-lv1).  The candidate is only returned when it actually
 * exists, so a regular file (a disk image) simply yields %NULL and its
 * partitions are stored raw.
 *
 * Returns: (nullable) (transfer full): the partition device path, or %NULL.
 */
static char *disk_partition_path(const char *source, uint32_t part_index)
{
    struct stat st;
    char resolved[PATH_MAX];
    const char *base = source;
    char *cand;
    size_t len;
    int sep;

    if (source == NULL || part_index == 0)
        return NULL;

    /* Resolve a symlink such as /dev/disk/by-id/... to the real device. */
    if (realpath(source, resolved) != NULL)
        base = resolved;

    if (strncmp(base, "/dev/", 5) != 0)
        return NULL;

    len = strlen(base);
    if (len == 0)
        return NULL;

    sep = (base[len - 1] >= '0' && base[len - 1] <= '9');

    cand = g_strdup_printf("%s%s%u", base, sep ? "p" : "", part_index);
    if (stat(cand, &st) == 0)
        return cand;
    g_free(cand);

    /* Fall back to the other separator convention before giving up. */
    cand = g_strdup_printf("%s%s%u", base, sep ? "" : "p", part_index);
    if (stat(cand, &st) == 0)
        return cand;
    g_free(cand);

    return NULL;
}

/**
 * disk_emit_volume - write one volume descriptor and its child image.
 *
 * Shared by the raw and the filesystem paths of disk_clone_region(): the child
 * is always an ordinary format 0002 image whose blocks are read from the
 * source disk starting at region->offset.
 */
static void disk_emit_volume(int *dfr, int *dfw, const ptc_region *region,
                             const ptc_part_entry *pe,
                             const file_system_info *fs_info,
                             const image_options *img_opt,
                             unsigned long *bitmap, uint16_t kind,
                             uint16_t vol_flags, unsigned int block_size,
                             unsigned int vol_index, unsigned int vol_count,
                             int target_stdout)
{
    image_volume_desc_v3 desc;
    unsigned long long payload;
    pthread_t prog_thread;
    void *p_result;
    int flag, start;
    unsigned long long stop;

    payload = disk_payload_length(fs_info, img_opt, &opt);

    ptc_disk_init_desc(&desc, kind, vol_flags, region->offset, region->length,
                       payload, block_size, fs_info->fs);
    desc.part_number = region->is_part ? region->part_index : 0;
    if (pe != NULL) {
        memcpy(desc.part_type, pe->type, sizeof(desc.part_type));
        memcpy(desc.part_guid, pe->guid, sizeof(desc.part_guid));
        if (pe->name[0] != '\0')
            strncpy(desc.label, pe->name, sizeof(desc.label) - 1);
    }

    log_mesg(0, 0, 1, opt.debug,
             "Volume: offset %llu length %llu (%s%s), %llu blocks\n",
             region->offset, region->length, ptc_disk_vol_kind_str(kind),
             region->is_part ? ", partition" : ", gap", fs_info->totalblock);
    if (pe != NULL)
        log_mesg(0, 0, 0, opt.debug, "Partition %u: flags %s, name \"%s\"\n",
                 pe->index, ptc_table_part_flag_str(pe->flags), pe->name);

    if (!target_stdout && opt.check) {
        unsigned long long needed = PTC_VOLUME_DESC_SIZE + payload;
        check_free_space(opt.target, needed);
    }

    ptc_disk_write_desc(dfw, &desc, &opt);

    disk_fill_volume_info(vol_index, vol_count, &desc);
    disk_emit_volume_started();

    start = 0;
    stop = fs_info->usedblocks;
    flag = opt.no_block_detail ? NO_BLOCK_DETAIL : IO;
    progress_init(&prog, start, stop, fs_info->totalblock, flag, fs_info->block_size);
    prog.prog_second = opt.prog_second;
    prog.binary_prefix = opt.binary_prefix;
    if (opt.prog_second)
        strncpy(prog.time_unit, "sec", 4);
    copied = 0;
    block_id = 0;
    disk_done = 0;
    json_status_init(opt.json_path, fs_info, &opt);
    if (pthread_create(&prog_thread, NULL, disk_progress_thread, NULL))
        log_mesg(0, 1, 1, opt.debug, "%s, %i, thread create error\n", __func__, __LINE__);

    log_mesg(1, 0, 0, opt.debug, "start backup data...\n");
    disk_write_child(dfr, dfw, region->offset, fs_info, img_opt, bitmap, &opt);

    disk_done = 1;
    pthread_join(prog_thread, &p_result);
    update_pui(&prog, copied, block_id, 1);
    json_status_finish(&prog, copied, block_id);
    disk_emit_volume_finished();
}

/**
 * disk_clone_region - store one disk region as a child volume.
 *
 * A partition whose kernel partition device exposes a recognised filesystem is
 * stored as a filesystem volume (phase 3): the per-filesystem backend builds
 * the super block and the used-block bitmap, exactly like partclone.<fs>.  The
 * block data itself is still read from the source disk at the region offset.
 * Everything else - an unsupported or reserved partition, a gap, and a disk
 * without a partition table - is stored as a raw volume, as in phase 2.
 *
 * Regions come from ptc_table_regions() (partitions and gaps) or, for a disk
 * without a partition table, a single region covering the whole device.
 */
static void disk_clone_region(int *dfr, int *dfw, const ptc_disk_table *tbl,
                              const ptc_region *region, unsigned int sector_size,
                              unsigned int vol_index, unsigned int vol_count,
                              int target_stdout)
{
    file_system_info fs_info;
    image_options img_opt;
    unsigned long *bitmap;
    unsigned int block_size;
    unsigned int buffer_capacity;
    uint16_t kind = PTC_VOL_RAW_FULL, vol_flags = 0;
    const ptc_part_entry *pe = NULL;
    const ptc_fs_backend *backend = NULL;
    char *part_path = NULL;
    int sparse = 0;

    init_fs_info(&fs_info);
    init_image_options(&img_opt);

    img_opt.image_version = 0x0002;
    img_opt.bitmap_mode = BM_BIT;
    img_opt.checksum_mode = opt.checksum_mode;
    img_opt.checksum_size = get_checksum_size(opt.checksum_mode, opt.debug);
    img_opt.blocks_per_checksum = opt.blocks_per_checksum;
    img_opt.reseed_checksum = opt.reseed_checksum;

    if (region->is_part) {
        int pi = disk_find_part(tbl, region->part_index);
        if (pi >= 0)
            pe = &tbl->parts[pi];

        /* Phase 3: use the kernel partition device to build a filesystem
         * volume when one is recognised. */
        if (!opt.no_fs_volumes && pe != NULL &&
            !(pe->flags & (PTC_PART_CONTAINER | PTC_PART_RESERVED))) {
            part_path = disk_partition_path(opt.source, region->part_index);
            if (part_path != NULL)
                backend = ptc_fs_backend_detect(part_path);
        }
    }

    if (backend != NULL) {
        /* -------- filesystem volume -------- */
        log_mesg(0, 0, 1, opt.debug, "Reading filesystem of %s (%s)...\n",
                 part_path, backend->name);

        backend->read_super_blocks(part_path, &fs_info);
        if (fs_info.totalblock == 0 || fs_info.block_size == 0)
            log_mesg(0, 1, 1, opt.debug, "cannot read the super block of %s.\n", part_path);

        /* The child image covers the whole partition, not just the filesystem
         * extent (see IMAGE_FORMATS.md "Child volume image"). */
        fs_info.device_size = region->length;

        block_size = fs_info.block_size;
        buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1;
        if (img_opt.checksum_mode != CSM_NONE && img_opt.blocks_per_checksum == 0)
            img_opt.blocks_per_checksum = buffer_capacity;

        check_mem_size(fs_info, img_opt, opt);

        bitmap = pc_alloc_bitmap(fs_info.totalblock);
        if (bitmap == NULL)
            log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

        log_mesg(0, 0, 1, opt.debug, "Calculating bitmap of %s...\n", part_path);
        backend->read_bitmap(part_path, fs_info, bitmap, disk_pui);
        update_used_blocks_count(&fs_info, bitmap);

        kind = PTC_VOL_FILESYSTEM;
        if (pe->flags & PTC_PART_BOOTABLE)
            vol_flags |= PTC_VOL_BOOTABLE;
    } else {
        /* -------- raw volume (phase 1 / phase 2 behaviour) -------- */
        if (region->is_part)
            sparse = opt.sparse;
        else
            /* Decision 2: metadata and sparse gaps use a zero-detecting
             * bitmap, full gaps are stored raw. */
            sparse = opt.gaps == PTC_GAPS_METADATA || opt.gaps == PTC_GAPS_SPARSE;
        kind = sparse ? PTC_VOL_RAW_SPARSE : PTC_VOL_RAW_FULL;

        block_size = sector_size;
        strncpy(fs_info.fs, raw_MAGIC, FS_MAGIC_SIZE);
        fs_info.device_size = region->length;
        fs_info.totalblock = (region->length + block_size - 1) / block_size;
        fs_info.block_size = block_size;

        buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1;
        if (img_opt.checksum_mode != CSM_NONE && img_opt.blocks_per_checksum == 0)
            img_opt.blocks_per_checksum = buffer_capacity;

        bitmap = pc_alloc_bitmap(fs_info.totalblock);
        if (bitmap == NULL)
            log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

        if (sparse) {
            log_mesg(1, 0, 0, opt.debug, "Scanning region at %llu for zero blocks...\n",
                     region->offset);
            disk_scan_bitmap(dfr, region->offset, bitmap, &fs_info, &opt);
        } else {
            pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);
        }
        update_used_blocks_count(&fs_info, bitmap);
        check_mem_size(fs_info, img_opt, opt);

        if (sparse)
            vol_flags |= PTC_VOL_SPARSE;
        if (pe != NULL && (pe->flags & PTC_PART_BOOTABLE))
            vol_flags |= PTC_VOL_BOOTABLE;
    }

    disk_emit_volume(dfr, dfw, region, pe, &fs_info, &img_opt, bitmap, kind,
                     vol_flags, block_size, vol_index, vol_count, target_stdout);

    free(bitmap);
    if (part_path != NULL)
        free(part_path);
}

static int disk_clone(int *dfr, int *dfw, int target_stdout)
{
    ptc_disk_table tbl;
    ptc_region *regions = NULL;
    uint32_t n_regions = 0, i;
    file_system_info fs_info;
    image_disk_head_v3 head;
    unsigned long long device_size, table_size;
    unsigned int sector_size;
    uint32_t head_flags = 0;
    int gaps_included = 0;

    init_fs_info(&fs_info);

    device_size = opt.device_size ? opt.device_size : get_partition_size(dfr);
    if (device_size == 0)
        log_mesg(0, 1, 1, opt.debug,
                 "Cannot determine the size of '%s'. Use -S/--device-size.\n", opt.source);

    sector_size = disk_get_sector_size(dfr);
    if (sector_size < PART_SECTOR_SIZE)
        sector_size = PART_SECTOR_SIZE;

    if (ptc_table_probe(dfr, device_size, sector_size, &tbl, &opt) != 0)
        log_mesg(0, 1, 1, opt.debug, "unable to read the partition table of %s.\n", opt.source);

    if (tbl.type != PTC_TABLE_NONE) {
        if (ptc_table_regions(&tbl, device_size, PTC_DEFAULT_GAP_THRESHOLD,
                              opt.gaps, &regions, &n_regions, &opt) != 0)
            log_mesg(0, 1, 1, opt.debug, "unable to build the disk layout of %s.\n",
                     opt.source);
    } else {
        regions = malloc(sizeof(*regions));
        if (regions == NULL)
            log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);
        regions[0].offset = 0;
        regions[0].length = device_size;
        regions[0].is_part = 1;
        regions[0].part_index = 0;
        n_regions = 1;
    }

    table_size = (tbl.type == PTC_TABLE_NONE) ? 0 : ptc_table_blob_size(&tbl);

    for (i = 0; i < n_regions; i++)
        if (!regions[i].is_part)
            gaps_included = 1;

    if (opt.sparse)
        head_flags |= PTC_DISK_SPARSE_RAW;
    if (table_size > 0)
        head_flags |= PTC_DISK_TABLE_PRESENT;
    if (gaps_included)
        head_flags |= PTC_DISK_GAPS_INCLUDED;

    strncpy(fs_info.fs, raw_MAGIC, FS_MAGIC_SIZE);
    fs_info.device_size = device_size;
    fs_info.totalblock = (device_size + sector_size - 1) / sector_size;
    fs_info.block_size = sector_size;

    ptc_disk_init_head(&head, &fs_info, n_regions, head_flags, tbl.type,
                       (uint32_t)table_size, &opt);

    log_mesg(0, 0, 1, opt.debug,
             "Disk image: %s, size %llu bytes, sector %u, table %s, %u volume(s)\n",
             opt.target, device_size, sector_size, ptc_table_type_str(tbl.type), n_regions);

    ptc_disk_write_head(dfw, &head, &opt);
    if (table_size > 0)
        ptc_table_write_blob(dfw, &tbl, &opt);

    for (i = 0; i < n_regions; i++) {
        if (disk_poll_cancel())
            break;
        disk_clone_region(dfr, dfw, &tbl, &regions[i], sector_size,
                          i + 1, n_regions, target_stdout);
    }

    free(regions);
    ptc_table_free(&tbl);
    return disk_cancelled ? -2 : 0;
}

/**
 * disk_fs_grow_command - recommended command to grow a filesystem.
 *
 * After a size-changing restore, --grow only extends the partition entry (see
 * decision 4 in IMAGE_FORMATS.md); growing the filesystem itself is left to its
 * own tool, which this returns for the known filesystems.
 *
 * Returns: (nullable): the command name, or %NULL when there is no known one.
 */
static const char *disk_fs_grow_command(const char *fs)
{
    if (fs == NULL || fs[0] == '\0')
        return NULL;
    if (strcmp(fs, extfs_MAGIC) == 0 || strcmp(fs, ext2_MAGIC) == 0 ||
        strcmp(fs, ext3_MAGIC) == 0 || strcmp(fs, ext4_MAGIC) == 0)
        return "resize2fs";
    if (strcmp(fs, xfs_MAGIC) == 0)
        return "xfs_growfs";
    if (strcmp(fs, btrfs_MAGIC) == 0)
        return "btrfs filesystem resize max";
    if (strcmp(fs, f2fs_MAGIC) == 0)
        return "resize.f2fs";
    if (strcmp(fs, ntfs_MAGIC) == 0)
        return "ntfsresize";
    if (strcmp(fs, jfs_MAGIC) == 0)
        return "resize.jfs";
    return NULL;
}

static int disk_restore(int *dfr, int *dfw, int chkimg, int target_stdout)
{
    image_disk_head_v3 head;
    image_volume_desc_v3 desc;
    ptc_disk_table tbl;
    unsigned long long target_size = 0;
    unsigned long i;
    int have_table = 0;
    int grow_applied = 0;
    char grow_fs[64][FS_MAGIC_SIZE + 1];
    uint32_t grow_part[64];
    uint32_t n_grow_fs = 0;

    if (ptc_disk_load_head(dfr, &head, &opt) != 0)
        log_mesg(0, 1, 1, opt.debug, "not a valid format 0003 disk image.\n");

    log_mesg(0, 0, 1, opt.debug,
             "Disk image: %s, size %llu bytes, sector %u, %u volume(s), table %u (%u bytes)\n",
             opt.source, head.disk_size, head.sector_size, head.volume_count,
             head.table_type, head.table_size);

    if (head.table_size > 0) {
        if (ptc_table_read_blob(dfr, &tbl, head.table_size, &opt) != 0)
            log_mesg(0, 1, 1, opt.debug, "unable to read the partition table blob.\n");
        tbl.disk_size = head.disk_size;
        if (head.sector_size != 0)
            tbl.sector_size = head.sector_size;
        have_table = 1;
    } else {
        ptc_table_init(&tbl);
    }

    if (!chkimg && !target_stdout && opt.check)
        check_size(dfw, head.disk_size);

    if (have_table && !chkimg && !target_stdout) {
        struct stat st;
        int want_regen = 0;

        target_size = get_partition_size(dfw);

        /* A regular-file target is grown to the source size; a block device
         * keeps its own size, which is what triggers regeneration below. */
        if (fstat(*dfw, &st) == 0 && S_ISREG(st.st_mode) &&
            (unsigned long long)st.st_size < head.disk_size) {
            if (ftruncate(*dfw, (off_t)head.disk_size) == 0)
                target_size = head.disk_size;
        }

        /* --grow: extend the last partition entry to fill a larger target
         * (partition entry only, never the filesystem; see decision 4). */
        if (opt.grow && target_size > head.disk_size) {
            if (ptc_table_grow(&tbl, target_size, &opt) >= 0)
                grow_applied = 1;
            else
                log_mesg(1, 0, 0, opt.debug,
                         "warning: cannot grow the partition table of this image\n");
        }

        if (opt.ptable == PTC_PT_REGENERATE)
            want_regen = 1;
        else if (opt.ptable == PTC_PT_AUTO && tbl.type == PTC_TABLE_GPT &&
                 target_size != 0 && target_size != head.disk_size)
            want_regen = 1;
        if (grow_applied && tbl.type == PTC_TABLE_GPT)
            want_regen = 1;

        if (want_regen) {
            if (ptc_table_regenerate(&tbl, opt.target, target_size, &opt) != 0) {
                if (!opt.force)
                    log_mesg(0, 1, 1, opt.debug,
                             "cannot regenerate the partition table; "
                             "use --partition-table=raw or -F\n");
                log_mesg(0, 0, 1, opt.debug, "writing the partition table verbatim\n");
                ptc_table_write_raw(&tbl, dfw, &opt);
            }
        } else {
            if (tbl.type == PTC_TABLE_GPT && target_size != 0 &&
                target_size != head.disk_size)
                log_mesg(1, 0, 0, opt.debug,
                         "warning: writing a GPT verbatim to a differently-sized "
                         "target; use --partition-table=regenerate\n");
            ptc_table_write_raw(&tbl, dfw, &opt);
        }
    } else if (have_table && chkimg) {
        log_mesg(1, 0, 0, opt.debug, "partition table: %s, %u entr(ies), %llu bytes\n",
                 ptc_table_type_str(tbl.type), tbl.n_parts, ptc_table_blob_size(&tbl));
    }

    for (i = 0; i < head.volume_count; i++) {
        if (disk_poll_cancel())
            break;

        ptc_disk_load_desc(dfr, &desc, &opt);

        log_mesg(0, 0, 0, opt.debug,
                 "Volume %lu/%u: kind=%s part=%u offset=%llu length=%llu payload=%llu\n",
                 i + 1, head.volume_count, ptc_disk_vol_kind_str(desc.kind),
                 desc.part_number, desc.disk_offset, desc.disk_length, desc.payload_length);

        if (grow_applied && desc.kind == PTC_VOL_FILESYSTEM && n_grow_fs < 64) {
            memcpy(grow_fs[n_grow_fs], desc.fs, FS_MAGIC_SIZE);
            grow_fs[n_grow_fs][FS_MAGIC_SIZE] = '\0';
            grow_part[n_grow_fs] = desc.part_number;
            n_grow_fs++;
        }

        disk_fill_volume_info(i + 1, head.volume_count, &desc);
        disk_emit_volume_started();

        switch (desc.kind) {
        case PTC_VOL_RAW_FULL:
        case PTC_VOL_RAW_SPARSE:
        case PTC_VOL_FILESYSTEM:
            disk_restore_child(dfr, dfw, &desc, chkimg, target_stdout, 0);
            break;

        case PTC_VOL_ZERO:
            /* No stored data: make sure the region is zero on the target. */
            if (!chkimg && !target_stdout &&
                lseek(*dfw, (off_t)(desc.disk_offset + desc.disk_length), SEEK_SET) == (off_t)-1)
                log_mesg(0, 1, 1, opt.debug, "target seek ERROR:%s\n", strerror(errno));
            break;

        case PTC_VOL_NONE:
            break;

        default:
            log_mesg(0, 1, 1, opt.debug, "unknown volume kind %u\n", desc.kind);
        }

        disk_emit_volume_finished();
    }

    /* --grow only extended the partition entry: tell the user how to grow the
     * filesystem itself (decision 4). */
    if (grow_applied) {
        if (n_grow_fs == 0)
            log_mesg(0, 0, 1, opt.debug,
                     "The partition table was grown; grow the filesystem yourself if needed.\n");
        for (i = 0; i < n_grow_fs; i++) {
            const char *cmd = disk_fs_grow_command(grow_fs[i]);
            if (cmd != NULL)
                log_mesg(0, 0, 1, opt.debug,
                         "Grown partition %u (%s): run '%s' on its device to grow the filesystem.\n",
                         grow_part[i], grow_fs[i], cmd);
            else
                log_mesg(0, 0, 1, opt.debug,
                         "Grown partition %u (%s): grow the filesystem yourself if needed.\n",
                         grow_part[i], grow_fs[i]);
        }
    }

    /* Ask the kernel to re-read the partition table of a block device target. */
#ifdef BLKRRPART
    if (!chkimg && !target_stdout && have_table) {
        struct stat st;
        if (fstat(*dfw, &st) == 0 && S_ISBLK(st.st_mode) &&
            ioctl(*dfw, BLKRRPART) == 0)
            log_mesg(1, 0, 0, opt.debug, "re-read the partition table of %s\n", opt.target);
    }
#endif

    /* A regular-file target must cover the whole disk even when the trailing
     * blocks are not stored (sparse volumes). */
    if (!chkimg && !target_stdout) {
        struct stat st;
        if (fstat(*dfw, &st) == 0 && S_ISREG(st.st_mode) &&
            (unsigned long long)st.st_size < head.disk_size) {
            if (ftruncate(*dfw, (off_t)head.disk_size) == -1)
                log_mesg(0, 0, 1, opt.debug, "ftruncate ERROR:%s\n", strerror(errno));
            else
                log_mesg(1, 0, 0, opt.debug, "ftruncate:%llu\n", head.disk_size);
        }
    }

    if (have_table)
        ptc_table_free(&tbl);

    return disk_cancelled ? -2 : 0;
}

/**
 * ptc_disk_engine_run - run clone/restore/check for a whole-disk image.
 *
 * Uses the global #opt (see ptc_parse_options()).  The mode is selected by
 * #opt.clone, #opt.chkimg or #opt.restore.
 */
int ptc_disk_engine_run(const ptc_disk_progress *progress)
{
    int debug = opt.debug;
    int pui, tui;
    int dfr = -1, dfw = -1;
    int target_stdout = 0;
    int ret = 0;
    int r;

    disk_cbs = progress;
    disk_cancelled = FALSE;

    if (!opt.clone && !opt.restore && !opt.chkimg)
        log_mesg(0, 1, 1, debug, "No operation selected for disk image.\n");

    open_log(opt.logfile);

    pui = opt.ncurses ? NCURSES : TEXT;
    tui = open_pui(pui, opt.fresh);
    if (opt.ncurses && !tui) {
        opt.ncurses = 0;
        pui = TEXT;
    }
    disk_pui = pui;

    /* The filesystem backend modules read fs_opt (see engine.c); fill it in
     * before any backend is invoked for a filesystem volume. */
    memset(&fs_opt, 0, sizeof(fs_cmd_opt));
    fs_opt.debug = debug;
    fs_opt.ignore_fschk = opt.ignore_fschk;

    print_partclone_info(opt);

    if (!opt.chkimg && geteuid() != 0)
        log_mesg(0, 0, 1, debug, "You are not logged as root. You may have \"access denied\" errors when working.\n");

    dfr = open_source(opt.source, &opt);
    if (dfr == -1)
        log_mesg(0, 1, 1, debug, "Error exit\n");

    if (!opt.chkimg) {
        dfw = open_target(opt.target, &opt);
        if (dfw == -1)
            log_mesg(0, 1, 1, debug, "Error exit\n");
        if (strcmp(opt.target, "-") == 0)
            target_stdout = 1;
    }

    if (opt.clone)
        r = disk_clone(&dfr, &dfw, target_stdout);
    else
        r = disk_restore(&dfr, &dfw, opt.chkimg, target_stdout);
    if (r != 0)
        ret = r;

    if (!opt.chkimg && !disk_cancelled)
        sync_data(dfw, &opt);
    if (!disk_cancelled)
        print_finish_info(opt);

    close(dfr);
    if (dfw != -1)
        close_target(dfw);
    close_pui(pui);

    if (disk_cancelled)
        log_mesg(0, 0, 1, debug, "Operation cancelled.\n");
    else if (!opt.chkimg)
        fprintf(stderr, "Cloned successfully.\n");
    else
        printf("Checked successfully.\n");

    if (opt.debug)
        close_log();

    release_checksum();

    return ret;
}

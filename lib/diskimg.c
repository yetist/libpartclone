/**
 * diskimg.c - whole-disk image container (format 0003), part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Phase 1 of the format 0003 work described in IMAGE_FORMATS.md:
 *
 *   - the container reader/writer (512-byte disk header + 256-byte volume
 *     descriptors, both CRC32 protected);
 *   - raw-only volumes: a whole disk is stored as a single raw volume covering
 *     the entire device, without a partition table yet.
 *
 * This is enough to give whole-disk raw backup/restore in the 0003 container.
 * The child image of a volume is an ordinary format 0002 image, produced and
 * consumed with the same helpers as partclone.<fs>.
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

/// cmd_opt structure defined in partclone.h, instantiated in engine.c
extern cmd_opt opt;

static ptc_progress_func disk_progress_cb;
static gpointer disk_progress_data;
static int disk_done;

/**
 * disk_progress_thread - periodic progress updates.
 *
 * Mirrors engine.c's thread_update_pui() but also supports the GObject API
 * progress callback.
 */
static void *disk_progress_thread(void *arg)
{
    (void)arg;
    while (!disk_done) {
        if (!opt.quiet)
            update_pui(&prog, copied, block_id, disk_done);
        json_status_update(&prog, copied, block_id, JSON_STATE_RUNNING);
        if (disk_progress_cb != NULL)
            disk_progress_cb(copied, prog.total, disk_progress_data);
        sleep(opt.fresh);
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
 * disk_skip_stream - discard @count bytes from a stream.
 */
static void disk_skip_stream(int *fd, unsigned long long count, cmd_opt *opt)
{
    char buf[65536];

    while (count > 0) {
        unsigned long long n = count > sizeof(buf) ? sizeof(buf) : count;
        if (read_all(fd, buf, n, opt) != (int)n)
            log_mesg(0, 1, 1, opt->debug, "unexpected end of image while skipping data.\n");
        count -= n;
    }
}

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
static void disk_scan_bitmap(int *dfr, unsigned long *bitmap,
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

    if (lseek(*dfr, 0, SEEK_SET) == (off_t)-1)
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
static void disk_write_child(int *dfr, int *dfw,
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

    if (lseek(*dfr, 0, SEEK_SET) == (off_t)-1)
        log_mesg(0, 1, 1, opt->debug, "source seek ERROR:%s\n", strerror(errno));

    blocks_in_cs = 0;
    init_checksum(img_opt->checksum_mode, checksum, opt->debug);

    block_id = 0;
    for (;;) {
        unsigned long long i, blocks_skip, blocks_read, read_size;
        unsigned int cs_added = 0, write_offset = 0;
        off_t offset;
        int r_size, w_size;

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
        if (lseek(*dfr, offset, SEEK_SET) == (off_t)-1)
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

static int disk_clone(int *dfr, int *dfw, int target_stdout)
{
    file_system_info fs_info;
    image_options img_opt;
    image_disk_head_v3 head;
    image_volume_desc_v3 desc;
    unsigned long *bitmap = NULL;
    unsigned long long device_size, totalblock, payload;
    unsigned int sector_size, block_size, buffer_capacity;
    uint16_t kind, vol_flags;
    uint32_t head_flags;
    pthread_t prog_thread;
    void *p_result;
    int flag, start;
    unsigned long long stop;

    init_fs_info(&fs_info);
    init_image_options(&img_opt);

    device_size = opt.device_size ? opt.device_size : get_partition_size(dfr);
    if (device_size == 0)
        log_mesg(0, 1, 1, opt.debug,
                 "Cannot determine the size of '%s'. Use -S/--device-size.\n", opt.source);

    sector_size = disk_get_sector_size(dfr);
    if (sector_size < PART_SECTOR_SIZE)
        sector_size = PART_SECTOR_SIZE;
    block_size = sector_size;
    totalblock = (device_size + block_size - 1) / block_size;

    strncpy(fs_info.fs, raw_MAGIC, FS_MAGIC_SIZE);
    fs_info.device_size = device_size;
    fs_info.totalblock = totalblock;
    fs_info.block_size = block_size;

    img_opt.image_version = 0x0002;
    img_opt.bitmap_mode = BM_BIT;
    img_opt.checksum_mode = opt.checksum_mode;
    img_opt.checksum_size = get_checksum_size(opt.checksum_mode, opt.debug);
    img_opt.blocks_per_checksum = opt.blocks_per_checksum;
    img_opt.reseed_checksum = opt.reseed_checksum;

    buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1;
    if (img_opt.checksum_mode != CSM_NONE && img_opt.blocks_per_checksum == 0)
        img_opt.blocks_per_checksum = buffer_capacity;

    bitmap = pc_alloc_bitmap(totalblock);
    if (bitmap == NULL)
        log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    if (opt.sparse) {
        log_mesg(0, 0, 1, opt.debug, "Scanning %s for zero blocks...\n", opt.source);
        disk_scan_bitmap(dfr, bitmap, &fs_info, &opt);
        kind = PTC_VOL_RAW_SPARSE;
    } else {
        pc_init_bitmap(bitmap, 0xFF, totalblock);
        kind = PTC_VOL_RAW_FULL;
    }
    update_used_blocks_count(&fs_info, bitmap);
    check_mem_size(fs_info, img_opt, opt);

    vol_flags = (kind == PTC_VOL_RAW_SPARSE) ? PTC_VOL_SPARSE : 0;
    head_flags = (kind == PTC_VOL_RAW_SPARSE) ? PTC_DISK_SPARSE_RAW : 0;

    payload = disk_payload_length(&fs_info, &img_opt, &opt);

    ptc_disk_init_head(&head, &fs_info, 1, head_flags, PTC_TABLE_NONE, 0, &opt);
    ptc_disk_init_desc(&desc, kind, vol_flags, 0, device_size, payload, block_size,
                       raw_MAGIC);

    log_mesg(0, 0, 1, opt.debug,
             "Disk image: %s, size %llu bytes, sector %u, volume %s, %llu blocks\n",
             opt.target, device_size, block_size, ptc_disk_vol_kind_str(kind), totalblock);

    if (!target_stdout && opt.check) {
        unsigned long long needed = PTC_DISK_HEADER_SIZE + PTC_VOLUME_DESC_SIZE + payload;
        check_free_space(opt.target, needed);
    }

    ptc_disk_write_head(dfw, &head, &opt);
    ptc_disk_write_desc(dfw, &desc, &opt);

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

    log_mesg(1, 0, 0, opt.debug, "start backup data...\n");
    disk_write_child(dfr, dfw, &fs_info, &img_opt, bitmap, &opt);

    disk_done = 1;
    pthread_join(prog_thread, &p_result);
    update_pui(&prog, copied, block_id, 1);
    json_status_finish(&prog, copied, block_id);

    free(bitmap);
    return 0;
}

static int disk_restore(int *dfr, int *dfw, int chkimg, int target_stdout)
{
    image_disk_head_v3 head;
    image_volume_desc_v3 desc;
    unsigned long i;

    if (ptc_disk_load_head(dfr, &head, &opt) != 0)
        log_mesg(0, 1, 1, opt.debug, "not a valid format 0003 disk image.\n");

    log_mesg(0, 0, 1, opt.debug,
             "Disk image: %s, size %llu bytes, sector %u, %u volume(s), table %u (%u bytes)\n",
             opt.source, head.disk_size, head.sector_size, head.volume_count,
             head.table_type, head.table_size);

    /* Phase 1 does not restore a partition table; just skip its blob. */
    if (head.table_size > 0)
        disk_skip_stream(dfr, head.table_size, &opt);

    if (!chkimg && !target_stdout && opt.check)
        check_size(dfw, head.disk_size);

    for (i = 0; i < head.volume_count; i++) {
        ptc_disk_load_desc(dfr, &desc, &opt);

        log_mesg(0, 0, 0, opt.debug,
                 "Volume %lu/%u: kind=%s offset=%llu length=%llu payload=%llu\n",
                 i + 1, head.volume_count, ptc_disk_vol_kind_str(desc.kind),
                 desc.disk_offset, desc.disk_length, desc.payload_length);

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

        default:
            log_mesg(0, 1, 1, opt.debug, "unknown volume kind %u\n", desc.kind);
        }
    }

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

    return 0;
}

/**
 * ptc_disk_engine_run - run clone/restore/check for a whole-disk image.
 *
 * Uses the global #opt (see ptc_parse_options()).  The mode is selected by
 * #opt.clone, #opt.chkimg or #opt.restore.
 */
int ptc_disk_engine_run(ptc_progress_func progress, gpointer user_data)
{
    int debug = opt.debug;
    int pui, tui;
    int dfr = -1, dfw = -1;
    int target_stdout = 0;
    int ret = 0;
    int r;

    disk_progress_cb = progress;
    disk_progress_data = user_data;

    if (!opt.clone && !opt.restore && !opt.chkimg)
        log_mesg(0, 1, 1, debug, "No operation selected for disk image.\n");

    open_log(opt.logfile);

    pui = opt.ncurses ? NCURSES : TEXT;
    tui = open_pui(pui, opt.fresh);
    if (opt.ncurses && !tui) {
        opt.ncurses = 0;
        pui = TEXT;
    }

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

    if (!opt.chkimg)
        sync_data(dfw, &opt);
    print_finish_info(opt);

    close(dfr);
    if (dfw != -1)
        close_target(dfw);
    close_pui(pui);

    if (!opt.chkimg)
        fprintf(stderr, "Cloned successfully.\n");
    else
        printf("Checked successfully.\n");

    if (opt.debug)
        close_log();

    release_checksum();

    return ret;
}

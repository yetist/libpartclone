/**
 * disktable.c - partition table handling for format 0003, part of libpartclone
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Reads MBR and GPT partition tables, builds the format 0003 partition table
 * blob (raw records + manifest), writes the table back verbatim and - when
 * built with libfdisk - regenerates it from the manifest for a target disk of
 * a different size.  See IMAGE_FORMATS.md ("Format 0003") for the field layout.
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#ifdef HAVE_FDISK
#include <libfdisk/libfdisk.h>
#endif

#include "partclone.h"
#include "disktable.h"

/* ------------------------------------------------------------------------- */
/* Little-endian byte access                                                 */
/* ------------------------------------------------------------------------- */

static void st16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void st32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void st64(unsigned char *p, uint64_t v)
{
    int i;
    for (i = 0; i < 8; i++)
        p[i] = (unsigned char)(v >> (8 * i));
}

static uint16_t ld16(const unsigned char *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t ld32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t ld64(const unsigned char *p)
{
    uint64_t v = 0;
    int i;
    for (i = 7; i >= 0; i--)
        v = (v << 8) | p[i];
    return v;
}

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

static int table_read_at(int *fd, uint64_t off, void *buf, size_t len, cmd_opt *opt)
{
    if (lseek(*fd, (off_t)off, SEEK_SET) == (off_t)-1)
        return -1;
    if (read_all(fd, (char *)buf, len, opt) != (int)len)
        return -1;
    return 0;
}

static const char *table_type_str(uint32_t type)
{
    switch (type) {
    case PTC_TABLE_NONE:   return "none";
    case PTC_TABLE_MBR:    return "MBR";
    case PTC_TABLE_GPT:    return "GPT";
    case PTC_TABLE_HYBRID: return "hybrid";
    default:               return "other";
    }
}

const char *ptc_table_type_str(uint32_t type)
{
    return table_type_str(type);
}

const char *ptc_table_part_flag_str(unsigned int flags)
{
    static char buf[64];
    buf[0] = '\0';
    if (flags & PTC_PART_BOOTABLE)  g_strlcat(buf, "bootable,", sizeof(buf));
    if (flags & PTC_PART_CONTAINER) g_strlcat(buf, "container,", sizeof(buf));
    if (flags & PTC_PART_LOGICAL)   g_strlcat(buf, "logical,", sizeof(buf));
    if (flags & PTC_PART_RESERVED)  g_strlcat(buf, "reserved,", sizeof(buf));
    {
        size_t l = strlen(buf);
        if (l > 0)
            buf[l - 1] = '\0';
    }
    return buf[0] ? buf : "-";
}

void ptc_table_init(ptc_disk_table *t)
{
    memset(t, 0, sizeof(*t));
}

void ptc_table_free(ptc_disk_table *t)
{
    uint32_t i;

    if (t == NULL)
        return;
    if (t->records != NULL) {
        for (i = 0; i < t->n_records; i++)
            free(t->records[i].data);
        free(t->records);
    }
    free(t->parts);
    ptc_table_init(t);
}

static int table_add_record(ptc_disk_table *t, uint64_t offset, uint32_t length,
                            const unsigned char *data)
{
    ptc_raw_record *r;
    uint32_t i;

    if (length == 0)
        return 0;
    /* ignore a duplicate (an EBR sector can be visited twice) */
    for (i = 0; i < t->n_records; i++)
        if (t->records[i].offset == offset && t->records[i].length == length)
            return 0;
    if (t->n_records >= PTC_TABLE_MAX_RECORDS)
        return -1;

    r = realloc(t->records, (t->n_records + 1) * sizeof(*r));
    if (r == NULL)
        return -1;
    t->records = r;
    r = &t->records[t->n_records];
    r->offset = offset;
    r->length = length;
    r->data = malloc(length);
    if (r->data == NULL)
        return -1;
    memcpy(r->data, data, length);
    t->n_records++;
    return 0;
}

static int table_add_part(ptc_disk_table *t, const ptc_part_entry *src)
{
    ptc_part_entry *p;

    if (t->n_parts >= PTC_TABLE_MAX_PARTS)
        return -1;
    p = realloc(t->parts, (t->n_parts + 1) * sizeof(*p));
    if (p == NULL)
        return -1;
    t->parts = p;
    t->parts[t->n_parts] = *src;
    t->n_parts++;
    return 0;
}

static void utf16le_to_utf8(const unsigned char *src, size_t bytes,
                            char *dst, size_t dstsize)
{
    size_t i = 0, o = 0;

    if (dstsize == 0)
        return;
    while (i + 1 < bytes && o + 4 < dstsize) {
        uint32_t cp = (uint32_t)src[i] | ((uint32_t)src[i + 1] << 8);
        i += 2;
        if (cp == 0)
            break;
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < bytes) {
            uint32_t lo = (uint32_t)src[i] | ((uint32_t)src[i + 1] << 8);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        if (cp < 0x80) {
            dst[o++] = (char)cp;
        } else if (cp < 0x800) {
            dst[o++] = (char)(0xC0 | (cp >> 6));
            dst[o++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            dst[o++] = (char)(0xE0 | (cp >> 12));
            dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[o++] = (char)(0x80 | (cp & 0x3F));
        } else {
            dst[o++] = (char)(0xF0 | (cp >> 18));
            dst[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[o++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    dst[o] = '\0';
}

#ifdef HAVE_FDISK
static int guid_is_zero(const uint8_t g[16])
{
    int i;
    for (i = 0; i < 16; i++)
        if (g[i])
            return 0;
    return 1;
}

static void guid_to_str(const uint8_t g[16], char out[40])
{
    g_snprintf(out, 40,
               "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               g[3], g[2], g[1], g[0], g[5], g[4], g[7], g[6],
               g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
}
#endif

/* Known GUIDs that hold no user data.  Stored in on-disk (mixed-endian) form. */
static const uint8_t gpt_type_bios_boot[16] =
    { 0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6f, 0x6e,
      0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 };
static const uint8_t gpt_type_msr[16] =
    { 0x16, 0xe3, 0xc9, 0xe3, 0x5c, 0x0b, 0xb8, 0x4d,
      0x81, 0x7d, 0xf9, 0x2d, 0xf0, 0x02, 0x15, 0xae };

static int gpt_type_is_reserved(const uint8_t type[16])
{
    return memcmp(type, gpt_type_bios_boot, 16) == 0 ||
           memcmp(type, gpt_type_msr, 16) == 0;
}

/* ------------------------------------------------------------------------- */
/* Partition table parsers                                                   */
/* ------------------------------------------------------------------------- */

static int mbr_is_extended(uint8_t type)
{
    return type == 0x05 || type == 0x0F || type == 0x85;
}

static int parse_ebr_chain(int *fd, uint64_t disk_size, uint32_t ss,
                           ptc_disk_table *t, uint64_t ext_base, cmd_opt *opt)
{
    uint64_t ebr = ext_base;
    uint32_t idx = 5;
    int guard = 0;

    while (guard++ < PTC_TABLE_MAX_PARTS) {
        unsigned char *sec;
        const unsigned char *e0, *e1;
        uint32_t next;

        if (ebr == 0 || (ebr + 1) * (uint64_t)ss > disk_size)
            return 0;

        sec = malloc(ss);
        if (sec == NULL)
            return -1;
        if (table_read_at(fd, ebr * ss, sec, ss, opt) != 0) {
            free(sec);
            return 0;
        }
        if (table_add_record(t, ebr * ss, ss, sec) != 0) {
            free(sec);
            return -1;
        }

        e0 = sec + 446;
        e1 = sec + 462;

        if (e0[4] != 0 && ld32(e0 + 12) != 0) {
            ptc_part_entry pe;
            memset(&pe, 0, sizeof(pe));
            pe.index = idx++;
            pe.flags = PTC_PART_LOGICAL;
            if (e0[0] == 0x80)
                pe.flags |= PTC_PART_BOOTABLE;
            pe.start_lba = ebr + ld32(e0 + 8);
            pe.size_lba = ld32(e0 + 12);
            pe.type[0] = e0[4];
            pe.mbr_type = e0[4];
            if (table_add_part(t, &pe) != 0) {
                free(sec);
                return -1;
            }
        }

        if (e1[4] != 0 && ld32(e1 + 12) != 0) {
            next = ld32(e1 + 8);
            if (next == 0) {
                free(sec);
                return 0;
            }
            ebr = ext_base + next;
        } else {
            free(sec);
            return 0;
        }
        free(sec);
    }
    return 0;
}

static int parse_mbr(int *fd, uint64_t disk_size, uint32_t ss,
                     ptc_disk_table *t, const unsigned char *sec0, cmd_opt *opt)
{
    int i;

    if (table_add_record(t, 0, ss, sec0) != 0)
        return -1;

    for (i = 0; i < 4; i++) {
        const unsigned char *e = sec0 + 446 + i * 16;
        uint8_t status = e[0], type = e[4];
        uint32_t lba = ld32(e + 8), cnt = ld32(e + 12);
        ptc_part_entry pe;

        if (type == 0 || cnt == 0)
            continue;

        memset(&pe, 0, sizeof(pe));
        pe.index = (uint32_t)i + 1;
        pe.start_lba = lba;
        pe.size_lba = cnt;
        pe.type[0] = type;
        pe.mbr_type = type;
        if (status == 0x80)
            pe.flags |= PTC_PART_BOOTABLE;

        if (mbr_is_extended(type)) {
            pe.flags |= PTC_PART_CONTAINER;
            if (table_add_part(t, &pe) != 0)
                return -1;
            if (parse_ebr_chain(fd, disk_size, ss, t, lba, opt) != 0)
                return -1;
        } else {
            if (table_add_part(t, &pe) != 0)
                return -1;
        }
    }

    t->type = PTC_TABLE_MBR;
    return 0;
}

static int parse_gpt(int *fd, uint64_t disk_size, uint32_t ss,
                     ptc_disk_table *t, const unsigned char *mbr_sector, cmd_opt *opt)
{
    unsigned char *hdr = NULL, *entries = NULL, *bentries = NULL, *bhdr = NULL;
    uint64_t entry_lba, entry_bytes, entry_sectors, backup_lba, ben_off, bhdr_off;
    uint32_t nents, esize, i;
    int rc = -1, have_backup = 0;

    if (ss < 512)
        return -1;

    hdr = malloc(ss);
    if (hdr == NULL)
        return -1;
    if (table_read_at(fd, ss, hdr, ss, opt) != 0) {
        free(hdr);
        return -1;
    }
    if (memcmp(hdr, "EFI PART", 8) != 0) {
        free(hdr);
        return -1;
    }

    entry_lba = ld64(hdr + 72);
    nents = ld32(hdr + 80);
    esize = ld32(hdr + 84);
    backup_lba = ld64(hdr + 32);

    if (nents == 0 || nents > 4096 || esize < 128 || esize > 4096)
        goto out;
    entry_bytes = (uint64_t)nents * esize;
    if (entry_bytes > (1u << 20))
        goto out;
    entry_sectors = (entry_bytes + ss - 1) / ss;
    if (entry_lba == 0 || backup_lba < entry_sectors + 2)
        goto out;

    entries = malloc(entry_sectors * ss);
    if (entries == NULL)
        goto out;
    if (table_read_at(fd, entry_lba * ss, entries, entry_sectors * ss, opt) != 0) {
        log_mesg(1, 0, 0, opt->debug, "GPT: cannot read the primary entry array\n");
        goto out;
    }

    ben_off = (backup_lba - entry_sectors) * ss;
    bhdr_off = backup_lba * ss;
    bentries = malloc(entry_sectors * ss);
    if (bentries == NULL)
        goto out;
    if (ben_off + entry_sectors * ss <= disk_size &&
        table_read_at(fd, ben_off, bentries, entry_sectors * ss, opt) == 0) {
        bhdr = malloc(ss);
        if (bhdr != NULL && bhdr_off + ss <= disk_size &&
            table_read_at(fd, bhdr_off, bhdr, ss, opt) == 0 &&
            memcmp(bhdr, "EFI PART", 8) == 0)
            have_backup = 1;
    }

    /* raw records: protective MBR, primary header, primary entries, and the
     * backup entries/header when they are intact. */
    if (table_add_record(t, 0, ss, mbr_sector) != 0)
        goto out;
    if (table_add_record(t, ss, ss, hdr) != 0)
        goto out;
    if (table_add_record(t, entry_lba * ss, (uint32_t)(entry_sectors * ss), entries) != 0)
        goto out;
    if (have_backup) {
        if (table_add_record(t, ben_off, (uint32_t)(entry_sectors * ss), bentries) != 0)
            goto out;
        if (table_add_record(t, bhdr_off, ss, bhdr) != 0)
            goto out;
    }

    for (i = 0; i < nents; i++) {
        const unsigned char *e = entries + (uint64_t)i * esize;
        uint64_t first, last;
        ptc_part_entry pe;
        int k, allzero = 1;

        for (k = 0; k < 16; k++) {
            if (e[k]) {
                allzero = 0;
                break;
            }
        }
        if (allzero)
            continue;

        first = ld64(e + 32);
        last = ld64(e + 40);
        if (last < first)
            continue;

        memset(&pe, 0, sizeof(pe));
        pe.index = i + 1;
        pe.start_lba = first;
        pe.size_lba = last - first + 1;
        memcpy(pe.type, e, 16);
        memcpy(pe.guid, e + 16, 16);
        pe.gpt_attrs = (uint32_t)ld64(e + 48);
        utf16le_to_utf8(e + 56, 72, pe.name, sizeof(pe.name));
        if (gpt_type_is_reserved(pe.type))
            pe.flags |= PTC_PART_RESERVED;

        if (table_add_part(t, &pe) != 0)
            goto out;
    }

    t->type = PTC_TABLE_GPT;
    rc = 0;
out:
    free(hdr);
    free(entries);
    free(bentries);
    free(bhdr);
    return rc;
}

int ptc_table_probe(int *fd, uint64_t disk_size, uint32_t ss,
                    ptc_disk_table *t, cmd_opt *opt)
{
    unsigned char *sec0;
    int is_gpt = 0, rc;

    ptc_table_init(t);
    t->sector_size = ss;
    t->disk_size = disk_size;
    t->type = PTC_TABLE_NONE;

    if (ss < 512 || disk_size < ss)
        return 0;

    sec0 = malloc(ss);
    if (sec0 == NULL)
        return -1;
    if (table_read_at(fd, 0, sec0, ss, opt) != 0) {
        free(sec0);
        return -1;
    }

    if (sec0[510] != 0x55 || sec0[511] != 0xAA) {
        /* no MBR signature: treat as a tableless disk */
        free(sec0);
        return 0;
    }

    if (disk_size >= 2 * (uint64_t)ss) {
        unsigned char sig[8];
        if (table_read_at(fd, ss, sig, 8, opt) == 0 && memcmp(sig, "EFI PART", 8) == 0)
            is_gpt = 1;
    }

    if (is_gpt) {
        rc = parse_gpt(fd, disk_size, ss, t, sec0, opt);
        if (rc != 0) {
            log_mesg(1, 0, 0, opt->debug,
                     "damaged GPT detected; storing the whole disk as one raw volume\n");
            ptc_table_free(t);
            t->sector_size = ss;
            t->disk_size = disk_size;
            t->type = PTC_TABLE_NONE;
        }
    } else {
        rc = parse_mbr(fd, disk_size, ss, t, sec0, opt);
        if (rc != 0) {
            free(sec0);
            return -1;
        }
    }

    free(sec0);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Blob serialization                                                        */
/* ------------------------------------------------------------------------- */

static void write_manifest_entry(unsigned char *m, const ptc_part_entry *p)
{
    memset(m, 0, PTC_MANIFEST_SIZE);
    st32(m + 0, p->index);
    st32(m + 4, p->flags);
    st64(m + 8, p->start_lba);
    st64(m + 16, p->size_lba);
    memcpy(m + 24, p->type, 16);
    memcpy(m + 40, p->guid, 16);
    st32(m + 56, p->mbr_type);
    st32(m + 60, p->gpt_attrs);
    memcpy(m + 64, p->name, 64);
}

static void read_manifest_entry(const unsigned char *m, ptc_part_entry *p)
{
    memset(p, 0, sizeof(*p));
    p->index = ld32(m + 0);
    p->flags = ld32(m + 4);
    p->start_lba = ld64(m + 8);
    p->size_lba = ld64(m + 16);
    memcpy(p->type, m + 24, 16);
    memcpy(p->guid, m + 40, 16);
    p->mbr_type = ld32(m + 56);
    p->gpt_attrs = ld32(m + 60);
    memcpy(p->name, m + 64, 64);
    p->name[63] = '\0';
}

unsigned long long ptc_table_blob_size(const ptc_disk_table *t)
{
    unsigned long long sz = PTC_BLOB_HEADER_SIZE +
        (unsigned long long)t->n_records * PTC_RAW_RECORD_SIZE +
        (unsigned long long)t->n_parts * PTC_MANIFEST_SIZE;
    uint32_t i;

    for (i = 0; i < t->n_records; i++)
        sz += t->records[i].length;
    return sz;
}

void ptc_table_write_blob(int *ret, const ptc_disk_table *t, cmd_opt *opt)
{
    unsigned char hdr[PTC_BLOB_HEADER_SIZE];
    unsigned long long raw_data_size = 0;
    uint32_t i;

    memset(hdr, 0, sizeof(hdr));
    st16(hdr + 0, (uint16_t)t->type);
    st16(hdr + 2, (uint16_t)t->sector_size);
    st32(hdr + 4, t->n_records);
    st32(hdr + 8, t->n_parts);
    st32(hdr + 12, PTC_MANIFEST_SIZE);
    for (i = 0; i < t->n_records; i++)
        raw_data_size += t->records[i].length;
    st64(hdr + 16, raw_data_size);

    if (write_all(ret, (char *)hdr, sizeof(hdr), opt) != (int)sizeof(hdr))
        log_mesg(0, 1, 1, opt->debug, "write table blob header error: %s\n", strerror(errno));

    for (i = 0; i < t->n_records; i++) {
        unsigned char rec[PTC_RAW_RECORD_SIZE];
        st64(rec + 0, t->records[i].offset);
        st32(rec + 8, t->records[i].length);
        if (write_all(ret, (char *)rec, sizeof(rec), opt) != (int)sizeof(rec))
            log_mesg(0, 1, 1, opt->debug, "write table raw record error: %s\n", strerror(errno));
    }

    for (i = 0; i < t->n_parts; i++) {
        unsigned char m[PTC_MANIFEST_SIZE];
        write_manifest_entry(m, &t->parts[i]);
        if (write_all(ret, (char *)m, sizeof(m), opt) != (int)sizeof(m))
            log_mesg(0, 1, 1, opt->debug, "write table manifest error: %s\n", strerror(errno));
    }

    for (i = 0; i < t->n_records; i++) {
        if (write_all(ret, (char *)t->records[i].data, t->records[i].length, opt)
                != (int)t->records[i].length)
            log_mesg(0, 1, 1, opt->debug, "write table raw data error: %s\n", strerror(errno));
    }
}

int ptc_table_read_blob(int *ret, ptc_disk_table *t, uint32_t table_size, cmd_opt *opt)
{
    unsigned char hdr[PTC_BLOB_HEADER_SIZE];
    unsigned long long raw_data_size, expect;
    uint32_t manifest_size, i;

    ptc_table_init(t);

    if (table_size < PTC_BLOB_HEADER_SIZE) {
        log_mesg(0, 0, 1, opt->debug, "partition table blob is too small\n");
        return -1;
    }
    if (read_all(ret, (char *)hdr, sizeof(hdr), opt) != (int)sizeof(hdr)) {
        log_mesg(0, 0, 1, opt->debug, "unable to read the partition table blob\n");
        return -1;
    }

    t->type = ld16(hdr + 0);
    t->sector_size = ld16(hdr + 2);
    t->n_records = ld32(hdr + 4);
    t->n_parts = ld32(hdr + 8);
    manifest_size = ld32(hdr + 12);
    raw_data_size = ld64(hdr + 16);

    if (manifest_size != PTC_MANIFEST_SIZE) {
        log_mesg(0, 0, 1, opt->debug, "unsupported manifest entry size %u\n", manifest_size);
        return -1;
    }
    if (t->sector_size < 512 || t->sector_size > 65536) {
        log_mesg(0, 0, 1, opt->debug, "invalid sector size %u in the table blob\n",
                 t->sector_size);
        return -1;
    }
    if (t->n_records > PTC_TABLE_MAX_RECORDS) {
        log_mesg(0, 0, 1, opt->debug, "too many raw records (%u)\n", t->n_records);
        return -1;
    }
    if (t->n_parts > PTC_TABLE_MAX_PARTS) {
        log_mesg(0, 0, 1, opt->debug, "too many manifest entries (%u)\n", t->n_parts);
        return -1;
    }

    expect = PTC_BLOB_HEADER_SIZE +
        (unsigned long long)t->n_records * PTC_RAW_RECORD_SIZE +
        (unsigned long long)t->n_parts * PTC_MANIFEST_SIZE + raw_data_size;
    if (expect != table_size) {
        log_mesg(0, 0, 1, opt->debug,
                 "partition table blob size mismatch (%llu != %u)\n", expect, table_size);
        return -1;
    }

    if (t->n_records > 0) {
        t->records = calloc(t->n_records, sizeof(*t->records));
        if (t->records == NULL) {
            log_mesg(0, 0, 1, opt->debug, "out of memory\n");
            return -1;
        }
    }
    if (t->n_parts > 0) {
        t->parts = calloc(t->n_parts, sizeof(*t->parts));
        if (t->parts == NULL) {
            log_mesg(0, 0, 1, opt->debug, "out of memory\n");
            return -1;
        }
    }

    for (i = 0; i < t->n_records; i++) {
        unsigned char rec[PTC_RAW_RECORD_SIZE];
        if (read_all(ret, (char *)rec, sizeof(rec), opt) != (int)sizeof(rec))
            log_mesg(0, 1, 1, opt->debug, "unable to read a raw record\n");
        t->records[i].offset = ld64(rec + 0);
        t->records[i].length = ld32(rec + 8);
    }

    for (i = 0; i < t->n_parts; i++) {
        unsigned char m[PTC_MANIFEST_SIZE];
        if (read_all(ret, (char *)m, sizeof(m), opt) != (int)sizeof(m))
            log_mesg(0, 1, 1, opt->debug, "unable to read a manifest entry\n");
        read_manifest_entry(m, &t->parts[i]);
    }

    for (i = 0; i < t->n_records; i++) {
        uint32_t len = t->records[i].length;
        t->records[i].data = malloc(len ? len : 1);
        if (t->records[i].data == NULL)
            log_mesg(0, 1, 1, opt->debug, "out of memory\n");
        if (len > 0 && read_all(ret, (char *)t->records[i].data, len, opt) != (int)len)
            log_mesg(0, 1, 1, opt->debug, "unable to read raw record data\n");
    }

    return 0;
}

int ptc_table_write_raw(const ptc_disk_table *t, int *dfw, cmd_opt *opt)
{
    uint32_t i;

    for (i = 0; i < t->n_records; i++) {
        if (lseek(*dfw, (off_t)t->records[i].offset, SEEK_SET) == (off_t)-1) {
            log_mesg(0, 0, 1, opt->debug, "target seek ERROR: %s\n", strerror(errno));
            return -1;
        }
        if (write_all(dfw, (char *)t->records[i].data, t->records[i].length, opt)
                != (int)t->records[i].length) {
            log_mesg(0, 0, 1, opt->debug, "write partition table ERROR: %s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Table regeneration through libfdisk                                       */
/* ------------------------------------------------------------------------- */

#ifdef HAVE_FDISK
static int find_gpt_disk_guid(const ptc_disk_table *t, uint8_t guid[16])
{
    uint32_t i;

    for (i = 0; i < t->n_records; i++) {
        if (t->records[i].length >= 92 &&
            memcmp(t->records[i].data, "EFI PART", 8) == 0) {
            memcpy(guid, t->records[i].data + 56, 16);
            return 1;
        }
    }
    return 0;
}

static void append_escaped(GString *s, const char *name)
{
    const char *p;

    for (p = name; *p; p++) {
        if (*p == '"' || *p == '\\')
            g_string_append_c(s, '\\');
        g_string_append_c(s, *p);
    }
}
#endif

int ptc_table_regenerate(const ptc_disk_table *t, const char *path,
                         uint64_t target_size, cmd_opt *opt)
{
#ifdef HAVE_FDISK
    struct fdisk_context *cxt = NULL;
    struct fdisk_script *dp = NULL;
    GString *s = NULL;
    FILE *f = NULL;
    uint8_t dg[16];
    uint32_t i;
    int rc = -1;

    (void)target_size;

    if (t->type != PTC_TABLE_GPT) {
        log_mesg(0, 0, 1, opt->debug, "regeneration is only supported for GPT\n");
        return -1;
    }
    if (path == NULL || strcmp(path, "-") == 0) {
        log_mesg(0, 0, 1, opt->debug, "cannot regenerate a table on a stream\n");
        return -1;
    }

    s = g_string_new(NULL);
    g_string_append(s, "label: gpt\nunit: sectors\n");
    if (find_gpt_disk_guid(t, dg)) {
        char id[40];
        guid_to_str(dg, id);
        g_string_append_printf(s, "label-id: %s\n", id);
    }

    for (i = 0; i < t->n_parts; i++) {
        const ptc_part_entry *p = &t->parts[i];
        char type_str[40];

        if ((p->flags & PTC_PART_CONTAINER) || p->size_lba == 0)
            continue;
        guid_to_str(p->type, type_str);
        g_string_append_printf(s, "start=%llu, size=%llu, type=%s",
                               (unsigned long long)p->start_lba,
                               (unsigned long long)p->size_lba, type_str);
        if (!guid_is_zero(p->guid)) {
            char u[40];
            guid_to_str(p->guid, u);
            g_string_append_printf(s, ", uuid=%s", u);
        }
        if (p->name[0]) {
            g_string_append(s, ", name=\"");
            append_escaped(s, p->name);
            g_string_append_c(s, '"');
        }
        g_string_append_c(s, '\n');
    }

    cxt = fdisk_new_context();
    if (cxt == NULL) {
        log_mesg(0, 0, 1, opt->debug, "fdisk_new_context() failed\n");
        goto out;
    }
    if (fdisk_assign_device(cxt, path, 0) != 0) {
        log_mesg(0, 0, 1, opt->debug, "cannot open %s for partition table restore\n", path);
        goto out;
    }
    fdisk_disable_dialogs(cxt, 1);
    fdisk_enable_wipe(cxt, 0);

    f = fmemopen(s->str, s->len, "r");
    if (f == NULL) {
        log_mesg(0, 0, 1, opt->debug, "fmemopen() failed\n");
        goto out;
    }
    dp = fdisk_new_script(cxt);
    if (dp == NULL) {
        log_mesg(0, 0, 1, opt->debug, "fdisk_new_script() failed\n");
        goto out;
    }
    if (fdisk_script_read_file(dp, f) != 0) {
        log_mesg(0, 0, 1, opt->debug, "cannot parse the generated script\n");
        goto out;
    }
    fclose(f);
    f = NULL;

    if (fdisk_create_disklabel(cxt, "gpt") != 0) {
        log_mesg(0, 0, 1, opt->debug, "fdisk_create_disklabel() failed\n");
        goto out;
    }
    if (fdisk_apply_script_headers(cxt, dp) != 0 || fdisk_apply_script(cxt, dp) != 0) {
        log_mesg(0, 0, 1, opt->debug, "cannot apply the partition table\n");
        goto out;
    }
    if (fdisk_write_disklabel(cxt) != 0) {
        log_mesg(0, 0, 1, opt->debug, "cannot write the partition table\n");
        goto out;
    }

    log_mesg(1, 0, 0, opt->debug, "regenerated GPT from the manifest (%u partitions)\n",
             t->n_parts);
    rc = 0;
out:
    if (f != NULL)
        fclose(f);
    if (dp != NULL)
        fdisk_unref_script(dp);
    if (cxt != NULL)
        fdisk_unref_context(cxt);
    if (s != NULL)
        g_string_free(s, TRUE);
    return rc;
#else
    (void)t;
    (void)path;
    (void)target_size;
    log_mesg(0, 0, 1, opt->debug,
             "this build has no libfdisk support; cannot regenerate the partition table\n");
    return -1;
#endif
}

/* ------------------------------------------------------------------------- */
/* Coverage model                                                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint64_t start, end;
    int is_part;
    uint32_t idx;
} interval;

static int cmp_interval(const void *a, const void *b)
{
    const interval *x = a, *y = b;
    if (x->start < y->start) return -1;
    if (x->start > y->start) return 1;
    return 0;
}

static int cmp_region(const void *a, const void *b)
{
    const ptc_region *x = a, *y = b;
    if (x->offset < y->offset) return -1;
    if (x->offset > y->offset) return 1;
    return 0;
}

static int add_interval(interval **arr, uint32_t *n, uint32_t *cap,
                        uint64_t start, uint64_t end, int is_part, uint32_t idx)
{
    interval *p;

    if (*n >= *cap) {
        uint32_t ncap = *cap ? *cap * 2 : 16;
        p = realloc(*arr, ncap * sizeof(**arr));
        if (p == NULL)
            return -1;
        *arr = p;
        *cap = ncap;
    }
    (*arr)[*n].start = start;
    (*arr)[*n].end = end;
    (*arr)[*n].is_part = is_part;
    (*arr)[*n].idx = idx;
    (*n)++;
    return 0;
}

static int add_region(ptc_region **arr, uint32_t *n, uint32_t *cap,
                      uint64_t offset, uint64_t length, int is_part, uint32_t idx)
{
    ptc_region *p;

    if (*n >= *cap) {
        uint32_t ncap = *cap ? *cap * 2 : 16;
        p = realloc(*arr, ncap * sizeof(**arr));
        if (p == NULL)
            return -1;
        *arr = p;
        *cap = ncap;
    }
    (*arr)[*n].offset = offset;
    (*arr)[*n].length = length;
    (*arr)[*n].is_part = is_part;
    (*arr)[*n].part_index = idx;
    (*n)++;
    return 0;
}

static int gap_wanted(int mode, uint64_t len, uint64_t threshold)
{
    switch (mode) {
    case PTC_GAPS_NONE:     return 0;
    case PTC_GAPS_METADATA: return len <= threshold;
    case PTC_GAPS_SPARSE:   return 1;
    case PTC_GAPS_FULL:     return 1;
    default:                return 0;
    }
}

static int part_span(const ptc_part_entry *p, const ptc_disk_table *t,
                     uint64_t disk_size, uint64_t *s_out, uint64_t *e_out)
{
    uint64_t ss = t->sector_size;
    uint64_t s;

    if (ss == 0 || (p->flags & PTC_PART_CONTAINER) || p->size_lba == 0)
        return -1;
    if (p->start_lba > disk_size / ss)
        return -1;
    s = p->start_lba * ss;
    if (s >= disk_size)
        return -1;
    if (p->size_lba > (disk_size - s) / ss)
        *e_out = disk_size;
    else
        *e_out = s + p->size_lba * ss;
    if (*e_out <= s)
        return -1;
    *s_out = s;
    return 0;
}

int ptc_table_regions(const ptc_disk_table *t, uint64_t disk_size,
                      uint64_t gap_threshold, int gaps_mode,
                      ptc_region **out, uint32_t *n_out, cmd_opt *opt)
{
    interval *cov = NULL;
    uint32_t ncov = 0, cov_cap = 0;
    ptc_region *reg = NULL;
    uint32_t nreg = 0, reg_cap = 0;
    uint64_t cur = 0;
    uint32_t i;
    int rc = -1;

    (void)opt;

    for (i = 0; i < t->n_parts; i++) {
        uint64_t s, e;
        if (part_span(&t->parts[i], t, disk_size, &s, &e) != 0)
            continue;
        if (add_interval(&cov, &ncov, &cov_cap, s, e, 1, t->parts[i].index) != 0)
            goto out;
    }

    for (i = 0; i < t->n_records; i++) {
        uint64_t s = t->records[i].offset;
        uint64_t e;
        if (s >= disk_size)
            continue;
        if (t->records[i].length > disk_size - s)
            e = disk_size;
        else
            e = s + t->records[i].length;
        if (e <= s)
            continue;
        if (add_interval(&cov, &ncov, &cov_cap, s, e, 0, 0) != 0)
            goto out;
    }

    if (ncov > 1)
        qsort(cov, ncov, sizeof(*cov), cmp_interval);

    for (i = 0; i < ncov; i++) {
        if (cov[i].start > cur && gap_wanted(gaps_mode, cov[i].start - cur, gap_threshold)) {
            if (add_region(&reg, &nreg, &reg_cap, cur, cov[i].start - cur, 0, 0) != 0)
                goto out;
        }
        if (cov[i].end > cur)
            cur = cov[i].end;
    }
    if (cur < disk_size && gap_wanted(gaps_mode, disk_size - cur, gap_threshold)) {
        if (add_region(&reg, &nreg, &reg_cap, cur, disk_size - cur, 0, 0) != 0)
            goto out;
    }

    for (i = 0; i < t->n_parts; i++) {
        uint64_t s, e;
        if (part_span(&t->parts[i], t, disk_size, &s, &e) != 0)
            continue;
        if (add_region(&reg, &nreg, &reg_cap, s, e - s, 1, t->parts[i].index) != 0)
            goto out;
    }

    if (nreg > 1)
        qsort(reg, nreg, sizeof(*reg), cmp_region);

    *out = reg;
    *n_out = nreg;
    reg = NULL;
    rc = 0;

out:
    free(cov);
    free(reg);
    return rc;
}

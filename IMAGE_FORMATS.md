# Image formats

This file describes Partclone's image formats. 2 versions exist: 0001 and 0002.

The generic structure of the images is like this:

|   section   |
|-------------|
| Image header|
|   Bitmap    |
|   Blocks    |

Only the blocks present in the bitmap are recorded in the blocks section.


# Format 0001

This is the original format. It is used by all partclone versions lower than 0.3.0.

## Image header

|Byte Offset | Size | Description
|------------|------|-------------
|          0 |   15 | Partclone image signature
|         15 |   15 | File system's type
|         30 |    4 | **_Image version, in text: "0001"_**
|         34 |    2 | Padding (no meaning)
|         36 |    4 | File system's block size
|         40 |    8 | File system's total size
|         48 |    8 | File system's total block count
|         56 |    8 | File system's used block count
|         64 | 4096 | Unused buffer
|       4160 |      | Total size of image header

## Bitmap

|Byte Offset| Size | Description
|-----------|------|-------------
|         0 | File system's total block count | 1 byte per block: 0 = Not present; 1 = Present; other = ??
| File system's total block count | 8 | Bitmap's signature: "BiTmAgIc"

**Note:** It seems that some bitmaps contain values other than 0 or 1. The meaning of these values are not know and were probably caused by a bug manipulating the block's presence.

## Blocks

|Byte Offset | Size | Description
|------------|------|-------------
|          N | File system's block size | 1 block of data
|          N |    4 | CRC32 for the block (see https://sourceforge.net/p/partclone/bugs/6/)


# Format 0002

This new format is used since partclone 0.3.0. Partclone only write in this format, but it can read the previous format and restore the data.

The main reason why this version has been created was to fix the issue with the checksum present after each block in 0001.

The new format also support a few new features and has some flexibility for future extensions.

## Image header

|Byte Offset | Size | Description
|------------|------|-------------
|          0 |   16 | Partclone image signature
|         16 |   14 | Partclone version used to create the image, ex: 0.3.1
|         30 |    4 | **_Image version, in text: "0002"_**
|         34 |    2 | Endianess marker: 0xC0DE = little-endian, 0xDEC0 = big-endian
|         36 |   16 | File system's type
|         52 |    8 | File system's total size
|         60 |    8 | File system's total block count
|         68 |    8 | File system's used block count based on super-block
|         76 |    8 | File system's used block count based on bitmap
|         84 |    4 | File system's block size
|         88 |    4 | Size of feature section
|         92 |    2 | Image version, in binary: 0x0002
|         94 |    2 | Number of bits for CPU data: 32, 64, other?
|         96 |    2 | Checksum mode for the block strip: 0 = None, 1 = CRC32, 2 = XXH64
|         98 |    2 | Checksum size, in bytes (4 for CRC32, 8 for XXH64)
|        100 |    4 | Blocks per checksum, default is 256.
|        104 |    1 | Reseed checksum: 1 = yes, 0 = no
|        105 |    1 | Bitmap mode, see bitmap_mode_enum
|        106 |    4 | CRC32 of the previous bytes
|        110 |      | Total size of image header

## Bitmap

|Byte Offset| Size | Description
|-----------|------|-------------
|         0 | ceil(File system's total block count / 8) | 1 **bit** per block: 0 = Not present; 1 = Present
| ceil(File system's total block count / 8) | 4 | Bitmap's CRC32

**Note:** If the bitmap's CRC does not match, the image data cannot be restored.

## Blocks

|Byte Offset | Size | Description
|------------|------|-------------
|          N | File system's block size * Blocks per checksum | 1 strip of data
|          N | Checksum size | Checksum for the strip, if checksum mode

**Note:** If "Checksum mode" is None, "Blocks per checksum" is 0. In this case, the blocks are simply
written one after the other.

## Special notes

  - Image 0002 stores a few details about the version used to create the image and the platform used to
    help to handle issues with the image if some bugs are found later. By example, earlier versions of
    partclone did not write the right data size when it was run on a 64 bits platform. Since image 0001
    does not has any detail about the tool used, the code has to detect the presence of that issue.
    Another issue was related to the endianess of the plateform.
  - Image 0002 allows to disable the checksum because the data is usually piped in an archiver.
    If the archive gets corrupted, the archiver will abort before sending the data base into partclone.
    Disabling the checksum allows partclone to run a little bit faster.
  - By default, Image 0002 reset the checksum after writting it. This allows to verify a "checksum strip"
    without reading all the blocks since the beginning of the image.


# Format 0003 — whole-disk images

> **Status: phases 1 to 4 implemented.** This section describes an image
> format and the backup/restore strategy for backing up an *entire disk*
> (partition table plus every partition) with `partclone` / `libpartclone`.
> Phase 1 (the container reader/writer and raw-only volumes), phase 2
> (MBR/GPT parsing, the partition table blob and manifest, per-partition raw
> volumes, gaps, and table restore - verbatim or regenerated through libfdisk),
> phase 3 (filesystem volumes built through the kernel partition devices,
> reusing the per-filesystem backends) and phase 4 (`--grow`, the
> `partclone.info` view of the volumes and the GObject API - `PtcImage` plus the
> whole-disk `PtcClient` modes, with `::volume-*` signals and cancellation) are
> implemented and exposed through the new `partclone.disk` tool.  Device windows
> - reading a partition of a disk image file without a kernel partition device -
> are still design only.
> Formats 0001 and 0002 are unchanged and remain the formats produced by the
> per-filesystem tools.

## Motivation

Formats 0001 and 0002 each describe **exactly one filesystem** (or one raw
device) and carry no information about where that filesystem sits on a disk.
Backing up a whole disk therefore currently requires an external orchestrator
(for example Clonezilla) that saves the MBR/GPT separately and runs partclone
once per partition, then reassembles everything on restore.

Format 0003 makes the whole disk a first-class object:

  - one self-describing image for one disk;
  - the partition table, the gaps between partitions (boot loaders, GPT
    backup header, alignment padding) and every partition are preserved;
  - space is still saved per partition through the existing per-filesystem
    bitmaps and checksums;
  - both directions stay sequential and pipe friendly (`... | partclone.disk`,
    `partclone.disk | ...`).

### Goals

  - Back up a whole block device to a single image.
  - Restore that image to a disk of the same size or larger.
  - Reuse the existing clone/restore engine and per-filesystem backends.
  - Remain streamable (no seeking required) in both directions.
  - Stay additive: 0001/0002 images keep working exactly as before.

### Non-goals

  - Reassembling multi-device storage (LVM volume groups, md/RAID, btrfs
    multi-device, ...). Format 0003 describes **one** block device only.
  - Automatic filesystem shrinking. Growing an existing filesystem to fill a
    bigger partition is an optional, per-filesystem hook (see below).
  - Concurrent operation. The engine keeps its state in globals and already
    processes exactly one volume image at a time.

## Design overview

Format 0003 is a **container**.  It has a small, fixed-size *disk header*, a
*partition table blob* and then a sequence of *volumes*.  Each volume is a
`(volume descriptor, child image)` pair, and the **child image is a completely
ordinary format 0002 image** (header + bitmap + blocks, see above).

Because 0002 is self-delimiting once its header has been read (the bitmap size
follows from `totalblock` and `bitmap_mode`, the block section size follows
from `usedblocks` and `blocks_per_checksum`), a reader can skip or stream any
child without knowing the container length in advance.  A *volume descriptor*
only adds the missing information: **where on the disk** the child belongs and
what it is.

This keeps the amount of new code small:

  - writing a filesystem volume = what `partclone.<fs> -c` already does, plus a
    descriptor in front;
  - restoring a filesystem volume = what `partclone.<fs> -r` already does,
    started at the volume's disk offset (the engine already supports an
    arbitrary start offset through `opt.offset`);
  - raw gaps and unsupported partitions reuse the `raw` backend.

### On-disk layout

```
image offset 0
  +==================================================+  <- 512 bytes
  | Disk header (format 0003)                        |
  +==================================================+
  | Partition table blob                             |  table_size bytes
  +--------------------------------------------------+
  | Volume descriptor #1 (256 bytes)                 |
  +--------------------------------------------------+
  | Child image #1 (format 0002)                     |  payload_length bytes
  +--------------------------------------------------+
  | Volume descriptor #2 (256 bytes)                 |
  +--------------------------------------------------+
  | Child image #2 (format 0002)                     |
  +--------------------------------------------------+
  | ...                                              |
  +--------------------------------------------------+
  | Trailer (optional, seekable images only)         |
  +==================================================+  <- end of image
```

Volumes are stored in ascending disk-offset order, which is also the order in
which they are produced and consumed.  The `volume_count` in the disk header is
known before any data is read (it comes from the partition table), so the header
can be written first; each descriptor's `payload_length` is known as soon as
that volume's bitmap has been built, so descriptors can be interleaved with
payloads in a single pass.

All multi-byte integers are stored little-endian unless the `endianess` marker
says otherwise.  Every structure is byte-packed; readers must not assume
natural alignment.

## Disk header

Fixed size, **512 bytes** (a 0002 reader reads the first 110 bytes, finds
`version == "0003"` and rejects the image cleanly).

| Byte Offset | Size | Description
|-------------|------|-------------
|           0 |   16 | Image signature, `"partclone-image"` + NUL (same as 0002)
|          16 |   14 | Partclone version that created the image, e.g. `"0.3.50"`
|          30 |    4 | **_Image version, in text: `"0003"`_**
|          34 |    2 | Endianness marker: `0xC0DE` little-endian, `0xDEC0` big-endian
|          36 |    4 | Size of this header, in bytes (512)
|          40 |    2 | Image version, in binary: `0x0003`
|          42 |    2 | Number of bits of the CPU that created the image (32, 64, ...)
|          44 |    8 | Source disk size, in bytes
|          52 |    4 | Logical sector size of the source disk, in bytes (512, 4096, ...)
|          56 |    4 | Physical sector size of the source disk, in bytes (informative)
|          60 |    4 | Number of volumes (== number of volume descriptors)
|          64 |    4 | Size of one volume descriptor, in bytes (256 for 0003)
|          68 |    4 | Flags, see below
|          72 |    4 | Partition table type, see below
|          76 |    4 | Size of the partition table blob, in bytes
|          80 |    2 | Disk-level checksum mode (informative, same enum as 0002)
|          82 |    2 | Disk-level checksum size, in bytes
|          84 |    4 | Size of the feature section, for future extensions
|          88 |    8 | Total image size in bytes, or 0 when unknown (streamed output)
|          96 |  412 | Reserved, zero
|         508 |    4 | CRC32 of the previous 508 bytes

Flags:

| Bit | Name | Meaning
|-----|------|--------
|   0 | `PTC_DISK_TABLE_PRESENT` | A partition table blob follows the header
|   1 | `PTC_DISK_HAS_TRAILER`   | A trailer/index is present at the end of the image
|   2 | `PTC_DISK_GAPS_INCLUDED` | Gap volumes (boot loaders, padding) are stored
|   3 | `PTC_DISK_SPARSE_RAW`    | Raw volumes use a zero-detecting bitmap

Partition table type:

| Value | Name | Meaning
|-------|------|--------
|     0 | `PTC_TABLE_NONE`    | No partition table (single whole-disk volume)
|     1 | `PTC_TABLE_MBR`     | DOS/MBR partition table
|     2 | `PTC_TABLE_GPT`     | GUID Partition Table
|     3 | `PTC_TABLE_HYBRID`  | Protective/hybrid MBR + GPT
|     4 | `PTC_TABLE_OTHER`   | Recognised by a tool we do not regenerate; raw only

The reserved area and `feature_size` follow the same extension idea as 0002:
future revisions may append fields, and older readers must ignore unknown
trailing bytes within `feature_size`.

## Partition table blob

The blob is placed immediately after the disk header and is `table_size` bytes
long.  It has two parts: the **raw sectors** needed to rebuild the table
verbatim, and a **manifest** that describes the partitions in a device-neutral
way (used for reporting, for validation, and to regenerate the table when the
target disk is a different size).

```
+--------------------------------------------------+
| Blob header (32 bytes)                           |
+--------------------------------------------------+
| Raw record table   (raw_count   * 12 bytes)      |
+--------------------------------------------------+
| Manifest entries   (part_count  * manifest_size) |
+--------------------------------------------------+
| Raw data           (raw_data_size bytes)         |
+--------------------------------------------------+
```

Blob header:

| Byte Offset | Size | Description
|-------------|------|-------------
|           0 |    2 | Partition table type (same values as the disk header)
|           2 |    2 | Sector size used for the LBA arithmetic, in bytes
|           4 |    4 | Number of raw records
|           8 |    4 | Number of manifest entries
|          12 |    4 | Size of one manifest entry, in bytes (128 for 0003)
|          16 |    8 | Total size of the raw data area, in bytes
|          24 |    8 | Reserved, zero

A **raw record** points at a byte range that must be written back verbatim:

| Byte Offset | Size | Description
|-------------|------|-------------
|           0 |    8 | Byte offset of the range on the disk
|           8 |    4 | Length of the range, in bytes

Typical raw records for GPT are: the protective MBR (LBA 0, 512 bytes), the
primary header + entry array (LBA 1 .. 1 + N), and the backup entry array +
backup header at the end of the disk.  For MBR there is a single 512-byte
record (LBA 0), which contains both the boot code and the partition table.

A **manifest entry** describes one partition:

| Byte Offset | Size | Description
|-------------|------|-------------
|           0 |    4 | Partition index (1-based, as the OS numbers it)
|           4 |    4 | Flags, see below
|           8 |    8 | Start LBA
|          16 |    8 | Size, in LBAs
|          24 |   16 | Partition type: MBR type byte in byte 0, or a GPT type GUID
|          40 |   16 | Partition unique GUID (GPT) or all-zero (MBR)
|          56 |    4 | MBR type byte (duplicated for convenience)
|          60 |    4 | GPT attributes
|          64 |   64 | Partition name, UTF-8, NUL padded

Manifest flags:

| Bit | Name | Meaning
|-----|------|--------
|   0 | `PTC_PART_BOOTABLE` | Boot flag set
|   1 | `PTC_PART_CONTAINER`| Extended partition (container of logical ones)
|   2 | `PTC_PART_LOGICAL`  | Logical partition inside an extended partition
|   3 | `PTC_PART_RESERVED` | No user data (e.g. BIOS boot, MSR, EFI with no FS)

The manifest is *derived* information: it can always be recomputed from the raw
sectors with a partition-table library.  Storing it makes the image
introspectable without that library and lets a restore regenerate a table for a
different target geometry.

## Volume descriptor

Fixed size, **256 bytes** (`descriptor_size` in the disk header).  One
descriptor directly precedes its child image.

| Byte Offset | Size | Description
|-------------|------|-------------
|           0 |    2 | Volume kind, see below
|           2 |    2 | Flags, see below
|           4 |    4 | Partition number, or 0 for a non-partition region
|           8 |   16 | Filesystem magic of the child image (`"EXTFS"`, `"NTFS"`, `"raw"`, ...)
|          24 |    8 | Byte offset of this volume on the **disk**
|          32 |    8 | Byte length of this volume on the disk
|          40 |    8 | Length of the child image that follows, in bytes
|          48 |    4 | Block size used by the child image bitmap, in bytes
|          52 |    4 | Reserved, zero
|          56 |   16 | Partition type (MBR type byte or GPT type GUID)
|          72 |   16 | Partition unique GUID (GPT)
|          88 |   72 | Filesystem label or partition name, UTF-8, NUL padded
|         160 |   92 | Reserved, zero
|         252 |    4 | CRC32 of the previous 252 bytes

Volume kinds:

| Value | Name | Meaning
|-------|------|--------
|     0 | `PTC_VOL_NONE`       | Reserved
|     1 | `PTC_VOL_FILESYSTEM` | Child image of a filesystem partition
|     2 | `PTC_VOL_RAW_SPARSE` | Child image of a raw region, zero blocks omitted
|     3 | `PTC_VOL_RAW_FULL`   | Child image of a raw region, every block stored
|     4 | `PTC_VOL_ZERO`       | Region known to be zero; no child image

Volume flags:

| Bit | Name | Meaning
|-----|------|--------
|   0 | `PTC_VOL_SPARSE`   | The bitmap marks only blocks with non-zero content
|   1 | `PTC_VOL_BOOTABLE` | Partition carries the boot flag
|   2 | `PTC_VOL_SKIP`     | Do not restore this volume (unless explicitly asked)
|   3 | `PTC_VOL_GROWABLE` | This partition may be grown on a larger target

## Child volume image

For `PTC_VOL_FILESYSTEM`, `PTC_VOL_RAW_SPARSE` and `PTC_VOL_RAW_FULL`, the
payload is **exactly a format 0002 image** (see the "Format 0002" section):

```
+-------------------------------+  sizeof(image_desc_v2) = 110 bytes
| image_desc_v2 (magic .. crc)  |  version field is "0002"
+-------------------------------+  bitmap_size bytes
| bitmap (+ CRC32)              |
+-------------------------------+  blocks_size bytes
| used blocks, interleaved with |
| per-strip checksums           |
+-------------------------------+
```

Conventions:

  - `fs_info.device_size` is the volume's `disk_length`, so
    `totalblock * block_size` accounts for exactly that range.
  - `fs_info.fs` is the filesystem magic for filesystem volumes and `"raw"`
    for raw volumes.
  - For `PTC_VOL_RAW_FULL` the bitmap has every bit set, i.e. it behaves like a
    `partclone.imager` / `partclone.dd` image.
  - For `PTC_VOL_RAW_SPARSE` the bitmap marks each block whose content is not
    all-zero; the descriptor sets `PTC_VOL_SPARSE`.
  - `payload_length` equals `110 + bitmap_size + usedblocks * block_size +
    checksum_bytes`, where `checksum_bytes = ceil(usedblocks /
    blocks_per_checksum) * checksum_size` when checksums are enabled.  The
    writer fills it in; readers may use it to skip a volume without parsing it.

`PTC_VOL_ZERO` volumes carry no payload (`payload_length == 0`); they are used
for free space that is known to be empty so that the region is still described.

## Coverage model

The whole disk address space is divided into non-overlapping regions, emitted in
ascending offset order:

  - the **partition table sectors** (recorded in the table blob, not as a
    volume);
  - one region per **filesystem partition**;
  - one region per **non-filesystem partition** (BIOS boot, MSR, unknown),
    stored as raw;
  - the remaining **gaps** (MBR gap, alignment padding, free space, and the
    area holding the backup GPT).

Gaps are the only subtle part.  They may contain data that is not part of any
partition but that matters for booting, most notably the GRUB core image that
lives in the post-MBR gap or in a BIOS boot partition.  Because free space can
also be enormous, gap handling is a policy:

| Policy | Stored
|--------|-------
| `none`     | No gap volumes at all
| `metadata` | Only gaps smaller than a threshold (default 1 MiB), for boot loaders
| `sparse`   | Every gap, using a zero-detecting bitmap
| `full`     | Every gap, stored as raw

The adopted default is `metadata` (decision 2 below).  `sparse` is the safest
choice for full fidelity; `none` is fastest but discards boot loaders that live
outside a partition.

"Free space" that is not stored is described by `PTC_VOL_ZERO` volumes so the
region map still covers the disk, or simply omitted.

## Backup procedure

```
1.  Open the disk read-only and lock out concurrent users.
2.  Read logical and physical sector sizes (BLKSSZGET / BLKPBSZGET).
3.  Read the partition table:
      - MBR  -> build a 512-byte raw record and a manifest;
      - GPT  -> build raw records (protective MBR, primary and backup GPT)
                and a manifest;
      - none -> if the whole disk contains one filesystem, emit a single
                filesystem volume covering the disk; otherwise emit a single
                raw volume covering the disk.
4.  Enumerate partitions and classify each one:
      - probe the filesystem type (libblkid) -> filesystem volume;
      - no recognised filesystem -> raw volume (sparse).
5.  Build the gap list from the coverage model and classify gaps.
6.  Assemble the partition table blob.
7.  Write the disk header (volume_count, table_size, disk_size, ...).
8.  Write the partition table blob.
9.  For every region, in disk-offset order:
      a. write the volume descriptor;
      b. write the child format 0002 image.
10. Optionally append the trailer, then flush and close.
```

Step 9.b for a filesystem volume is the existing clone path: read the super
block, build the bitmap, write `image_desc_v2`, write the bitmap, then stream the
present blocks with per-strip checksums.  For a raw volume it is the same path
with the `raw` backend (all blocks present) or with a zero-detecting scan.

The important consequence is that **only one partition bitmap is in memory at a
time**, exactly like today, so memory use does not grow with the number of
partitions.

### Reading partitions during backup

Building the super block and bitmap of a partition requires reading that
partition's bytes.  Two strategies exist; v1 uses the first and the second is
the planned follow-up (decision 7):

  - **Kernel partition devices (v1).**  On Linux each partition is available as
    `/dev/sdaN` (or an equivalent device-mapper node), so the existing backends,
    which take a path and read from offset 0, work unchanged.  This requires the
    partitions to be exposed by the kernel and the process to be root, and it
    does not work when the source is a regular file holding a disk image.
  - **Device windows (future).**  A descriptor of `(backing fd, offset, length)`
    handed to the backends, so a partition inside an image file or inside a
    device with an unusual table can be read directly.  This needs an extension
    of the `ptc_fs_backend` interface and, for libraries that only open by path,
    either a small shim or a loop device.

v1 uses kernel partition devices (decision 7) and is implemented; device
windows are the natural follow-up.

## Restore procedure

```
1.  Read the disk header and the partition table blob.
2.  Open the target and check it is large enough for disk_size
    (allow a larger target; allow a smaller one only with --force).
3.  Check sector sizes and warn/handle a mismatch.
4.  Recreate the partition table:
      - when libfdisk is available, rebuild it from the manifest so the
        secondary GPT and all CRCs are placed correctly for the actual target
        size (required for a target of a different size);
      - otherwise write the raw records back verbatim (same-size target only).
5.  For every volume, in stream order:
      a. read the descriptor;
      b. read the child format 0002 image;
      c. seek the target to disk_offset;
      d. restore the child's present blocks (reusing the existing restore
         loop, with opt.offset set to disk_offset).
6.  If the target is larger, optionally zero the trailing area.
7.  Flush and re-read the partition table (BLKRRPART / partprobe).
```

The per-filesystem `post_clone_fixup` hook is not run for a filesystem volume
in v1: the existing hooks (HFS+) work on absolute target offsets and are not
offset aware, so they cannot address a volume that lives inside a disk.  A
disk-aware version is left to the phase that adds filesystem growth
(decision 4).

Restoring a child at an arbitrary disk offset is already expressible: the
restore loop starts by seeking `opt.offset` bytes and then writes blocks
sequentially relative to that position.  A disk restore therefore sets
`opt.offset = volume.disk_offset` (and seeks the target to 0) before restoring
each volume.

### Skipped blocks and gaps

Blocks absent from a child bitmap are not written by the existing restore loop,
i.e. whatever is already on the target is left in place.  On a freshly
partitioned disk those areas read as zero, which is the usual case.  For a
defined result on a reused disk, disk restore follows decision 3:

  - write zeros for blocks absent from the bitmap within each volume (default);
  - `--no-zero-skipped`: seek over them, like the single-partition restore.

Gap volumes are ordinary raw volumes, so their content (mostly zeros) is written
by the same code.

## Target size changes

  - **Same size:** write the table verbatim and restore every volume in place.
  - **Larger (GPT):** keep partition offsets; place the backup GPT at the end of
    the target instead of at the end of the source.
  - **Larger (MBR):** keep offsets unchanged.
  - **Smaller:** refuse unless `--force`, and then only if every partition still
    fits.

`--grow` additionally extends the last partition entry to fill a larger target
(GPT last usable LBA, or the last primary/extended MBR entry).  As stated in
decision 4 below, v1 does **not** resize the filesystem itself; it stays
consistent and smaller than its partition, and partclone prints the recommended
growth command.

## CLI surface (proposal)

A dedicated front end keeps the existing per-filesystem tools untouched:

```
partclone.disk -c -s /dev/sda -o /backup/sda.img          # back up the disk
partclone.disk -r -s /backup/sda.img -o /dev/sda          # restore the disk
partclone.disk --check-image -s /backup/sda.img           # check the disk image
partclone.info /backup/sda.img                            # list the volumes
```

Options below are the eventual surface; `--sparse`, `--no-zero-skipped`,
`--check-image`, `--gaps`, `--partition-table`, `--no-fs-volumes` and `--grow`
exist today, the rest are planned:

| Option | Meaning
|--------|--------
| `--sparse`                         | Clone partition raw volumes with a zero-detecting bitmap (`PTC_VOL_RAW_SPARSE`)
| `--check-image`                    | Check a whole-disk image (`partclone.disk` check mode)
| `--[no-]zero-skipped`              | Zero blocks that are not in the bitmap (default on for disk restore)
| `--gaps=none|metadata|sparse|full` | Gap policy (default `metadata`, see decision 2)
| `--partition-table=auto|raw|regenerate` | How to write the table on restore (default `auto`)
| `--no-fs-volumes`                  | Store every partition as a raw volume instead of a filesystem volume
| `--partitions=LIST`                | Restrict an operation to selected partitions
| `--grow`                           | Grow the last partition entry (not the filesystem) on a larger target
| `--sector-size=N`                  | Override the logical sector size
| `-S, --device-size=N`              | As today, override the recorded disk size

`partclone.imager` keeps its current meaning (raw device image) and is not
changed; whole-disk images are handled by `partclone.disk` (decision 5).

## libpartclone API

The container is a natural fit for the GObject API; this is implemented in
phase 4:

  - `PtcClient` gains `PTC_MODE_DISK_CLONE`, `PTC_MODE_DISK_RESTORE` and
    `PTC_MODE_DISK_CHKIMG`, plus the whole-disk options `sparse`,
    `no-zero-skipped`, `no-fs-volumes`, `grow`, `gaps` and `partition-table`.
  - A new introspectable `PtcImage` exposes the volume list as `PtcVolume`
    objects: index, partition number, kind, disk offset, disk length, payload
    length, filesystem, label and flags.
  - New signals report per-partition progress, `::volume-started`,
    `::volume-progress` and `::volume-finished`, in addition to the existing
    overall `::progress`.
  - `ptc_image_is_disk()` distinguishes a 0003 container from a plain 0002 image.
  - A `GCancellable` aborts the operation: between blocks for the plain
    clone/restore modes, and between volumes and block strips for the
    whole-disk ones.  The target is left incomplete and the call fails with
    %G_IO_ERROR_CANCELLED.

## Integrity, validation and security

  - The disk header and each volume descriptor carry a CRC32; a mismatch is a
    hard error, as with the 0002 header.
  - `disk_size` is validated against the sum of the regions; regions must be
    non-overlapping and must lie inside the disk.
  - `volume_count`, `descriptor_size` and `table_size` are bounded before any
    allocation, mirroring the `totalblock` / `block_size` checks in
    `load_image_desc_v2()`.
  - Each child image is validated by the existing 0002 checks (block size,
    usedblocks <= totalblock, checksum mode, overflow guards).
  - Offset arithmetic uses checked 64-bit operations; overflow is rejected.
  - The endianness marker must match; a foreign-endian image is refused, as
    today.
  - Restoring a partition table is privileged and destructive: the operation
    refuses mounted targets and requires explicit confirmation before writing a
    table.

## Compatibility and versioning

  - 0001 and 0002 are untouched; a 0003 reader must still read them.
  - A 0001/0002 reader sees `version == "0003"` in the first 110 bytes and
    reports "The image version is not supported", exactly like any unknown
    future version.
  - Single-partition use cases should keep producing 0002 images so that
    existing workflows and tools are unaffected.  0003 is used when the source
    is a whole disk.
  - `descriptor_size`, `feature_size` and the reserved areas allow the container
    to grow without another version number; only incompatible changes bump to
    `0004`.

### Optional trailer / index

For seekable images, a trailer can be appended (flag `PTC_DISK_HAS_TRAILER`):

  - a small fixed header with a magic and its own length;
  - a directory of `(image offset, image length, disk offset, disk length)` per
    volume, for random access to a single partition;
  - an optional whole-image digest for end-to-end verification.

A pipe reader simply ignores the trailer.  A seekable reader finds it by looking
at the last few hundred bytes and validating the magic.

## Alternatives considered

  - **One 0002 image per partition plus a separate partition-table file**, which
    is what Clonezilla does today.  It works, but it is not self-describing and
    needs the orchestrator to track the relationships.  It remains the correct
    behaviour for single-partition workflows; 0003 is the native alternative
    when a single file is preferable.
  - **Concatenating 0002 images with a text manifest.**  Rejected: fragile to
    parse, no integrity, no defined framing.
  - **Adding a partition map to format 0002.**  Rejected: it breaks the
    invariant that 0002 is exactly one filesystem, complicates streaming, and
    risks the existing readers.  A container is cleaner.
  - **Storing the disk as one raw image** (the existing `raw` / `imager`
    backend).  Rejected as the default because it stores free space and loses
    per-filesystem awareness, but kept as the fallback for a disk with no table
    or no recognised filesystems.

## Implementation phases

  1. **Done.** Define the container reader/writer and the raw-only volumes; this
     alone gives whole-disk raw backup/restore as 0003.  Implemented in
     `lib/diskimg.c` / `lib/diskimg.h` and exposed as `partclone.disk` with the
     `--sparse` (zero-detecting raw volume) and `--check-image` options.  A
     single raw volume covering the whole device is emitted (no partition table
     yet).
  2. **Done.** Add MBR/GPT parsing, the table blob and manifest, and table
     restore via a partition-table library.  Implemented in `lib/disktable.c` /
     `lib/disktable.h`: `partclone.disk` reads the MBR (primary, extended and
     logical partitions) or GPT (primary and backup) of the source, writes the
     blob (raw records + manifest) after the disk header, and emits one
     `PTC_VOL_RAW_FULL`/`PTC_VOL_RAW_SPARSE` volume per partition plus gap
     volumes according to `--gaps`.  Restore writes the table back verbatim, or
     - when built with the optional libfdisk dependency and the target size
     differs from the source - regenerates it from the manifest
     (`--partition-table=auto|raw|regenerate`).
  3. **Done.** Filesystem volumes through kernel partition devices, reusing the
     per-filesystem backends.  For every partition whose kernel partition
     device (`/dev/sda1`, `/dev/nvme0n1p1`, `/dev/loop0p1`, ...) exposes a
     filesystem that libblkid recognises, `partclone.disk` builds the super
     block and the used-block bitmap with the matching backend and emits a
     `PTC_VOL_FILESYSTEM` volume; the block data is still read from the disk at
     the partition offset.  Unsupported or reserved partitions, gaps and a
     source without partition devices (a disk image file) fall back to raw
     volumes, and `--no-fs-volumes` forces that fallback.  Restore reuses the
     ordinary 0002 child restore at the volume offset.
  4. **Done.** `--grow` (partition entry only), the GObject API and the
     `partclone.info` view of the volumes.  `--grow` extends the last partition
     entry of a larger target - the highest-start GPT partition, resized to the
     target's last usable LBA, or the last primary/extended MBR entry, resized
     to the end of the disk - and never the filesystem.  `partclone.info` lists
     the volumes of a 0003 image (`ptc_disk_image_load()` /
     `ptc_disk_image_print()` in `lib/diskimg.c`).  The GObject API adds
     `PtcImage`/`PtcVolume` (`lib/ptc-image.{c,h}`), the
     `PTC_MODE_DISK_CLONE|DISK_RESTORE|DISK_CHKIMG` modes, the `::volume-*`
     signals and a `GCancellable`-driven cancellation between volumes and block
     strips.  A per-filesystem `resize_fs` hook is a later phase (decision 4).

## Resolved design decisions

The following choices are adopted for format 0003 and its implementation.

  1. **Partition table library — libfdisk, optional.**  libfdisk (from the same
     util-linux source as the already used libblkid) is adopted as an optional
     build dependency.  When it is present, restore rebuilds the table from the
     manifest so the secondary GPT and all CRCs land correctly for the actual
     target size; this is required to restore onto a target of a different
     size.  When it is absent, only verbatim raw-sector write-back is possible
     and a different-size target is refused.  Rationale: correct table math
     without hand-rolling GPT placement, while keeping minimal builds possible.

  2. **Default gap policy — `metadata`.**  `--gaps` defaults to `metadata`: gaps
     up to a 1 MiB threshold are stored as sparse raw volumes, which keeps the
     common boot-loader locations (post-MBR gap, BIOS boot area) without
     scanning or storing arbitrarily large free space.  `--gaps=sparse` is the
     full-fidelity choice and `--gaps=none` the fastest one.  Rationale: the
     useful gap data is almost always small.

  3. **Absent blocks — zeroed by default for disk restore.**  A disk restore
     writes zeros for blocks absent from a child bitmap so that a reused target
     has a defined result; `--no-zero-skipped` restores the current seek
     behaviour.  Single-partition 0002 restore is unchanged and continues to
     seek unless `--zero-skipped` is given.  Rationale: a whole-disk image is
     meant to reproduce the disk, not to leave stale blocks behind.

  4. **Filesystem growth — out of scope in v1.**  `--grow` extends the *last
     partition entry* to fill a larger target but does not resize the
     filesystem, which stays consistent and simply occupies less than its
     partition.  After a size-changing restore, partclone prints the
     filesystem's recommended growth command (for example `resize2fs`,
     `xfs_growfs`, `btrfs filesystem resize`, `resize.f2fs`).  A per-filesystem
     `resize_fs` hook, symmetric with `post_clone_fixup`, is reserved for a
     later phase.  Rationale: in-library filesystem growth is filesystem
     specific and risky, while partition growth is safe.

  5. **A new `partclone.disk` program; `partclone.imager` is unchanged.**
     Whole-disk images are produced and consumed by a dedicated front end and a
     new library mode.  `partclone.imager` keeps its raw-device meaning and
     does not auto-detect whole disks, so existing behaviour and images are
     preserved.  `partclone.info` learns to list the volumes of a 0003 image.
     Rationale: no silent change of meaning for an existing tool.

  6. **Compression and encryption stay external.**  They remain outside the
     format and are applied through pipes around partclone, exactly as today
     (`--compresscmd`, or gzip/zstd/xz on the command line).  The container
     stays strictly sequential so this keeps working.  Rationale: reuse a solved
     problem and keep the format minimal.

  7. **Partition access — kernel partition devices for v1.**  Backup reads each
     filesystem partition through its kernel partition device, leaving the
     existing backends unchanged.  Offset-aware device windows that also work
     on disk image files are planned as a follow-up.  Rationale: smallest
     possible change to reach a working implementation.

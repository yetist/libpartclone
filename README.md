# Partclone

Partclone is a project similar to the well-known backup utility "Partition
Image" (a.k.a. partimage). It backs up and restores **used blocks** of a
partition instead of copying it byte for byte, and it is designed for high
filesystem compatibility by reusing the filesystem's own library (for example,
libext2fs is used to read and write ext2/3/4 partitions).

Partclone also understands **whole disks**: it can save the partition table,
the gaps between partitions and every partition into a single streamable image,
and restore it — including onto a larger disk.

* Website: <http://partclone.org>
* Source and issue tracker: <https://github.com/Thomas-Tsai/partclone>
* Image format reference: [IMAGE_FORMATS.md](IMAGE_FORMATS.md)

## Features

* Used-block cloning for 16 filesystem backends, so images are much smaller
  than a raw copy.
* Whole-disk images (format `0003`): partition table, gaps, filesystem
  volumes, sparse raw volumes, stream-friendly.
* Image inspection (`partclone.info`), verification (`partclone.chkimg`),
  filesystem-agnostic restore (`partclone.restore`) and raw copy
  (`partclone.dd`, `partclone.imager`).
* Three image formats — `0001` (legacy), `0002` (per-filesystem, default) and
  `0003` (whole disk).
* Progress on the terminal, with ncurses or as JSON (`--json-status`), plus
  `--quiet`, `--binary-prefix` and `--prog-second`.
* Optional checksums (CRC32, xxHash64, xxHash128), compression through an
  external command (`-x`), pipes on stdin/stdout, direct I/O and rescue mode.
* Clean handling of `SIGINT`/`SIGTERM`: the operation stops at a block
  boundary and exits with status `254`.
* `libpartclone`: a shared library with a GObject/GIO API (`PtcClient`,
  `PtcImage`, `PtcVolume`), async operations, progress signals and
  `GCancellable`.
* Optional GTK4 front end (`ptc-gtk4`), a text UI (ncursesw), image mounting
  (`partclone.imgfuse`) and bash completion.
* Translations for `de`, `fr_FR`, `ka`, `pt_BR`, `ru`, `vi`, `zh_CN` and
  `zh_TW`.
* Man pages in [scdoc](https://git.sr.ht/~sircmpwn/scdoc) format in `docs/`.

## Supported filesystems

| Filesystem | Program(s) | Backend dependency |
|---|---|---|
| ext2/ext3/ext4 | `partclone.extfs`, `partclone.ext2`, `partclone.ext3`, `partclone.ext4`, `partclone.ext4dev` | libext2fs, libcom_err |
| Btrfs | `partclone.btrfs` | libuuid, libblkid, OpenSSL, zlib, zstd |
| XFS | `partclone.xfs` | liburcu, libuuid, libblkid |
| ReiserFS 3.6 | `partclone.reiserfs` | libreiserfs |
| Reiser4 | `partclone.reiser4` | libreiser4 (patched) |
| JFS | `partclone.jfs` | libjfs, libuuid |
| NILFS2 | `partclone.nilfs2` | libnilfs |
| F2FS | `partclone.f2fs` | libuuid |
| FAT12/16/32 | `partclone.fat`, `partclone.fat12`, `partclone.fat16`, `partclone.fat32`, `partclone.vfat` | built-in |
| exFAT | `partclone.exfat` | built-in |
| NTFS | `partclone.ntfs` | libntfs-3g (or libntfs) |
| HFS+ | `partclone.hfsp`, `partclone.hfs+`, `partclone.hfsplus` | built-in |
| APFS | `partclone.apfs` | built-in |
| UFS1/UFS2 | `partclone.ufs` | libufs, libbsd (patched) |
| VMFS3/VMFS5 | `partclone.vmfs`, `partclone.vmfs5`, `partclone.vmfs3`, `partclone.VMFS_volume_member` | libvmfs (patched), libuuid |
| MINIX | `partclone.minix` | built-in |

Filesystems without a backend can be copied block by block with
`partclone.dd` (dd-like, all blocks) or `partclone.imager` (raw partclone
image).

### Tools that are not tied to a filesystem

| Tool | Purpose |
|---|---|
| `partclone.info` | Show the header and volumes of an image (formats `0001`, `0002`, `0003`). |
| `partclone.chkimg` | Verify an image and its checksums. |
| `partclone.restore` | Restore an image onto a device without needing the filesystem backend. |
| `partclone.dd` | Raw block copy, dd-like, for unsupported filesystems. |
| `partclone.imager` | Raw (filesystem-agnostic) image tool. |
| `partclone.disk` | Clone, restore and check **whole disks** (format `0003`). |
| `partclone.ntfsfixboot` | Fix the NTFS boot sector (alias: `partclone.ntfsreloc`). |
| `partclone.fstype` | Detect a VMFS volume (built with the VMFS backend). |
| `partclone.imgfuse` | Mount a partclone image as a block device via FUSE (`-Dfuse=enabled`). |
| `ptc-gtk4` | GTK4 graphical front end (`-Dgtk4=enabled`). |

## Basic usage

Clone a partition to an image and restore it:

```sh
partclone.ext4 -c -s /dev/sda1 -o sda1.img
partclone.ext4 -r -s sda1.img -o /dev/sda1
```

Copy a partition to another partition on the fly:

```sh
partclone.ext4 -b -s /dev/sda1 -o /dev/sdb1
```

Inspect and verify an image:

```sh
partclone.info -s sda1.img
partclone.chkimg -s sda1.img
```

Stream and compress without an intermediate file:

```sh
partclone.ext4 -c -s /dev/sda1 -o - | zstd -T0 > sda1.img.zst
zstd -dc sda1.img.zst | partclone.ext4 -r -s - -o /dev/sda1
```

Restore an image from any filesystem with the filesystem-agnostic tool:

```sh
partclone.restore -s sda1.img -o /dev/sda1
```

## Whole-disk images (`partclone.disk`)

`partclone.disk` stores a whole disk — MBR/GPT, the gaps between partitions
and every partition — in one format `0003` image. Partitions with a supported
filesystem are stored as filesystem volumes (used blocks only); the rest can
be stored raw, optionally sparse.

```sh
# Back up a whole disk to a compressed image.
partclone.disk -c -s /dev/sda -o - | zstd -T0 > sda.img.zst

# Restore it.
zstd -dc sda.img.zst | partclone.disk -r -s - -o /dev/sda

# Verify a whole-disk image.
partclone.disk --check-image -s sda.img

# Restore onto a larger disk and grow the last partition entry.
partclone.disk -r --grow -s sda.img -o /dev/sdb
```

Useful options:

| Option | Meaning |
|---|---|
| `--gaps=none\|metadata\|sparse\|full` | Which non-partition areas to store (default: `metadata`, up to 1 MiB). |
| `--partition-table=auto\|raw\|regenerate` | Write the stored table verbatim or rebuild it for the target size with libfdisk (default: `auto`). |
| `--sparse` | Store raw volumes with a zero-detecting bitmap. |
| `--no-zero-skipped` | On restore, do not write zeros over blocks absent from a bitmap. |
| `--no-fs-volumes` | Store every partition as a raw volume. |
| `--grow` | Grow the last partition entry to fill a larger target (the filesystem itself is not resized). |
| `-S`, `--device-size=N` | Override an undetectable source size, in bytes. |

`--grow` only resizes the partition entry; afterwards run the appropriate
filesystem growth command (`resize2fs`, `xfs_growfs`, `btrfs filesystem
resize`, `resize.f2fs`, …), which partclone prints for you.

## Building from source

Partclone uses [Meson](https://mesonbuild.com/). Required:

* a C compiler and `pkg-config`
* Meson ≥ 0.61 and Ninja
* GLib/GObject/GIO ≥ 2.44 (`gio-2.0`, `gmodule-no-export-2.0`)
* `uuid`, OpenSSL, zlib and zstd
* `gettext` (optional, for translations)
* `scdoc` (optional, to render the man pages)

Each filesystem backend needs its own library; see the table above. Some
backends (Reiser4, UFS, VMFS) need patched libraries that are not part of the
major distributions.

Configure, build and install:

```sh
meson setup build --prefix=/usr -Dall=true
meson compile -C build
sudo meson install -C build
```

Useful options (see `meson configure build` for the full list):

| Option | Default | Meaning |
|---|---|---|
| `-Dall=true` | `false` | Enable every filesystem backend whose libraries are present. |
| `-D<fs>=enabled\|disabled\|auto` | `disabled` | Enable one backend: `extfs`, `xfs`, `reiserfs`, `reiser4`, `hfsp`, `apfs`, `fat`, `exfat`, `f2fs`, `nilfs2`, `ntfs`, `ufs`, `vmfs`, `jfs`, `btrfs`, `minix`. |
| `-Dgtk4=enabled\|auto\|disabled` | `auto` | Build the GTK4 front end (requires GTK ≥ 4.10). |
| `-Dncursesw=enabled` | `disabled` | Build the ncursesw text UI. |
| `-Dfuse=enabled` | `disabled` | Build `partclone.imgfuse` (requires FUSE 3). |
| `-Dxxhash=false` | `true` | Disable xxHash checksums. |
| `-Disal=true` | `false` | Use Intel ISA-L for CRC32. |
| `-Dfs-test=true` | `false` | Enable the filesystem clone/restore tests. |
| `-Dfeature-test=true` | `false` | Enable the feature tests. |
| `-Dbash-completion=false` | `true` | Do not install bash completion. |

Everything is installed below `--prefix`: the commands in `sbin`, the GTK4
front end in `bin`, `libpartclone` and its headers in `lib`/`include`, the
filesystem backends as plugins in `lib/partclone`, and the man pages in
`share/man`.

## libpartclone

The command line tools are thin front ends over `libpartclone`, a shared
library with a GObject/GIO API. It is installed together with a pkg-config
file named `partclone`.

```c
#include <libpartclone.h>

PtcClient *client = ptc_client_new();
GError *error = NULL;

ptc_client_set_mode(client, PTC_MODE_CLONE);
ptc_client_set_backend(client, "extfs");
ptc_client_set_source(client, "/dev/sda1");
ptc_client_set_target(client, "sda1.img");

if (!ptc_client_run(client, NULL, &error)) {
    g_printerr("%s\n", error->message);
    g_error_free(error);
    g_object_unref(client);
    return 1;
}
g_object_unref(client);
```

The API provides:

* `PtcClient` — modes `PTC_MODE_CLONE`, `RESTORE`, `CHKIMG`, `DD`, `DOMAIN`,
  `DISK_CLONE`, `DISK_RESTORE` and `DISK_CHKIMG`, with properties for source,
  target, backend, debug, log file, sparse/gaps/partition-table/grow options.
* Progress through the `::progress` signal and, for whole-disk operations,
  `::volume-started`, `::volume-progress` and `::volume-finished`.
* `GCancellable` support and an asynchronous `ptc_client_run_async()`.
* `PtcImage` and `PtcVolume` — inspect an image (version, partition table,
  disk size, sector size, volumes, labels, offsets) without restoring it.
* `ptc_fs_backend_list()` — enumerate the available filesystem backends.

A complete example is in [examples/ptc-demo.c](examples/ptc-demo.c); build it
out of tree with:

```sh
cc ptc-demo.c $(pkg-config --cflags --libs partclone) -o ptc-demo
```

## GTK4 front end

When GTK4 is available, `ptc-gtk4` offers a graphical front end for all eight
operations (clone, restore, check, dd, domain log, disk clone, disk restore
and disk check), with per-mode options, live per-volume progress, image
inspection, cancellation and an optional debug log.

```sh
meson setup build -Dgtk4=enabled
meson compile -C build
./build/src/ptc-gtk4
```

For automated environments it has a headless self-test that performs a
partition round-trip followed by a whole-disk round-trip:

```sh
xvfb-run -a ./build/src/ptc-gtk4 --self-test
```

## Localization

Messages are translated with gettext, and only the level-0 messages that go to
stderr are translatable. Select a language with the usual environment
variables:

```sh
LANGUAGE=zh_TW partclone.ext4 -c -s /dev/sda1 -o sda1.img
```

The available languages are `de`, `fr_FR`, `ka`, `pt_BR`, `ru`, `vi`,
`zh_CN` and `zh_TW`. Translation sources live in `po/`.

## Documentation

Man pages are installed with the tools. The sources are in `docs/`: the
general pages (`partclone.8`, `partclone.chkimg.8`, `partclone.dd.8`,
`partclone.disk.8`, `partclone.info.8`, `partclone.restore.8`, …) are written
in scdoc `.scd` format and rendered with `scdoc`; the per-filesystem pages are
shipped pre-generated.

```sh
man partclone
man partclone.disk
```

See [IMAGE_FORMATS.md](IMAGE_FORMATS.md) for the on-disk image layout and
[docs/README.md](docs/README.md) for how to contribute documentation.

## Testing

```sh
meson test -C build
```

The default suite covers the command line tools, whole-disk images,
progress/JSON output and, when built, the GTK4 self-test. Enable the
filesystem and feature suites with `-Dfs-test=true` and `-Dfeature-test=true`.
The filesystem tests need root and the corresponding `mkfs` tools; the GTK4
test runs under `xvfb-run` and is skipped when no X server is available.

## Limitations

* The filesystem being backed up must be unmounted and inaccessible to other
  programs.
* Restoring a whole-disk image overwrites the target disk; make sure it is not
  mounted.
* `--grow` grows the last partition entry but does not resize the filesystem
  inside it.
* Filesystems without a backend must be copied with `partclone.dd` or
  `partclone.imager`.
* Format `0003` device windows (reading a partition from an image file without
  a kernel partition device) are still design-only.

## License

Partclone is free software, licensed under the GNU General Public License
version 2 or (at your option) any later version. See [COPYING](COPYING).

For more information, visit <http://partclone.org>.

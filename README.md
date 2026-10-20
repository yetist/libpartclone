# libpartclone

A library for efficient, filesystem-aware partition backup, restoration and
cloning, based on [Partclone](https://github.com/Thomas-Tsai/partclone).

It backs up and restores **used blocks** of a partition instead of copying it
byte for byte, reusing each filesystem's own library (e.g. libext2fs for
ext2/3/4), and it handles **whole disks**: partition table, gaps and every
partition in one streamable image. The library ships with the `partclone.*`
command line tools and an optional GTK4 front end.

* Source: <https://github.com/yetist/libpartclone>
* Upstream: <https://github.com/Thomas-Tsai/partclone>
* Website: <http://partclone.org>
* Image formats: [IMAGE_FORMATS.md](IMAGE_FORMATS.md)

## Features

* Used-block cloning for 16 filesystem backends.
* Whole-disk images (format `0003`), restorable onto a larger disk.
* Image inspection (`partclone.info`), verification (`partclone.chkimg`),
  filesystem-agnostic restore (`partclone.restore`) and raw copy
  (`partclone.dd`, `partclone.imager`).
* Terminal, ncurses or JSON progress; checksums (CRC32, xxHash64/128);
  compression through an external command (`-x`); pipes; direct I/O; rescue.
* `SIGINT`/`SIGTERM` stop at a block boundary with exit status `254`.
* GObject/GIO API (`PtcClient`, `PtcImage`, `PtcVolume`), async operations and
  `GCancellable`.
* Optional GTK4 front end, ncursesw UI, FUSE image mount and bash completion.
* Translations: `de`, `fr_FR`, `ka`, `pt_BR`, `ru`, `vi`, `zh_CN`, `zh_TW`.

## Supported filesystems

| Filesystem | Command | Backend library |
|---|---|---|
| ext2/3/4 | `partclone.extfs` (`ext2`, `ext3`, `ext4`, `ext4dev`) | libext2fs |
| Btrfs | `partclone.btrfs` | libuuid, libblkid, OpenSSL, zlib, zstd |
| XFS | `partclone.xfs` | liburcu, libuuid, libblkid |
| ReiserFS 3.6 | `partclone.reiserfs` | libreiserfs |
| Reiser4 | `partclone.reiser4` | libreiser4 (patched) |
| JFS | `partclone.jfs` | libjfs, libuuid |
| NILFS2 | `partclone.nilfs2` | libnilfs |
| F2FS | `partclone.f2fs` | libuuid |
| FAT12/16/32 | `partclone.fat` (`fat12`, `fat16`, `fat32`, `vfat`) | built-in |
| exFAT | `partclone.exfat` | built-in |
| NTFS | `partclone.ntfs` | libntfs-3g or libntfs |
| HFS+ | `partclone.hfsp` (`hfs+`, `hfsplus`) | built-in |
| APFS | `partclone.apfs` | built-in |
| UFS1/2 | `partclone.ufs` | libufs, libbsd (patched) |
| VMFS3/5 | `partclone.vmfs` (`vmfs5`, `vmfs3`) | libvmfs (patched), libuuid |
| MINIX | `partclone.minix` | built-in |

Unsupported filesystems can be copied with `partclone.dd` (all blocks) or
`partclone.imager` (raw image). Other tools: `partclone.disk` (whole disk),
`partclone.ntfsfixboot`, `partclone.fstype` (VMFS), `partclone.imgfuse`.

## Usage

```sh
# Clone and restore a partition.
partclone.ext4 -c -s /dev/sda1 -o sda1.img
partclone.ext4 -r -s sda1.img -o /dev/sda1

# Partition to partition, inspect, verify.
partclone.ext4 -b -s /dev/sda1 -o /dev/sdb1
partclone.info   -s sda1.img
partclone.chkimg -s sda1.img

# Stream and compress.
partclone.ext4 -c -s /dev/sda1 -o - | zstd -T0 > sda1.img.zst
zstd -dc sda1.img.zst | partclone.ext4 -r -s - -o /dev/sda1
```

### Whole-disk images

`partclone.disk` stores an MBR/GPT, the gaps and every partition in one format
`0003` image. Supported filesystems become filesystem volumes (used blocks
only); the rest can be stored raw, optionally sparse.

```sh
partclone.disk -c -s /dev/sda -o sda.img
partclone.disk -r --grow -s sda.img -o /dev/sdb
partclone.disk --check-image -s sda.img
```

Options: `--gaps=none|metadata|sparse|full` (default `metadata`),
`--partition-table=auto|raw|regenerate` (default `auto`), `--sparse`,
`--no-zero-skipped`, `--no-fs-volumes`, `--grow`, `-S N`. `--grow` only
enlarges the last partition entry; partclone prints the filesystem growth
command to run afterwards.

## Building

This project needs a C compiler, pkg-config, Meson ≥ 0.61, GLib/GObject/GIO ≥ 2.44,
libuuid, OpenSSL, zlib and zstd. Each backend needs its library (see above);
`scdoc` and `gettext` are optional.

```sh
meson setup build --prefix=/usr -Dall=true
meson compile -C build
sudo meson install -C build
```

Useful options (`meson configure build` lists them all):

| Option | Default | Meaning |
|---|---|---|
| `-Dall=true` | `false` | Enable every backend whose libraries are found. |
| `-D<fs>=enabled\|auto` | `disabled` | Enable one backend. |
| `-Dgtk4=enabled` | `auto` | Build `ptc-gtk4` (GTK ≥ 4.10). |
| `-Dncursesw=enabled` | `disabled` | Build the ncursesw UI. |
| `-Dfuse=enabled` | `disabled` | Build `partclone.imgfuse`. |
| `-Dfs-test=true`, `-Dfeature-test=true` | `false` | Enable the extra test suites. |

Commands go to `sbin`, `ptc-gtk4` to `bin`, `libpartclone` and headers to
`lib`/`include`, backends (plugins) to `lib/partclone`, man pages to
`share/man`.

## libpartclone

The command line tools are thin front ends over `libpartclone`, installed with
a pkg-config file named `partclone`. See
[examples/ptc-demo.c](examples/ptc-demo.c) for a full example.

```c
#include <libpartclone.h>

PtcClient *client = ptc_client_new();
GError *error = NULL;

ptc_client_set_mode(client, PTC_MODE_CLONE);
ptc_client_set_backend(client, "extfs");
ptc_client_set_source(client, "/dev/sda1");
ptc_client_set_target(client, "sda1.img");

if (!ptc_client_run(client, NULL, &error)) { /* handle error */ }
g_object_unref(client);
```

Besides the eight modes (`CLONE`, `RESTORE`, `CHKIMG`, `DD`, `DOMAIN`,
`DISK_CLONE`, `DISK_RESTORE`, `DISK_CHKIMG`), the API offers `::progress` and
`::volume-*` signals, `ptc_client_run_async()`, and `PtcImage`/`PtcVolume` for
inspecting images.

## Front end, translations and docs

```sh
meson setup build -Dgtk4=enabled && meson compile -C build
./build/src/ptc-gtk4                         # GTK4 front end
xvfb-run -a ./build/src/ptc-gtk4 --self-test # headless self-test
LANGUAGE=zh_TW partclone.ext4 -c -s /dev/sda1 -o sda1.img
meson test -C build                          # test suite
man partclone; man partclone.disk            # scdoc pages in docs/
```

## Limitations

* The source filesystem must be unmounted and inaccessible to other programs.
* Whole-disk restore overwrites the target disk; make sure it is not mounted.
* `--grow` does not resize the filesystem inside the grown partition.
* Format `0003` device windows (reading a partition from an image file without
  a kernel partition device) are still design-only.

## License

This project is licensed under the GNU General Public License v2 or later.
See [COPYING](COPYING).

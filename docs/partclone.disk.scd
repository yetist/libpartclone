partclone.disk(8) "Partclone User Manual"

# NAME

partclone.disk - back up and restore a whole disk (partition table, gaps and
every partition) to and from a single image.

# SYNOPSIS

*partclone.disk* [*-c* | *--clone*] [*-r* | *--restore*] [*--check-image*]
\[*-s* | *--source*] _source_ \[[*-o* | *--output*] [*-O* | *--overwrite*]]
_target_ \[*--gaps=*_POLICY_] [*--partition-table=*_MODE_] [*--sparse*]
\[*--no-zero-skipped*] [*--no-fs-volumes*] [*--grow*] [*-S* | *--device-size=*_N_]
\[*-dX* | *--debug=X*] [*-L* | *--logfile*] _logfile_
\[*-z* | *--buffer_size* _SIZE_] [*-N* | *--ncurses*] [*-q* | *--quiet*]
\[*-f* | *--UI-fresh* _sec_] [*-F* | *--force*] [*-C* | *--no_check*]
\[*-I* | *--ignore_fschk*] [*-i* | *--ignore_crc*] [*-R* | *--rescue*]
\[*-w* | *--skip_write_error*] [*-B* | *--no_block_detail*]
\[*--binary-prefix*] [*--prog-second*] [*--json-status=[*_FILE_*]*]
\[*--json-fresh=N*] [*-x* | *--compresscmd* _CMD_] [*-n* | *--note* _NOTE_]

# DESCRIPTION

*partclone.disk* backs up and restores an entire block device - its partition
table, the gaps between partitions and every partition - to and from a single
image.

The per-filesystem tools (*partclone*.[fstype]) and *partclone.dd* each
describe exactly one filesystem or one raw device (formats 0001 and 0002) and
carry no information about where that filesystem sits on a disk. The
*partclone.disk* image is a container (format 0003): a 512 byte disk header, a
partition table blob and then one *volume* per region of the disk. Each volume
is a volume descriptor followed by an ordinary format 0002 child image, so a
partition with a supported filesystem is stored with the same per-filesystem
bitmap and checksums as *partclone*.[fstype], while gaps and partitions whose
filesystem is not supported are stored as raw volumes. *partclone.disk* is not
meant to replace the per-filesystem tools: single-partition workflows should
keep producing 0002 images.

The image is strictly sequential, so it can be piped through external
compression or encryption, for example:

```
 partclone.disk -c -s /dev/sda -o - | zstd -T0 > sda.img.zst
 zstd -dc sda.img.zst | partclone.disk -r -s - -o /dev/sda
```

Clone reads each filesystem partition through its kernel partition device and
therefore normally requires root. Restore writes the partition table (verbatim
or regenerated, see *--partition-table*) and then every volume. A target of the
same size or larger is supported; *--grow* can extend the last partition entry
to fill a larger target, but the filesystem inside it is not resized, and
partclone prints the recommended growth command.

*partclone.info* can list the volumes of a format 0003 image.

# OPTIONS

The program follows the usual GNU command line syntax, with long options
starting with two dashes (-). A summary of options is included below.

*-c*, *--clone*
	Save a whole disk to a format 0003 image.

*-r*, *--restore*
	Restore a whole disk from a format 0003 image.

*--check-image*
	Check a format 0003 image instead of restoring it. Use with *-s* _IMAGE_.

*-s* _FILE_, *--source* _FILE_
	Source FILE. For clone this is the disk device; for restore and
	*--check-image* it is the image. Use '-' to read the image from standard
	input.

*-o* _FILE_, *--output* _FILE_
	Output FILE. For clone this is the image; for restore it is the target
	device. Use '-' to write the image to standard output.

*-O* _FILE_, *--overwrite* _FILE_
	Overwrite FILE, overwriting if it exists.

*-W*, *--restore_raw_file*
	Create a special raw file for a loop device.

*--gaps=*_POLICY_
	Where to store the gaps, i.e. the areas of the disk that belong to no
	partition (boot loader code, alignment padding, the backup GPT, ...).
	_POLICY_ is one of:

	*none*: do not store gaps.

	*metadata*: store gaps up to a 1 MiB threshold as sparse raw volumes
	(default). This keeps the usual boot loader locations without scanning
	arbitrarily large free space.

	*sparse*: store every gap as a sparse raw volume (all-zero blocks are
	skipped).

	*full*: store every gap verbatim.

*--partition-table=*_MODE_
	How to write the partition table on restore. _MODE_ is one of:

	*auto*: write the stored table verbatim, and regenerate it when the target
	size differs from the source and the table is GPT (default).

	*raw*: write the stored sectors back verbatim. Correct only on a target of
	the same size; on a differently-sized GPT target partclone only warns.

	*regenerate*: rebuild the table from the stored manifest with libfdisk, so
	that the secondary GPT and all checksums match the target size. Requires
	libfdisk and a table type that can be regenerated (MBR or GPT).

*--sparse*
	Store raw volumes (gaps and partitions with an unsupported filesystem)
	with a zero-detecting bitmap instead of their full content. Filesystem
	partitions always use their filesystem bitmap.

*--no-zero-skipped*
	On restore, seek over blocks that are absent from a volume bitmap instead
	of writing zeros to them. By default disk restore writes zeros so that a
	reused target has a defined result.

*--no-fs-volumes*
	Store every partition as a raw volume instead of a filesystem volume, i.e.
	do not run the per-filesystem backend.

*--grow*
	Grow the last partition entry so that it fills a larger target. The
	filesystem inside it is not resized; it stays consistent and smaller than
	its partition, and partclone prints the recommended growth command
	(resize2fs, xfs_growfs, btrfs, resize.f2fs, ...).

*-S* _N_, *--device-size=*_N_
	Override the size of the source disk, in bytes, when it cannot be
	determined automatically.

*-d* _level_, *--debug* _level_
	Set the debug level [1|2|3]

*-L* _FILE_, *--logfile* _FILE_
	put special path to record partclone log information. (default
	/var/log/partclone.log)

*-z* _size_, *--buffer_size* _size_
	Read/write buffer size (default: 1048576)

*-N*, *--ncurses*
	Using Ncurses Text User Interface.

*-q*, *--quiet*
	Disable progress message.

*-f* _sec_, *--UI-fresh* _sec_
	put special second to different interval.

*-F*, *--force*
	Force progress.

*-C*, *--no_check*
	Don't check device size and free space.

*-I*, *--ignore_fschk*
	Ignore filesystem check.

*-i*, *--ignore_crc*
	Ignore crc check error.

*-R*, *--rescue*
	Continue after disk read errors.

*-w*, *--skip_write_error*
	Continue restore while write errors

*-B*, *--no_block_detail*
	Show progress message without block detail

*--binary-prefix*
	Show progress with bit size (default is MB, GB...)

*--prog-second*
	Show progress in seconds (default is minute)

*--json-status=[*_FILE_*]*
	Write progress status to FILE in JSON format (default FILE:
	/var/log/partclone.json).

*--json-fresh=N*
	JSON status fresh interval in seconds (default: 1).

*-x* _CMD_, *--compresscmd* _CMD_
	Start CMD as an output pipe to compress the cloned image.

*-n* _NOTE_, *--note* _NOTE_
	Display Message Note (128 words).

*-h*, *--help*
	Show summary of options.

*-v*, *--version*
	Show version of program.

# FILES

*/var/log/partclone.log*
	The log file of *partclone.disk*

*/var/log/partclone.json*
	Default progress file used by *--json-status*.

# EXAMPLES

```
 Back up a whole disk to a compressed image.
   partclone.disk -c -s /dev/sda -o - | zstd -T0 > sda.img.zst

 Restore the whole disk from the compressed image.
   zstd -dc sda.img.zst | partclone.disk -r -s - -o /dev/sda

 Check a whole-disk image.
   partclone.disk --check-image -s sda.img

 Restore onto a larger disk and grow the last partition entry.
   partclone.disk -r --grow -s sda.img -o /dev/sdb
```

# DIAGNOSTICS

The following diagnostics may be issued on *stderr*.

*partclone.disk* provides some return codes, that can be used in scripts:

[[ *Code*
:- *Diagnostic*
|  0
:  Program exited successfully.
|  1
:  Clone or Restore seem failed.
|  254
:  Operation cancelled by SIGINT or SIGTERM.

*partclone.disk* catches SIGINT (Ctrl-C) and SIGTERM during a long running
clone, restore or check. The operation then stops at the next volume or block
boundary, the target (image or device) is left incomplete and the exit status
is *254*.

# BUGS

Report bugs to thomas@clonezilla.org or <http://partclone.org>.

You can get support at http://partclone.org

# SEE ALSO

*partclone*(8), *partclone.chkimg*(8), *partclone.restore*(8),
*partclone.dd*(8), *partclone.info*(8)

# AUTHOR

*Yu-Chin Tsai* <thomas@clonezilla.org>

# COPYRIGHT

Copyright © 2007 Yu-Chin Tsai

This manual page was written for the Debian system (and may be used by
others).

Permission is granted to copy, distribute and/or modify this document under
the terms of the GNU General Public License, Version 2 or (at your option)
any later version published by the Free Software Foundation.

On Debian systems, the complete text of the GNU General Public License can
be found in /usr/share/common-licenses/GPL.

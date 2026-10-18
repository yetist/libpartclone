partclone.dd(8) "Partclone User Manual"

# NAME

partclone.dd - unsupported file system backup utility (like *dd*).

# SYNOPSIS

*partclone.dd* [*-s* | *--source*] _source_
\[[*-o* | *--output*] [*-O* | *--overwrite*]] _target_ [*-dX* | *--debug=X*]
\[*-W* | *--restore_raw_file*] [*-z* | *--buffer_size*] [*-N* | *--ncurses*]
\[*-q* | *--quiet*] [*-f* | *--UI-fresh*] [*-F* | *--force*]
\[*-I* | *--ignore_fschk*] [*-i* | *--ignore_crc*] [*-C* | *--no_check*]
\[*-L* | *--logfile*] _logfile_ [*-w* | *--skip_write_error*]
\[*-E* | *--offset=X*] [*-T* | *--btfiles*] [*-t* | *--btfiles_torrent*]
\[*-B* | *--no_block_detail*] [*--binary-prefix*] [*--prog-second*]
\[*--write-direct-io*] [*--read-direct-io*] [*-S* | *--device-size*]

# DESCRIPTION

*partclone.dd* is a part of *Partclone* project to clone unsupported file
system with dd method. It will backup all block from partition. Partclone
provide utilities to backup used blocks and design for higher compatibility
of the file system by using existing library, e.g. e2fslibs is used to read
the used block of ext2 partition.

*Partclone* supported file system include btrfs, ext2, ext3, ext4, reiserfs,
reiser4, xfs and jfs for LINUX. Also support some non-linux operation
system, ex: NTFS and FAT (for Windows), HFS plus (APPLE MAC OS), UFS2
(FreeBSD), VMFS (VMWare Vsphere). All partclone utils could be run like
partclone.xxx is very similar fsck or mkfs. For example, for backup/restore
hfsplus, just run partclone.hfsp.

# OPTIONS

The program follows the usual GNU command line syntax, with long options
starting with two dashes (-). A summary of options is included below.

*-s* _FILE_, *--source* _FILE_
	Source FILE. The FILE could be a image file (made by partclone) or
	device depend on your action. Normally, backup source is device,
	restore source is image file.

	Receving data from pipe line is supported ONLY for restoring, just
	ignore -s option or use '-' means receive data from stdin.

*-o* _FILE_, *--output* _FILE_
	Output FILE. The FILE could be a image file (partclone will generate)
	or device depend on your action. Normally, backup output to image file
	and restore output to device.

	Sending data to pipe line is also supported ONLY for back-up, just
	ignore -o option or use '-' means send data to stdout.

*-O* _FILE_, *--overwrite* _FILE_
	Overwrite FILE, overwriting if exists.

*-W*, *--restore_raw_file*
	Create special raw file for loop device.

*-L* _FILE_, *--logfile* _FILE_
	put special path to record partclone log information. (default
	/var/log/partclone.log)

*-C*, *--no_check*
	Don't check device size and free space.

*-N*, *--ncurses*
	Using Ncurses Text User Interface.

*-I*, *--ignore_fschk*
	Ignore filesystem check.

*-i*, *--ignore_crc*
	Ignore crc check error.

*-F*, *--force*
	Force progress.

*-f* _sec_, *--UI-fresh* _sec_
	put special second to different interval.

*-z* _size_, *--buffer_size* _size_
	Read/write buffer size (default: 1048576)

*-q*, *--quiet*
	Disable progress message.

*-d* _level_, *--debug* _level_
	Set the debug level [1|2|3]

*-w*, *--skip_write_error*
	Continue restore while write errors

*--write-direct-io*
	Writing data to TARGET partition without cache

*--read-direct-io*
	Reading data from SOURCE partition without cache

*-S*, *--device-size*
	Define device size

*-B*, *--no_block_detail*
	Show progress message without block detail

*--binary-prefix*
	Show progress with bit size (default is MB, GB...)

*--prog-second*
	Show progress in seconds (default is minute)

*-E*, *--offset=X*
	Add offset X (bytes) to OUTPUT

*-T*, *--btfiles*
	Restore block as file for ClonezillaBT.

*-t*, *--btfiles_torrent*
	Restore block as file for ClonezillaBT but only generate torrent.

*-h*, *--help*
	Show summary of options.

*-v*, *--version*
	Show version of program.

# FILES

*/var/log/partclone.log*
	The log file of *partclone.dd*

# EXAMPLES

```
 clone /dev/hda1 to hda1.dd.img and display debug information.
   partclone.dd -d -s /dev/hda1 -o hda1.dd.img

 restore /dev/hda1 from hda1.dd.img and display debug information.
   partclone.dd -d -s hda1.dd.img -o /dev/hda1
```

# DIAGNOSTICS

The following diagnostics may be issued on *stderr*.

*partclone.dd* provides some return codes, that can be used in scripts:

[[ *Code*
:- *Diagnostic*
|  0
:  Program exited successfully.
|  1
:  Clone or Restore seem failed.
|  254
:  Operation cancelled by SIGINT or SIGTERM.

*partclone.dd* catches SIGINT (Ctrl-C) and SIGTERM during a long running
clone, restore or check. The operation then stops at the next block boundary,
the target (image or device) is left incomplete and the exit status is
*254*.

# BUGS

Report bugs to thomas@clonezilla.org or <http://partclone.org>.

You can get support at http://partclone.org

# SEE ALSO

*partclone*(8), *partclone.chkimg*(8), *partclone.restore*(8),
*partclone.info*(8), *partclone.disk*(8)

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

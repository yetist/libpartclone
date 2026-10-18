partclone.chkimg(8) "Partclone User Manual"

# NAME

partclone.chkimg - the utility to check image made by partclone.

# SYNOPSIS

*partclone.chkimg* [*-s* | *--source*] _source_ [*-dX* | *--debug=X*]
\[*-N* | *--ncurses*] [*-f* | *--UI-fresh*] [*-F* | *--force*]
\[*--ignore_crc*] [*-C* | *--no_check*] [*-B* | *--no_block_detail*]
\[*-z* | *--buffer_size*] [*-L* | *--logfile*] _logfile_

# DESCRIPTION

*partclone.chkimg* is a part of *Partclone* project to verify image files.
Partclone provide utilities to backup used blocks and design for higher
compatibility of the file system by using existing library, e.g. e2fslibs is
used to read the used block of ext2 partition.

*Partclone* supported file system include btrfs, ext2, ext3, ext4, reiserfs,
reiser4, xfs and jfs for LINUX. Also support some non-linux operation
system, ex: NTFS, FAT, EXFAT (for Windows), HFS plus (APPLE MAC OS), UFS2
(FreeBSD), VMFS (VMWare Vsphere). All partclone utils could be run like
partclone.xxx is very smiliar fsck or mkfs. For example, for backup/restore
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

*-L* _FILE_, *--logfile* _FILE_
	put special path to record partclone log information. (default
	/var/log/partclone.log)

*-C*, *--no_check*
	Don't check device size and free space.

*-N*, *--ncurses*
	Using Ncurses Text User Interface.

*--ignore_crc*
	Ignore crc check error.

*-F*, *--force*
	Force progress.

*-f* _sec_, *--UI-fresh* _sec_
	put special second to different interval.

*-d* _level_, *--debug* _level_
	Set the debug level [1|2|3]

*-h*, *--help*
	Show summary of options.

*-v*, *--version*
	Show version of program.

# FILES

*/var/log/partclone.log*
	The log file of *partclone.chkimg*

# EXAMPLES

```
 check part.img file is correct or not.
   partclone.chkimg -d -s partclone.img
```

# DIAGNOSTICS

The following diagnostics may be issued on *stderr*.

*partclone.chkimg* provides some return codes, that can be used in scripts:

[[ *Code*
:- *Diagnostic*
|  0
:  Program exited successfully.
|  1
:  Clone or Restore seem failed.
|  254
:  Operation cancelled by SIGINT or SIGTERM.

*partclone.chkimg* catches SIGINT (Ctrl-C) and SIGTERM during a long running
clone, restore or check. The operation then stops at the next block boundary,
the target (image or device) is left incomplete and the exit status is
*254*.

# BUGS

Report bugs to thomas@clonezilla.org or <http://partclone.org>.

You can get support at http://partclone.org

# SEE ALSO

*partclone*(8), *partclone.restore*(8), *partclone.dd*(8),
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

partclone.ntfsfixboot(8) "Partclone User Manual"

# NAME

partclone.ntfsfixboot - deals with braindeadness with moving NTFS
filesystems.

# SYNOPSIS

*partclone.ntfsfixboot* [*-w*] [*-h* _n_] [*-t* _n_] [*-s* _n_] [*-b*]
\[*-f*] [*-p*] _DEVICE_

# DESCRIPTION

*partclone.ntfsfixboot* is NOT a part of *Partclone* project but included to
fix ntfs boot issue. Partclone provide utilities to backup used blocks and
design for higher compatibility of the file system by using existing library,
e.g. e2fslibs is used to read the used block of ext2 partition.

*partclone.ntfsfixboot* deals with braindeadness with moving NTFS
filesystems. writted by Orgad Shaneh (2009) and Daniel J. Grace (2006).

# OPTIONS

The program follows the usual GNU command line syntax, a summary of options
is included below.

*-w*
	Write new start sector to the partition.

*-h* _num_
	Specify number of heads per track. If omitted, determined via ioctl.

*-t* _num_
	Specify number of sectors per track. If omitted, determined via ioctl.

*-s* _num_
	New start sector to write. If omitted, determined via ioctl.

*-b*
	Proceed even if the specified device is not a partition.

*-f*
	Force the operation to occur even if device does not look like a valid
	NTFS partition or values are equal.

*-p*
	Print debug information (values read, values requested etc.)

_DEVICE_
	where device points to an NTFS partition

# DIAGNOSTICS

The following diagnostics may be issued on *stderr*.

*partclone.ntfsfixboot* provides some return codes, that can be used in
scripts:

[[ *Code*
:- *Diagnostic*
|  0
:  success (values are correct, or changed successfully)
|  1
:  a change is needed, but -w was not specified
|  2
:  an error occured

# BUGS

Report bugs to thomas@clonezilla.org or <http://partclone.org>.

Report bugs to upstrem <http://sourceforge.net/projects/ntfsfixboot/>.

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

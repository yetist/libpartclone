## ---------------------------------------------------------------------------
## partclone command line tools
##
## Thin front ends that link against the shared library built in ../lib.
## ---------------------------------------------------------------------------

AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-D_FILE_OFFSET_BITS=64 \
	-I$(top_srcdir)/lib \
	$(GLIB_CFLAGS) $(ISAL_CFLAGS)

AM_CFLAGS = -D_FILE_OFFSET_BITS=64

LDADD = $(LIBINTL)

if ENABLE_STATIC
AM_LDFLAGS = -all-static
endif

if ENABLE_MEMTRACE
AM_CFLAGS += -DMEMTRACE -DMEMWATCH -DMW_STDIO
endif

# The library every command line tool links against.
PCL_LIBPARTCLONE = $(top_builddir)/lib/libpartclone.la

sbin_PROGRAMS = partclone.info partclone.dd partclone.restore partclone.chkimg partclone.imager

partclone_info_SOURCES = info.c
partclone_info_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)

partclone_dd_SOURCES = main.c
partclone_dd_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)

partclone_restore_SOURCES = main.c
partclone_restore_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)

partclone_chkimg_SOURCES = main.c
partclone_chkimg_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)

partclone_imager_SOURCES = main.c
partclone_imager_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)

## ntfsfixboot is a standalone, libc only helper.
sbin_PROGRAMS += partclone.ntfsfixboot
partclone_ntfsfixboot_SOURCES = ntfsfixboot.c

## ---------------------------------------------------------------------------
## One front end per filesystem backend
## ---------------------------------------------------------------------------

if ENABLE_EXTFS
sbin_PROGRAMS += partclone.extfs
partclone_extfs_SOURCES = main.c
partclone_extfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_REISERFS
sbin_PROGRAMS += partclone.reiserfs
partclone_reiserfs_SOURCES = main.c
partclone_reiserfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_REISER4
sbin_PROGRAMS += partclone.reiser4
partclone_reiser4_SOURCES = main.c
partclone_reiser4_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_HFSP
sbin_PROGRAMS += partclone.hfsp
partclone_hfsp_SOURCES = main.c
partclone_hfsp_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_APFS
sbin_PROGRAMS += partclone.apfs
partclone_apfs_SOURCES = main.c
partclone_apfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_FAT
sbin_PROGRAMS += partclone.fat
partclone_fat_SOURCES = main.c
partclone_fat_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_EXFAT
sbin_PROGRAMS += partclone.exfat
partclone_exfat_SOURCES = main.c
partclone_exfat_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_F2FS
sbin_PROGRAMS += partclone.f2fs
partclone_f2fs_SOURCES = main.c
partclone_f2fs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_NILFS2
sbin_PROGRAMS += partclone.nilfs2
partclone_nilfs2_SOURCES = main.c
partclone_nilfs2_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_NTFS
sbin_PROGRAMS += partclone.ntfs
partclone_ntfs_SOURCES = main.c
partclone_ntfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_UFS
sbin_PROGRAMS += partclone.ufs
partclone_ufs_SOURCES = main.c
partclone_ufs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_VMFS
sbin_PROGRAMS += partclone.vmfs partclone.vmfs5 partclone.fstype
partclone_vmfs_SOURCES = main.c
partclone_vmfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
partclone_vmfs5_SOURCES = main.c
partclone_vmfs5_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
partclone_fstype_SOURCES = fstype.c
partclone_fstype_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_fstype_LDADD = -lvmfs -luuid $(LDADD)
endif

if ENABLE_JFS
sbin_PROGRAMS += partclone.jfs
partclone_jfs_SOURCES = main.c
partclone_jfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_BTRFS
sbin_PROGRAMS += partclone.btrfs
partclone_btrfs_SOURCES = main.c
partclone_btrfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_MINIX
sbin_PROGRAMS += partclone.minix
partclone_minix_SOURCES = main.c
partclone_minix_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_XFS
sbin_PROGRAMS += partclone.xfs
partclone_xfs_SOURCES = main.c
partclone_xfs_LDADD = $(PCL_LIBPARTCLONE) $(LDADD)
endif

if ENABLE_FUSE
sbin_PROGRAMS += partclone.imgfuse
partclone_imgfuse_SOURCES = fuseimg.c
partclone_imgfuse_CFLAGS = $(AM_CFLAGS) $(FUSE_CFLAGS)
partclone_imgfuse_LDADD = $(PCL_LIBPARTCLONE) $(FUSE_LIBS) $(LDADD)
if ENABLE_STATIC
partclone_imgfuse_LDADD += -ldl
endif
endif

## ---------------------------------------------------------------------------
## Installation extras
## ---------------------------------------------------------------------------

if ENABLE_BASH_COMPLETION
dist_bashcomp_DATA = partclone-completion

install-data-hook:
	mv $(DESTDIR)$(bashcompdir)/partclone-completion $(DESTDIR)$(bashcompdir)/partclone

uninstall-hook:
	rm -f $(DESTDIR)$(bashcompdir)/partclone
endif

# Command name aliases (partclone.ext2, partclone.vfat, ...)
install-exec-hook:
if ENABLE_EXTFS
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext2
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext3
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext4
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext4dev
endif
if ENABLE_HFSP
	$(LN_S) -f partclone.hfsp $(DESTDIR)$(sbindir)/partclone.hfs+
	$(LN_S) -f partclone.hfsp $(DESTDIR)$(sbindir)/partclone.hfsplus
endif
if ENABLE_NTFS
	$(LN_S) -f partclone.ntfsfixboot $(DESTDIR)$(sbindir)/partclone.ntfsreloc
endif
if ENABLE_FAT
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.fat12
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.fat16
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.fat32
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.vfat
endif
if ENABLE_VMFS
	$(LN_S) -f partclone.vmfs $(DESTDIR)$(sbindir)/partclone.VMFS_volume_member
	$(LN_S) -f partclone.vmfs $(DESTDIR)$(sbindir)/partclone.vmfs3
#	$(LN_S) -f partclone.vmfs $(DESTDIR)$(sbindir)/partclone.vmfs5
endif

uninstall-local:
if ENABLE_EXTFS
	$(RM) -f $(sbindir)/partclone.ext4dev
	$(RM) -f $(sbindir)/partclone.ext4
	$(RM) -f $(sbindir)/partclone.ext3
	$(RM) -f $(sbindir)/partclone.ext2
endif
if ENABLE_HFSP
	$(RM) -f $(sbindir)/partclone.hfs+
	$(RM) -f $(sbindir)/partclone.hfsplus
endif
if ENABLE_FAT
	$(RM) -f $(sbindir)/partclone.fat12
	$(RM) -f $(sbindir)/partclone.fat16
	$(RM) -f $(sbindir)/partclone.fat32
	$(RM) -f $(sbindir)/partclone.vfat
endif
if ENABLE_NTFS
	$(RM) -f $(sbindir)/partclone.ntfsreloc
endif

if ENABLE_VMFS
	$(RM) -f $(sbindir)/partclone.VMFS_volume_member
endif

MAINTAINERCLEANFILES = Makefile.in

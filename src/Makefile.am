## ---------------------------------------------------------------------------
## libpartclone
##
## Every enabled filesystem backend is compiled into one shared library and
## selected at runtime (see src/ptc-fs-backend.c).  The command line tools are
## thin front ends that link against libpartclone.la.
## ---------------------------------------------------------------------------

if ENABLE_XXHASH
PCL_XXHASH_LIBS = $(XXHASH_LIBS)
PCL_XXHASH_CFLAGS = $(XXHASH_CFLAGS)
else
PCL_XXHASH_LIBS =
PCL_XXHASH_CFLAGS =
endif

AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" -D_FILE_OFFSET_BITS=64 $(PCL_XXHASH_CFLAGS) $(ISAL_CFLAGS) $(GLIB_CFLAGS)
LDADD = $(LIBINTL) $(PCL_XXHASH_LIBS)
LIBS += $(ISAL_LIBS)
sbin_PROGRAMS=partclone.info partclone.dd partclone.restore partclone.chkimg partclone.imager
TOOLBOX = srcdir=$(top_srcdir) builddir=$(top_builddir) $(top_srcdir)/toolbox


BTRFS_SOURCE=btrfs/crypto/blake2b-ref.c btrfs/crypto/crc32c.c btrfs/crypto/hash.c btrfs/crypto/sha224-256.c btrfs/kernel-lib/list_sort.c btrfs/kernel-lib/raid56.c btrfs/kernel-lib/rbtree.c btrfs/kernel-lib/tables.c btrfs/kernel-lib/bitmap.h btrfs/kernel-shared/ulist.c btrfs/kernel-shared/zoned.c btrfs/kernel-shared/extent-tree.c btrfs/kernel-shared/ctree.c btrfs/kernel-shared/volumes.c btrfs/kernel-shared/disk-io.c btrfs/kernel-shared/delayed-ref.c btrfs/kernel-shared/dir-item.c  btrfs/kernel-shared/extent_io.c btrfs/kernel-shared/file-item.c btrfs/kernel-shared/free-space-cache.c btrfs/kernel-shared/free-space-tree.c btrfs/kernel-shared/inode-item.c btrfs/kernel-shared/print-tree.c btrfs/kernel-shared/root-tree.c btrfs/kernel-shared/transaction.c  btrfs/kernel-shared/uuid-tree.c btrfs/kernel-shared/compression.h btrfs/kernel-shared/accessors.c btrfs/kernel-shared/extent-io-tree.c btrfs/kernel-shared/locking.c btrfs/kernel-shared/messages.c btrfs/kernel-shared/async-thread.c btrfs/kernel-shared/tree-checker.c btrfs/libbtrfsutil/stubs.c btrfs/libbtrfsutil/subvolume.c btrfs/libbtrfsutil/qgroup.c btrfs/libbtrfs/send-stream.c btrfs/libbtrfs/ioctl.h btrfs/libbtrfs/kerncompat.h btrfs/common/device-scan.c btrfs/common/messages.c btrfs/common/path-utils.c btrfs/common/rbtree-utils.c btrfs/common/utils.c btrfs/common/device-utils.c btrfs/common/fsfeatures.c  btrfs/common/units.c btrfs/common/open-utils.c btrfs/common/parse-utils.c btrfs/common/extent-cache.c btrfs/common/string-utils.c btrfs/common/filesystem-utils.c btrfs/common/cpu-utils.c btrfs/common/sysfs-utils.c  btrfs/check/repair.c
if X64
BTRFS_SOURCE+=btrfs/crypto/crc32c-pcl-intel-asm_64.S
endif

XFS_LIBFROG=xfs/libfrog/radix-tree.c xfs/libfrog/list_sort.c xfs/libfrog/crc32.c xfs/libfrog/linux.c xfs/libfrog/util.c xfs/libfrog/bitmap.c xfs/libfrog/avl64.c
XFS_SOURCE=xfs/libxfs/buf_mem.c xfs/libxfs/cache.c xfs/libxfs/defer_item.c xfs/libxfs/init.c xfs/libxfs/inode.c xfs/libxfs/iunlink.c xfs/libxfs/kmem.c xfs/libxfs/listxattr.c xfs/libxfs/logitem.c xfs/libxfs/rdwr.c xfs/libxfs/topology.c xfs/libxfs/trans.c xfs/libxfs/util.c xfs/libxfs/xfblob.c xfs/libxfs/xfile.c xfs/libxfs/xfs_ag.c xfs/libxfs/xfs_ag_resv.c xfs/libxfs/xfs_alloc.c xfs/libxfs/xfs_alloc_btree.c xfs/libxfs/xfs_attr.c xfs/libxfs/xfs_attr_leaf.c xfs/libxfs/xfs_attr_remote.c xfs/libxfs/xfs_bit.c xfs/libxfs/xfs_bmap.c xfs/libxfs/xfs_bmap_btree.c xfs/libxfs/xfs_btree.c xfs/libxfs/xfs_btree_mem.c xfs/libxfs/xfs_btree_staging.c xfs/libxfs/xfs_da_btree.c xfs/libxfs/xfs_defer.c xfs/libxfs/xfs_dir2.c xfs/libxfs/xfs_dir2_block.c xfs/libxfs/xfs_dir2_data.c xfs/libxfs/xfs_dir2_leaf.c xfs/libxfs/xfs_dir2_node.c xfs/libxfs/xfs_dir2_sf.c xfs/libxfs/xfs_dquot_buf.c xfs/libxfs/xfs_exchmaps.c xfs/libxfs/xfs_group.c xfs/libxfs/xfs_ialloc.c xfs/libxfs/xfs_iext_tree.c xfs/libxfs/xfs_inode_buf.c xfs/libxfs/xfs_inode_fork.c xfs/libxfs/xfs_inode_util.c xfs/libxfs/xfs_ialloc_btree.c xfs/libxfs/xfs_metadir.c xfs/libxfs/xfs_metafile.c xfs/libxfs/xfs_log_rlimit.c xfs/libxfs/xfs_parent.c xfs/libxfs/xfs_refcount.c xfs/libxfs/xfs_refcount_btree.c xfs/libxfs/xfs_rmap.c xfs/libxfs/xfs_rmap_btree.c xfs/libxfs/xfs_rtbitmap.c xfs/libxfs/xfs_rtgroup.c xfs/libxfs/xfs_sb.c xfs/libxfs/xfs_symlink_remote.c xfs/libxfs/xfs_trans_inode.c xfs/libxfs/xfs_trans_resv.c xfs/libxfs/xfs_trans_space.c xfs/libxfs/xfs_types.c $(XFS_LIBFROG)

EXFATFS_SOURCE=exfat/cluster.c exfat/utf.c exfat/utils.c exfat/lookup.c exfat/io.c exfat/log.c exfat/node.c exfat/mount.c exfat/time.c

F2FS_SOURCE=f2fs/fsck.c f2fs/fsck.h f2fs/f2fs.h f2fs/mount.c f2fs/mkquota.c f2fs/quotaio.c f2fs/dump.c f2fs/dir.c f2fs/quotaio_v2.c f2fs/node.c f2fs/segment.c f2fs/xattr.c  f2fs/dict.c f2fs/quotaio_tree.c f2fs/libf2fs.c f2fs/libf2fs_io.c f2fs/libf2fs_zoned.c f2fs/nls_utf8.c

CRYPTO_DEPS = $(OPENSSL_LIBS) $(ZLIB_LIBS) $(ZSTD_LIBS)

if ENABLE_STATIC
AM_LDFLAGS=-all-static
LDADD_static=-ldl
endif

if ENABLE_TINFO
LIBS+=-ltinfo
endif

AM_CFLAGS=-D_FILE_OFFSET_BITS=64

if ENABLE_MEMTRACE
AM_CFLAGS+=-DMEMTRACE -DMEMWATCH -DMW_STDIO
endif

if ENABLE_NCURSESW
AM_CFLAGS+=-I/usr/include/ncursesw
endif

version.h: FORCE

## ---------------------------------------------------------------------------
## The shared library
## ---------------------------------------------------------------------------

lib_LTLIBRARIES = libpartclone.la

PTC_LIB_SOURCES = \
	partclone.c partclone.h \
	progress.c progress.h \
	json_status.c json_status.h \
	checksum.c checksum.h \
	torrent_helper.c torrent_helper.h \
	engine.c engine.h \
	ptc-fs-backend.c ptc-fs-backend.h \
	ptc-client.c ptc-client.h \
	libpartclone.h \
	gettext.h bitmap.h fs_common.h

libpartclone_la_SOURCES = $(PTC_LIB_SOURCES)

# Filesystem backends, convenience libraries and external dependencies are
# accumulated by the per-filesystem blocks below.
PTC_BACKEND_LIBS = libptc_raw.la
PTC_FS_DEFINES = -DPTC_HAVE_DD

libpartclone_la_CFLAGS = $(AM_CFLAGS) $(GLIB_CFLAGS) $(BLKID_CFLAGS) $(PTC_FS_DEFINES)
libpartclone_la_LDFLAGS = -version-info 0:0:0 -no-undefined
libpartclone_la_LIBADD = \
	$(PTC_BACKEND_LIBS) \
	$(GLIB_LIBS) $(BLKID_LIBS) $(CRYPTO_DEPS) $(PCL_XXHASH_LIBS) \
	$(LIBINTL) $(URCU_LIBS) -lpthread

# Public, self contained headers and pkg-config file.
partcloneincludedir = $(includedir)/partclone
partcloneinclude_HEADERS = libpartclone.h ptc-client.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = partclone.pc
EXTRA_DIST = partclone.pc.in

# The raw (dd) backend is always available.
noinst_LTLIBRARIES = libptc_raw.la
libptc_raw_la_SOURCES = ddclone.c ddclone.h
libptc_raw_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=dd_read_super_blocks \
	-Dread_bitmap=dd_read_bitmap

## ---------------------------------------------------------------------------
## Command line front ends
## ---------------------------------------------------------------------------

partclone_info_SOURCES = info.c
partclone_info_LDADD = libpartclone.la

partclone_dd_SOURCES = main.c
partclone_dd_LDADD = libpartclone.la

partclone_restore_SOURCES = main.c
partclone_restore_LDADD = libpartclone.la

partclone_chkimg_SOURCES = main.c
partclone_chkimg_LDADD = libpartclone.la

partclone_imager_SOURCES = main.c
partclone_imager_LDADD = libpartclone.la

## ntfsfixboot is a standalone, libc only helper.
sbin_PROGRAMS += partclone.ntfsfixboot
partclone_ntfsfixboot_SOURCES = ntfsfixboot.c

## ---------------------------------------------------------------------------
## Filesystem backends
## ---------------------------------------------------------------------------

if ENABLE_EXTFS
noinst_LTLIBRARIES += libptc_extfs.la
libptc_extfs_la_SOURCES = extfsclone.c extfsclone.h
libptc_extfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=extfs_read_super_blocks \
	-Dread_bitmap=extfs_read_bitmap
PTC_BACKEND_LIBS += libptc_extfs.la -lext2fs -lcom_err
PTC_FS_DEFINES += -DPTC_HAVE_EXTFS

sbin_PROGRAMS += partclone.extfs
partclone_extfs_SOURCES = main.c
partclone_extfs_LDADD = libpartclone.la
endif

if ENABLE_REISERFS
noinst_LTLIBRARIES += libptc_reiserfs.la
libptc_reiserfs_la_SOURCES = reiserfsclone.c reiserfsclone.h
libptc_reiserfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=reiserfs_read_super_blocks \
	-Dread_bitmap=reiserfs_read_bitmap
PTC_BACKEND_LIBS += libptc_reiserfs.la -lreiserfs -ldal
PTC_FS_DEFINES += -DPTC_HAVE_REISERFS

sbin_PROGRAMS += partclone.reiserfs
partclone_reiserfs_SOURCES = main.c
partclone_reiserfs_LDADD = libpartclone.la
endif

if ENABLE_REISER4
noinst_LTLIBRARIES += libptc_reiser4.la
libptc_reiser4_la_SOURCES = reiser4clone.c reiser4clone.h
libptc_reiser4_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=reiser4_read_super_blocks \
	-Dread_bitmap=reiser4_read_bitmap
PTC_BACKEND_LIBS += libptc_reiser4.la -lreiser4 -laal
PTC_FS_DEFINES += -DPTC_HAVE_REISER4

sbin_PROGRAMS += partclone.reiser4
partclone_reiser4_SOURCES = main.c
partclone_reiser4_LDADD = libpartclone.la
endif

if ENABLE_HFSP
noinst_LTLIBRARIES += libptc_hfsp.la
libptc_hfsp_la_SOURCES = hfsplusclone.c hfsplusclone.h
libptc_hfsp_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=hfsp_read_super_blocks \
	-Dread_bitmap=hfsp_read_bitmap \
	-Dpost_clone_fixup=hfsp_post_clone_fixup
PTC_BACKEND_LIBS += libptc_hfsp.la
PTC_FS_DEFINES += -DPTC_HAVE_HFSP

sbin_PROGRAMS += partclone.hfsp
partclone_hfsp_SOURCES = main.c
partclone_hfsp_LDADD = libpartclone.la
endif

if ENABLE_APFS
noinst_LTLIBRARIES += libptc_apfs.la
libptc_apfs_la_SOURCES = apfsclone.c apfsclone.h
libptc_apfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=apfs_read_super_blocks \
	-Dread_bitmap=apfs_read_bitmap
PTC_BACKEND_LIBS += libptc_apfs.la
PTC_FS_DEFINES += -DPTC_HAVE_APFS

sbin_PROGRAMS += partclone.apfs
partclone_apfs_SOURCES = main.c
partclone_apfs_LDADD = libpartclone.la
endif

if ENABLE_FAT
noinst_LTLIBRARIES += libptc_fat.la
libptc_fat_la_SOURCES = fatclone.c fatclone.h
libptc_fat_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=fat_read_super_blocks \
	-Dread_bitmap=fat_read_bitmap
PTC_BACKEND_LIBS += libptc_fat.la
PTC_FS_DEFINES += -DPTC_HAVE_FAT

sbin_PROGRAMS += partclone.fat
partclone_fat_SOURCES = main.c
partclone_fat_LDADD = libpartclone.la
endif

if ENABLE_EXFAT
noinst_LTLIBRARIES += libptc_exfat.la
libptc_exfat_la_SOURCES = exfatclone.c exfatclone.h $(EXFATFS_SOURCE)
libptc_exfat_la_CFLAGS = $(AM_CFLAGS) -DEXFAT -D_GNU_SOURCE -std=c99 \
	-Dread_super_blocks=exfat_read_super_blocks \
	-Dread_bitmap=exfat_read_bitmap \
	-Dutf16_to_utf8=exfat_utf16_to_utf8 \
	-Dutf8_to_utf16=exfat_utf8_to_utf16
PTC_BACKEND_LIBS += libptc_exfat.la
PTC_FS_DEFINES += -DPTC_HAVE_EXFAT

sbin_PROGRAMS += partclone.exfat
partclone_exfat_SOURCES = main.c
partclone_exfat_LDADD = libpartclone.la
endif

if ENABLE_F2FS
noinst_LTLIBRARIES += libptc_f2fs.la
libptc_f2fs_la_SOURCES = f2fsclone.c f2fsclone.h $(F2FS_SOURCE)
libptc_f2fs_la_CFLAGS = $(AM_CFLAGS) -DF2FS -DHAVE_MNTENT_H=1 -DHAVE_SYS_MOUNT_H=1 -DHAVE_SYS_SYSMACROS_H=1 -DHAVE_SYS_UTSNAME_H=1 -DHAVE_SCSI_SG_H=1  -DHAVE_SYS_IOCTL_H=1 -DHAVE_LINUX_HDREG_H  -DHAVE_LINUX_LIMITS_H=1 -DHAVE_LINUX_TYPES_H=1 -DHAVE_LSEEK64=1 -DHAVE_SETMNTENT=1  -I$(srcdir)/f2fs/ \
	-Dread_super_blocks=f2fs_read_super_blocks \
	-Dread_bitmap=f2fs_read_bitmap \
	-Dutf16_to_utf8=f2fs_utf16_to_utf8 \
	-Dutf8_to_utf16=f2fs_utf8_to_utf16
PTC_BACKEND_LIBS += libptc_f2fs.la -luuid
PTC_FS_DEFINES += -DPTC_HAVE_F2FS

sbin_PROGRAMS += partclone.f2fs
partclone_f2fs_SOURCES = main.c
partclone_f2fs_LDADD = libpartclone.la
endif

if ENABLE_NILFS2
noinst_LTLIBRARIES += libptc_nilfs2.la
libptc_nilfs2_la_SOURCES = nilfsclone.c nilfsclone.h
libptc_nilfs2_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=nilfs2_read_super_blocks \
	-Dread_bitmap=nilfs2_read_bitmap
PTC_BACKEND_LIBS += libptc_nilfs2.la -lnilfs
PTC_FS_DEFINES += -DPTC_HAVE_NILFS2

sbin_PROGRAMS += partclone.nilfs2
partclone_nilfs2_SOURCES = main.c
partclone_nilfs2_LDADD = libpartclone.la
endif

if ENABLE_NTFS
noinst_LTLIBRARIES += libptc_ntfs.la
libptc_ntfs_la_SOURCES = ntfsclone-ng.c ntfsclone-ng.h
if ENABLE_NTFS_3G
libptc_ntfs_la_CFLAGS = $(AM_CFLAGS) -DNTFS3G \
	-Dread_super_blocks=ntfs_read_super_blocks \
	-Dread_bitmap=ntfs_read_bitmap
PTC_BACKEND_LIBS += libptc_ntfs.la -lntfs-3g
else
libptc_ntfs_la_CFLAGS = $(AM_CFLAGS) -DNTFS \
	-Dread_super_blocks=ntfs_read_super_blocks \
	-Dread_bitmap=ntfs_read_bitmap
PTC_BACKEND_LIBS += libptc_ntfs.la -lntfs
endif
PTC_FS_DEFINES += -DPTC_HAVE_NTFS

sbin_PROGRAMS += partclone.ntfs
partclone_ntfs_SOURCES = main.c
partclone_ntfs_LDADD = libpartclone.la
endif

if ENABLE_UFS
noinst_LTLIBRARIES += libptc_ufs.la
libptc_ufs_la_SOURCES = ufsclone.c ufsclone.h
libptc_ufs_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE \
	-Dread_super_blocks=ufs_read_super_blocks \
	-Dread_bitmap=ufs_read_bitmap
PTC_BACKEND_LIBS += libptc_ufs.la -lufs -lbsd
PTC_FS_DEFINES += -DPTC_HAVE_UFS

sbin_PROGRAMS += partclone.ufs
partclone_ufs_SOURCES = main.c
partclone_ufs_LDADD = libpartclone.la
endif

if ENABLE_VMFS
noinst_LTLIBRARIES += libptc_vmfs.la libptc_vmfs5.la
libptc_vmfs_la_SOURCES = vmfsclone.c vmfsclone.h
libptc_vmfs_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(UUID_CFLAGS) \
	-Dread_super_blocks=vmfs_read_super_blocks \
	-Dread_bitmap=vmfs_read_bitmap
libptc_vmfs5_la_SOURCES = vmfs5clone.c vmfsclone.h
libptc_vmfs5_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(UUID_CFLAGS) \
	-Dread_super_blocks=vmfs5_read_super_blocks \
	-Dread_bitmap=vmfs5_read_bitmap
PTC_BACKEND_LIBS += libptc_vmfs.la libptc_vmfs5.la -lvmfs -luuid
PTC_FS_DEFINES += -DPTC_HAVE_VMFS -DPTC_HAVE_VMFS5

sbin_PROGRAMS += partclone.vmfs partclone.vmfs5 partclone.fstype
partclone_vmfs_SOURCES = main.c
partclone_vmfs_LDADD = libpartclone.la
partclone_vmfs5_SOURCES = main.c
partclone_vmfs5_LDADD = libpartclone.la
partclone_fstype_SOURCES = fstype.c
partclone_fstype_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_fstype_LDADD = -lvmfs -luuid $(LDADD)
endif

if ENABLE_JFS
noinst_LTLIBRARIES += libptc_jfs.la
libptc_jfs_la_SOURCES = jfsclone.c jfsclone.h
libptc_jfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=jfs_read_super_blocks \
	-Dread_bitmap=jfs_read_bitmap
PTC_BACKEND_LIBS += libptc_jfs.la -ljfs -luuid
PTC_FS_DEFINES += -DPTC_HAVE_JFS

sbin_PROGRAMS += partclone.jfs
partclone_jfs_SOURCES = main.c
partclone_jfs_LDADD = libpartclone.la
endif

if ENABLE_BTRFS
noinst_LTLIBRARIES += libptc_btrfs.la
libptc_btrfs_la_SOURCES = btrfsclone.c btrfsclone.h btrfs_static_fixes.c $(BTRFS_SOURCE)
libptc_btrfs_la_CFLAGS = $(AM_CFLAGS) -std=gnu11 -DBTRFS -DBTRFS_FLAT_INCLUDES -D_XOPEN_SOURCE=700 -D_GNU_SOURCE -DCRYPTOPROVIDER_BUILTIN=1 -I$(srcdir)/btrfs -I$(srcdir)/btrfs/include -I$(srcdir)/btrfs/libbtrfsutil \
	-Dread_super_blocks=btrfs_read_super_blocks \
	-Dread_bitmap=btrfs_read_bitmap \
	-Dcrc32c_le=btrfs_crc32c_le \
	-Dlist_sort=btrfs_list_sort
if ENABLE_STATIC
libptc_btrfs_la_CFLAGS += -Dparse_range=btrfs_parse_range
endif
PTC_BACKEND_LIBS += libptc_btrfs.la -luuid -lblkid
PTC_FS_DEFINES += -DPTC_HAVE_BTRFS

sbin_PROGRAMS += partclone.btrfs
partclone_btrfs_SOURCES = main.c
partclone_btrfs_LDADD = libpartclone.la
endif

if ENABLE_MINIX
noinst_LTLIBRARIES += libptc_minix.la
libptc_minix_la_SOURCES = minixclone.c minixclone.h
libptc_minix_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=minix_read_super_blocks \
	-Dread_bitmap=minix_read_bitmap
PTC_BACKEND_LIBS += libptc_minix.la
PTC_FS_DEFINES += -DPTC_HAVE_MINIX

sbin_PROGRAMS += partclone.minix
partclone_minix_SOURCES = main.c
partclone_minix_LDADD = libpartclone.la
endif

if ENABLE_XFS
noinst_LTLIBRARIES += libptc_xfs.la
libptc_xfs_la_SOURCES = xfsclone.c xfsclone.h $(XFS_SOURCE)
libptc_xfs_la_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/xfs/include -I$(srcdir)/xfs/libxfs/ -I$(srcdir)/xfs/include/xfs/ -I$(srcdir)/xfs/ -DXFS -D_GNU_SOURCE -DNDEBUG $(UUID_CFLAGS) $(URCU_CFLAGS) \
	-Dread_super_blocks=xfs_read_super_blocks \
	-Dread_bitmap=xfs_read_bitmap \
	-Dcrc32c_le=xfs_crc32c_le \
	-Dlist_sort=xfs_list_sort
PTC_BACKEND_LIBS += libptc_xfs.la -lrt -luuid -lblkid
PTC_FS_DEFINES += -DPTC_HAVE_XFS

sbin_PROGRAMS += partclone.xfs
partclone_xfs_SOURCES = main.c
partclone_xfs_LDADD = libpartclone.la
endif

if ENABLE_FUSE
sbin_PROGRAMS += partclone.imgfuse
partclone_imgfuse_SOURCES = fuseimg.c
partclone_imgfuse_CFLAGS = $(AM_CFLAGS) $(FUSE_CFLAGS)
partclone_imgfuse_LDADD = libpartclone.la $(FUSE_LIBS)
if ENABLE_STATIC
partclone_imgfuse_LDADD += -ldl
endif
endif


if ENABLE_BASH_COMPLETION
dist_bashcomp_DATA = partclone-completion

install-data-hook:
	mv $(DESTDIR)$(bashcompdir)/partclone-completion $(DESTDIR)$(bashcompdir)/partclone

uninstall-hook:
	rm -f $(DESTDIR)$(bashcompdir)/partclone
endif



# Extra
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

distclean-local:
	-rm -rf .deps f2fs/.deps btrfs/libbtrfs/.deps btrfs/kernel-lib/.deps btrfs/crypto/.deps btrfs/kernel-shared/.deps btrfs/libbtrfsutil/.deps btrfs/common/.deps btrfs/check/.deps xfs/libxfs/.deps xfs/libfrog/.deps exfat/.deps

MAINTAINERCLEANFILES = Makefile.in

FORCE:

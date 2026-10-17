## ---------------------------------------------------------------------------
## libpartclone
##
## libpartclone.so contains the core engine only.  Every filesystem backend is
## built as its own loadable module (libpartclone_<name>.so) and discovered at
## runtime (see ptc-fs-backend.c).  The command line tools in ../src link
## against the core library.
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
## Filesystem backend modules
##
## Each backend is built in two parts:
##   * a convenience library (libptc_<name>.la) holding the implementation,
##     compiled with the historical entry points renamed to unique symbols;
##   * a loadable module (libpartclone_<name>.la) that wraps it with a
##     ptc_fs_backend descriptor and is installed into $(partclone_plugindir).
## ---------------------------------------------------------------------------

partclone_plugindir = $(libdir)/partclone

# Common link flags for a loadable module.
PTC_MODULE_LDFLAGS = -module -avoid-version -no-undefined
# Everything a module needs from the core library.
PTC_MODULE_LIBS = libpartclone.la $(GLIB_LIBS) -lpthread

## ---------------------------------------------------------------------------
## The core library
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

libpartclone_la_CFLAGS = $(AM_CFLAGS) $(GLIB_CFLAGS) $(BLKID_CFLAGS) \
	-DPTC_PLUGIN_DIR=\"$(partclone_plugindir)\"
libpartclone_la_LDFLAGS = -version-info 0:0:0 -no-undefined
libpartclone_la_LIBADD = \
	$(GLIB_LIBS) $(BLKID_LIBS) $(CRYPTO_DEPS) $(PCL_XXHASH_LIBS) \
	$(LIBINTL) -lgmodule-2.0 -lpthread

# Public, self contained headers and pkg-config file.
partcloneincludedir = $(includedir)/partclone
partcloneinclude_HEADERS = libpartclone.h ptc-client.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = partclone.pc
EXTRA_DIST = partclone.pc.in

## ---------------------------------------------------------------------------
## Filesystem backends
## ---------------------------------------------------------------------------

partclone_plugin_LTLIBRARIES = libpartclone_raw.la

# The raw (dd) backend is always available.
noinst_LTLIBRARIES = libptc_raw.la
libptc_raw_la_SOURCES = ddclone.c ddclone.h
libptc_raw_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=dd_read_super_blocks \
	-Dread_bitmap=dd_read_bitmap
libpartclone_raw_la_SOURCES = backend-module.c
libpartclone_raw_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"raw\" \
	-DPTC_MODULE_MAGIC=raw_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"Raw / device to device (dd)"' \
	-DPTC_MODULE_READ_SUPER=dd_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=dd_read_bitmap
libpartclone_raw_la_LIBADD = libptc_raw.la $(PTC_MODULE_LIBS)
libpartclone_raw_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)

if ENABLE_EXTFS
noinst_LTLIBRARIES += libptc_extfs.la
libptc_extfs_la_SOURCES = extfsclone.c extfsclone.h
libptc_extfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=extfs_read_super_blocks \
	-Dread_bitmap=extfs_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_extfs.la
libpartclone_extfs_la_SOURCES = backend-module.c
libpartclone_extfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"extfs\" \
	-DPTC_MODULE_MAGIC=extfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"ext2/ext3/ext4"' \
	-DPTC_MODULE_READ_SUPER=extfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=extfs_read_bitmap
libpartclone_extfs_la_LIBADD = libptc_extfs.la $(PTC_MODULE_LIBS) -lext2fs -lcom_err
libpartclone_extfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_REISERFS
noinst_LTLIBRARIES += libptc_reiserfs.la
libptc_reiserfs_la_SOURCES = reiserfsclone.c reiserfsclone.h
libptc_reiserfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=reiserfs_read_super_blocks \
	-Dread_bitmap=reiserfs_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_reiserfs.la
libpartclone_reiserfs_la_SOURCES = backend-module.c
libpartclone_reiserfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"reiserfs\" \
	-DPTC_MODULE_MAGIC=reiserfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"ReiserFS 3.6"' \
	-DPTC_MODULE_READ_SUPER=reiserfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=reiserfs_read_bitmap
libpartclone_reiserfs_la_LIBADD = libptc_reiserfs.la $(PTC_MODULE_LIBS) -lreiserfs -ldal
libpartclone_reiserfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_REISER4
noinst_LTLIBRARIES += libptc_reiser4.la
libptc_reiser4_la_SOURCES = reiser4clone.c reiser4clone.h
libptc_reiser4_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=reiser4_read_super_blocks \
	-Dread_bitmap=reiser4_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_reiser4.la
libpartclone_reiser4_la_SOURCES = backend-module.c
libpartclone_reiser4_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"reiser4\" \
	-DPTC_MODULE_MAGIC=reiser4_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"Reiser4"' \
	-DPTC_MODULE_READ_SUPER=reiser4_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=reiser4_read_bitmap
libpartclone_reiser4_la_LIBADD = libptc_reiser4.la $(PTC_MODULE_LIBS) -lreiser4 -laal
libpartclone_reiser4_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_HFSP
noinst_LTLIBRARIES += libptc_hfsp.la
libptc_hfsp_la_SOURCES = hfsplusclone.c hfsplusclone.h
libptc_hfsp_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=hfsp_read_super_blocks \
	-Dread_bitmap=hfsp_read_bitmap \
	-Dpost_clone_fixup=hfsp_post_clone_fixup
partclone_plugin_LTLIBRARIES += libpartclone_hfsp.la
libpartclone_hfsp_la_SOURCES = backend-module.c
libpartclone_hfsp_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"hfsp\" \
	-DPTC_MODULE_MAGIC=hfsplus_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"HFS+"' \
	-DPTC_MODULE_READ_SUPER=hfsp_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=hfsp_read_bitmap \
	-DPTC_MODULE_POST_FIXUP=hfsp_post_clone_fixup
libpartclone_hfsp_la_LIBADD = libptc_hfsp.la $(PTC_MODULE_LIBS)
libpartclone_hfsp_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_APFS
noinst_LTLIBRARIES += libptc_apfs.la
libptc_apfs_la_SOURCES = apfsclone.c apfsclone.h
libptc_apfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=apfs_read_super_blocks \
	-Dread_bitmap=apfs_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_apfs.la
libpartclone_apfs_la_SOURCES = backend-module.c
libpartclone_apfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"apfs\" \
	-DPTC_MODULE_MAGIC=apfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"APFS"' \
	-DPTC_MODULE_READ_SUPER=apfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=apfs_read_bitmap
libpartclone_apfs_la_LIBADD = libptc_apfs.la $(PTC_MODULE_LIBS)
libpartclone_apfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_FAT
noinst_LTLIBRARIES += libptc_fat.la
libptc_fat_la_SOURCES = fatclone.c fatclone.h
libptc_fat_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=fat_read_super_blocks \
	-Dread_bitmap=fat_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_fat.la
libpartclone_fat_la_SOURCES = backend-module.c
libpartclone_fat_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"fat\" \
	-DPTC_MODULE_MAGIC=fat_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"FAT12/16/32"' \
	-DPTC_MODULE_READ_SUPER=fat_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=fat_read_bitmap
libpartclone_fat_la_LIBADD = libptc_fat.la $(PTC_MODULE_LIBS)
libpartclone_fat_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_EXFAT
noinst_LTLIBRARIES += libptc_exfat.la
libptc_exfat_la_SOURCES = exfatclone.c exfatclone.h $(EXFATFS_SOURCE)
libptc_exfat_la_CFLAGS = $(AM_CFLAGS) -DEXFAT -D_GNU_SOURCE -std=c99 \
	-Dread_super_blocks=exfat_read_super_blocks \
	-Dread_bitmap=exfat_read_bitmap \
	-Dutf16_to_utf8=exfat_utf16_to_utf8 \
	-Dutf8_to_utf16=exfat_utf8_to_utf16
partclone_plugin_LTLIBRARIES += libpartclone_exfat.la
libpartclone_exfat_la_SOURCES = backend-module.c
libpartclone_exfat_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"exfat\" \
	-DPTC_MODULE_MAGIC=exfat_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"exFAT"' \
	-DPTC_MODULE_READ_SUPER=exfat_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=exfat_read_bitmap
libpartclone_exfat_la_LIBADD = libptc_exfat.la $(PTC_MODULE_LIBS)
libpartclone_exfat_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_F2FS
noinst_LTLIBRARIES += libptc_f2fs.la
libptc_f2fs_la_SOURCES = f2fsclone.c f2fsclone.h $(F2FS_SOURCE)
libptc_f2fs_la_CFLAGS = $(AM_CFLAGS) -DF2FS -DHAVE_MNTENT_H=1 -DHAVE_SYS_MOUNT_H=1 -DHAVE_SYS_SYSMACROS_H=1 -DHAVE_SYS_UTSNAME_H=1 -DHAVE_SCSI_SG_H=1  -DHAVE_SYS_IOCTL_H=1 -DHAVE_LINUX_HDREG_H  -DHAVE_LINUX_LIMITS_H=1 -DHAVE_LINUX_TYPES_H=1 -DHAVE_LSEEK64=1 -DHAVE_SETMNTENT=1  -I$(srcdir)/f2fs/ \
	-Dread_super_blocks=f2fs_read_super_blocks \
	-Dread_bitmap=f2fs_read_bitmap \
	-Dutf16_to_utf8=f2fs_utf16_to_utf8 \
	-Dutf8_to_utf16=f2fs_utf8_to_utf16
partclone_plugin_LTLIBRARIES += libpartclone_f2fs.la
libpartclone_f2fs_la_SOURCES = backend-module.c
libpartclone_f2fs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"f2fs\" \
	-DPTC_MODULE_MAGIC=f2fs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"F2FS"' \
	-DPTC_MODULE_READ_SUPER=f2fs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=f2fs_read_bitmap
libpartclone_f2fs_la_LIBADD = libptc_f2fs.la $(PTC_MODULE_LIBS) -luuid
libpartclone_f2fs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_NILFS2
noinst_LTLIBRARIES += libptc_nilfs2.la
libptc_nilfs2_la_SOURCES = nilfsclone.c nilfsclone.h
libptc_nilfs2_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=nilfs2_read_super_blocks \
	-Dread_bitmap=nilfs2_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_nilfs2.la
libpartclone_nilfs2_la_SOURCES = backend-module.c
libpartclone_nilfs2_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"nilfs2\" \
	-DPTC_MODULE_MAGIC=nilfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"NILFS2"' \
	-DPTC_MODULE_READ_SUPER=nilfs2_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=nilfs2_read_bitmap
libpartclone_nilfs2_la_LIBADD = libptc_nilfs2.la $(PTC_MODULE_LIBS) -lnilfs
libpartclone_nilfs2_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_NTFS
noinst_LTLIBRARIES += libptc_ntfs.la
libptc_ntfs_la_SOURCES = ntfsclone-ng.c ntfsclone-ng.h
if ENABLE_NTFS_3G
libptc_ntfs_la_CFLAGS = $(AM_CFLAGS) -DNTFS3G \
	-Dread_super_blocks=ntfs_read_super_blocks \
	-Dread_bitmap=ntfs_read_bitmap
PTC_NTFS_LIBS = -lntfs-3g
else
libptc_ntfs_la_CFLAGS = $(AM_CFLAGS) -DNTFS \
	-Dread_super_blocks=ntfs_read_super_blocks \
	-Dread_bitmap=ntfs_read_bitmap
PTC_NTFS_LIBS = -lntfs
endif
partclone_plugin_LTLIBRARIES += libpartclone_ntfs.la
libpartclone_ntfs_la_SOURCES = backend-module.c
libpartclone_ntfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"ntfs\" \
	-DPTC_MODULE_MAGIC=ntfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"NTFS"' \
	-DPTC_MODULE_READ_SUPER=ntfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=ntfs_read_bitmap
libpartclone_ntfs_la_LIBADD = libptc_ntfs.la $(PTC_MODULE_LIBS) $(PTC_NTFS_LIBS)
libpartclone_ntfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_UFS
noinst_LTLIBRARIES += libptc_ufs.la
libptc_ufs_la_SOURCES = ufsclone.c ufsclone.h
libptc_ufs_la_CFLAGS = $(AM_CFLAGS) -D_GNU_SOURCE \
	-Dread_super_blocks=ufs_read_super_blocks \
	-Dread_bitmap=ufs_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_ufs.la
libpartclone_ufs_la_SOURCES = backend-module.c
libpartclone_ufs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"ufs\" \
	-DPTC_MODULE_MAGIC=ufs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"UFS1/UFS2"' \
	-DPTC_MODULE_READ_SUPER=ufs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=ufs_read_bitmap
libpartclone_ufs_la_LIBADD = libptc_ufs.la $(PTC_MODULE_LIBS) -lufs -lbsd
libpartclone_ufs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
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
partclone_plugin_LTLIBRARIES += libpartclone_vmfs.la libpartclone_vmfs5.la
libpartclone_vmfs_la_SOURCES = backend-module.c
libpartclone_vmfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"vmfs\" \
	-DPTC_MODULE_MAGIC=vmfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"VMFS"' \
	-DPTC_MODULE_READ_SUPER=vmfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=vmfs_read_bitmap
libpartclone_vmfs_la_LIBADD = libptc_vmfs.la $(PTC_MODULE_LIBS) -lvmfs -luuid
libpartclone_vmfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
libpartclone_vmfs5_la_SOURCES = backend-module.c
libpartclone_vmfs5_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"vmfs5\" \
	-DPTC_MODULE_MAGIC=vmfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"VMFS5"' \
	-DPTC_MODULE_READ_SUPER=vmfs5_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=vmfs5_read_bitmap
libpartclone_vmfs5_la_LIBADD = libptc_vmfs5.la $(PTC_MODULE_LIBS) -lvmfs -luuid
libpartclone_vmfs5_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_JFS
noinst_LTLIBRARIES += libptc_jfs.la
libptc_jfs_la_SOURCES = jfsclone.c jfsclone.h
libptc_jfs_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=jfs_read_super_blocks \
	-Dread_bitmap=jfs_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_jfs.la
libpartclone_jfs_la_SOURCES = backend-module.c
libpartclone_jfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"jfs\" \
	-DPTC_MODULE_MAGIC=jfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"JFS"' \
	-DPTC_MODULE_READ_SUPER=jfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=jfs_read_bitmap
libpartclone_jfs_la_LIBADD = libptc_jfs.la $(PTC_MODULE_LIBS) -ljfs -luuid
libpartclone_jfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
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
partclone_plugin_LTLIBRARIES += libpartclone_btrfs.la
libpartclone_btrfs_la_SOURCES = backend-module.c
libpartclone_btrfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"btrfs\" \
	-DPTC_MODULE_MAGIC=btrfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"BTRFS"' \
	-DPTC_MODULE_READ_SUPER=btrfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=btrfs_read_bitmap
libpartclone_btrfs_la_LIBADD = libptc_btrfs.la $(PTC_MODULE_LIBS) -luuid -lblkid $(CRYPTO_DEPS)
libpartclone_btrfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_MINIX
noinst_LTLIBRARIES += libptc_minix.la
libptc_minix_la_SOURCES = minixclone.c minixclone.h
libptc_minix_la_CFLAGS = $(AM_CFLAGS) \
	-Dread_super_blocks=minix_read_super_blocks \
	-Dread_bitmap=minix_read_bitmap
partclone_plugin_LTLIBRARIES += libpartclone_minix.la
libpartclone_minix_la_SOURCES = backend-module.c
libpartclone_minix_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"minix\" \
	-DPTC_MODULE_MAGIC=minix_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"MINIX"' \
	-DPTC_MODULE_READ_SUPER=minix_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=minix_read_bitmap
libpartclone_minix_la_LIBADD = libptc_minix.la $(PTC_MODULE_LIBS)
libpartclone_minix_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

if ENABLE_XFS
noinst_LTLIBRARIES += libptc_xfs.la
libptc_xfs_la_SOURCES = xfsclone.c xfsclone.h $(XFS_SOURCE)
libptc_xfs_la_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/xfs/include -I$(srcdir)/xfs/libxfs/ -I$(srcdir)/xfs/include/xfs/ -I$(srcdir)/xfs/ -DXFS -D_GNU_SOURCE -DNDEBUG $(UUID_CFLAGS) $(URCU_CFLAGS) \
	-Dread_super_blocks=xfs_read_super_blocks \
	-Dread_bitmap=xfs_read_bitmap \
	-Dcrc32c_le=xfs_crc32c_le \
	-Dlist_sort=xfs_list_sort
partclone_plugin_LTLIBRARIES += libpartclone_xfs.la
libpartclone_xfs_la_SOURCES = backend-module.c
libpartclone_xfs_la_CFLAGS = $(AM_CFLAGS) \
	-DPTC_MODULE_NAME=\"xfs\" \
	-DPTC_MODULE_MAGIC=xfs_MAGIC \
	-DPTC_MODULE_DESCRIPTION='"XFS"' \
	-DPTC_MODULE_READ_SUPER=xfs_read_super_blocks \
	-DPTC_MODULE_READ_BITMAP=xfs_read_bitmap
libpartclone_xfs_la_LIBADD = libptc_xfs.la $(PTC_MODULE_LIBS) -lrt -luuid -lblkid $(URCU_LIBS)
libpartclone_xfs_la_LDFLAGS = $(PTC_MODULE_LDFLAGS)
endif

distclean-local:
	-rm -rf .deps f2fs/.deps btrfs/libbtrfs/.deps btrfs/kernel-lib/.deps btrfs/crypto/.deps btrfs/kernel-shared/.deps btrfs/libbtrfsutil/.deps btrfs/common/.deps btrfs/check/.deps xfs/libxfs/.deps xfs/libfrog/.deps exfat/.deps

MAINTAINERCLEANFILES = Makefile.in

FORCE:

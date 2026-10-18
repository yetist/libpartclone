## ---------------------------------------------------------------------------
## libpartclone
##
## libpartclone.so contains the core engine only.  Every filesystem backend is
## built as its own loadable module (libpartclone_<name>.so) and discovered at
## runtime (see ptc-fs-backend.c).  The command line tools in ../src link
## against the core library.
## ---------------------------------------------------------------------------

## ---------------------------------------------------------------------------
## The core library
## ---------------------------------------------------------------------------
core_sources = files(
  'partclone.c',
  'progress.c',
  'json_status.c',
  'checksum.c',
  'torrent_helper.c',
  'engine.c',
  'diskimg.c',
  'disktable.c',
  'ptc-fs-backend.c',
  'ptc-client.c',
  'ptc-image.c',
)

core_deps = [
  gio_dep,
  openssl_dep,
  zlib_dep,
  zstd_dep,
  gmodule_dep,
  threads_dep,
]
foreach extra : [blkid_dep, fdisk_dep, xxhash_dep, isal_dep, intl_dep, math_dep, ncurses_dep, tinfo_dep]
  if extra.found()
    core_deps += [extra]
  endif
endforeach

core_lib = library('partclone',
  core_sources,
  c_args : '-DPTC_PLUGIN_DIR="@0@"'.format(plugin_dir),
  include_directories : top_inc,
  dependencies : core_deps,
  version : '0.0.0',
  soversion : '0',
  install : true,
)

libpartclone_inc = include_directories('.')

libpartclone_dep = declare_dependency(
  link_with : core_lib,
  dependencies : [gio_dep],
  include_directories : [libpartclone_inc, top_inc],
)

# ---------------------------------------------------------------------------
# Public, self contained headers and pkg-config file
# ---------------------------------------------------------------------------
install_headers('libpartclone.h', 'ptc-client.h', 'ptc-image.h', subdir : 'partclone')

pkg = import('pkgconfig')
pkg.generate(core_lib,
  name : 'partclone',
  description : 'Partition cloning library (libpartclone)',
  requires : [gio_dep],
  subdirs : 'partclone',
)

## ---------------------------------------------------------------------------
## Filesystem backend sources
## ---------------------------------------------------------------------------

btrfs_sources = [
  'btrfsclone.c',
  'btrfs_static_fixes.c',
  'btrfs/crypto/blake2b-ref.c',
  'btrfs/crypto/crc32c.c',
  'btrfs/crypto/hash.c',
  'btrfs/crypto/sha224-256.c',
  'btrfs/kernel-lib/list_sort.c',
  'btrfs/kernel-lib/raid56.c',
  'btrfs/kernel-lib/rbtree.c',
  'btrfs/kernel-lib/tables.c',
  'btrfs/kernel-shared/ulist.c',
  'btrfs/kernel-shared/zoned.c',
  'btrfs/kernel-shared/extent-tree.c',
  'btrfs/kernel-shared/ctree.c',
  'btrfs/kernel-shared/volumes.c',
  'btrfs/kernel-shared/disk-io.c',
  'btrfs/kernel-shared/delayed-ref.c',
  'btrfs/kernel-shared/dir-item.c',
  'btrfs/kernel-shared/extent_io.c',
  'btrfs/kernel-shared/file-item.c',
  'btrfs/kernel-shared/free-space-cache.c',
  'btrfs/kernel-shared/free-space-tree.c',
  'btrfs/kernel-shared/inode-item.c',
  'btrfs/kernel-shared/print-tree.c',
  'btrfs/kernel-shared/root-tree.c',
  'btrfs/kernel-shared/transaction.c',
  'btrfs/kernel-shared/uuid-tree.c',
  'btrfs/kernel-shared/accessors.c',
  'btrfs/kernel-shared/extent-io-tree.c',
  'btrfs/kernel-shared/locking.c',
  'btrfs/kernel-shared/messages.c',
  'btrfs/kernel-shared/async-thread.c',
  'btrfs/kernel-shared/tree-checker.c',
  'btrfs/libbtrfsutil/stubs.c',
  'btrfs/libbtrfsutil/subvolume.c',
  'btrfs/libbtrfsutil/qgroup.c',
  'btrfs/libbtrfs/send-stream.c',
  'btrfs/common/device-scan.c',
  'btrfs/common/messages.c',
  'btrfs/common/path-utils.c',
  'btrfs/common/rbtree-utils.c',
  'btrfs/common/utils.c',
  'btrfs/common/device-utils.c',
  'btrfs/common/fsfeatures.c',
  'btrfs/common/units.c',
  'btrfs/common/open-utils.c',
  'btrfs/common/parse-utils.c',
  'btrfs/common/extent-cache.c',
  'btrfs/common/string-utils.c',
  'btrfs/common/filesystem-utils.c',
  'btrfs/common/cpu-utils.c',
  'btrfs/common/sysfs-utils.c',
  'btrfs/check/repair.c',
]
if host_machine.cpu_family() == 'x86_64'
  btrfs_sources += ['btrfs/crypto/crc32c-pcl-intel-asm_64.S']
endif

xfs_libfrog = [
  'xfs/libfrog/radix-tree.c',
  'xfs/libfrog/list_sort.c',
  'xfs/libfrog/crc32.c',
  'xfs/libfrog/linux.c',
  'xfs/libfrog/util.c',
  'xfs/libfrog/bitmap.c',
  'xfs/libfrog/avl64.c',
]

xfs_libxfs = [
  'xfs/libxfs/buf_mem.c',
  'xfs/libxfs/cache.c',
  'xfs/libxfs/defer_item.c',
  'xfs/libxfs/init.c',
  'xfs/libxfs/inode.c',
  'xfs/libxfs/iunlink.c',
  'xfs/libxfs/kmem.c',
  'xfs/libxfs/listxattr.c',
  'xfs/libxfs/logitem.c',
  'xfs/libxfs/rdwr.c',
  'xfs/libxfs/topology.c',
  'xfs/libxfs/trans.c',
  'xfs/libxfs/util.c',
  'xfs/libxfs/xfblob.c',
  'xfs/libxfs/xfile.c',
  'xfs/libxfs/xfs_ag.c',
  'xfs/libxfs/xfs_ag_resv.c',
  'xfs/libxfs/xfs_alloc.c',
  'xfs/libxfs/xfs_alloc_btree.c',
  'xfs/libxfs/xfs_attr.c',
  'xfs/libxfs/xfs_attr_leaf.c',
  'xfs/libxfs/xfs_attr_remote.c',
  'xfs/libxfs/xfs_bit.c',
  'xfs/libxfs/xfs_bmap.c',
  'xfs/libxfs/xfs_bmap_btree.c',
  'xfs/libxfs/xfs_btree.c',
  'xfs/libxfs/xfs_btree_mem.c',
  'xfs/libxfs/xfs_btree_staging.c',
  'xfs/libxfs/xfs_da_btree.c',
  'xfs/libxfs/xfs_defer.c',
  'xfs/libxfs/xfs_dir2.c',
  'xfs/libxfs/xfs_dir2_block.c',
  'xfs/libxfs/xfs_dir2_data.c',
  'xfs/libxfs/xfs_dir2_leaf.c',
  'xfs/libxfs/xfs_dir2_node.c',
  'xfs/libxfs/xfs_dir2_sf.c',
  'xfs/libxfs/xfs_dquot_buf.c',
  'xfs/libxfs/xfs_exchmaps.c',
  'xfs/libxfs/xfs_group.c',
  'xfs/libxfs/xfs_ialloc.c',
  'xfs/libxfs/xfs_iext_tree.c',
  'xfs/libxfs/xfs_inode_buf.c',
  'xfs/libxfs/xfs_inode_fork.c',
  'xfs/libxfs/xfs_inode_util.c',
  'xfs/libxfs/xfs_ialloc_btree.c',
  'xfs/libxfs/xfs_metadir.c',
  'xfs/libxfs/xfs_metafile.c',
  'xfs/libxfs/xfs_log_rlimit.c',
  'xfs/libxfs/xfs_parent.c',
  'xfs/libxfs/xfs_refcount.c',
  'xfs/libxfs/xfs_refcount_btree.c',
  'xfs/libxfs/xfs_rmap.c',
  'xfs/libxfs/xfs_rmap_btree.c',
  'xfs/libxfs/xfs_rtbitmap.c',
  'xfs/libxfs/xfs_rtgroup.c',
  'xfs/libxfs/xfs_sb.c',
  'xfs/libxfs/xfs_symlink_remote.c',
  'xfs/libxfs/xfs_trans_inode.c',
  'xfs/libxfs/xfs_trans_resv.c',
  'xfs/libxfs/xfs_trans_space.c',
  'xfs/libxfs/xfs_types.c',
]

exfat_sources = [
  'exfat/cluster.c',
  'exfat/utf.c',
  'exfat/utils.c',
  'exfat/lookup.c',
  'exfat/io.c',
  'exfat/log.c',
  'exfat/node.c',
  'exfat/mount.c',
  'exfat/time.c',
]

f2fs_sources = [
  'f2fs/fsck.c',
  'f2fs/mount.c',
  'f2fs/mkquota.c',
  'f2fs/quotaio.c',
  'f2fs/dump.c',
  'f2fs/dir.c',
  'f2fs/quotaio_v2.c',
  'f2fs/node.c',
  'f2fs/segment.c',
  'f2fs/xattr.c',
  'f2fs/dict.c',
  'f2fs/quotaio_tree.c',
  'f2fs/libf2fs.c',
  'f2fs/libf2fs_io.c',
  'f2fs/libf2fs_zoned.c',
  'f2fs/nls_utf8.c',
]

## Cache for include_directories() objects used by several backends.
inc_btrfs = include_directories('btrfs', 'btrfs/include', 'btrfs/libbtrfsutil')
inc_xfs   = include_directories('xfs', 'xfs/include', 'xfs/libxfs', 'xfs/include/xfs')
inc_f2fs  = include_directories('f2fs')

## ---------------------------------------------------------------------------
## Backend descriptors.
##
## Each backend is built in two parts:
##   * a convenience (static) library holding the implementation, compiled with
##     the historical entry points renamed to unique symbols;
##   * a loadable module (libpartclone_<name>.so) that wraps it with a
##     ptc_fs_backend descriptor and is installed into $(libdir)/partclone.
## ---------------------------------------------------------------------------
backend = {}

backend += {'raw' : {
  'sources'     : ['ddclone.c'],
  'renames'     : {'read_super_blocks' : 'dd_read_super_blocks',
                   'read_bitmap' : 'dd_read_bitmap'},
  'name'        : 'raw',
  'magic'       : 'raw_MAGIC',
  'desc'        : 'Raw / device to device (dd)',
  'read_super'  : 'dd_read_super_blocks',
  'read_bitmap' : 'dd_read_bitmap',
}}

backend += {'extfs' : {
  'sources'     : ['extfsclone.c'],
  'renames'     : {'read_super_blocks' : 'extfs_read_super_blocks',
                   'read_bitmap' : 'extfs_read_bitmap'},
  'name'        : 'extfs',
  'magic'       : 'extfs_MAGIC',
  'desc'        : 'ext2/ext3/ext4',
  'read_super'  : 'extfs_read_super_blocks',
  'read_bitmap' : 'extfs_read_bitmap',
}}

backend += {'reiserfs' : {
  'sources'     : ['reiserfsclone.c'],
  'renames'     : {'read_super_blocks' : 'reiserfs_read_super_blocks',
                   'read_bitmap' : 'reiserfs_read_bitmap'},
  'name'        : 'reiserfs',
  'magic'       : 'reiserfs_MAGIC',
  'desc'        : 'ReiserFS 3.6',
  'read_super'  : 'reiserfs_read_super_blocks',
  'read_bitmap' : 'reiserfs_read_bitmap',
}}

backend += {'reiser4' : {
  'sources'     : ['reiser4clone.c'],
  'renames'     : {'read_super_blocks' : 'reiser4_read_super_blocks',
                   'read_bitmap' : 'reiser4_read_bitmap'},
  'name'        : 'reiser4',
  'magic'       : 'reiser4_MAGIC',
  'desc'        : 'Reiser4',
  'read_super'  : 'reiser4_read_super_blocks',
  'read_bitmap' : 'reiser4_read_bitmap',
}}

backend += {'hfsp' : {
  'sources'     : ['hfsplusclone.c'],
  'renames'     : {'read_super_blocks' : 'hfsp_read_super_blocks',
                   'read_bitmap' : 'hfsp_read_bitmap',
                   'post_clone_fixup' : 'hfsp_post_clone_fixup'},
  'name'        : 'hfsp',
  'magic'       : 'hfsplus_MAGIC',
  'desc'        : 'HFS+',
  'read_super'  : 'hfsp_read_super_blocks',
  'read_bitmap' : 'hfsp_read_bitmap',
  'post_fixup'  : 'hfsp_post_clone_fixup',
}}

backend += {'apfs' : {
  'sources'     : ['apfsclone.c'],
  'renames'     : {'read_super_blocks' : 'apfs_read_super_blocks',
                   'read_bitmap' : 'apfs_read_bitmap'},
  'name'        : 'apfs',
  'magic'       : 'apfs_MAGIC',
  'desc'        : 'APFS',
  'read_super'  : 'apfs_read_super_blocks',
  'read_bitmap' : 'apfs_read_bitmap',
}}

backend += {'fat' : {
  'sources'     : ['fatclone.c'],
  'renames'     : {'read_super_blocks' : 'fat_read_super_blocks',
                   'read_bitmap' : 'fat_read_bitmap'},
  'name'        : 'fat',
  'magic'       : 'fat_MAGIC',
  'desc'        : 'FAT12/16/32',
  'read_super'  : 'fat_read_super_blocks',
  'read_bitmap' : 'fat_read_bitmap',
}}

backend += {'exfat' : {
  'sources'     : ['exfatclone.c'] + exfat_sources,
  'renames'     : {'read_super_blocks' : 'exfat_read_super_blocks',
                   'read_bitmap' : 'exfat_read_bitmap',
                   'utf16_to_utf8' : 'exfat_utf16_to_utf8',
                   'utf8_to_utf16' : 'exfat_utf8_to_utf16'},
  'c_args'      : ['-DEXFAT', '-D_GNU_SOURCE', '-std=c99'],
  'name'        : 'exfat',
  'magic'       : 'exfat_MAGIC',
  'desc'        : 'exFAT',
  'read_super'  : 'exfat_read_super_blocks',
  'read_bitmap' : 'exfat_read_bitmap',
}}

backend += {'f2fs' : {
  'sources'     : ['f2fsclone.c'] + f2fs_sources,
  'renames'     : {'read_super_blocks' : 'f2fs_read_super_blocks',
                   'read_bitmap' : 'f2fs_read_bitmap',
                   'utf16_to_utf8' : 'f2fs_utf16_to_utf8',
                   'utf8_to_utf16' : 'f2fs_utf8_to_utf16'},
  'c_args'      : ['-DF2FS',
                   '-DHAVE_MNTENT_H=1', '-DHAVE_SYS_MOUNT_H=1',
                   '-DHAVE_SYS_SYSMACROS_H=1', '-DHAVE_SYS_UTSNAME_H=1',
                   '-DHAVE_SCSI_SG_H=1', '-DHAVE_SYS_IOCTL_H=1',
                   '-DHAVE_LINUX_HDREG_H', '-DHAVE_LINUX_LIMITS_H=1',
                   '-DHAVE_LINUX_TYPES_H=1', '-DHAVE_LSEEK64=1',
                   '-DHAVE_SETMNTENT=1'],
  'incs'        : [inc_f2fs],
  'name'        : 'f2fs',
  'magic'       : 'f2fs_MAGIC',
  'desc'        : 'F2FS',
  'read_super'  : 'f2fs_read_super_blocks',
  'read_bitmap' : 'f2fs_read_bitmap',
}}

backend += {'nilfs2' : {
  'sources'     : ['nilfsclone.c'],
  'renames'     : {'read_super_blocks' : 'nilfs2_read_super_blocks',
                   'read_bitmap' : 'nilfs2_read_bitmap'},
  'name'        : 'nilfs2',
  'magic'       : 'nilfs_MAGIC',
  'desc'        : 'NILFS2',
  'read_super'  : 'nilfs2_read_super_blocks',
  'read_bitmap' : 'nilfs2_read_bitmap',
}}

backend += {'ntfs' : {
  'sources'     : ['ntfsclone-ng.c'],
  'renames'     : {'read_super_blocks' : 'ntfs_read_super_blocks',
                   'read_bitmap' : 'ntfs_read_bitmap'},
  'c_args'      : ntfs_c_args,
  'name'        : 'ntfs',
  'magic'       : 'ntfs_MAGIC',
  'desc'        : 'NTFS',
  'read_super'  : 'ntfs_read_super_blocks',
  'read_bitmap' : 'ntfs_read_bitmap',
}}

backend += {'ufs' : {
  'sources'     : ['ufsclone.c'],
  'renames'     : {'read_super_blocks' : 'ufs_read_super_blocks',
                   'read_bitmap' : 'ufs_read_bitmap'},
  'c_args'      : ['-D_GNU_SOURCE'],
  'name'        : 'ufs',
  'magic'       : 'ufs_MAGIC',
  'desc'        : 'UFS1/UFS2',
  'read_super'  : 'ufs_read_super_blocks',
  'read_bitmap' : 'ufs_read_bitmap',
}}

backend += {'vmfs' : {
  'sources'     : ['vmfsclone.c'],
  'renames'     : {'read_super_blocks' : 'vmfs_read_super_blocks',
                   'read_bitmap' : 'vmfs_read_bitmap'},
  'c_args'      : ['-D_GNU_SOURCE'],
  'name'        : 'vmfs',
  'magic'       : 'vmfs_MAGIC',
  'desc'        : 'VMFS',
  'read_super'  : 'vmfs_read_super_blocks',
  'read_bitmap' : 'vmfs_read_bitmap',
}}

backend += {'vmfs5' : {
  'gate'        : 'vmfs',
  'sources'     : ['vmfs5clone.c'],
  'renames'     : {'read_super_blocks' : 'vmfs5_read_super_blocks',
                   'read_bitmap' : 'vmfs5_read_bitmap'},
  'c_args'      : ['-D_GNU_SOURCE'],
  'name'        : 'vmfs5',
  'magic'       : 'vmfs_MAGIC',
  'desc'        : 'VMFS5',
  'read_super'  : 'vmfs5_read_super_blocks',
  'read_bitmap' : 'vmfs5_read_bitmap',
}}

backend += {'jfs' : {
  'sources'     : ['jfsclone.c'],
  'renames'     : {'read_super_blocks' : 'jfs_read_super_blocks',
                   'read_bitmap' : 'jfs_read_bitmap'},
  'name'        : 'jfs',
  'magic'       : 'jfs_MAGIC',
  'desc'        : 'JFS',
  'read_super'  : 'jfs_read_super_blocks',
  'read_bitmap' : 'jfs_read_bitmap',
}}

backend += {'btrfs' : {
  'sources'     : btrfs_sources,
  'renames'     : {'read_super_blocks' : 'btrfs_read_super_blocks',
                   'read_bitmap' : 'btrfs_read_bitmap',
                   'crc32c_le' : 'btrfs_crc32c_le',
                   'list_sort' : 'btrfs_list_sort'},
  'c_args'      : ['-std=gnu11', '-DBTRFS', '-DBTRFS_FLAT_INCLUDES',
                   '-D_XOPEN_SOURCE=700', '-D_GNU_SOURCE',
                   '-DCRYPTOPROVIDER_BUILTIN=1'],
  'incs'        : [inc_btrfs],
  'name'        : 'btrfs',
  'magic'       : 'btrfs_MAGIC',
  'desc'        : 'BTRFS',
  'read_super'  : 'btrfs_read_super_blocks',
  'read_bitmap' : 'btrfs_read_bitmap',
}}

backend += {'minix' : {
  'sources'     : ['minixclone.c'],
  'renames'     : {'read_super_blocks' : 'minix_read_super_blocks',
                   'read_bitmap' : 'minix_read_bitmap'},
  'name'        : 'minix',
  'magic'       : 'minix_MAGIC',
  'desc'        : 'MINIX',
  'read_super'  : 'minix_read_super_blocks',
  'read_bitmap' : 'minix_read_bitmap',
}}

backend += {'xfs' : {
  'sources'     : ['xfsclone.c'] + xfs_libxfs + xfs_libfrog,
  'renames'     : {'read_super_blocks' : 'xfs_read_super_blocks',
                   'read_bitmap' : 'xfs_read_bitmap',
                   'crc32c_le' : 'xfs_crc32c_le',
                   'list_sort' : 'xfs_list_sort'},
  'c_args'      : ['-DXFS', '-D_GNU_SOURCE', '-DNDEBUG'],
  'incs'        : [inc_xfs],
  'name'        : 'xfs',
  'magic'       : 'xfs_MAGIC',
  'desc'        : 'XFS',
  'read_super'  : 'xfs_read_super_blocks',
  'read_bitmap' : 'xfs_read_bitmap',
}}

foreach name, spec : backend
  gate = spec.get('gate', name)
  if not fs_build.get(gate, false)
    continue
  endif

  conv_args = []
  foreach old, new : spec.get('renames', {})
    conv_args += '-D@0@=@1@'.format(old, new)
  endforeach
  conv_args += spec.get('c_args', [])

  deps = fs_deps.get(gate, [])

  conv = static_library('ptc_' + name,
    files(spec['sources']),
    c_args : conv_args,
    include_directories : [top_inc] + spec.get('incs', []),
    dependencies : deps,
    pic : true,
  )

  mod_args = [
    '-DPTC_MODULE_NAME="@0@"'.format(spec['name']),
    '-DPTC_MODULE_MAGIC=@0@'.format(spec['magic']),
    '-DPTC_MODULE_DESCRIPTION="@0@"'.format(spec['desc']),
    '-DPTC_MODULE_READ_SUPER=@0@'.format(spec['read_super']),
    '-DPTC_MODULE_READ_BITMAP=@0@'.format(spec['read_bitmap']),
  ]
  if spec.has_key('post_fixup')
    mod_args += '-DPTC_MODULE_POST_FIXUP=@0@'.format(spec['post_fixup'])
  endif

  shared_module('partclone_' + name,
    'backend-module.c',
    c_args : mod_args,
    include_directories : top_inc,
    link_with : [conv, core_lib],
    dependencies : [threads_dep] + deps,
    override_options : ['b_lundef=true'],
    install : true,
    install_dir : plugin_dir,
  )
endforeach

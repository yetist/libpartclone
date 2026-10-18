## ---------------------------------------------------------------------------
## Manual pages
##
## Most .8 pages are pre-generated and shipped, so they are installed directly.
## The "corporeal" pages (partclone.8, partclone.chkimg.8, ...) are written in
## the scdoc format (.scd) and rendered to roff with scdoc(1).  Generation is
## best effort: when scdoc is unavailable those pages are simply not built.
## ---------------------------------------------------------------------------
man_mans = ['partclone.imager.8']

if fs_build.get('extfs', false)
  man_mans += [
    'partclone.extfs.8',
    'partclone.ext2.8',
    'partclone.ext3.8',
    'partclone.ext4.8',
    'partclone.ext4dev.8',
  ]
endif

if fs_build.get('apfs', false)
  man_mans += ['partclone.apfs.8']
endif

if fs_build.get('reiserfs', false)
  man_mans += ['partclone.reiserfs.8']
endif

if fs_build.get('reiser4', false)
  man_mans += ['partclone.reiser4.8']
endif

if fs_build.get('hfsp', false)
  man_mans += [
    'partclone.hfsplus.8',
    'partclone.hfs+.8',
    'partclone.hfsp.8',
  ]
endif

if fs_build.get('xfs', false)
  man_mans += ['partclone.xfs.8']
endif

if fs_build.get('f2fs', false)
  man_mans += ['partclone.f2fs.8']
endif

if fs_build.get('nilfs2', false)
  man_mans += ['partclone.nilfs2.8']
endif

if fs_build.get('exfat', false)
  man_mans += ['partclone.exfat.8']
endif

if fs_build.get('fat', false)
  man_mans += [
    'partclone.fat.8',
    'partclone.vfat.8',
    'partclone.fat12.8',
    'partclone.fat16.8',
    'partclone.fat32.8',
  ]
endif

if fs_build.get('ufs', false)
  man_mans += ['partclone.ufs.8']
endif

if fs_build.get('vmfs', false)
  man_mans += [
    'partclone.vmfs.8',
    'partclone.vmfs3.8',
    'partclone.vmfs5.8',
    'partclone.VMFS_volume_member.8',
  ]
endif

if fs_build.get('jfs', false)
  man_mans += ['partclone.jfs.8']
endif

if fs_build.get('btrfs', false)
  man_mans += ['partclone.btrfs.8']
endif

if fs_build.get('ntfs', false)
  man_mans += [
    'partclone.ntfs.8',
    'partclone.ntfsreloc.8',
  ]
endif

if fs_build.get('minix', false)
  man_mans += ['partclone.minix.8']
endif

install_man(man_mans)

## Pages generated from scdoc sources.
corporeal = [
  'partclone',
  'partclone.chkimg',
  'partclone.dd',
  'partclone.info',
  'partclone.restore',
]
if fs_build.get('ntfs', false)
  corporeal += ['partclone.ntfsfixboot']
endif
if fs_build.get('vmfs', false)
  corporeal += ['partclone.fstype']
endif

scdoc = find_program('scdoc', required : false)

if scdoc.found()
  foreach page : corporeal
    custom_target(page + '.8',
      input : page + '.scd',
      output : page + '.8',
      command : [scdoc],
      feed : true,
      capture : true,
      install : true,
      install_dir : get_option('mandir') / 'man8',
    )
  endforeach
else
  warning('scdoc not found: the partclone.8 family of manual pages will not be installed')
endif

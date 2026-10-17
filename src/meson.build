## ---------------------------------------------------------------------------
## partclone command line tools
##
## Thin front ends that link against the shared library built in ../lib.
## ---------------------------------------------------------------------------

## Base tools, always built.
executable('partclone.info', 'info.c',
  dependencies : libpartclone_dep,
  install : true,
  install_dir : sbindir,
)

foreach prog : ['partclone.dd', 'partclone.restore', 'partclone.chkimg', 'partclone.imager']
  executable(prog, 'main.c',
    dependencies : libpartclone_dep,
    install : true,
    install_dir : sbindir,
  )
endforeach

## ntfsfixboot is a standalone, libc only helper.
executable('partclone.ntfsfixboot', 'ntfsfixboot.c',
  include_directories : top_inc,
  install : true,
  install_dir : sbindir,
)

## ---------------------------------------------------------------------------
## One front end per filesystem backend
## ---------------------------------------------------------------------------
fs_programs = {
  'extfs'    : ['partclone.extfs'],
  'reiserfs' : ['partclone.reiserfs'],
  'reiser4'  : ['partclone.reiser4'],
  'hfsp'     : ['partclone.hfsp'],
  'apfs'     : ['partclone.apfs'],
  'fat'      : ['partclone.fat'],
  'exfat'    : ['partclone.exfat'],
  'f2fs'     : ['partclone.f2fs'],
  'nilfs2'   : ['partclone.nilfs2'],
  'ntfs'     : ['partclone.ntfs'],
  'ufs'      : ['partclone.ufs'],
  'vmfs'     : ['partclone.vmfs', 'partclone.vmfs5'],
  'jfs'      : ['partclone.jfs'],
  'btrfs'    : ['partclone.btrfs'],
  'minix'    : ['partclone.minix'],
  'xfs'      : ['partclone.xfs'],
}

foreach fs, programs : fs_programs
  if not fs_build.get(fs, false)
    continue
  endif
  foreach prog : programs
    executable(prog, 'main.c',
      dependencies : libpartclone_dep,
      install : true,
      install_dir : sbindir,
    )
  endforeach
endforeach

if fs_build.get('vmfs', false)
  executable('partclone.fstype', 'fstype.c',
    c_args : '-D_GNU_SOURCE',
    include_directories : top_inc,
    dependencies : fs_deps['vmfs'],
    install : true,
    install_dir : sbindir,
  )
endif

if build_fuse
  executable('partclone.imgfuse', 'fuseimg.c',
    dependencies : [libpartclone_dep, fuse_dep],
    install : true,
    install_dir : sbindir,
  )
endif

## ---------------------------------------------------------------------------
## Command name aliases (partclone.ext2, partclone.vfat, ...)
## ---------------------------------------------------------------------------
aliases = []

if fs_build.get('extfs', false)
  foreach a : ['partclone.ext2', 'partclone.ext3', 'partclone.ext4', 'partclone.ext4dev']
    aliases += [[a, 'partclone.extfs']]
  endforeach
endif

if fs_build.get('hfsp', false)
  aliases += [['partclone.hfs+', 'partclone.hfsp'],
              ['partclone.hfsplus', 'partclone.hfsp']]
endif

if fs_build.get('ntfs', false)
  aliases += [['partclone.ntfsreloc', 'partclone.ntfsfixboot']]
endif

if fs_build.get('fat', false)
  foreach a : ['partclone.fat12', 'partclone.fat16', 'partclone.fat32', 'partclone.vfat']
    aliases += [[a, 'partclone.fat']]
  endforeach
endif

if fs_build.get('vmfs', false)
  aliases += [['partclone.VMFS_volume_member', 'partclone.vmfs'],
              ['partclone.vmfs3', 'partclone.vmfs']]
endif

foreach alias : aliases
  install_symlink(alias[0],
    pointing_to : alias[1],
    install_dir : sbindir,
  )
endforeach

## ---------------------------------------------------------------------------
## bash completion
## ---------------------------------------------------------------------------
if get_option('bash-completion')
  bashcompdir = get_option('bashcompdir')
  if bashcompdir == ''
    bashcomp_dep = dependency('bash-completion', required : false)
    if bashcomp_dep.found()
      bashcompdir = bashcomp_dep.get_variable(pkgconfig : 'completionsdir')
    else
      bashcompdir = datadir / 'bash-completion' / 'completions'
    endif
  endif
  install_data('partclone-completion',
    install_dir : bashcompdir,
    rename : 'partclone',
  )
endif

## ---------------------------------------------------------------------------
## Test suite
##
## The tests are shell scripts that expect to run from a directory whose
## ../src contains the freshly built tools, exactly like "make check" did.
## ---------------------------------------------------------------------------
bash = find_program('bash')
test_workdir = meson.current_build_dir()

tests = { 'dd' : 'dd.test', 'disk' : 'disk.test' }

if get_option('fs-test')
  if fs_build.get('extfs', false)
    tests += {'ext2' : 'ext2.test', 'ext3' : 'ext3.test', 'ext4' : 'ext4.test'}
  endif
  if fs_build.get('btrfs', false)
    tests += {
      'btrfs'            : 'btrfs.test',
      'btrfs_bgt'        : 'btrfs_bgt.test',
      'btrfs_trees'      : 'btrfs_trees.test',
      'btrfs_multilevel' : 'btrfs_multilevel.test',
      'btrfs_uninitvalgrind' : 'btrfs_uninitvalgrind.test',
    }
  endif
  if fs_build.get('fat', false)
    tests += {'fat' : 'fat.test'}
  endif
  if fs_build.get('reiserfs', false)
    tests += {'reiserfs' : 'reiserfs.test'}
  endif
  if fs_build.get('reiser4', false)
    tests += {'reiser4' : 'reiser4.test'}
  endif
  if fs_build.get('hfsp', false)
    tests += {'hfsplus' : 'hfsplus.test'}
  endif
  if fs_build.get('xfs', false)
    tests += {'xfs' : 'xfs.test'}
  endif
  if fs_build.get('exfat', false)
    tests += {'exfat' : 'exfat.test', 'exfat_last_clusters' : 'exfat_last_clusters.test'}
  endif
  if fs_build.get('f2fs', false)
    tests += {'f2fs' : 'f2fs.test'}
  endif
  if fs_build.get('ntfs', false)
    tests += {'ntfs' : 'ntfs.test', 'ntfs_backup_bootsector' : 'ntfs_backup_bootsector.test'}
  endif
  if fs_build.get('jfs', false)
    tests += {'jfs' : 'jfs.test'}
  endif
  if want_ncurses and ncurses_dep.found()
    tests += {'ncursesw' : 'ncursesw.test'}
  endif
endif

if get_option('feature-test')
  if want_ncurses and ncurses_dep.found()
    tests += {'ncursesw' : 'ncursesw.test'}
  endif
  if fs_build.get('extfs', false)
    tests += {'jsonprogress' : 'jsonprogress.test'}
  endif
  tests += {
    'btfile'    : 'btfile.test',
    'info'      : 'info.test',
    'quiet'     : 'quiet.test',
    'pipe'      : 'pipe.test',
    'note'      : 'note.test',
    'imager'    : 'imager.test',
    'domain'    : 'domain.test',
    'checksum'  : 'checksum.test',
  }
endif

foreach name, script : tests
  test(name, bash,
    args : files(script),
    workdir : test_workdir,
    timeout : 600,
  )
endforeach

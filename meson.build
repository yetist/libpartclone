project('partclone', 'c',
  version : '0.3.50',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.61.0',
  default_options : [
    'c_std=gnu11',
    'warning_level=1',
    'default_library=shared',
  ])

cc = meson.get_compiler('c')

# ---------------------------------------------------------------------------
# Install layout
# ---------------------------------------------------------------------------
prefix     = get_option('prefix')
libdir     = get_option('libdir')
datadir    = get_option('datadir')
sbindir   = get_option('sbindir')
localedir  = prefix / get_option('localedir')
plugin_dir = prefix / libdir / 'partclone'
docdir     = datadir / 'doc' / meson.project_name()

top_inc = include_directories('.')

# ---------------------------------------------------------------------------
# Global build configuration
# ---------------------------------------------------------------------------
if get_option('static-linking')
  error('static-linking is not supported: the filesystem backends are loadable modules and require dynamic linking')
endif

add_project_arguments(
  '-D_FILE_OFFSET_BITS=64',
  '-DHAVE_CONFIG_H',
  language : 'c',
)

if get_option('mtrace')
  add_project_arguments('-DMEMTRACE', '-DMEMWATCH', '-DMW_STDIO', language : 'c')
endif

# ---------------------------------------------------------------------------
# Required dependencies
# ---------------------------------------------------------------------------
# gio-2.0 pulls in gobject-2.0 and glib-2.0 (and gobject in turn requires
# glib), so a single dependency provides the entire GLib/GObject/GIO API used
# by libpartclone and its public headers.
gio_dep     = dependency('gio-2.0', version : '>= 2.44')
uuid_dep    = dependency('uuid')
openssl_dep = dependency('openssl')
zlib_dep    = dependency('zlib')
zstd_dep    = dependency('libzstd')
threads_dep = dependency('threads')
intl_dep    = dependency('intl', required : false)
math_dep    = cc.find_library('m', required : false)

# gmodule is used to dlopen the backend modules.  The -no-export pkg-config
# module provides the same library as gmodule-2.0 but without adding
# -Wl,--export-dynamic to the command line.
gmodule_dep = dependency('gmodule-no-export-2.0')

# ---------------------------------------------------------------------------
# Optional dependencies
# ---------------------------------------------------------------------------
blkid_dep = dependency('blkid', required : false)

want_xxhash = get_option('xxhash')
xxhash_dep  = dependency('libxxhash', required : want_xxhash)

want_isal = get_option('isal')
isal_dep  = dependency('libisal', required : want_isal)

want_ncurses = get_option('ncursesw').enabled()
ncurses_dep  = cc.find_library('ncursesw', required : want_ncurses)
# The old build system linked libtinfo (if present) into the core library when
# the ncursesw text user interface was available.
tinfo_dep    = cc.find_library('tinfo', required : false)

fuse_opt  = get_option('fuse')
build_fuse = get_option('all') or fuse_opt.enabled()
fuse_dep  = dependency('fuse3', required : build_fuse)

gtk4_opt  = get_option('gtk4')
gtk4_dep  = dependency('gtk4', version : '>= 4.10', required : gtk4_opt.enabled())
build_gtk4 = gtk4_opt.enabled() or (gtk4_opt.auto() and gtk4_dep.found())

# ---------------------------------------------------------------------------
# config.h
# ---------------------------------------------------------------------------
conf = configuration_data()
conf.set_quoted('PACKAGE', 'partclone')
conf.set_quoted('PACKAGE_NAME', 'Partclone')
conf.set_quoted('PACKAGE_TARNAME', 'partclone')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', 'Partclone @0@'.format(meson.project_version()))
conf.set_quoted('PACKAGE_BUGREPORT', 'thomas@clonezilla.org')
conf.set_quoted('PACKAGE_URL', '')
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('LOCALEDIR', localedir)

if get_option('nls')
  conf.set('ENABLE_NLS', 1)
endif

if blkid_dep.found()
  conf.set('HAVE_BLKID', 1)
endif
if xxhash_dep.found()
  conf.set('HAVE_XXHASH', 1)
endif
if isal_dep.found()
  conf.set('HAVE_ISAL', 1)
endif
if want_ncurses and ncurses_dep.found()
  conf.set('HAVE_LIBNCURSESW', 1)
endif

# OpenSSL 1.1+ API.
foreach fn : ['EVP_MD_CTX_new', 'EVP_MD_CTX_create', 'EVP_MD_CTX_reset']
  if cc.has_function(fn, dependencies : openssl_dep)
    conf.set(('HAVE_' + fn).underscorify().to_upper(), 1)
  endif
endforeach

# Common headers.  The bundled filesystem code and the core consult a number
# of HAVE_<HEADER> macros, so probe a broad set like the old configure did.
foreach header : [
    'ctype.h', 'fcntl.h', 'grp.h', 'libintl.h', 'limits.h',
    'linux/magic.h', 'linux/types.h', 'locale.h', 'mntent.h', 'mqueue.h',
    'paths.h', 'pwd.h', 'semaphore.h', 'stdlib.h', 'string.h', 'strings.h',
    'sys/ioctl.h', 'sys/mount.h', 'sys/time.h', 'syslog.h', 'sys/stat.h',
    'time.h', 'unistd.h', 'byteswap.h', 'sys/sysmacros.h', 'sys/utsname.h',
    'sys/xattr.h', 'sys/acl.h', 'sys/types.h', 'sys/statvfs.h',
    'scsi/sg.h', 'linux/limits.h', 'linux/hdreg.h', 'linux/posix_acl.h',
    'linux/blkzoned.h', 'ftw.h', 'attr/xattr.h', 'blkid/blkid.h',
    'stdint.h', 'stdio.h', 'inttypes.h', 'ncurses.h',
  ]
  if cc.has_header(header)
    conf.set(('HAVE_' + header).underscorify().to_upper(), 1)
  endif
endforeach

if cc.has_function('memfd_create', prefix : '#define _GNU_SOURCE\n#include <sys/mman.h>')
  conf.set('HAVE_MEMFD_CREATE', 1)
endif

# ---------------------------------------------------------------------------
# Filesystem backend selection
# ---------------------------------------------------------------------------
opt_all = get_option('all')

# Requirements per backend.  Each entry is a list of "kind:argument":
#   pc:NAME     pkg-config module
#   lib:NAME    linker library
#   hdr:HEADER  required header
# ntfs is handled separately (ntfs-3g vs. ntfsprogs).
fs_reqs = {
  'extfs'    : ['lib:ext2fs', 'lib:com_err', 'hdr:ext2fs/ext2fs.h'],
  'xfs'      : ['pc:liburcu', 'pc:uuid', 'pc:blkid'],
  'reiserfs' : ['lib:reiserfs', 'lib:dal', 'hdr:reiserfs/reiserfs.h'],
  'reiser4'  : ['lib:reiser4', 'lib:aal', 'hdr:reiser4/libreiser4.h'],
  'hfsp'     : [],
  'apfs'     : [],
  'fat'      : [],
  'exfat'    : [],
  'f2fs'     : ['pc:uuid'],
  'nilfs2'   : ['lib:nilfs', 'hdr:nilfs.h'],
  'ufs'      : ['lib:ufs', 'lib:bsd'],
  'vmfs'     : ['lib:vmfs', 'pc:uuid'],
  'jfs'      : ['lib:jfs', 'pc:uuid'],
  'btrfs'    : ['pc:uuid', 'pc:blkid', 'pc:openssl', 'pc:zlib', 'pc:libzstd'],
  'minix'    : [],
}

fs_build = { 'raw' : true }
fs_deps  = { 'raw' : [] }

foreach name, reqs : fs_reqs
  opt  = get_option(name)
  want = opt_all or opt.enabled()
  deps = []
  ok   = true
  foreach req : reqs
    parts = req.split(':')
    kind  = parts[0]
    arg   = parts[1]
    if kind == 'pc'
      d = dependency(arg, required : want)
      deps += [d]
      ok = ok and d.found()
    elif kind == 'lib'
      d = cc.find_library(arg, required : want)
      deps += [d]
      ok = ok and d.found()
    elif kind == 'hdr'
      if not cc.has_header(arg)
        ok = false
      endif
    endif
  endforeach

  if want and not ok
    error('filesystem backend "@0@" was requested but its dependencies were not found'.format(name))
  endif

  fs_build += { name : want or (opt.auto() and ok) }
  fs_deps  += { name : deps }
endforeach

# ntfs: prefer ntfs-3g, fall back to ntfsprogs.
ntfs_opt  = get_option('ntfs')
want_ntfs = opt_all or ntfs_opt.enabled()
ntfs_deps = []
ntfs_c_args = []
build_ntfs = false
if want_ntfs or ntfs_opt.auto()
  ntfs3g = cc.find_library('ntfs-3g', required : false)
  if ntfs3g.found() and cc.has_header('ntfs-3g/misc.h')
    ntfs_deps   = [ntfs3g]
    ntfs_c_args = ['-DNTFS3G']
    build_ntfs  = true
  else
    ntfs_lib = cc.find_library('ntfs', required : false)
    if ntfs_lib.found()
      ntfs_deps   = [ntfs_lib]
      ntfs_c_args = ['-DNTFS']
      build_ntfs  = true
    elif want_ntfs
      error('ntfs backend requested but neither libntfs-3g nor libntfs was found')
    endif
  endif
endif
fs_build += { 'ntfs' : build_ntfs }
fs_deps  += { 'ntfs' : ntfs_deps }

# NILFS layout accessors (used by nilfsclone.c).
if fs_build.get('nilfs2', false)
  if cc.has_type('struct nilfs_layout', prefix : '#include <nilfs.h>')
    conf.set('HAVE_STRUCT_NILFS_LAYOUT', 1)
  endif
  if cc.has_function('nilfs_get_layout', prefix : '#include <nilfs.h>')
    conf.set('HAVE_NILFS_GET_LAYOUT', 1)
  endif
endif

configure_file(output : 'config.h', configuration : conf)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
subdir('lib')
subdir('src')
subdir('docs')
subdir('examples')
subdir('po')
subdir('tests')

# ---------------------------------------------------------------------------
# Maintainer helper: regenerate ChangeLog from the git history.
# ---------------------------------------------------------------------------
run_target('changelog',
  command : [find_program('toolbox'), '--update-log'],
  env : { 'srcdir' : meson.project_source_root() },
)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
enabled_fs = []
foreach name, on : fs_build
  if on
    enabled_fs += [name]
  endif
endforeach

summary({
  'version'   : meson.project_version(),
  'backends'  : ', '.join(enabled_fs),
  'prefix'    : prefix,
  'blkid'     : blkid_dep.found(),
  'xxhash'    : xxhash_dep.found(),
  'isal'      : isal_dep.found(),
  'ncursesw'  : want_ncurses and ncurses_dep.found(),
  'fuse'      : build_fuse,
  'nls'       : get_option('nls'),
}, section : 'Configuration')

summary({
  'examples'  : true,
  'tests'     : get_option('fs-test') or get_option('feature-test'),
}, section : 'Extras')

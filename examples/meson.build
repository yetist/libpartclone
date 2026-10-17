## ---------------------------------------------------------------------------
## libpartclone examples
## ---------------------------------------------------------------------------

executable('ptc-demo', 'ptc-demo.c',
  dependencies : libpartclone_dep,
)

install_data('ptc-demo.c', install_dir : docdir / 'examples')

## Optional GTK4 front end, built only when GTK4 is available.
if build_gtk4
  executable('ptc-gtk4', 'ptc-gtk4.c',
    dependencies : [libpartclone_dep, gtk4_dep],
  )
  install_data('ptc-gtk4.c', install_dir : docdir / 'examples')
endif

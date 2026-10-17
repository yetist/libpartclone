## Build and install the libpartclone examples.

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)/lib

noinst_PROGRAMS = ptc-demo
ptc_demo_SOURCES = ptc-demo.c
ptc_demo_LDADD = $(top_builddir)/lib/libpartclone.la $(GLIB_LIBS)

## Optional GTK4 front end, built only when GTK4 is available.
if ENABLE_GTK4
noinst_PROGRAMS += ptc-gtk4
ptc_gtk4_SOURCES = ptc-gtk4.c
ptc_gtk4_CFLAGS = $(AM_CFLAGS) $(GTK4_CFLAGS)
ptc_gtk4_LDADD = $(top_builddir)/lib/libpartclone.la $(GTK4_LIBS) $(GLIB_LIBS)
endif

## Ship the sources as installed examples and in the tarball.
examplesdir = $(docdir)/examples
examples_DATA = ptc-demo.c
if ENABLE_GTK4
examples_DATA += ptc-gtk4.c
endif

## ptc-gtk4.c is not a program source when GTK4 is missing, so distribute it
## unconditionally.
EXTRA_DIST = ptc-gtk4.c

MAINTAINERCLEANFILES = Makefile.in

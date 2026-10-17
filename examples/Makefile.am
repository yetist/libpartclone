## Build and install the libpartclone example.

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)/lib

noinst_PROGRAMS = ptc-demo
ptc_demo_SOURCES = ptc-demo.c
ptc_demo_LDADD = $(top_builddir)/lib/libpartclone.la $(GLIB_LIBS)

## Ship the source as an installed example and in the tarball.
examplesdir = $(docdir)/examples
examples_DATA = ptc-demo.c

EXTRA_DIST = $(examples_DATA)

MAINTAINERCLEANFILES = Makefile.in

## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = gnu
MAINTAINERCLEANFILES = Makefile.in

LDFLAGS = -no-undefined -export-dynamic @DLLDFLAGS@

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src @INTLINCL@
include_HEADERS = m4module.h
pkginclude_HEADERS = error.h list.h obstack.h regex.h system.h

lib_LTLIBRARIES = libm4.la
libm4_la_SOURCES = list.c m4module.c error.c obstack.c regex.c
libm4_la_LIBADD = @LTLIBOBJS@


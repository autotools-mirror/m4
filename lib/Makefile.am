## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = gnu ../src/ansi2knr
MAINTAINERCLEANFILES = Makefile.in

LDFLAGS = -no-undefined -export-dynamic @DLLDFLAGS@

INCLUDES = @INTLINCL@
include_HEADERS = m4module.h m4error.h m4obstack.h m4regex.h
noinst_HEADERS = m4private.h

lib_LTLIBRARIES = libm4.la
libm4_la_SOURCES = m4module.c m4error.c m4obstack.c m4regex.c
libm4_la_LIBADD = @LTLIBOBJS@


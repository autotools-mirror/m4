## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = gnu ../src/ansi2knr

MAINTAINERCLEANFILES = Makefile.in

noinst_LIBRARIES = libm4.a

INCLUDES = @INTLINCL@

libm4_a_SOURCES = error.c

noinst_HEADERS = getopt.h error.h getdate.h obstack.h regex.h

libm4_a_LIBADD = @LIBOBJS@

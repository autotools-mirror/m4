AUTOMAKE_OPTIONS = ansi2knr

CLEANFILES = m4@EXEEXT@
MAINTAINERCLEANFILES = Makefile.in ansi2knr.c ansi2knr.1 pathconf.h

AM_CPPFLAGS = @LIBM4_DLL_IMPORT@
INCLUDES = -I$(top_srcdir)/lib @INTLINCL@
  
CFLAGS = @CFLAGS@

LIBS = @LIBS@

MODULE_PATH = @MODULE_PATH@

noinst_HEADERS = getopt.h ltdl.h

bin_PROGRAMS = m4
m4_SOURCES = m4.c m4.h builtin.c debug.c eval.c evalmp.c \
	format.c freeze.c input.c macro.c output.c path.c symtab.c \
	stackovf.c module.c ltdl.c getopt.c getopt1.c
EXTRA_m4_SOURCES = numb.c numb.h


m4_LDADD = @M4OBJS@ $(top_srcdir)/lib/libm4.la @INTLLIBS@
m4_LDFLAGS = -no-undefined @DLLDFLAGS@

$(bin_PROGRAMS): @M4OBJS@
path.o: path.c pathconf.h

pathconf.h: Makefile
	@echo "#define MODULE_PATH \"${MODULE_PATH}\"" >pathconf.h

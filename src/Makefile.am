AUTOMAKE_OPTIONS = ansi2knr

MAINTAINERCLEANFILES = Makefile.in ansi2knr.c ansi2knr.1 pathconf.h

LIBS = @LIBS@

CFLAGS = @CFLAGS@

MODULE_PATH=@MODULE_PATH@

bin_PROGRAMS = m4

INCLUDES = -I$(top_srcdir)/libltdl -I$(top_srcdir)/lib @INTLINCL@

m4_SOURCES = m4.c m4.h builtin.c builtin.h debug.c eval.c evalmp.c \
	format.c freeze.c input.c macro.c output.c path.c symtab.c \
	stackovf.c module.c

EXTRA_m4_SOURCES = numb.c numb.h

m4_LDADD = ../libltdl/libltdlc.la ../lib/libm4.a @INTLLIBS@
m4_LDFLAGS = @DLLDFLAGS@

path.o: path.c pathconf.h

pathconf.h: Makefile
	@echo "#define MODULE_PATH \"${MODULE_PATH}\"" >pathconf.h

EXTRA_DIST = 

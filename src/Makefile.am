AUTOMAKE_OPTIONS = ansi2knr

LIBS = @LIBS@

CFLAGS = -Wall @CFLAGS@

MODULE_PATH=@MODULE_PATH@

bin_PROGRAMS = m4

INCLUDES = -I$(top_srcdir)/lib

m4_SOURCES = m4.c m4.h builtin.c builtin.h debug.c eval.c numb.c numb.h \
	format.c freeze.c input.c macro.c output.c path.c symtab.c \
	stackovf.c module.c

m4_LDADD = ../lib/libm4.a @INTLLIBS@
m4_LDFLAGS = @DLLDFLAGS@

path.o: path.c pathconf.h

pathconf.h: Makefile
	@echo "#define MODULE_PATH \"${MODULE_PATH}\"" >pathconf.h

EXTRA_DIST = 

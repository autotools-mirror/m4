AUTOMAKE_OPTIONS = ansi2knr

LIBS = @LIBS@
CFLAGS = -Wall @CFLAGS@

bin_PROGRAMS = m4

INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_srcdir)/lib

m4_SOURCES = m4.c m4.h builtin.c debug.c eval.c numb.c numb.h format.c \
	freeze.c input.c macro.c output.c path.c symtab.c stackovf.c

m4_LDADD = ../lib/libm4.a @INTLLIBS@



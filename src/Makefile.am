CLEANFILES = m4$(EXEEXT) pathconf.h
MAINTAINERCLEANFILES = Makefile.in ansi2knr.c ansi2knr.1 pathconf.h

AM_CPPFLAGS = $(LIBM4_DLL_IMPORT)
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/m4 $(INCLTDL) $(INTLINCL)
  
MODULE_PATH = $(pkglibexecdir)

noinst_HEADERS = getopt.h ltdl.h m4private.h

bin_PROGRAMS = m4
m4_SOURCES = m4.c m4.h builtin.c debug.c eval.c evalmp.c \
	format.c freeze.c input.c macro.c output.c path.c \
	symtab.c stackovf.c module.c ltdl.c
EXTRA_m4_SOURCES = numb.c numb.h getopt.c getopt1.c


m4_LDADD = $(M4OBJS) $(top_builddir)/m4/libm4.la $(LIBADD_DL) $(INTLLIBS)
m4_LDFLAGS = -no-undefined -export-dynamic $(DLPREOPEN)

$(bin_PROGRAMS): $(M4OBJS)
path.o: path.c pathconf.h

pathconf.h: Makefile
	@echo "#define MODULE_PATH \"${MODULE_PATH}\"" >pathconf.h

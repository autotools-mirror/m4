MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = frozen.m4 modtest.m4 shadow.m4 time.m4 time2.m4 stdlib.m4 \
	 unfrozen.m4 $(TESTS) defs

TESTS = modtest.test shadow.test modfreeze.test \
	modpath1.test modpath2.test modpath3.test modpath4.test \
	unload.test

AM_CPPFLAGS = @LIBM4_DLL_IMPORT@
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/m4 $(INCLTDL) $(INTLINCL)
LIBS = $(top_builddir)/m4/libm4.la
LDFLAGS = -no-undefined -avoid-version

if WITH_MODULES
pkglibexec_LTLIBRARIES	= modtest.la perl.la shadow.la time.la stdlib.la
endif

modtest_la_SOURCES = modtest.c
modtest_la_LDFLAGS = -module

perl_la_SOURCES = perl.c
perl_la_CFLAGS = `perl -MExtUtils::Embed -e ccopts`
perl_la_LDFLAGS = -module `perl -MExtUtils::Embed -e ldopts`

shadow_la_SOURCES = shadow.c
shadow_la_LDFLAGS = -module

time_la_SOURCES = time.c
time_la_LDFLAGS = -module

stdlib_la_SOURCES = stdlib.c
stdlib_la_LDFLAGS = -module

perlxsi.c:
	perl -MExtUtils::Embed -e xsinit -- -o perlxsi.c

perl.lo: perl.c perlxsi.c
	@echo '$(LTCOMPILE) $(perl_la_CFLAGS) -c $<'; \
	$(LTCOMPILE) $(perl_la_CFLAGS) -c $<

distclean-local:
	-rm -rf testSubDir

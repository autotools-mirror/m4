MAINTAINERCLEANFILES = Makefile.in

pkglibexec_LTLIBRARIES = test.la shadow.la time.la stdlib.la

EXTRA_DIST = test.m4 shadow.m4 time.m4 time2.m4 stdlib.m4 \
	 $(TESTS) defs

TESTS = test.test time.test shadow.test \
	modpath1.test modpath2.test modpath3.test modpath4.test

AM_CPPFLAGS = @LIBM4_DLL_IMPORT@
INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/lib -I$(top_srcdir)/libltdl -I/usr/local/include @INTLINCL@
 
test_la_SOURCES = test.c
shadow_la_SOURCES = shadow.c
time_la_SOURCES = time.c
stdlib_la_SOURCES = stdlib.c

LIBS = $(top_srcdir)/lib/libm4.la
LDFLAGS = -no-undefined -module -avoid-version @DLLDFLAGS@

test_la_LDFLAGS = -module
shadow_la_LDFLAGS = -module
time_la_LDFLAGS = -module
stdlib_la_LDFLAGS = -module

# This is experimental - in fact it doesn't work at all

#perl_la_SOURCES = perl.c

#perl_la_CFLAGS = `perl -MExtUtils::Embed -e ccopts`
#perl_la_LDFLAGS = -module `perl -MExtUtils::Embed -e ldopts`

perlxsi.c:
	perl -MExtUtils::Embed -e xsinit -- -o perlxsi.c

perl.lo: perl.c perlxsi.c
	@echo '$(LTCOMPILE) $(perl_la_CFLAGS) -c $<'; \
	$(LTCOMPILE) -Wp,-MD,.deps/$(*F).pp $(perl_la_CFLAGS) -c $<
	@-sed -e 's/^\([^:]*\)\.o[ 	]*:/\1.lo \1.o :/' \
	  < .deps/$(*F).pp > .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm -f .deps/$(*F).pp

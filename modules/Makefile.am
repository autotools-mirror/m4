# To get libtool and Automake to handle this correctly it is necessary
# to pretend we are compiling shared libraries (which we are) and then
# change the names when installing.

MAINTAINERCLEANFILES = Makefile.in

pkglibexec_LTLIBRARIES = test.la time.la stdlib.la

EXTRA_DIST = test.m4 time.m4 stdlib.m4

INCLUDES = -I$(srcdir)/../src -I$(srcdir)/../lib @INTLINCL@

test_la_SOURCES = test.c
time_la_SOURCES = time.c
stdlib_la_SOURCES = stdlib.c

test_la_LDFLAGS = -module
time_la_LDFLAGS = -module
stdlib_la_LDFLAGS = -module

LIBS = 
LDFLAGS = -no-undefined -module -avoid-version @DLLDFLAGS@

# libperl_la_SOURCES = perl.c perlxsi.c
libperl_la_OBJECTS = perl.lo

perlxsi.c:
	perl -MExtUtils::Embed -e xsinit -- -o perlxsi.c

libperl_la_CFLAGS = `perl -MExtUtils::Embed -e ccopts`
PERLLDFLAGS = `perl -MExtUtils::Embed -e ldopts`

perl.lo: perl.c perlxsi.c
	@echo '$(LTCOMPILE) $(libperl_la_CFLAGS) -c perl.c'; \
	$(LTCOMPILE) -Wp,-MD,.deps/$(*F).p $(libperl_la_CFLAGS) -c perl.c
	@-sed -e 's/^\([^:]*\)\.o:/\1.lo \1.o:/' \
	  < .deps/$(*F).p > .deps/$(*F).P
	@-rm -f .deps/$(*F).p

libperl.la: $(libperl_la_OBJECTS) $(libperl_la_DEPENDENCIES)
	$(LINK) -rpath $(pkglibexecdir) $(libperl_la_LDFLAGS) $(PERLLDFLAGS) $(libperl_la_OBJECTS) $(libperl_la_LIBADD) $(LIBS)

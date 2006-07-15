## This file is part of GNU M4.
##
## Copyright (C) 2000, 2001, 2003, 2004, 2005, 2006 Free Software
## Foundation, Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; see the file COPYING.  If not, write to
## the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
## Boston, MA 02110-1301, USA.
##
## Written by Gary V. Vaughan <gary@gnu.org>

config_aux_dir	= ltdl/config
config_macro_dir= ltdl/m4

SUBDIRS		= ltdl po gnu .

ACLOCAL_AMFLAGS = -I ltdl/m4
AM_CPPFLAGS	= -I. -I$(srcdir) \
		  -Ignu -I$(srcdir)/gnu \
		  -Im4 -I$(srcdir)/m4\
		  $(INTLINCL) $(LTDLINCL)
AM_LDFLAGS	= -no-undefined -export-dynamic

EXTRA_DIST	= bootstrap \
		  $(config_aux_dir)/mkstamp \
		  $(config_macro_dir)/gnulib-cache.m4
CLEANFILES	=
DISTCLEANFILES	=
MOSTLYCLEANFILES=
MAINTAINERCLEANFILES = \
		  ABOUT-NLS COPYING INSTALL Makefile.in aclocal.m4 \
		  config-h.in configure stamp-h.in \
		  po/cat-id-tbl.c po/m4.pot po/stamp-cat-id

BUILT_SOURCES	=

## We know that $(srcdir)/ChangeLog has been edited if stamp-vcl
## needs updating, so we assume we have write access to $(srcdir).
## If we try to maintain ./stamp-vcl to avoid writing to a possibly
## read-only $(srcdir), then when the two stamp-vcls differ, distcheck
## will try to update the one in the read-only source tree it makes.
MKSTAMP = $(SHELL) $(srcdir)/$(config_aux_dir)/mkstamp
$(srcdir)/stamp-vcl: vcl-tmp ChangeLog
vcl-tmp:
	@set dummy `$(MKSTAMP) < $(srcdir)/ChangeLog`; shift; \
	echo "$$1" > vcl.tmp; \
	diff vcl.tmp $(srcdir)/stamp-vcl >/dev/null 2>&1 \
	  || (echo "Updating stamp-vcl"; cp vcl.tmp $(srcdir)/stamp-vcl)
	-@rm -f vcl.tmp

# Make sure config.status is regenerated when the version timestamp changes
CONFIG_STATUS_DEPENDENCIES = stamp-vcl
EXTRA_DIST += stamp-vcl


## ------- ##
## GNU M4. ##
## ------- ##

bin_PROGRAMS	= src/m4
src_m4_SOURCES	= \
		  src/version-etc-fsf.c \
		  src/version-etc.c \
		  src/version-etc.h \
		  src/main.c \
		  src/m4.h \
		  src/freeze.c
if GETOPT
src_m4_SOURCES += \
		  src/getopt.c \
		  src/getopt1.c \
		  src/getopt.h
endif
if STACKOVF
src_m4_SOURCES += src/stackovf.c
endif
src_m4_CPPFLAGS	= $(AM_CPPFLAGS) -Isrc -I$(srcdir)/src $(LTDLINCL)
src_m4_LDFLAGS	= $(AM_LDFLAGS) $(DLPREOPEN)
src_m4_LDADD	= m4/libm4.la
src_m4_DEPENDENCIES = $(PREOPEN_DEPENDENCIES) m4/libm4.la

## Since we do not build a libtool library in src, Automake fails to infer
## that there may be a libs directory there.  FIXME - revisit this if
## automake 1.10 fixes the bug.
clean-local-src:
	-rm -rf src/.libs src/_libs


##                                                                      ##
##                 --- PASTED MANUALLY FROM GNULIB ---                  ##
##     To avoid adding unnecessary objects to libm4.la these gnulib     ##
##     modules are not imported by gnulib-tool, but added manually.     ##
##  (from: gnulib-tool --extract-automake-snippet getopt version-etc)   ##
##                                                                      ##
BUILT_SOURCES  += src/$(GETOPT_H)
EXTRA_DIST     += src/getopt_.h src/getopt_int.h

# We need the following in order to create an <getopt.h> when the
# system doesn't have one that works with the given compiler.
all-local: src/$(GETOPT_H)
src/getopt.h: src/getopt_.h
	cp $(srcdir)/src/getopt_.h $@-t
	mv $@-t $@
MOSTLYCLEANFILES += src/getopt.h src/getopt.h-t
##                                                                      ##
##                     --- END OF PASTED GNULIB ---                     ##
##                                                                      ##

## -------- ##
## Modules. ##
## -------- ##

pkgmodincludedir= $(includedir)/$(PACKAGE)/modules
pkglibexecdir	= $(libexecdir)/$(PACKAGE)

ETAGS_ARGS	= --regex='/M4BUILTIN_HANDLER[ \t]*)\([^)]*\)/\1/'

module_ldflags	= -module $(AM_LDFLAGS)
module_libadd	= m4/libm4.la

pkgmodinclude_HEADERS = modules/m4.h

pkglibexec_LTLIBRARIES = \
		  modules/gnu.la \
		  modules/load.la \
		  modules/m4.la \
		  modules/traditional.la

EXTRA_modules_gnu_la_SOURCES	= modules/format.c
modules_gnu_la_LDFLAGS		= $(module_ldflags)
modules_gnu_la_LIBADD		= $(module_libadd)
EXTRA_DIST		       += $(EXTRA_modules_gnu_la_SOURCES)

modules_load_la_LDFLAGS		= $(module_ldflags)
modules_load_la_LIBADD		= $(module_libadd)

EXTRA_modules_m4_la_SOURCES	= modules/evalparse.c
modules_m4_la_LDFLAGS		= $(module_ldflags)
modules_m4_la_LIBADD		= $(module_libadd)
EXTRA_DIST		       += $(EXTRA_modules_m4_la_SOURCES)

modules_traditional_la_LDFLAGS	= $(module_ldflags)
modules_traditional_la_LIBADD	= $(module_libadd)

if USE_GMP
pkglibexec_LTLIBRARIES	       += modules/mpeval.la
EXTRA_modules_mpeval_la_SOURCES	= modules/evalparse.c
modules_mpeval_la_LDFLAGS	= $(module_ldflags)
modules_mpeval_la_LIBADD	= $(module_libadd) $(LIBADD_GMP)
endif

## Disabled for now.  It is too fragile to be useful.
## pkglibexec_LTLIBRARIES         += modules/perl.la
## modules_perl_la_CPPFLAGS	= `$(PERL) -MExtUtils::Embed -e ccopts`
## modules_perl_la_LDFLAGS		= \
##		  $(module_ldflags) `$(PERL) -MExtUtils::Embed -e ldopts`
## modules_perl_la_LIBADD		= $(module_libadd)
## BUILT_SOURCES			= perlxsi.c
## DISTCLEANFILES		       += $(BUILT_SOURCES)
##
## perlxsi.c:
##	$(PERL) -MExtUtils::Embed -e xsinit -- -o perlxsi.c

## but we should still distribute it, because it is listed in POTFILES.in:
EXTRA_DIST += modules/perl.c

module_check	= -rpath /dev/null
check_LTLIBRARIES = \
		  modules/import.la \
		  modules/modtest.la \
		  modules/shadow.la \
		  modules/stdlib.la \
		  modules/time.la

modules_import_la_LDFLAGS	= $(module_ldflags) $(module_check)
modules_import_la_LIBADD	= $(module_libadd)

modules_modtest_la_LDFLAGS	= $(module_ldflags) $(module_check)
modules_modtest_la_LIBADD	= $(module_libadd)

modules_shadow_la_LDFLAGS	= $(module_ldflags) $(module_check)
modules_shadow_la_LIBADD	= $(module_libadd)

modules_stdlib_la_LDFLAGS	= $(module_ldflags) $(module_check)
modules_stdlib_la_LIBADD	= $(module_libadd)

modules_time_la_LDFLAGS		= $(module_ldflags) $(module_check)
modules_time_la_LIBADD		= $(module_libadd)

EXTRA_DIST     += \
		  modules/modtest.m4 \
		  modules/shadow.m4 \
		  modules/time.m4 \
		  modules/time2.m4 \
		  modules/stdlib.m4

## ----- ##
## libm4 ##
## ----- ##

lib_LTLIBRARIES	= m4/libm4.la

nobase_include_HEADERS = \
		  m4/m4module.h
nobase_pkginclude_HEADERS = \
		  m4/hash.h \
		  m4/system.h
m4_libm4_la_SOURCES	= \
		  m4/builtin.c \
		  m4/debug.c \
		  m4/hash.c \
		  m4/input.c \
		  m4/m4.c \
		  m4/m4private.h \
		  m4/macro.c \
		  m4/module.c \
		  m4/output.c \
		  m4/path.c \
		  m4/pathconf.h \
		  m4/resyntax.c \
		  m4/symtab.c \
		  m4/syntax.c \
		  m4/utility.c
m4_libm4_la_LIBADD = gnu/libgnu.la \
		  $(LIBLTDL) $(LTLIBINTL) $(LIBADD_DL)
m4_libm4_la_DEPENDENCIES = $(LIBLTDL)

m4/pathconf.h:
	echo "#define MODULE_PATH \"$(pkglibexecdir)\"" > m4/pathconf.h

MOSTLYCLEANFILES += m4/pathconf.h
BUILT_SOURCES    += m4/pathconf.h

# This file needs to be regenerated at configure time.
dist-hook:
	rm -f $(distdir)/m4/system.h
EXTRA_DIST     += m4/system_.h


## ------- ##
## libltdl ##
## ------- ##

ltdl/libltdlc.la:
	cd ltdl && $(MAKE) $(MAKEFLAGS) libltdlc.la

## -------------- ##
## Documentation. ##
## -------------- ##

info_TEXINFOS	= doc/m4.texinfo
doc_m4_TEXINFOS	= doc/regexprops-generic.texi doc/fdl.texi
dist_man_MANS	= doc/m4.1
EXTRA_DIST     += $(config_aux_dir)/gendocs.sh doc/gendocs_template
MAINTAINERCLEANFILES += $(config_aux_dir)/gendocs.sh doc/gendocs_template \
		  doc/fdl.texi

# Build the man page once in the srcdir, rather than in every VPATH build
# dir, to match how automake builds info pages.  This is safe for 'make
# distcheck' since it is distributed pre-built.
$(srcdir)/doc/m4.1: configure.ac src/main.c
	@if test -f src/m4$(EXEEXT); then \
	  echo "Updating the \`man' page \`$@'"; \
	  $(SHELL) $(srcdir)/$(config_aux_dir)/missing --run \
	    help2man -o $@ src/m4$(EXEEXT); \
	else \
	  echo "WARNING: The \`man' page \`$@' cannot be updated yet."; \
	  echo "         Retry once the program executable is ready."; \
	fi

## Disabled for now.  Clashes with earlier definition.
## TAGS_FILES	= $(infos_TEXINFOS)
## TAGS_DEPENDENCIES = $(TAGS_FILES)
## ETAGS_ARGS	= --language=none --regex='/@node \([^,]*\)/\1/' $(TAGS_FILES)

MAINTAINERCLEANFILES += $(dist_man_MANS)


## --------- ##
## Examples. ##
## --------- ##

EXTRA_DIST     += \
		  examples/WWW/Makefile \
		  examples/WWW/_footer.htm \
		  examples/WWW/_header.htm \
		  examples/WWW/man/Makefile \
		  examples/WWW/m4lib/bugs.m4 \
		  examples/WWW/m4lib/changelog.m4 \
		  examples/WWW/m4lib/download.m4 \
		  examples/WWW/m4lib/features.m4 \
		  examples/WWW/m4lib/feedback.m4 \
		  examples/WWW/m4lib/html.m4 \
		  examples/WWW/m4lib/index.m4 \
		  examples/WWW/m4lib/layout.m4 \
		  examples/WWW/m4lib/lists.m4 \
		  examples/WWW/m4lib/menu.m4 \
		  examples/WWW/m4lib/modules.m4 \
		  examples/WWW/m4lib/news.m4 \
		  examples/WWW/m4lib/readme.m4 \
		  examples/WWW/m4lib/setup.m4 \
		  examples/WWW/m4lib/test.m4 \
		  examples/WWW/m4lib/thanks.m4 \
		  examples/WWW/m4lib/thissite.m4 \
		  examples/WWW/m4lib/tmpl.m4 \
		  examples/WWW/m4lib/todo.m4 \
		  examples/WWW/m4lib/uses.m4 \
		  examples/WWW/m4lib/visions.m4 \
		  examples/WWW/m4lib/whatis.m4

dist_pkgdata_DATA = \
		  examples/capitalize.m4 \
		  examples/comments.m4 \
		  examples/ddivert.m4 \
		  examples/debug.m4 \
		  examples/esyscmd.m4 \
		  examples/exp.m4 \
		  examples/foreach.m4 \
		  examples/forloop.m4 \
		  examples/fstab.m4 \
		  examples/hanoi.m4 \
		  examples/incl-test.m4 \
		  examples/include.m4 \
		  examples/indir.m4 \
		  examples/misc.m4 \
		  examples/multiquotes.m4 \
		  examples/patsubst.m4 \
		  examples/pushpop.m4 \
		  examples/regexp.m4 \
		  examples/reverse.m4 \
		  examples/sysv-args.m4 \
		  examples/trace.m4 \
		  examples/translit.m4 \
		  examples/undivert.incl \
		  examples/undivert.m4 \
		  examples/wrap.m4

## ----------- ##
## Test suite. ##
## ----------- ##

# Use `$(srcdir)' for the benefit of non-GNU makes: this is
# how `testsuite' appears in our dependencies.
TESTSUITE = $(srcdir)/tests/testsuite
TESTSUITE_AT	= \
		  tests/builtins.at \
		  tests/freeze.at \
		  tests/generated.at \
		  tests/macros.at \
		  tests/modules.at \
		  tests/options.at \
		  tests/others.at \
		  tests/testsuite.at

EXTRA_DIST     += $(TESTSUITE) $(TESTSUITE_AT) $(OTHER_FILES) \
		  tests/atlocal.in \
		  tests/generate.awk \
		  tests/package.m4

# Be sure to reexport important environment variables:
TESTS_ENVIRONMENT = MAKE="$(MAKE)" CC="$(CC)" CFLAGS="$(CFLAGS)" \
	CPP="$(CPP)" CPPFLAGS="$(CPPFLAGS)" LD="$(LD)" LDFLAGS="$(LDFLAGS)" \
	LIBS="$(LIBS)" LN_S="$(LN_S)" NM="$(NM)" RANLIB="$(RANLIB)" \
	OBJEXT="$(OBJEXT)" EXEEXT="$(EXEEXT)" CONFIG_SHELL="$(SHELL)" \
	abs_top_builddir="$(abs_top_builddir)" \
	CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" CXXCPP="$(CXXCPP)" \
	F77="$(F77)" FFLAGS="$(FFLAGS)" \
	GCJ="$(GCJ)" GCJFLAGS="$(GCJFLAGS)"

# Using variables so that this snippet is not too wide and can
# be used as is in Texinfo @example/@end example.
m4_texinfo  = $(srcdir)/doc/m4.texinfo
generate    = $(AWK) -f $(srcdir)/tests/generate.awk
$(srcdir)/tests/generated.at: tests/generate.awk $(m4_texinfo)
	$(generate) $(m4_texinfo) >$@t
	mv $@t $@

AUTOM4TE = autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): tests/package.m4 $(TESTSUITE_AT)
	$(AUTOTEST) -I '$(srcdir)/tests' -o $@.tmp $@.at
	mv $@.tmp $@

$(srcdir)/tests/package.m4: configure.ac
	{					\
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} > $(srcdir)/tests/package.m4

tests/atconfig: config.status
	$(SHELL) ./config.status tests/atconfig
DISTCLEANFILES += tests/atconfig

# FIXME - After 2.60, autoconf might add an option to testsuite to specify
# which directory it should output testsuite.log.  Until then, we want it
# to drop in tests, so we have to do a cd inside the targets that invoke
# testsuite.  As a result, we need an absolute path since the relative
# $(srcdir) inside $(TESTSUITE) does not work after cd.
CD_TESTDIR	= cd ./tests

# Hook the test suite into the check rule
check-local: tests/atconfig $(TESTSUITE)
	$(CD_TESTDIR); \
	$(SHELL) '$(abs_srcdir)/tests/testsuite' $(TESTS_ENVIRONMENT) \
	  $(TESTSUITEFLAGS)

# Run the test suite on the *installed* tree.
installcheck-local: tests/atconfig $(TESTSUITE)
	$(CD_TESTDIR); \
	$(SHELL) '$(abs_srcdir)/tests/testsuite' $(TESTS_ENVIRONMENT) \
	  AUTOTEST_PATH="$(bindir)" $(TESTSUITEFLAGS)

# We need to remove any file droppings left behind by testsuite.
clean-local-tests:
	$(CD_TESTDIR); \
	test ! -f '$(abs_srcdir)/tests/testsuite' || \
	  $(SHELL) '$(abs_srcdir)/tests/testsuite' --clean

OTHER_FILES	= tests/iso8859.m4 tests/stackovf.test

DISTCLEANFILES += tests/atconfig tests/atlocal tests/m4
MAINTAINERCLEANFILES += tests/generated.at '$(TESTSUITE)'

clean-local: clean-local-src clean-local-tests

##                                                                      ##
##                   --- RULES FOR THE MAINTAINER ---                   ##
##           You shouldn't need to touch anything below here.           ##
##              (mostly copied from automake/Makefile.am)               ##
##                                                                      ##

CVS	= cvs # set it to `:' to avoid CVS operations at dist time

# Tag before making distribution.  Also, don't make a distribution if
# checks fail.  Also, make sure the NEWS file is up-to-date.
cvs-dist: distcheck # maintainer-check
	@if sed '1,2d;3q' $(srcdir)/NEWS | grep -e "$(VERSION)" > dev/null; \
	  echo "NEWS not updated; not releasing" 1>&2; \
	  exit 1;
	fi
	cd $(srcdir) && \
	  $(CVS) -q tag `echo "Release-$(VERSION)" | sed 's/\./-/g'`
	$(MAKE) dist

cvs-diff:
	thisver=`echo "Release-$(VERSION)" | sed 's/\./-/g'`; \
	if test -z "$$OLDVERSION"; then \
	  prevno=`echo "$(VERSION)" - 0.01 | bc | sed 's/^\./0./'`; \
	else prevno="$$OLDVERSION"; fi; \
	prevver=Release-`echo $$prevno | sed 's/\./-/g'`; \
	cvs -f rdiff -c -r $$prevver -r $$thisver $(PACKAGE) \
	    > $(PACKAGE)-$$prevno-$(VERSION).diff

.PHONY: web-manual
web-manual:
	@cd $(srcdir)/doc; \
	  $(SHELL) "$(abs_srcdir)/$(config_aux_dir)/gendocs.sh" $(PACKAGE) \
	    "GNU $(PACKAGE)- GNU macro processor"
	@echo " *** Upload the doc/manual directory to web-cvs."

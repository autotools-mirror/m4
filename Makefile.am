## Makefile.am - template for generating Makefile via Automake
##
## Copyright (C) 2000-2001, 2003-2010, 2013 Free Software Foundation,
## Inc.
##
## This file is part of GNU M4.
##
## GNU M4 is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## GNU M4 is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##
## Written by Gary V. Vaughan <gary@gnu.org>

config_aux_dir	= build-aux
config_macro_dir= ltdl/m4

SUBDIRS		= po gnu . doc tests/gnu

ACLOCAL_AMFLAGS = -I ltdl/m4
AUTOMAKE_OPTIONS = nostdinc

gitlog_to_changelog	= $(srcdir)/$(config_aux_dir)/gitlog-to-changelog
git_log_fix		= $(srcdir)/$(config_aux_dir)/git-log-fix
changelog		= $(srcdir)/ChangeLog
changelog_old		= $(srcdir)/ChangeLog.old

# Prime the variables that will be augmented by libtool.
# FIXME - the -export-symbols-regex ".*" is a hack to work around the
# fact that on cygwin, the convenience library libltdl exports symbols,
# which effectively disables the auto-exporting of all other symbols.
# Revisit this if libtool on cygwin improves.
# See http://lists.gnu.org/archive/html/libtool-patches/2007-02/msg00062.html
AM_CPPFLAGS	=
AM_CFLAGS	= $(WARN_CFLAGS) $(WERROR_CFLAGS)
AM_LDFLAGS	= -export-dynamic -export-symbols-regex ".*"
EXTRA_DIST	=
BUILT_SOURCES	=
CLEANFILES	=
DISTCLEANFILES	=
MOSTLYCLEANFILES=

include_HEADERS	=
noinst_LTLIBRARIES=
lib_LTLIBRARIES	=
EXTRA_LTLIBRARIES=

# Include Libtool's rules.
include ltdl/Makefile.inc

# Additional configuration.  Version management comes from suggestions
# given in build-aux/git-version-gen.
syntax_check_exceptions = \
	.x-sc_cast_of_argument_to_free \
	.x-sc_prohibit_tab_based_indentation \
	.x-sc_require_config_h \
	.x-sc_require_config_h_first \
	.x-update-copyright

AM_CPPFLAGS    += -Ignu -I$(srcdir)/gnu \
		  -Im4 -I$(srcdir)/m4
EXTRA_DIST     += bootstrap cfg.mk maint.mk .version .prev-version \
		  $(config_macro_dir)/gnulib-cache.m4 \
		  $(syntax_check_exceptions) $(gitlog_to_changelog)
BUILT_SOURCES  += .version
.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook: dist-tarball-version
dist-tarball-version:
	echo $(VERSION) > $(distdir)/.tarball-version

dist-hook: ChangeLog
changelog_start_date = 2011-01-01
$(changelog): FORCE
	$(AM_V_GEN)if test -d '$(srcdir)/.git'; then	\
	  $(gitlog_to_changelog)			\
	    --since=$(changelog_start_date) > '$@T';	\
	  rm -f '$@'; mv '$@T' '$@';			\
	  cat '$(changelog_old)' >> '$@';		\
	fi

MAINTAINERCLEANFILES = \
		  ABOUT-NLS INSTALL Makefile.in aclocal.m4 \
		  config-h.in configure stamp-h.in \
		  po/cat-id-tbl.c po/m4.pot po/stamp-cat-id


## ------- ##
## GNU M4. ##
## ------- ##

bin_PROGRAMS	= src/m4
src_m4_SOURCES	= \
		  src/xstrtoul.c \
		  src/xstrtol-error.c \
		  src/xstrtol.c \
		  src/xstrtol.h \
		  src/version-etc-fsf.c \
		  src/version-etc.c \
		  src/version-etc.h \
		  src/main.c \
		  src/m4.h \
		  src/freeze.c
if GETOPT
src_m4_SOURCES += \
		  src/getopt.c \
		  src/getopt1.c
endif
if STACKOVF
src_m4_SOURCES += src/stackovf.c
endif
src_m4_CPPFLAGS	= $(AM_CPPFLAGS) -Isrc -I$(srcdir)/src
src_m4_LDFLAGS	= $(AM_LDFLAGS) $(DLPREOPEN)
src_m4_LDADD	= m4/libm4.la $(LTLIBICONV) $(LTLIBTHREAD)
src_m4_DEPENDENCIES = $(PREOPEN_DEPENDENCIES) m4/libm4.la

##                                                                      ##
##                 --- PASTED MANUALLY FROM GNULIB ---                  ##
##     To avoid adding unnecessary objects to libm4.la these gnulib     ##
##     modules are not imported by gnulib-tool, but added manually.     ##
##     (from: gnulib-tool --extract-automake-snippet getopt-posix)      ##
##                                                                      ##
if GETOPT
BUILT_SOURCES  += src/$(GETOPT_H)
endif
EXTRA_DIST     += src/getopt.c src/getopt1.c src/getopt.in.h src/getopt_int.h

ARG_NONNULL_H = $(srcdir)/$(config_aux_dir)/arg-nonnull.h

# We need the following in order to create an <getopt.h> when the
# system doesn't have one that works with the given compiler.
src/getopt.h: src/getopt.in.h $(ARG_NONNULL_H)
	$(AM_V_GEN)rm -f $@-t $@ && \
	{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  sed -e 's|@''HAVE_GETOPT_H''@|$(HAVE_GETOPT_H)|g' \
	      -e 's|@''INCLUDE_NEXT''@|$(INCLUDE_NEXT)|g' \
	      -e 's|@''PRAGMA_SYSTEM_HEADER''@|@PRAGMA_SYSTEM_HEADER@|g' \
	      -e 's|@''NEXT_GETOPT_H''@|$(NEXT_GETOPT_H)|g' \
	      -e '/definition of _GL_ARG_NONNULL/r $(ARG_NONNULL_H)' \
	      < $(srcdir)/src/getopt.in.h; \
	} > $@-t && \
	mv -f $@-t $@
MOSTLYCLEANFILES += src/getopt.h src/getopt.h-t
##                                                                      ##
##                     --- END OF PASTED GNULIB ---                     ##
##                                                                      ##

## -------- ##
## Modules. ##
## -------- ##

pkgmodincludedir= $(pkgincludedir)/modules

ETAGS_ARGS	= --regex='/M4BUILTIN_HANDLER[ \t]*)\([^)]*\)/\1/'

module_ldflags	= -module $(AM_LDFLAGS)
module_libadd	= m4/libm4.la

pkgmodinclude_HEADERS = modules/m4.h

pkglib_LTLIBRARIES = \
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
pkglib_LTLIBRARIES	       += modules/mpeval.la
EXTRA_modules_mpeval_la_SOURCES	= modules/evalparse.c
modules_mpeval_la_LDFLAGS	= $(module_ldflags)
modules_mpeval_la_LIBADD	= $(module_libadd) $(LIBADD_GMP)
endif

## Disabled for now.  It is too fragile to be useful.
## pkglib_LTLIBRARIES         += modules/perl.la
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
		  tests/import.la \
		  tests/modtest.la \
		  tests/shadow.la \
		  tests/stdlib.la \
		  tests/time.la

tests_import_la_LDFLAGS		= $(module_ldflags) $(module_check)
tests_import_la_LIBADD		= $(module_libadd)
tests_import_la_SOURCES		= modules/import.c

tests_modtest_la_LDFLAGS	= $(module_ldflags) $(module_check)
tests_modtest_la_LIBADD		= $(module_libadd)
tests_modtest_la_SOURCES	= modules/modtest.c

tests_shadow_la_LDFLAGS		= $(module_ldflags) $(module_check)
tests_shadow_la_LIBADD		= $(module_libadd)
tests_shadow_la_SOURCES		= modules/shadow.c

tests_stdlib_la_LDFLAGS		= $(module_ldflags) $(module_check)
tests_stdlib_la_LIBADD		= $(module_libadd)
tests_stdlib_la_SOURCES		= modules/stdlib.c

tests_time_la_LDFLAGS		= $(module_ldflags) $(module_check)
tests_time_la_LIBADD		= $(module_libadd)
tests_time_la_SOURCES		= modules/time.c


## ----- ##
## libm4 ##
## ----- ##

lib_LTLIBRARIES	+= m4/libm4.la

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
		  m4/resyntax.c \
		  m4/symtab.c \
		  m4/syntax.c \
		  m4/utility.c
m4_libm4_la_LIBADD = gnu/libgnu.la \
		  $(LIBLTDL) $(LTLIBINTL) $(LIBADD_DL)
m4_libm4_la_DEPENDENCIES = $(LTDLDEPS) gnu/libgnu.la

# This file needs to be regenerated at configure time.
dist-hook:
	rm -f $(distdir)/m4/system.h
EXTRA_DIST     += m4/system_.h


## --------- ##
## Examples. ##
## --------- ##

EXTRA_DIST     += \
		  examples/WWW/Makefile \
		  examples/WWW/_footer.htm \
		  examples/WWW/_header.htm \
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
		  examples/capitalize2.m4 \
		  examples/comments.m4 \
		  examples/curry.m4 \
		  examples/ddivert.m4 \
		  examples/debug.m4 \
		  examples/esyscmd.m4 \
		  examples/exp.m4 \
		  examples/foreach.m4 \
		  examples/foreach2.m4 \
		  examples/foreachq.m4 \
		  examples/foreachq2.m4 \
		  examples/foreachq3.m4 \
		  examples/foreachq4.m4 \
		  examples/forloop.m4 \
		  examples/forloop2.m4 \
		  examples/forloop3.m4 \
		  examples/fstab.m4 \
		  examples/hanoi.m4 \
		  examples/incl-test.m4 \
		  examples/incl.m4 \
		  examples/include.m4 \
		  examples/indir.m4 \
		  examples/join.m4 \
		  examples/loop.m4 \
		  examples/misc.m4 \
		  examples/multiquotes.m4 \
		  examples/patsubst.m4 \
		  examples/pushpop.m4 \
		  examples/quote.m4 \
		  examples/regexp.m4 \
		  examples/reverse.m4 \
		  examples/stack.m4 \
		  examples/stack_sep.m4 \
		  examples/sysv-args.m4 \
		  examples/trace.m4 \
		  examples/translit.m4 \
		  examples/undivert.incl \
		  examples/undivert.m4 \
		  examples/wrap.m4 \
		  examples/wrapfifo.m4 \
		  examples/wraplifo.m4 \
		  examples/wraplifo2.m4

EXTRA_DIST     += \
		  examples/modtest.m4 \
		  examples/shadow.m4 \
		  examples/time.m4 \
		  examples/time2.m4 \
		  examples/stdlib.m4

## ----------- ##
## Test suite. ##
## ----------- ##

# Use `$(srcdir)' for the benefit of non-GNU makes: this is
# how `testsuite' appears in our dependencies.
TESTSUITE = $(srcdir)/tests/testsuite
TESTSUITE_AT	= \
		  tests/builtins.at \
		  tests/freeze.at \
		  $(srcdir)/tests/generated.at \
		  tests/macros.at \
		  tests/modules.at \
		  tests/options.at \
		  tests/others.at \
		  tests/testsuite.at

EXTRA_DIST     += $(TESTSUITE) $(TESTSUITE_AT) $(OTHER_FILES) \
		  tests/atlocal.in \
		  tests/generate.awk \
		  $(srcdir)/tests/package.m4

# Using variables so that this snippet is not too wide and can
# be used as is in Texinfo @example/@end example.
m4_texi     = $(srcdir)/doc/m4.texi
generate    = $(AWK) -f $(srcdir)/tests/generate.awk
$(srcdir)/tests/generated.at: tests/generate.awk $(m4_texi)
	$(generate) $(m4_texi) >$@t
	mv $@t $@

AUTOM4TE = $(SHELL) $(srcdir)/$(config_aux_dir)/missing --run autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/tests/package.m4 $(TESTSUITE_AT)
	$(AUTOTEST) -I '$(srcdir)/tests' -o $@.tmp $@.at
	mv $@.tmp $@

# The leading :; works around a bug in {}>f in some bash versions.
$(srcdir)/tests/package.m4: .version
	:; \
	{ \
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [$(PACKAGE_NAME)])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [$(PACKAGE_TARNAME)])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [$(PACKAGE_VERSION)])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [$(PACKAGE_STRING)])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [$(PACKAGE_BUGREPORT)])'; \
	} > $(srcdir)/tests/package.m4

tests/atconfig: config.status
	$(SHELL) ./config.status tests/atconfig
DISTCLEANFILES += tests/atconfig

# Hook the test suite into the check rule
check-local: tests/atconfig tests/atlocal tests/m4 $(TESTSUITE) \
		$(check_LTLIBRARIES)
	$(SHELL) '$(srcdir)/tests/testsuite' -C tests $(TESTSUITEFLAGS)

# Run the test suite on the *installed* tree, including any renames
# the user requested.
installcheck-local: tests/atconfig tests/atlocal $(TESTSUITE) \
		$(check_LTLIBRARIES)
	$(SHELL) '$(srcdir)/tests/testsuite' -C tests \
	  AUTOTEST_PATH="$(bindir)" \
	  M4="`echo m4 | sed '$(program_transform_name)'`" $(TESTSUITEFLAGS)

# Enough users install GNU M4 as gm4 that we make sure 'make installcheck'
# will handle that, as part of making a release.
DISTCHECK_CONFIGURE_FLAGS = \
	--disable-assert \
	--program-prefix=g \
	--enable-cxx

# We need to remove any file droppings left behind by testsuite.
clean-local-tests:
	test ! -f '$(srcdir)/tests/testsuite' || \
	  $(SHELL) '$(srcdir)/tests/testsuite' -C tests --clean

OTHER_FILES	= tests/iso8859.m4 tests/stackovf.test \
		tests/null.m4 tests/null.out tests/null.err

DISTCLEANFILES += tests/atconfig tests/atlocal tests/m4
MAINTAINERCLEANFILES += $(srcdir)/tests/generated.at '$(TESTSUITE)'

clean-local: clean-local-tests

FORCE:

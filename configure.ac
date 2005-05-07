# Configure template for GNU m4.			-*-Autoconf-*-
# Copyright (C) 1991, 1992, 1993, 1994, 2000, 2001, 2002, 2004, 2005
# Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

AC_PREREQ([2.59])

## ------------------------ ##
## Autoconf initialisation. ##
## ------------------------ ##
AC_INIT([GNU m4], [1.9a], [bug-m4@gnu.org])

AC_CONFIG_SRCDIR([src/m4.h])
AC_CONFIG_AUX_DIR([ltdl/config])
AC_CONFIG_MACRO_DIR([ltdl/m4])
AC_CONFIG_LIBOBJ_DIR([gnu])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_HEADERS([config.h:config-h.in])
AC_CONFIG_FILES([tests/m4], [chmod +x tests/m4])



## ---------------------------------------- ##
## Display a configure time version banner. ##
## ---------------------------------------- ##
TIMESTAMP=
case AC_PACKAGE_VERSION in
  *[[acegikmoqsuwy]])
    TIMESTAMP=`$CONFIG_SHELL $ac_aux_dir/mkstamp < $srcdir/ChangeLog`
    test -z "$TIMESTAMP" || TIMESTAMP=" (Build:$TIMESTAMP)"
    AS_BOX([Configuring AC_PACKAGE_TARNAME][$TIMESTAMP AC_PACKAGE_VERSION])
    echo
    ;;
esac
AC_DEFINE_UNQUOTED([TIMESTAMP], ["$TIMESTAMP"],
    [Defined to a CVS timestamp for alpha releases of M4])



## -------------------------- ##
## M4 specific configuration. ##
## -------------------------- ##
m4_pattern_allow([^m4_cv_])

# We use some of Jim's macros.
dnl m4_pattern_forbid([^jm_])
dnl m4_pattern_allow([^jm_cv_])
dnl m4_pattern_allow([^jm_ac_cv_])
dnl m4_pattern_allow([^jm_with_])

AC_MSG_CHECKING(for modules to preload)
  m4_pattern_allow([^m4_default_preload$])
  m4_default_preload="m4 traditional gnu"
  DLPREOPEN=

  AC_ARG_WITH([modules],
    [AC_HELP_STRING([--with-modules=MODULES],
                    [preload MODULES @<:@default: $m4_default_preload@:>@])],
    [use_modules="$withval"],
    [use_modules="$m4_default_preload"])

  DLPREOPEN="-dlpreopen force"
  if test -z "$use_modules"; then
    use_modules=none
  else
    if test "$use_modules" != yes; then
      for module in $use_modules; do
        DLPREOPEN="$DLPREOPEN -dlpreopen modules/$module.la"
      done
    fi
  fi
AC_MSG_RESULT($use_modules)
AC_SUBST(DLPREOPEN)



## ------------------------ ##
## Automake Initialisation. ##
## ------------------------ ##
AM_INIT_AUTOMAKE([1.9.5 subdir-objects dist-bzip2])



## ------------------ ##
## C compiler checks. ##
## ------------------ ##
AC_PROG_CC

gl_EARLY
AC_ISC_POSIX
AM_PROG_CC_STDC
AC_PROG_CPP
AM_PROG_CC_C_O
m4_CHECK_DEBUGGING

# Use gcc's -pipe option if available: for faster compilation.
case "$CFLAGS" in
  *-pipe* ) ;;
        * ) _LT_COMPILER_OPTION([if $compiler supports -pipe],
                [m4_cv_prog_compiler_pipe],
                [-pipe -c conftest.$ac_ext], [],
                [CFLAGS="$CFLAGS -pipe"])
              ;;
esac



## ----------------------- ##
## Libtool initialisation. ##
## ----------------------- ##
LT_PREREQ([2.0])
LT_INIT([shared dlopen win32-dll])
LT_WITH_LTDL([ltdl])



## ---------------- ##
## Gettext support. ##
## ---------------- ##
AM_GNU_GETTEXT(external, need-ngettext)
AM_GNU_GETTEXT_VERSION(0.14.1)
m4_GNU_GETTEXT



## --------------- ##
## Gnulib support, ##
## --------------- ##
gl_SOURCE_BASE(gnu)
gl_M4_BASE(ltdl/m4)
# We also use getopt and version-etc, but they are copied into
# the project tree by bootstrap since they are used only by m4 itself.
gl_MODULES(assert error exit free gettext mkstemp obstack progname regex stdbool strtol xalloc xalloc-die xstrndup)
gl_INIT

# Gnulib doesn't always do things quite the way M4 would like...
m4_ERROR
m4_GETOPT
m4_OBSTACK
m4_REGEX


## ------------------------ ##
## Other external programs. ##
## ------------------------ ##
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG(PERL, perl)
AC_PROG_AWK



## --------------------------- ##
## C compiler characteristics. ##
## --------------------------- ##
AM_C_PROTOTYPES
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([long long int])



## ------------------------- ##
## C headers required by M4. ##
## ------------------------- ##
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h locale.h memory.h string.h unistd.h errno.h])
AC_CHECK_HEADERS([signal.h sys/signal.h], break)

if test $ac_cv_header_stdbool_h = yes; then
  INCLUDE_STDBOOL_H='#include <stdbool.h>'
else
  INCLUDE_STDBOOL_H='#include <gnu/stdbool.h>'
fi
AC_SUBST([INCLUDE_STDBOOL_H])



## --------------------------------- ##
## Library functions required by M4. ##
## --------------------------------- ##
AC_CHECK_FUNCS([bzero calloc mkstemp mktemp strerror tmpfile])
AC_FUNC_ALLOCA
AC_FUNC_VPRINTF

AM_WITH_DMALLOC

m4_SYS_STACKOVF

# This is for the modules
AC_STRUCT_TM
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(getcwd gethostname mktime uname)
AC_CHECK_FUNCS(setenv unsetenv putenv clearenv)

m4_LIB_GMP
AM_CONDITIONAL([USE_GMP], [test "x$USE_GMP" = xyes])



## -------- ##
## Outputs. ##
## -------- ##
AC_CONFIG_FILES([
Makefile
gnu/Makefile
m4/system.h:m4/system_.h
tests/atlocal
])

AC_OUTPUT

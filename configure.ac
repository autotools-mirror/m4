# Configure template for GNU m4.			-*-Autoconf-*-
# Copyright (C) 1991, 1992, 1993, 1994, 2000, 2001, 2002, 2004, 2005, 2006
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

AC_PREREQ([2.60])

## ------------------------ ##
## Autoconf initialisation. ##
## ------------------------ ##
AC_INIT([GNU M4], [1.9a], [bug-m4@gnu.org])

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
    test -z "$TIMESTAMP" || TIMESTAMP=" Build:$TIMESTAMP"
    AS_BOX([Configuring AC_PACKAGE_TARNAME][$TIMESTAMP AC_PACKAGE_VERSION])
    echo
    ;;
esac
AC_DEFINE_UNQUOTED([TIMESTAMP], ["$TIMESTAMP"],
    [Defined to a CVS timestamp for alpha releases of M4])



## -------------------------- ##
## M4 specific configuration. ##
## -------------------------- ##
dnl Autoconf recommends that packages use lowercase for their package-specific
dnl prefix for cache variables.  But in the case of m4, that collides with
dnl the m4_ namespace provided by m4sugar, so we prefer M4_ as our
dnl package-specific prefix.  FIXME - consistently use M4_ in the m4 macros
dnl that feed the creation of configure.
m4_pattern_forbid([^M4_])
m4_pattern_allow([^M4_cv_])

AC_DEFUN([M4_DEFAULT_PRELOAD], [m4 traditional gnu])
AC_MSG_CHECKING([for modules to preload])
  DLPREOPEN=

  AC_ARG_WITH([modules],
    [AS_HELP_STRING([--with-modules=MODULES],
		    [preload MODULES @<:@default: ]M4_DEFAULT_PRELOAD[@:>@])],
    [use_modules="$withval"],
    [use_modules="M4_DEFAULT_PRELOAD"])

  PREOPEN_DEPENDENCIES=
  DLPREOPEN="-dlpreopen force"
  if test -z "$use_modules"; then
    use_modules=none
  else
    if test "$use_modules" != yes; then
      for module in $use_modules; do
	DLPREOPEN="$DLPREOPEN -dlpreopen modules/$module.la"
	PREOPEN_DEPENDENCIES="$PREOPEN_DEPENDENCIES modules/$module.la"
      done
    fi
  fi
AC_MSG_RESULT([$use_modules])
AC_SUBST([DLPREOPEN])
AC_SUBST([PREOPEN_DEPENDENCIES])



## ------------------------ ##
## Automake Initialisation. ##
## ------------------------ ##
AM_INIT_AUTOMAKE([1.9.6 subdir-objects dist-bzip2 gnits])



## ------------------ ##
## C compiler checks. ##
## ------------------ ##
AC_PROG_CC
M4_EARLY

AC_PROG_CPP
AM_PROG_CC_C_O
M4_CHECK_DEBUGGING



## ----------------------- ##
## Libtool initialisation. ##
## ----------------------- ##
LT_PREREQ([2.0])
LT_CONFIG_LTDL_DIR([ltdl])
LT_INIT([shared dlopen win32-dll])
LT_WITH_LTDL([ltdl])

# Use gcc's -pipe option if available: for faster compilation.
case "$CFLAGS" in
  *-pipe* ) ;;
	* ) _LT_COMPILER_OPTION([if $compiler supports -pipe],
		[M4_cv_prog_compiler_pipe],
		[-pipe -c conftest.$ac_ext], [],
		[CFLAGS="$CFLAGS -pipe"])
	      ;;
esac



## ---------------- ##
## Gettext support. ##
## ---------------- ##
AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION([0.14.5])
M4_GNU_GETTEXT



## --------------- ##
## Gnulib support, ##
## --------------- ##
M4_INIT

# Gnulib doesn't always do things quite the way M4 would like...
M4_ERROR
M4_GETOPT
M4_OBSTACK
M4_REGEX


## ------------------------ ##
## Other external programs. ##
## ------------------------ ##
AC_PATH_PROG([PERL], [perl])



## --------------------------- ##
## C compiler characteristics. ##
## --------------------------- ##
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([long long int])



## ------------------------- ##
## C headers required by M4. ##
## ------------------------- ##
AC_CHECK_HEADERS_ONCE([limits.h])

if test $ac_cv_header_stdbool_h = yes; then
  INCLUDE_STDBOOL_H='#include <stdbool.h>'
else
  INCLUDE_STDBOOL_H='#include <gnu/stdbool.h>'
fi
AC_SUBST([INCLUDE_STDBOOL_H])



## --------------------------------- ##
## Library functions required by M4. ##
## --------------------------------- ##
AC_CHECK_FUNCS_ONCE([calloc strerror tmpfile])

AM_WITH_DMALLOC

M4_SYS_STACKOVF

# This is for the modules
AC_STRUCT_TM
AC_FUNC_STRFTIME
AC_CHECK_FUNCS_ONCE([getcwd gethostname mktime uname])
AC_CHECK_FUNCS_ONCE([setenv unsetenv putenv clearenv])

M4_LIB_GMP
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

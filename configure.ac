# Configure template for GNU m4.			-*-Autoconf-*-
# Copyright (C) 1991, 1992, 1993, 1994, 2000, 2001, 2002, 2004, 2005, 2006,
# 2007 Free Software Foundation, Inc.
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
## Autoconf initialization. ##
## ------------------------ ##
AC_INIT([GNU M4], [1.9a], [bug-m4@gnu.org])

AC_CONFIG_SRCDIR([src/m4.h])
AC_CONFIG_AUX_DIR([ltdl/config])
AC_CONFIG_MACRO_DIR([ltdl/m4])
AC_CONFIG_LIBOBJ_DIR([gnu])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_HEADERS([gnu/config.h:gnu/config.hin])
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
AB_INIT()


## -------------------------- ##
## M4 specific configuration. ##
## -------------------------- ##
dnl Autoconf recommends that packages use lowercase for their package-specific
dnl prefix for cache variables.  But in the case of m4, that collides with
dnl the m4_ namespace provided by m4sugar, so we prefer M4_ as our
dnl package-specific prefix.
m4_pattern_forbid([^M4_[A-Z]])

AC_DEFUN([M4_DEFAULT_PRELOAD], [m4 traditional gnu])
M4_default_preload="M4_DEFAULT_PRELOAD"


## ------------------------ ##
## Automake Initialization. ##
## ------------------------ ##
AM_INIT_AUTOMAKE([1.10a subdir-objects dist-bzip2 gnits])



## ------------------ ##
## C compiler checks. ##
## ------------------ ##
AC_PROG_CC
M4_EARLY

AC_SYS_LARGEFILE
AC_PROG_CPP
AM_PROG_CC_C_O
M4_CHECK_DEBUGGING



## ----------------------- ##
## Libtool initialization. ##
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

## ------------------------------- ##
## Preloaded module configuration. ##
## ------------------------------- ##
AS_IF([test "x$enable_shared" != xno],
      [DYNAMIC_MODULES=yes], [DYNAMIC_MODULES=no])
AC_SUBST([DYNAMIC_MODULES], [$DYNAMIC_MODULES])

AC_MSG_CHECKING([for modules to preload])
  DLPREOPEN=

  AC_ARG_WITH([modules],
    [AS_HELP_STRING([--with-modules=MODULES],
		    [preload MODULES @<:@default: ]M4_DEFAULT_PRELOAD[@:>@])],
    [use_modules="$withval"],
    [use_modules="$M4_default_preload"])

  PREOPEN_DEPENDENCIES=
  DLPREOPEN="-dlpreopen force"
  if test -z "$use_modules"; then
    use_modules=none
  else
    if test "$use_modules" != yes; then
      for module in $use_modules; do
	case $module in
	  no|none) break ;;
	  m4|traditional|gnu|load|mpeval) dir=modules ;;
	  import|modtest|shadow|stdlib|time) dir=tests ;;
	  *) AC_MSG_ERROR([Unrecognized module `$module' in --with-modules])
	    ;;
	esac
	DLPREOPEN="$DLPREOPEN -dlpreopen $dir/$module.la"
	PREOPEN_DEPENDENCIES="$PREOPEN_DEPENDENCIES $dir/$module.la"
      done
    fi
  fi
AC_MSG_RESULT([$use_modules])
AC_SUBST([DLPREOPEN])
AC_SUBST([PREOPEN_DEPENDENCIES])


## ---------------- ##
## Gettext support. ##
## ---------------- ##
dnl M4 1.4.4 and earlier had a bug that tracing a macro made it falsely
dnl appear as defined with an empty definition, even though `invoking'
dnl the macro resulted in the macro name.  Gettext relies on whether
dnl AM_GNU_GETTEXT_INTL_SUBDIR is defined, and automake traces this
dnl macro, but we don't want to use an intl subdir, hence this workaround
dnl to allow bootstrapping even on systems with old M4.
dnl http://lists.gnu.org/archive/html/bug-gnu-utils/2006-11/msg00096.html
m4_ifdef([AM_GNU_GETTEXT_INTL_SUBDIR],
  [m4_if(m4_defn([AM_GNU_GETTEXT_INTL_SUBDIR]),[],
    [m4_ifval(AM_GNU_GETTEXT_INTL_SUBDIR,
      [m4_popdef([AM_GNU_GETTEXT_INTL_SUBDIR])])])])

AM_GNU_GETTEXT([external], [need-formatstring-macros])
AM_GNU_GETTEXT_VERSION([0.16])
M4_GNU_GETTEXT



## --------------- ##
## Gnulib support. ##
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
AC_CHECK_HEADERS_ONCE([limits.h sys/wait.h])

if test $ac_cv_header_stdbool_h = yes; then
  INCLUDE_STDBOOL_H='#include <stdbool.h>'
else
  INCLUDE_STDBOOL_H='#include <gnu/stdbool.h>'
fi
AC_SUBST([INCLUDE_STDBOOL_H])



## --------------------------------- ##
## Library functions required by M4. ##
## --------------------------------- ##
AC_CHECK_FUNCS_ONCE([calloc strerror])

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

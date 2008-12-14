# Configure template for GNU m4.			-*-Autoconf-*-
# Copyright (C) 1991, 1992, 1993, 1994, 2000, 2001, 2002, 2004, 2005, 2006,
# 2007, 2008 Free Software Foundation, Inc.
#
# This file is part of GNU M4.
#
# GNU M4 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GNU M4 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl We depend on autotest's ./testsuite -C.
AC_PREREQ([2.62])

## ------------------------ ##
## Autoconf initialization. ##
## ------------------------ ##
m4_define([M4_VERSION],
	  m4_esyscmd([build-aux/git-version-gen .tarball-version]))
m4_bmatch(m4_defn([M4_VERSION]), [^[0-9]], [],
	  [m4_define([M4_VERSION], [1.9a])])
AC_INIT([GNU M4], m4_defn([M4_VERSION]), [bug-m4@gnu.org])

AC_CONFIG_SRCDIR([src/m4.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([ltdl/m4])
AC_CONFIG_LIBOBJ_DIR([gnu])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_HEADERS([gnu/config.h:gnu/config.hin])
AC_CONFIG_FILES([tests/m4], [chmod +x tests/m4])

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
AM_INIT_AUTOMAKE([1.10a subdir-objects dist-bzip2 dist-lzma]
m4_bmatch(m4_defn([M4_VERSION]), [-], [gnu], [gnits]))



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
LT_PREREQ([2.2])
LT_CONFIG_LTDL_DIR([ltdl])
LT_INIT([shared dlopen win32-dll])
LTDL_INIT([nonrecursive])

# The lt_dlhandle_iterate symbol was added with libtool-2.2
if test "x$with_included_ltdl" != "xyes"; then
  save_CFLAGS="$CFLAGS"
  save_LDFLAGS="$LDFLAGS"
  CFLAGS="$CFLAGS $LTDLINCL"
  LDFLAGS="$LDFLAGS $LIBLTDL"
  AC_CHECK_LIB([ltdl], [lt_dlhandle_iterate],
	     [],
     [AC_MSG_ERROR([installed libltdl is too old])])
  LDFLAGS="$save_LDFLAGS"
  CFLAGS="$save_CFLAGS"
fi


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
M4_RENAME


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
tests/gnu/Makefile
])

AC_OUTPUT

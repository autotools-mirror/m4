# Configure template for GNU m4.           -*-Autoconf-*-
# Copyright (C) 1991-1994, 2000-2002, 2004-2013 Free Software
# Foundation, Inc.
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
AC_CONFIG_MACRO_DIR([build-aux/m4])
AC_CONFIG_LIBOBJ_DIR([m4/gnu])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([tests/m4], [chmod +x tests/m4])


## -------------------------- ##
## M4 specific configuration. ##
## -------------------------- ##
dnl Autoconf recommends that packages use lowercase for their package-specific
dnl prefix for cache variables.  But in the case of m4, that collides with
dnl the m4_ namespace provided by m4sugar, so we prefer M4_ as our
dnl package-specific prefix.
m4_pattern_forbid([^M4_[A-Z]])

AC_DEFUN([M4_DEFAULT_PRELOAD], [])
M4_default_preload="M4_DEFAULT_PRELOAD"


## ------------------------ ##
## Automake Initialization. ##
## ------------------------ ##
AM_INIT_AUTOMAKE([1.10b subdir-objects dist-bzip2 dist-xz
color-tests parallel-tests silent-rules]
m4_if(m4_index(m4_defn([M4_VERSION]), [-]), [-1], [gnits], [gnu]))
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


## ------------------ ##
## C compiler checks. ##
## ------------------ ##
AC_PROG_CC
AC_DEFUN([gl_CXX_CHOICE_DEFAULT_NO])
M4_EARLY

# Maintainer note - comment this line out if you plan to rerun
# GNULIB_POSIXCHECK testing to see if M4 should be using more modules.
# Leave it uncommented for normal releases, for faster ./configure.
gl_ASSERT_NO_GNULIB_POSIXCHECK

AC_SYS_LARGEFILE
AC_PROG_CPP
AM_PROG_CC_C_O
M4_CHECK_DEBUGGING


## ----------------------- ##
## Libtool initialization. ##
## ----------------------- ##
LT_PREREQ([2.2])
LT_INIT([shared dlopen win32-dll])

AC_ARG_ENABLE([gcc-warnings],
  [AS_HELP_STRING([--enable-gcc-warnings],
                  [turn on lots of GCC warnings (for developers)])],
  [case $enableval in
     yes|no) ;;
     *)      AC_MSG_ERROR([bad value $enableval for gcc-warnings option]) ;;
   esac
   gl_gcc_warnings=$enableval],
  [gl_gcc_warnings=no]
)

if test "$gl_gcc_warnings" = yes; then
  gl_WARN_ADD([-Werror], [WERROR_CFLAGS])
  AC_SUBST([WERROR_CFLAGS])

  # This, $nw, is the list of warnings we disable.
  nw=
  nw="$nw -Waggregate-return"       # K&R is anachronistic
  nw="$nw -Wtraditional-conversion" # K&R is anachronistic
  nw="$nw -Wundef"                  # K&R is anachronistic
  nw="$nw -Wlong-long"              # C90 is anachronistic
  nw="$nw -Wsystem-headers"         # Don't let system headers trigger warnings
  nw="$nw -Wpadded"                 # Our structs are not packed
  nw="$nw -Wformat-nonliteral"      # Needed in builtin.c
  nw="$nw -Wconversion"             # Too many warnings for now
  nw="$nw -Wsign-conversion"        # Too many warnings for now
  nw="$nw -Wcast-qual"              # Too many warnings for now
  nw="$nw -Wswitch-enum"            # Too many warnings for now
  # gcc 4.4.6 complains enum-compare is C++ only; gcc 4.7.0 implies it in -Wall
  nw="$nw -Wenum-compare"

  # Gnulib uses '#pragma GCC diagnostic push' to silence some
  # warnings, but older gcc doesn't support this.
  AC_CACHE_CHECK([whether pragma GCC diagnostic push works],
    [M4_cv_gcc_pragma_push_works], [
    save_CFLAGS=$CFLAGS
    CFLAGS='-Wunknown-pragmas -Werror'
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      #pragma GCC diagnostic push
      #pragma GCC diagnostic pop
    ]])],
    [M4_cv_gcc_pragma_push_works=yes],
    [M4_cv_gcc_pragma_push_works=no])
    CFLAGS=$save_CFLAGS])
  if test $M4_cv_gcc_pragma_push_works = no; then
    nw="$nw -Wmissing-prototypes"
    nw="$nw -Wmissing-declarations"
    nw="$nw -Wunreachable-code"
  fi

  gl_MANYWARN_ALL_GCC([ws])
  gl_MANYWARN_COMPLEMENT([ws], [$ws], [$nw])
  for w in $ws; do
    gl_WARN_ADD([$w])
  done

  gl_WARN_ADD([-fdiagnostics-show-option])
  gl_WARN_ADD([-funit-at-a-time])

  AC_SUBST([WARN_CFLAGS])

  AH_VERBATIM([FORTIFY_SOURCE],
  [/* Enable compile-time and run-time bounds-checking, and some warnings,
      without upsetting newer glibc. */
   #if defined __OPTIMIZE__ && __OPTIMIZE__
   # define _FORTIFY_SOURCE 2
   #endif
  ])
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
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_NEED([need-formatstring-macros])
AM_GNU_GETTEXT_VERSION([0.16])
M4_GNU_GETTEXT



## --------------- ##
## Gnulib support. ##
## --------------- ##
M4_INIT

gl_VERSION_ETC

# Gnulib doesn't always do things quite the way M4 would like...
M4_ERROR
M4_GETOPT
M4_OBSTACK
M4_REGEX
M4_RENAME


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
AC_CHECK_FUNCS_ONCE([calloc strerror])

AM_WITH_DMALLOC

M4_SYS_STACKOVF

# This is for the modules
AC_STRUCT_TM
AC_FUNC_STRFTIME
AC_CHECK_FUNCS_ONCE([getcwd gethostname mktime uname])


## ------------------ ##
## Configure options. ##
## ------------------ ##

M4_LIB_GMP
AM_CONDITIONAL([USE_GMP], [test "x$USE_GMP" = xyes])
M4_SYSCMD


## -------- ##
## Outputs. ##
## -------- ##
AC_CONFIG_FILES([
Makefile
m4/gnu/Makefile
m4/system.h:m4/system_.h
tests/atlocal
tests/gnu/Makefile
])

AC_OUTPUT

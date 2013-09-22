##                                                           -*- Autoconf -*-
## debug.m4 -- massage compiler flags for debugging/optimisation
##
## Copyright (C) 2000-2001, 2003, 2005-2007, 2010, 2013 Free Software
## Foundation, Inc.
## Copyright (C) 1999-2000 Ralf S. Engelschall
## Written by <rse@engelschall.com>
## Modified for M4 by Gary V. Vaughan <gary@gnu.org>
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

# serial 9

# M4_CHECK_DEBUGGING
# ------------------
# Debugging Support
AC_DEFUN([M4_CHECK_DEBUGGING],
[AC_REQUIRE([AC_PROG_CC])
AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
                           [build for debugging [default=no]])])
AC_MSG_CHECKING([for compilation debug mode])
AC_MSG_RESULT([${enable_debug-no}])

: ${rm=rm -f}
: ${RM=rm -f}

set dummy $CC
compiler="${compiler-[$]2}"
test -n "$rm" || rm="rm -f"

if test "X$enable_debug" = Xyes; then
  AC_DISABLE_SHARED
  AC_DEFINE([DEBUG], [1],
      [Define this to enable additional runtime debugging])
  M4_default_preload="m4 traditional gnu load \
import modtest mpeval shadow stdlib time"
  if test "$GCC" = yes; then
    case "$CFLAGS" in
      *-O* ) CFLAGS=`echo $CFLAGS | $SED 's/-O[[^ ]]* / /;s/-O[[^ ]]*$//'` ;;
    esac
    case "$CFLAGS" in
        *-g* ) ;;
         * ) AC_LIBTOOL_COMPILER_OPTION([if $compiler accepts -ggdb3],
                 [M4_cv_prog_compiler_ggdb3],
                 [-ggdb3 -c conftest.$ac_ext], [],
                 [CFLAGS="$CFLAGS -ggdb3"],
               [CFLAGS="$CFLAGS -g"])
             ;;
    esac
    CFLAGS="$CFLAGS -Wall"
    WMORE="-Wshadow -Wpointer-arith -Wcast-align -Wnested-externs"
    WMORE="$WMORE -Wmissing-prototypes -Wmissing-declarations -Winline"
    AC_LIBTOOL_COMPILER_OPTION([if $compiler accepts $WMORE],
        [M4_cv_prog_compiler_warning_flags],
        [$WMORE -c conftest.$ac_ext], [],
        [CFLAGS="$CFLAGS $WMORE"])

    AC_LIBTOOL_COMPILER_OPTION([if $compiler accepts -Wno-long-long],
        [M4_cv_prog_compiler_wnolonglong],
        [-Wno-long-long -c conftest.$ac_ext], [],
        [CFLAGS="$CFLAGS -Wno-long-long"])
  else
    case "$CFLAGS" in
        *-g* ) ;;
           * ) CFLAGS="$CFLAGS -g" ;;
    esac
  fi
else
  AC_ENABLE_SHARED
  case "$CFLAGS" in
    *-g* ) CFLAGS=`echo "$CFLAGS" |\
               $SED -e 's/ -g / /g;s/ -g$//;s/^-g //g;s/^-g$//'`
           ;;
  esac
  case "$CXXFLAGS" in
    *-g* ) CXXFLAGS=`echo "$CXXFLAGS" |\
               $SED -e 's/ -g / /g;s/ -g$//;s/^-g //g;s/^-g$//'`
           ;;
  esac
fi
])# M4_CHECK_DEBUGGING

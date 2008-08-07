# Configure template for GNU M4.			-*-Autoconf-*-
# Copyright (C) 1991, 1993, 1994, 2004, 2005, 2006, 2007, 2008 Free
# Software Foundation, Inc.
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

AC_PREREQ([2.60])
AC_INIT([GNU M4], m4_esyscmd([build-aux/git-version-gen .tarball-version]),
  [bug-m4@gnu.org])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.10.1 dist-bzip2 dist-lzma gnu])

m4_pattern_forbid([^M4_[A-Z]])

AC_CONFIG_SRCDIR([src/m4.h])
AC_CONFIG_HEADERS([lib/config.h:lib/config.hin])

AC_PROG_CC
M4_EARLY

# M4 is single-threaded; so we can optimize gnulib code by using this:
gl_DISABLE_THREADS

# Tandem/NSK is broken - it has 'long long int' but not
# 'unsigned long long int', which confuses assumptions made by gnulib.
# Simply pretend that neither type exists if both do not work.
AC_TYPE_LONG_LONG_INT
AC_TYPE_UNSIGNED_LONG_LONG_INT
if test $ac_cv_type_long_long_int:$ac_cv_type_unsigned_long_long_int = yes:no
then
  ac_cv_type_long_long_int=no
  AC_DEFINE([HAVE_LONG_LONG_INT], 0,
    [Define to 1 if the system has the type `long long int'.])
fi

M4_INIT

AC_CACHE_CHECK([if system() agrees with pclose()],
  [M4_cv_func_system_consistent],
  [AC_RUN_IFELSE([AC_LANG_PROGRAM([
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
], [int i1, i2;
  FILE *f;
  i1 = system ("exit 2");
  if (i1 == -1)
    return 1;
  f = popen ("exit 2", "r");
  if (!f)
    return 1;
  i2 = pclose (f);
  return i1 != i2;])],
  [M4_cv_func_system_consistent=yes], [M4_cv_func_system_consistent=no],
  [AC_COMPILE_IFELSE([
/* EMX on OS/2 defines WEXITSTATUS to be (x>>8)&0xff, and uses that for
   pclose(), but for system() it uses x&0xff instead.  Otherwise, we assume
   your system is sane and that pclose() and system() are consistent in their
   values.  If this heuristic is wrong for your platform, report it as a bug
   to bug-m4@gnu.org.  */
#ifdef __EMX__
choke me
#endif
], [M4_cv_func_system_consistent=yes], [M4_cv_func_system_consistent=no])])])
if test "$M4_cv_func_system_consistent" = no ; then
  AC_DEFINE([FUNC_SYSTEM_BROKEN], [1],
    [Define to 1 if the return value of system() disagrees with pclose().])
fi

dnl Don't let changeword get in our way, if bootstrapping with a version of
dnl m4 that already turned the feature on.
m4_ifdef([changeword], [m4_undefine([changeword])])dnl

AC_MSG_CHECKING([[if changeword is wanted]])
AC_ARG_ENABLE([changeword],
  [AS_HELP_STRING([--enable-changeword],
     [enable -W and changeword() builtin])],
  [if test "$enableval" = yes; then
    AC_MSG_RESULT([yes])
    AC_DEFINE([ENABLE_CHANGEWORD], [1],
      [Define to 1 if the changeword(REGEXP) functionality is wanted])
  else
    AC_MSG_RESULT([no])
  fi], [AC_MSG_RESULT([no])])

M4_WITH_DMALLOC

AC_CONFIG_COMMANDS([stamp-h], [[test -z "$CONFIG_HEADERS" || date > stamp-h]])

AC_CONFIG_FILES([Makefile
		 doc/Makefile
		 lib/Makefile
		 src/Makefile
		 tests/Makefile
		 checks/Makefile
		 examples/Makefile
])

AC_OUTPUT

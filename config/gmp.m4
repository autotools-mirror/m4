# -*- Autoconf -*-
# Copyright 2000, 2001 Free Software Foundation, Inc.
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307  USA

# serial 3

AC_DEFUN([AM_WITH_GMP],
  [AC_MSG_CHECKING(if extended and fractional arithmetic is wanted)
  AC_ARG_WITH(gmp,
  [  --with-gmp              use gmp for extended and fractional arithmetic],
  [use_gmp=$withval], [use_gmp=no])
  AC_MSG_RESULT($use_gmp)

  if test "$use_gmp" = yes; then
    LIBS="$LIBS -lgmp"
    AC_CHECK_HEADER([gmp.h],
      [AC_CACHE_CHECK([for mpq_init in libgmp], ac_cv_func_mpq_init_libgmp,
	 [AC_TRY_LINK([#include <gmp.h>],
	    [mpq_t x; (void)mpq_init(x)],
	    ac_cv_func_mpq_init_libgmp=yes,
	    ac_cv_func_mpq_init_libgmp=no)])],
	  ac_cv_func_mpq_init_libgmp=no)

    if test "$ac_cv_func_mpq_init_libgmp$ac_cv_header_gmp_h" = yesyes; then
      AC_DEFINE(WITH_GMP, 1,
      [Define to 1 if the GNU multiple precision library should be used.])
    else
      LIBS=`echo $LIBS | sed -e 's/-lgmp//'`
      AC_MSG_WARN([gmp library not found or does not appear to work])
      use_gmp=no
    fi
  fi

  if test "$use_gmp" != yes; then
    AC_CHECK_SIZEOF(long long int, 0)
  fi
  ])

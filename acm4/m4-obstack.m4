#                                                            -*- Autoconf -*-
# m4-obstack.m4 -- the libc supplied version of obstacks if available.
#
# Copyright (C) 2000, 2001, 2003, 2004 Free Software Foundation, Inc
# Written by Gary V. Vaughan <gary@gnu.org>
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
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

# serial 8

# m4_OBSTACK
# ----------
# Use the libc supplied version of obstacks if available.
AC_DEFUN([m4_OBSTACK],
[AC_PREREQ(2.56)dnl We use the new compiler based header checking in 2.56
AC_BEFORE([gl_OBSTACK], [m4_OBSTACK])
AC_ARG_WITH([included-obstack],
    [AC_HELP_STRING([--with-included-obstack],
                    [use the obstack implementation included here])])

AC_CHECK_HEADERS(obstack.h, [], [], [AC_INCLUDES_DEFAULT])

if test "x${with_included_obstack-no}" != xno; then
  ac_cv_func_obstack=no
fi

OBSTACK_H=
if test $ac_cv_func_obstack = yes; then
  # The system provides obstack.h, `#include <obstack.h>' will work
  INCLUDE_OBSTACK_H='#include <obstack.h>'
else
  # The system does not provide obstack.h, or the user has specified
  # to build without it.  Unfortunately we can't leave an obstack.h
  # file around anywhere in the include path if the system also
  # provides an implementation: So we ship gnulib/lib/obstack.h, and link
  # it to m4/obstack.h at Make time (to substitute the missing system
  # supplied version).  Hence, `#include <m4/obstack.h>' will work.
  INCLUDE_OBSTACK_H='#include <m4/obstack.h>'
  OBSTACK_H=obstack.h
fi
AC_SUBST(OBSTACK_H)
AC_SUBST(INCLUDE_OBSTACK_H)
])# m4_FUNC_OBSTACK

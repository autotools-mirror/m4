#                                                            -*- Autoconf -*-
# m4-obstack.m4 -- the libc supplied version of obstacks if available.
#
# Copyright (C) 2000, 2001, 2003 Free Software Foundation, Inc
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

# Written by Gary V. Vaughan <gary@gnu.org>

# serial 7

# m4_FUNC_OBSTACK
# ---------------
# Use the libc supplied version of obstacks if available.
AC_DEFUN([m4_FUNC_OBSTACK],
[AC_PREREQ(2.56)dnl We use the new compiler based header checking in 2.56
AC_REQUIRE([gl_OBSTACK])
m4_pattern_allow([^m4_cv_func_obstack$])dnl

AC_CHECK_HEADERS(obstack.h, [], [], [AC_INCLUDES_DEFAULT])

AC_ARG_WITH([included-obstack],
    [AC_HELP_STRING([--with-included-obstack],
                    [use the obstack implementation included here])])

if test "x${with_included_obstack-no}" = xno; then
  AC_CACHE_CHECK([for obstack in libc], m4_cv_func_obstack,
               [AC_TRY_LINK([#include "obstack.h"],
	                    [struct obstack *mem;obstack_free(mem,(char *) 0)],
	                    [m4_cv_func_obstack=yes],
	                    [m4_cv_func_obstack=no])])
else
  m4_cv_func_obstack=no
fi

OBSTACK_H=
if test $m4_cv_func_obstack = yes; then

  # The system provides obstack.h, `#include <obstack.h>' will work
  INCLUDE_OBSTACK_H='#include <obstack.h>'
  AC_DEFINE(HAVE_OBSTACK, 1, [Define if libc includes obstacks.])

else

  # The system does not provide obstack.h, or the user has specified
  # to build without it.  Unfortunately we can't leave an obstack.h
  # file around anywhere in the include path if the system also
  # provides an implementation: So we ship gnulib/lib/obstack.h, and link
  # it to m4/obstack.h at Make time (to substitute the missing system
  # supplied version).  Hence, `#include <m4/obstack.h>' will work.
  INCLUDE_OBSTACK_H='#include <m4/obstack.h>'
  OBSTACK_H=obstack.h

  # In the absence of a system implementation, we must compile our own:
  AC_LIBOBJ(obstack)

fi
AC_SUBST(OBSTACK_H)
AC_SUBST(INCLUDE_OBSTACK_H)
])# m4_FUNC_OBSTACK

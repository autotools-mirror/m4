#                                                            -*- Autoconf -*-
# gnu-obstack.m4 -- the libc supplied version of obstacks if available.
#
# Copyright 2000, 2001 Gary V. Vaughan <gary@gnu.org>
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

#serial 3

# FIXME: This is not portable I guess.  There is no reason for all
# the CPP in the world to support #include_next.  So?  Copy the
# actual content of obstack.h if CPP does not support 'include_next'?

AC_PREREQ(2.52)

# M4_AC_FUNC_OBSTACK
# ------------------
# Use the libc supplied version of obstacks if available.
AC_DEFUN([M4_AC_FUNC_OBSTACK],
[AC_CHECK_HEADER(obstack.h)
ifdef([m4_pattern_allow], [m4_pattern_allow([^m4_cv_func_obstack])])dnl
ifdef([m4_pattern_allow], [m4_pattern_allow([^m4_obstack_h])])dnl
AC_CACHE_CHECK([for obstack in libc], m4_cv_func_obstack,
               [AC_TRY_LINK([#include "obstack.h"],
	                    [struct obstack *mem;obstack_free(mem,(char *) 0)],
	                    m4_cv_func_obstack=yes,
	                    m4_cv_func_obstack=no)])
OBSTACK_H=
m4_obstack_h=m4/obstack.h
rm -f $m4_obstack_h
if test $m4_cv_func_obstack = yes; then
  AC_DEFINE(HAVE_OBSTACK, 1, [Define if libc includes obstacks.])
  cat >$m4_obstack_h <<EOF
/* The native header works properly. */
#include_next <obstack.h>
EOF
else
  LIBOBJS="$LIBOBJS obstack.$ac_objext"
  echo linking src/gnu-obstack.h to $m4_obstack_h
  $LN_S ./gnu-obstack.h $m4_obstack_h

  if test x"$ac_cv_header_obstack_h" != xyes; then
    OBSTACK_H=obstack.h
  fi
fi
AC_SUBST(OBSTACK_H)
])# M4_AC_FUNC_OBSTACK

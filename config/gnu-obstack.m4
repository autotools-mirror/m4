#                                                            -*- Autoconf -*-
# gnu-obstack.m4 -- the libc supplied version of obstacks if available.
#
# Copyright 2000, 2001, 2003 Gary V. Vaughan <gary@gnu.org>
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

# serial 6

# M4_AC_FUNC_OBSTACK
# ------------------
# Use the libc supplied version of obstacks if available.
AC_DEFUN([M4_AC_FUNC_OBSTACK],
[AC_PREREQ(2.56)dnl We use the new compiler based header checking in 2.56
AC_CHECK_HEADER(obstack.h, [], [], [AC_INCLUDES_DEFAULT])
m4_pattern_allow([^m4_cv_func_obstack$])dnl
m4_pattern_allow([^m4_obstack_h$])dnl

AC_ARG_WITH([included-obstack],
    [AC_HELP_STRING([--with-included-obstack],
                    [use the obstack imlementation included here])])

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
m4_obstack_h=m4/obstack.h
rm -f $m4_obstack_h
if test $m4_cv_func_obstack = yes; then

  # The system provides obstack.h, `#include <obstack.h>' will work
  INCLUDE_OBSTACK_H='#include <obstack.h>'
  AC_DEFINE(HAVE_OBSTACK, 1, [Define if libc includes obstacks.])

else

  # The system does not provide obstack.h, or the user has specified
  # to build without it.  Unfortunately we can't leave an obstack.h
  # file around anywhere in the include path if the system also
  # provides an implementation: So we ship m4/gnu-obstack.h, and link
  # it to m4/obstack.h here (to substitute the missing system supplied
  # version).  Hence, `#include <m4/obstack.h>' will work.
  INCLUDE_OBSTACK_H='#include <m4/obstack.h>'
  AC_CONFIG_LINKS($m4_obstack_h:${top_srcdir}/m4/gnu-obstack.h)

  if test x"$ac_cv_header_obstack_h" != xyes; then
    OBSTACK_H=obstack.h
  fi

  # In the absence of a system implementation, we must compile our own:
  AC_LIBOBJ(obstack)

fi
AC_SUBST(OBSTACK_H)
AC_SUBST(INCLUDE_OBSTACK_H)
])# M4_AC_FUNC_OBSTACK

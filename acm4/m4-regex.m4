#                                                            -*- Autoconf -*-
# m4-regex.m4 -- Use the installed regex if it is good enough.
# Written by Gary V. Vaughan <gary@gnu.org>
#
# Copyright (C) 2003, 2004 Free Software Foundation, Inc
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

# serial 2

# m4_REGEX([path/to/regex.c])
# ---------------------------
# Use the installed regex if it is good enough.
AC_DEFUN([m4_REGEX],
[AC_BEFORE([gl_REGEX], [m4_REGEX])
if test $ac_use_included_regex = no; then
  # The system provides a good regex.  `#include <regex.h>' will work.
  INCLUDE_REGEX_H='#include <regex.h>'
  REGEX_H=
else
  # The system does not provide regex.h, or the user has specified
  # to build without it.  Unfortunately we can't leave an regex.h
  # file around anywhere in the include path if the system also
  # provides an implementation: So we ship regex_.h, and link it to
  # m4/regex.h at Make time (to substitute the missing system supplied
  # version).  Hence, `#include <m4/regex.h>' will work.
  INCLUDE_REGEX_H='#include <m4/regex.h>'
  REGEX_H=regex.h
fi
AC_SUBST(REGEX_H)
AC_SUBST(INCLUDE_REGEX_H)
])# m4_REGEX

#                                                            -*- Autoconf -*-
# m4-error.m4 -- Use the installed version of error.h if available.
# Written by Gary V. Vaughan <gary@gnu.org>
#
# Copyright (C) 2003, 2004, 2006 Free Software Foundation, Inc
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
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

# serial 4

# M4_ERROR
# --------
# Use the installed version of error.h if available.
AC_DEFUN([M4_ERROR],
[AC_BEFORE([gl_ERROR], [M4_ERROR])

AC_CHECK_HEADERS([error.h],
    [ERROR_H=""], [ERROR_H="error.h"], [AC_INCLUDES_DEFAULT])
AC_SUBST([ERROR_H])

if test $ac_cv_header_error_h = yes; then
  INCLUDE_ERROR_H='#include <error.h>'
else
  INCLUDE_ERROR_H='#include <gnu/error.h>'
fi
AC_SUBST([INCLUDE_ERROR_H])
])# M4_ERROR

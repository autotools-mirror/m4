#                                                            -*- Autoconf -*-
# m4-getopt.m4 -- Use the installed version of getopt.h if available.
# Written by Gary V. Vaughan <gary@gnu.org>
#
# Copyright (C) 2005-2007, 2009-2010, 2013 Free Software Foundation,
# Inc.
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

# serial 3

# M4_GETOPT
# ---------
# Use the installed version of getopt.h if available.
AC_DEFUN([M4_GETOPT],
[
  m4_divert_text([INIT_PREPARE], [M4_replace_getopt=])
  m4_pushdef([AC_LIBOBJ], [M4_replace_getopt=:])
  AC_REQUIRE([gl_FUNC_GETOPT_GNU])
  m4_popdef([AC_LIBOBJ])
  AM_CONDITIONAL([GETOPT], [test -n "$M4_replace_getopt"])
])# M4_GETOPT

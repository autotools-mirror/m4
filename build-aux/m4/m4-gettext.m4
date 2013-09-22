#                                                            -*- Autoconf -*-
# m4-gettext.m4 -- Use the installed version of GNU gettext if available.
# Written by Gary V. Vaughan <gary@gnu.org>
#
# Copyright (C) 2003-2004, 2006-2007, 2010, 2013 Free Software
# Foundation, Inc.
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

# M4_GNU_GETTEXT
# --------------
# Use the installed version of GNU gettext if available.
AC_DEFUN([M4_GNU_GETTEXT],
[AC_BEFORE([AM_GNU_GETTEXT], [M4_GNU_GETTEXT])
AC_CHECK_HEADERS([gettext.h],
    [GETTEXT_H=""], [GETTEXT_H="gettext.h"], [AC_INCLUDES_DEFAULT])
AC_SUBST([GETTEXT_H])

AC_CONFIG_FILES([po/Makefile.in])
])# M4_GNU_GETTEXT

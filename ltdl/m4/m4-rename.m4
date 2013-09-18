#                                                            -*- Autoconf -*-
# m4-rename.m4 -- Test the abilities of rename.
# Written by Eric Blake <ebb9@byu.net>
#
# Copyright (C) 2008, 2010, 2013 Free Software Foundation, Inc.
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

# serial 1

# M4_RENAME
# ---------
# Detect platforms like mingw where rename can't move open files.
AC_DEFUN([M4_RENAME],
[AC_CACHE_CHECK([whether an open file can be renamed],
  [M4_cv_func_rename_open_file_works],
  [AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
      [FILE *f = fopen ("conftest.1", "w+");
       int result = rename ("conftest.1", "conftest.2");
       fclose (f); remove ("conftest.1"); remove ("conftest.2");
       return result;])],
    [M4_cv_func_rename_open_file_works=yes],
    [M4_cv_func_rename_open_file_works=no],
    [M4_cv_func_rename_open_file_works='guessing no'])])
if test "$M4_cv_func_rename_open_file_works" = yes ; then
  M4_rename_open_works=1
else
  M4_rename_open_works=0
fi
AC_DEFINE_UNQUOTED([RENAME_OPEN_FILE_WORKS], [$M4_rename_open_works],
  [Define to 1 if a file can be renamed while open, or to 0 if not.])
])

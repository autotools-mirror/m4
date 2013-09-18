#                                                            -*- Autoconf -*-
# m4-syscmd.m4 -- Allow choice of syscmd shell.
# Written by Eric Blake <ebb9@byu.net>
#
# Copyright (C) 2009-2010, 2013 Free Software Foundation, Inc.
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

# M4_SYSCMD
# ---------
# Allow user to choose different shell than /bin/sh for e?syscmd.
AC_DEFUN([M4_SYSCMD],
[AC_MSG_CHECKING([[which shell to use for syscmd]])
AC_ARG_WITH([syscmd-shell],
  [AS_HELP_STRING([--with-syscmd-shell], [shell used by syscmd [/bin/sh]])],
  [case $withval in
    yes[)] with_syscmd_shell=no;;
   esac], [with_syscmd_shell=no])
if test "$with_syscmd_shell" = no ; then
  with_syscmd_shell=/bin/sh
  if test "$cross_compiling" != yes ; then
dnl Give mingw a default that is more likely to be available.
    AS_IF([AS_EXECUTABLE_P([/bin/sh])], [],
      [if (cmd /c) 2>/dev/null; then with_syscmd_shell=cmd; fi])
dnl Too bad _AS_PATH_WALK is not public.
    M4_save_IFS=$IFS; IFS=$PATH_SEPARATOR
    for M4_dir in `if (command -p getconf PATH) 2>/dev/null ; then
 command -p getconf PATH
      else
 echo "/bin$PATH_SEPARATOR$PATH"
      fi`
    do
      IFS=$M4_save_IFS
      test -z "$M4_dir" && continue
      AS_EXECUTABLE_P(["$M4_dir/sh"]) \
 && { with_syscmd_shell=$M4_dir/sh; break; }
    done
    IFS=$M4_save_IFS
  fi
fi
AC_MSG_RESULT([$with_syscmd_shell])
AC_DEFINE_UNQUOTED([M4_SYSCMD_SHELL], ["$with_syscmd_shell"],
  [Shell used by syscmd and esyscmd, must accept -c argument.])
])

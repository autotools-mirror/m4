## debug.m4 -- massage compiler flags for debugging/optimisation
##
## Copyright 1999-2000 Ralf S. Engelschall
## Written by <rse@engelschall.com>
## Modified for M4 by Gary V. Vaughan <gary@gnu.org>
## 
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
## 
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
## 
## You should have received a copy of the GNU General Public License
## along with this program; see the file COPYING.  If not, write to
## the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
## Boston, MA 02111-1307, USA.

# serial 2

# M4_AC_COMPILER_OPTION(NAME, DISPLAY, OPTION, ACTION-SUCCESS, ACTION-FAILURE)
# -----------------------------------------------------------------------------
# Check whether compiler option works
AC_DEFUN(M4_AC_COMPILER_OPTION,[dnl
AC_MSG_CHECKING(whether compiler option(s) $2 work)
AC_CACHE_VAL(ac_cv_compiler_option_$1,[
SAVE_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $3"
AC_TRY_COMPILE([],[], ac_cv_compiler_option_$1=yes, ac_cv_compiler_option_$1=no)
CFLAGS="$SAVE_CFLAGS"
])dnl
if test ".$ac_cv_compiler_option_$1" = .yes; then
    ifelse([$4], , :, [$4])
else
    ifelse([$5], , :, [$5])
fi
AC_MSG_RESULT([$ac_cv_compiler_option_$1])
])# M4_AC_COMPILER_OPTION

# M4_AC_CHECK_DEBUGGING
# ----------------------
# Debugging Support
AC_DEFUN(M4_AC_CHECK_DEBUGGING,
[AC_REQUIRE([AC_PROG_CC])dnl
AC_ARG_ENABLE(debug,
[  --enable-debug          build for debugging [default=no]],
[if test ".$ac_cv_prog_gcc" = ".yes"; then
    case "$CFLAGS" in
        *-O* ) CFLAGS=`echo $CFLAGS | sed '[s/-O[^ ]* / /;s/-O[^ ]*$//]'` ;;
    esac
    case "$CFLAGS" in
        *-g* ) ;;
           * ) CFLAGS="$CFLAGS -g" ;;
    esac
    case "$CFLAGS" in
        *-pipe* ) ;;
              * ) M4_AC_COMPILER_OPTION(pipe, -pipe, -pipe, CFLAGS="$CFLAGS -pipe") ;;
    esac
    M4_AC_COMPILER_OPTION(ggdb3, -ggdb3, -ggdb3, CFLAGS="$CFLAGS -ggdb3")
    #CFLAGS="$CFLAGS -ansi"
    CFLAGS="$CFLAGS -Wall"
    WMORE="-Wshadow -Wpointer-arith -Wcast-align -Winline"
    WMORE="$WMORE -Wmissing-prototypes -Wmissing-declarations -Wnested-externs"
    M4_AC_COMPILER_OPTION(wmore, -W<xxx>, $WMORE, CFLAGS="$CFLAGS $WMORE")
    M4_AC_COMPILER_OPTION(wnolonglong, -Wno-long-long, -Wno-long-long, CFLAGS="$CFLAGS -Wno-long-long")
else
    case "$CFLAGS" in
        *-g* ) ;;
           * ) CFLAGS="$CFLAGS -g" ;;
    esac
fi
msg="enabled"
AC_DEFINE([DEBUG], 1, [Define this to enable additional runtime debugging])
],[
if test ".$ac_cv_prog_gcc" = ".yes"; then
case "$CFLAGS" in
    *-pipe* ) ;;
          * ) M4_AC_COMPILER_OPTION(pipe, -pipe, -pipe, CFLAGS="$CFLAGS -pipe") ;;
esac
fi
case "$CFLAGS" in
    *-g* ) CFLAGS=`echo "$CFLAGS" |\
                   sed -e 's/ -g / /g' -e 's/ -g$//' -e 's/^-g //g' -e 's/^-g$//'` ;;
esac
case "$CXXFLAGS" in
    *-g* ) CXXFLAGS=`echo "$CXXFLAGS" |\
                     sed -e 's/ -g / /g' -e 's/ -g$//' -e 's/^-g //g' -e 's/^-g$//'` ;;
esac
msg=disabled
])dnl
AC_MSG_CHECKING(for compilation debug mode)
AC_MSG_RESULT([$msg])
if test ".$msg" = .enabled; then
    enable_shared=no
fi
])

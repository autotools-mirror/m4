##                                                           -*- Autoconf -*-
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

# serial 5

# AC_LIBTOOL_COMPILER_OPTION(MESSAGE, VARIABLE-NAME, FLAGS,
#		[OUTPUT-FILE], [ACTION-SUCCESS], [ACTION-FAILURE])
# ----------------------------------------------------------------
# Check whether the given compiler option works
ifdef([AC_LIBTOOL_COMPILER_OPTION], [],
[AC_DEFUN([AC_LIBTOOL_COMPILER_OPTION],
  [AC_CACHE_CHECK([$1], [$2],
    [$2=no
    ifelse([$4], , [ac_outfile=conftest.$ac_objext], [ac_outfile=$4])
     save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS $3"
     echo "$lt_simple_compile_test_code" > conftest.$ac_ext
     if (eval $ac_compile 2>conftest.err) && test -s $ac_outfile; then
       # The compiler can only warn and ignore the option if not recognized
       # So say no if there are warnings
       if test -s conftest.err; then
         # Append any errors to the config.log.
         cat conftest.err 1>&AS_MESSAGE_LOG_FD
       else
         $2=yes
       fi
     fi
     $rm conftest*
     CFLAGS="$save_CFLAGS"
   ])

  if test x"[$]$2" = xyes; then
      ifelse([$5], , :, [$5])
  else
      ifelse([$6], , :, [$6])
  fi
  ])
])# AC_LIBTOOL_COMPILER_OPTION


# M4_AC_CHECK_DEBUGGING
# ---------------------
# Debugging Support
AC_DEFUN([M4_AC_CHECK_DEBUGGING],
[AC_REQUIRE([AC_PROG_CC])
AC_ARG_ENABLE([debug], [AC_HELP_STRING([--enable-debug],
                           [build for debugging [default=no]])])
AC_MSG_CHECKING(for compilation debug mode)
AC_MSG_RESULT([${enable_debug-no}])

set dummy $CC
compiler="${compiler-[$]2}"
test -n "$rm" || rm="rm -f"

if test "X$enable_debug" = Xyes; then
  AC_DISABLE_SHARED
  AC_DEFINE([DEBUG], 1,
      [Define this to enable additional runtime debugging])
  if test "$GCC" = yes; then
    case "$CFLAGS" in
      *-O* ) CFLAGS=`echo $CFLAGS | sed 's/-O[[^ ]]* / /;s/-O[[^ ]]*$//'` ;;
    esac
    case "$CFLAGS" in
        *-g* ) ;;
         * ) AC_LIBTOOL_COMPILER_OPTION([if $compiler accepts -ggdb3],
                 [m4_cv_prog_compiler_ggdb3],
                 [-ggdb3 -c conftest.$ac_ext], [],
                 [CFLAGS="$CFLAGS -ggdb3"],
               [CFLAGS="$CFLAGS -g"])
             ;;
    esac
    CFLAGS="$CFLAGS -Wall"
    WMORE="-Wshadow -Wpointer-arith -Wcast-align -Wnested-externs"
    WMORE="$WMORE -Wmissing-prototypes -Wmissing-declarations -Winline"
    AC_LIBTOOL_COMPILER_OPTION([if $compiler accepts $WMORE],
        [m4_cv_prog_compiler_warning_flags],
        [$WMORE -c conftest.$ac_ext], [],
        [CFLAGS="$CFLAGS $WMORE"])

    AC_LIBTOOL_COMPILER_OPTION([if $compiler accepts -Wno-long-long],
        [m4_cv_prog_compiler_wnolonglong],
        [-Wno-long-long -c conftest.$ac_ext], [],
        [CFLAGS="$CFLAGS -Wno-long-long"])
  else
    case "$CFLAGS" in
        *-g* ) ;;
           * ) CFLAGS="$CFLAGS -g" ;;
    esac
  fi
else
  AC_ENABLE_SHARED
  case "$CFLAGS" in
    *-g* ) CFLAGS=`echo "$CFLAGS" |\
               sed -e 's/ -g / /g;s/ -g$//;s/^-g //g;s/^-g$//'`
           ;;
  esac
  case "$CXXFLAGS" in
    *-g* ) CXXFLAGS=`echo "$CXXFLAGS" |\
               sed -e 's/ -g / /g;s/ -g$//;s/^-g //g;s/^-g$//'`
           ;;
  esac
fi
])# M4_AC_CHECK_DEBUGGING

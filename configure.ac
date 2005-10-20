# Configure template for GNU M4.			-*-Autoconf-*-
# Copyright (C) 1991, 1993, 1994, 2004, 2005 Free Software Foundation, Inc.
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA


AC_INIT([GNU M4], [1.4.4], [bug-m4@gnu.org])
PACKAGE=$PACKAGE_TARNAME; AC_SUBST(PACKAGE)
VERSION=$PACKAGE_VERSION; AC_SUBST(VERSION)

AC_CONFIG_SRCDIR([src/m4.h])
AC_CONFIG_HEADERS([config.h:config-h.in])
AC_ARG_PROGRAM

AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_AIX
AC_ISC_POSIX
AC_MINIX
fp_C_PROTOTYPES
AC_C_CONST

AC_CHECK_HEADERS([limits.h memory.h siginfo.h string.h unistd.h])
AC_CHECK_HEADERS([signal.h sys/signal.h], [break])
AC_HEADER_STDC
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T

AC_CHECK_FUNCS([mkstemp sigaction sigaltstack sigstack sigvec strerror tmpfile])
AC_FUNC_ALLOCA
AC_FUNC_VPRINTF
AC_REPLACE_FUNCS([strtol])


AC_MSG_CHECKING([ecvt declaration])
AC_EGREP_HEADER([ecvt], [stdlib.h],
  [AC_MSG_RESULT(yes)
   AC_DEFINE([HAVE_EFGCVT], [2],
     [Define to 1 if you have ecvt(3), fcvt(3) and gcvt(3).  Define to 2 if they are declared in stdlib.h])
  ],
  [AC_MSG_RESULT(no); AC_CHECK_FUNCS(ecvt)])


AC_MSG_CHECKING(if stack overflow is detectable)
# Code from Jim Avera <jima@netcom.com>.
# stackovf.c requires:
#  1. Either sigaction with SA_ONSTACK, or sigvec with SV_ONSTACK
#  2. Either sigaltstack or sigstack
#  3. getrlimit, including support for RLIMIT_STACK
use_stackovf=no
if test "$ac_cv_func_sigaction" = yes || test "$ac_cv_func_sigvec" = yes; then
  if test "$ac_cv_func_sigaltstack" = yes || test "$ac_cv_func_sigstack" = yes; then
    AC_TRY_LINK([#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>],
      [struct rlimit r; int i; getrlimit (RLIMIT_STACK, &r)
#if (!defined(HAVE_SIGACTION) || !defined(SA_ONSTACK)) \
    && (!defined(HAVE_SIGVEC) || !defined(SV_ONSTACK))
choke me		/* SA_ONSTACK and/or SV_ONSTACK are not defined */
#endif],
      use_stackovf=yes)
  fi
fi
AC_MSG_RESULT($use_stackovf)
if test "$use_stackovf" = yes; then
  AC_DEFINE([USE_STACKOVF], [1],
    [Define to 1 if using stack overflow detection])
  STACKOVF=stackovf.${U}o
  AC_SUBST(STACKOVF)
  AC_EGREP_HEADER([rlim_t], [sys/resource.h], [],
    [AC_DEFINE([rlim_t], [int],
      [Define to int if rlim_t is not defined in sys/resource.h])
  ])
  AC_EGREP_HEADER([stack_t], [signal.h], [],
    [AC_DEFINE([stack_t], [struct sigaltstack],
      [Define to struct sigaltstack if stack_t is not in sys/signal.h])
  ])
  AC_EGREP_HEADER([sigcontext], [signal.h],
    [AC_DEFINE([HAVE_SIGCONTEXT], [1],
      [Define to 1 if signal.h declares struct sigcontext])
  ])
fi

AC_MSG_CHECKING([[if changeword is wanted]])
AC_ARG_ENABLE([changeword],
  [[  --enable-changeword     enable -W and changeword() builtin]],
  [if test "$enableval" = yes; then
    AC_MSG_RESULT(yes)
    AC_DEFINE([ENABLE_CHANGEWORD], [1],
      [Define to 1 if the changeword(REGEXP) functionality is wanted])
  else
    AC_MSG_RESULT(no)
  fi], [AC_MSG_RESULT(no)])

fp_WITH_DMALLOC

AC_CONFIG_FILES([stamp-h], [[test -z "$CONFIG_HEADERS" || date > stamp-h]])

AC_CONFIG_FILES([Makefile
		 doc/Makefile
		 lib/Makefile
		 src/Makefile
		 checks/Makefile
		 examples/Makefile
])

AC_OUTPUT

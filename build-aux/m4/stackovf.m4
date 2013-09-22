#                                                            -*- Autoconf -*-
# stackovf.m4 -- how do we deal with stack overflow?
#
# Copyright (C) 2000, 2003, 2006-2007, 2010, 2013 Free Software
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

# serial 7

# M4_SYS_STACKOVF
# ---------------
AC_DEFUN([M4_SYS_STACKOVF],
[AC_PREREQ([2.60])dnl We use the _ONCE variants
AC_REQUIRE([AC_TYPE_SIGNAL])dnl

AC_CHECK_HEADERS_ONCE([siginfo.h])
AC_CHECK_FUNCS_ONCE([sigaction sigaltstack sigstack sigvec])
AC_CHECK_MEMBERS([stack_t.ss_sp], [], [],
[[#include <signal.h>
#if HAVE_SIGINFO_H
# include <siginfo.h>
#endif
]])

# Code from Jim Avera <jima@netcom.com>.
# stackovf.c requires:
#  1. Either sigaction with SA_ONSTACK, or sigvec with SV_ONSTACK
#  2. Either sigaltstack or sigstack
#  3. getrlimit, including support for RLIMIT_STACK
AC_CACHE_CHECK([if stack overflow is detectable], [M4_cv_use_stackovf],
[M4_cv_use_stackovf=no
if test "$ac_cv_func_sigaction" = yes || test "$ac_cv_func_sigvec" = yes; then
  if test "$ac_cv_func_sigaltstack" = yes || test "$ac_cv_func_sigstack" = yes; then
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
]], [[struct rlimit r; getrlimit (RLIMIT_STACK, &r);
#if (!defined HAVE_SIGACTION || !defined SA_ONSTACK) \
    && (!defined HAVE_SIGVEC || !defined SV_ONSTACK)
choke me /* SA_ONSTACK and/or SV_ONSTACK are not defined */
#endif
]])], [M4_cv_use_stackovf=yes])
  fi
fi])

AM_CONDITIONAL([STACKOVF], [test "$M4_cv_use_stackovf" = yes])
if test "$M4_cv_use_stackovf" = yes; then
  AC_DEFINE([USE_STACKOVF], [1],
    [Define to 1 if using stack overflow detection.])
  AC_CHECK_TYPES([rlim_t], [],
    [AC_DEFINE([rlim_t], [int],
      [Define to int if rlim_t is not defined in sys/resource.h])],
    [[#include <sys/resource.h>
]])
  AC_CHECK_TYPES([stack_t], [],
    [AC_DEFINE([stack_t], [struct sigaltstack],
      [Define to struct sigaltstack if stack_t is not in signal.h])],
    [[#include <signal.h>
]])
  AC_CHECK_TYPES([sigcontext], [], [], [[#include <signal.h>
]])
  AC_CHECK_TYPES([siginfo_t], [], [], [[#include <signal.h>
#if HAVE_SIGINFO_H
# include <siginfo.h>
#endif
]])
  AC_CHECK_MEMBERS([struct sigaction.sa_sigaction], [], [],
[[#include <signal.h>
]])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <signal.h>
]],
            [[struct sigaltstack x; x.ss_base = 0;]])],
            [AC_DEFINE([ss_sp], [ss_base],
            [Define to ss_base if stack_t has ss_base instead of ss_sp.])])
fi
])# M4_SYS_STACKOVF

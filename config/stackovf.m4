#                                                            -*- Autoconf -*-
# stackovf.m4 -- how do we deal with stack overflow?
#
# Copyright 2000, 2003 Gary V. Vaughan <gary@gnu.org>
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

# serial 1

# M4_AC_SYS_STACKOVF
# ------------------
AC_DEFUN([M4_AC_SYS_STACKOVF],
[AC_PREREQ(2.56)dnl We use the new compiler based header checking in 2.56
AC_REQUIRE([AC_TYPE_SIGNAL])dnl

AC_CHECK_HEADERS(siginfo.h, [], [], $ac_default_headers)
AC_CHECK_FUNCS(sigaction sigaltstack sigstack sigvec)
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
  AC_DEFINE(USE_STACKOVF, 1, [Define to 1 if using stack overflow detection.])
  STACKOVF=stackovf.${U}o
  AC_SUBST(STACKOVF)
  AC_EGREP_HEADER(rlim_t, sys/resource.h, ,
	AC_DEFINE(rlim_t, int,
	[Define to int if rlim_t is not defined in <sys/resource.h>.]))
  AC_EGREP_HEADER(stack_t, signal.h, ,
	AC_DEFINE(stack_t, struct sigaltstack,
	[Define to struct sigaltstack if stack_t is not defined in <sys/signal.h>.]))
  AC_EGREP_HEADER(sigcontext, signal.h,
	AC_DEFINE(HAVE_SIGCONTEXT, 1,
	[Define to 1 if <signal.h> declares sigcontext.]))
  AC_EGREP_HEADER(siginfo_t, signal.h,
		  AC_DEFINE(HAVE_SIGINFO_T, 1,
	[Define to 1 if <signal.h> declares siginfo_t.]))

  AC_TRY_COMPILE([#include <signal.h>],
	    [struct sigaltstack x; x.ss_base = 0;],
	    AC_DEFINE(ss_sp, ss_base,
	    [Define to ss_base if stack_t has ss_base instead of ss_sp.]))
fi
])# M4_AC_SYS_STACKOVF

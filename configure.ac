# Configure template for GNU m4.			-*-Autoconf-*-
# Copyright (C) 1991-1994, 2000, 2001, 2002  Free Software Foundation, Inc.
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307  USA


## ------------------------ ##
## Autoconf initialisation. ##
## ------------------------ ##
AC_INIT([GNU m4], [1.4q], [bug-m4@gnu.org])

AC_CONFIG_SRCDIR([src/m4.h])
AC_CONFIG_AUX_DIR([gnulib/config])
AC_CONFIG_LIBOBJ_DIR([gnulib/m4])
AC_CONFIG_HEADERS([config.h:config-h.in])
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_FILES([tests/m4], [chmod +x tests/m4])



## ---------------------------------------- ##
## Display a configure time version banner. ##
## ---------------------------------------- ##
TIMESTAMP=
case AC_PACKAGE_VERSION in
  *[[acegikmoqsuwy]])
    TIMESTAMP=`$CONFIG_SHELL config/mkstamp < $srcdir/ChangeLog`
    AS_BOX([Configuring AC_PACKAGE_TARNAME AC_PACKAGE_VERSION$TIMESTAMP])
    echo
    ;;
esac
AC_DEFINE_UNQUOTED([TIMESTAMP], ["$TIMESTAMP"],
    [Defined to a CVS timestamp for alpha releases of M4])



## -------------------------- ##
## M4 specific configuration. ##
## -------------------------- ##
m4_pattern_allow([^m4_cv_])

# We use some of Jim's macros.
m4_pattern_forbid([^jm_])
m4_pattern_allow([^jm_cv_])
m4_pattern_allow([^jm_ac_cv_])

pkglibexecdir='${libexecdir}'/$PACKAGE
AC_SUBST([pkglibexecdir])

AC_MSG_CHECKING(for modules to preload)
  m4_pattern_allow([^m4_default_preload$])
  m4_default_preload="m4 traditional gnu"
  DLPREOPEN=

  AC_ARG_WITH([modules],
    [AC_HELP_STRING([--with-modules=MODULES],
                    [preload MODULES @<:@default: $m4_default_preload@:>@])],
    [use_modules="$withval"],
    [use_modules="$m4_default_preload"])

  DLPREOPEN="-dlpreopen force"
  if test -z "$use_modules"; then
    use_modules=none
  else
    if test "$use_modules" != yes; then
      for module in $use_modules; do
        DLPREOPEN="$DLPREOPEN -dlpreopen ../modules/$module.la"
      done
    fi
  fi
AC_MSG_RESULT($use_modules)
AC_SUBST(DLPREOPEN)



## ------------------------ ##
## Automake Initialisation. ##
## ------------------------ ##
AM_INIT_AUTOMAKE([1.7a dist-bzip2])



## ------------------ ##
## C compiler checks. ##
## ------------------ ##
AC_PROG_CC
jm_FUNC_GLIBC_UNLOCKED_IO
AC_ISC_POSIX
AM_PROG_CC_STDC
AC_PROG_CPP
AC_PROG_CC_C_O
m4_CHECK_DEBUGGING

# Use gcc's -pipe option if available: for faster compilation.
case "$CFLAGS" in
  *-pipe* ) ;;
        * ) AC_LIBTOOL_COMPILER_OPTION([if $compiler supports -pipe],
                [m4_cv_prog_compiler_pipe],
                [-pipe -c conftest.$ac_ext], [],
                [CFLAGS="$CFLAGS -pipe"])
              ;;
esac



## ----------------------- ##
## Libtool initialisation. ##
## ----------------------- ##
AM_ENABLE_SHARED
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_LIB_LTDL

AC_SUBST([LTDLINCL], ["${LTDLINCL-INCLTDL}"])



## ---------------- ##
## Gettext support. ##
## ---------------- ##
AM_GNU_GETTEXT(external, need-ngettext)
AM_GNU_GETTEXT_VERSION(0.12)
m4_GNU_GETTEXT



## ------------------------ ##
## Other external programs. ##
## ------------------------ ##
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG(PERL,perl)
AC_PROG_AWK



## --------------------------- ##
## C compiler characteristics. ##
## --------------------------- ##
AM_C_PROTOTYPES
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([long long int])



## ------------------------- ##
## C headers required by M4. ##
## ------------------------- ##
AC_HEADER_STDC
AM_STDBOOL_H
AC_CHECK_HEADERS(limits.h locale.h memory.h string.h unistd.h errno.h)

if test $ac_cv_header_stdbool_h = yes; then
  INCLUDE_STDBOOL_H='#include <stdbool.h>'
else
  INCLUDE_STDBOOL_H='#include <m4/stdbool.h>'
fi
AC_SUBST([INCLUDE_STDBOOL_H])



## --------------------------------- ##
## Library functions required by M4. ##
## --------------------------------- ##
AC_CHECK_FUNCS(bzero calloc mkstemp mktemp strerror tmpfile)
AC_REPLACE_FUNCS(strtol)
AC_FUNC_ALLOCA
AC_FUNC_VPRINTF

AM_WITH_DMALLOC

gl_XALLOC
gl_XSTRNDUP
m4_ERROR
m4_OBSTACK
m4_SYS_STACKOVF

M4OBJS=
m4_pattern_allow([^m4_getopt_h$])
m4_getopt_h=src/getopt.h
rm -f $m4_getopt_h
AC_CHECK_FUNC([getopt_long], [],
    [M4OBJS="getopt1.$ac_objext getopt.$ac_objext"
    AC_CONFIG_LINKS([$m4_getopt_h:src/gnu-getopt.h])])
AC_SUBST([M4OBJS])


# This is for the modules
AC_STRUCT_TM
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(getcwd gethostname mktime uname)
AC_CHECK_FUNCS(setenv unsetenv putenv clearenv)

m4_LIB_GMP
AM_CONDITIONAL([USE_GMP], [test "x$USE_GMP" = xyes])



## -------- ##
## Outputs. ##
## -------- ##
AC_CONFIG_FILES([Makefile
		config/Makefile
		doc/Makefile
		examples/Makefile
		gnulib/Makefile
		gnulib/m4/Makefile
		m4/Makefile
		m4/system.h:m4/system_.h
		modules/Makefile
		src/Makefile
		tests/Makefile
		tests/atlocal
])

AC_OUTPUT

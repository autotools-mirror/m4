## This file is part of GNU m4
## Copyright (C) 2003 Free Software Foundation, Inc.
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
##
## Written by Gary V. Vaughan <gary@gnu.org>

AM_LDFLAGS		= -no-undefined -export-dynamic
BUILT_SOURCES		=

noinst_LTLIBRARIES	= libgnu.la
libgnu_la_SOURCES	=
libgnu_la_LIBADD	= $(LTLIBOBJS) $(LTLIBINTL)

pkginclude_HEADERS	= $(STDBOOL_H) exit.h gettext.h obstack.h regex.h \
			  xalloc.h xstrndup.h
EXTRA_HEADERS		= stdbool.h
EXTRA_DIST		=

MOSTLYCLEANFILES	=
MAINTAINERCLEANFILES	= Makefile.in


# --------------------------- #
# Gnulib Makefile.am snippets #
# --------------------------- #

## ((( gnulib module alloca
BUILT_SOURCES += $(ALLOCA_H)
EXTRA_DIST += alloca_.h

# We need the following in order to create an <alloca.h> when the system
# doesn't have one that works with the given compiler.
all-local $(lib_OBJECTS): $(ALLOCA_H)
alloca.h: alloca_.h
	cp $(srcdir)/alloca_.h $@-t
	mv $@-t $@
MOSTLYCLEANFILES += alloca.h alloca.h-t
## ))) gnulib module alloca
## ((( gnulib module error
## ))) gnulib module error
## ((( gnulib module exit
libgnu_la_SOURCES += exit.h
## ))) gnulib module exit
## ((( gnulib module exitfail
libgnu_la_SOURCES += exitfail.h exitfail.c
## ))) gnulib module exitfail
## ((( gnulib module free
## ))) gnulib module free
## ((( gnulib module getopt
libgnu_la_SOURCES += getopt.h getopt.c getopt1.c
## ))) gnulib module getopt
## ((( gnulib module gettext
libgnu_la_SOURCES += gettext.h
## ))) gnulib module gettext
## ((( gnulib module malloc
## ))) gnulib module malloc
## ((( gnulib module obstack
## ))) gnulib module obstack
## ((( gnulib module progname
libgnu_la_SOURCES += progname.h progname.c
## ))) gnulib module progname
## ((( gnulib module realloc
## ))) gnulib module realloc
## ((( gnulib module regex
libgnu_la_SOURCES += regex.h
## ))) gnulib module regex
## ((( gnulib module restrict
## ))) gnulib module restrict
## ((( gnulib module stdbool
BUILT_SOURCES += $(STDBOOL_H)
EXTRA_DIST += stdbool_.h

# We need the following in order to create an <stdbool.h> when the system
# doesn't have one that works.
all-local $(lib_OBJECTS): $(STDBOOL_H)
stdbool.h: stdbool_.h
	sed -e 's/@''HAVE__BOOL''@/$(HAVE__BOOL)/g' < $(srcdir)/stdbool_.h > $@-t
	mv $@-t $@
MOSTLYCLEANFILES += stdbool.h stdbool.h-t
## ))) gnulib module stdbool
## ((( gnulib module strndup
libgnu_la_SOURCES += strndup.h
## ))) gnulib module strndup
## ((( gnulib module strtol
## ))) gnulib module strtol
## ((( gnulib module unlocked-io
libgnu_la_SOURCES += unlocked-io.h
## ))) gnulib module unlocked-io
## ((( gnulib module version-etc
libgnu_la_SOURCES += version-etc.h version-etc.c
## ))) gnulib module version-etc
## ((( gnulib module xalloc
libgnu_la_SOURCES += xalloc.h xmalloc.c xstrdup.c
## ))) gnulib module xalloc
## ((( gnulib module xstrndup
libgnu_la_SOURCES += xstrndup.h xstrndup.c
## ))) gnulib module xstrndup

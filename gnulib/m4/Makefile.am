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

pkgincdir		= $(includedir)/@PACKAGE@
pkginc_HEADERS		= $(GETTEXT_H) $(OBSTACK_H) $(STDBOOL_H)
EXTRA_HEADERS		= gettext.h obstack.h stdbool.h
EXTRA_DIST		=

MOSTLYCLEANFILES	=
MAINTAINERCLEANFILES	= Makefile.in


# --------------------------- #
# Gnulib Makefile.am snippets #
# --------------------------- #

## error: (not yet in gnulib)
BUILT_SOURCES += $(ERROR_H)
EXTRA_DIST += error_.h

all-local $(lib_OBJECTS): $(ERROR_H)
error.h: error_.h
	rm -f $@ && $(LN_S) error_.h $@

MOSTLYCLEANFILES += error.h


## gettext: (not yet in gnulib)
BUILT_SOURCES += $(GETTEXT_H)
EXTRA_DIST += gettext_.h

all-local $(lib_OBJECTS): $(GETTEXT_H)
gettext.h: gettext_.h
	rm -f $@ && $(LN_S) gettext_.h $@

MOSTLYCLEANFILES += gettext.h


## obstack: (not yet in gnulib)
BUILT_SOURCES += $(OBSTACK_H)
EXTRA_DIST += obstack_.h

all-local $(lib_OBJECTS): $(OBSTACK_H)
obstack.h: obstack_.h
	rm -f $@ && $(LN_S) obstack_.h $@

MOSTLYCLEANFILES += obstack.h


## progname:
libgnu_la_SOURCES += progname.c progname.h


## stdbool:
BUILT_SOURCES += $(STDBOOL_H)
EXTRA_DIST += stdbool_.h

# We need the following in order to create an <stdbool.h> when the system
# doesn't have one that works.
all-local $(lib_OBJECTS): $(STDBOOL_H)
stdbool.h: $(gnulib_srcdir)/stdbool_.h
	sed -e 's/@''HAVE__BOOL''@/$(HAVE__BOOL)/g' < $(gnulib_srcdir)/stdbool_.h > $@-t
	mv $@-t $@

MOSTLYCLEANFILES += stdbool.h stdbool.h-t


## unlocked-io:
libgnu_la_SOURCES += unlocked-io.h

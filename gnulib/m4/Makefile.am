## This file is part of GNU m4
## Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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
include gnulib.am

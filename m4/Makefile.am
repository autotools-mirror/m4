## This file is part of GNU m4
## Copyright (C) 2000, 2001, 2003, 2004 Free Software Foundation, Inc.
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

AM_CPPFLAGS	= -I$(top_builddir)/gnulib -I$(top_srcdir)/gnulib \
		  -I$(top_builddir) -I$(top_srcdir) $(INCLTDL) $(INTLINCL)
AM_LDFLAGS 	= -no-undefined -export-dynamic

lib_LTLIBRARIES		= libm4.la
libm4_la_SOURCES 	= builtin.c debug.c hash.c input.c \
			  m4.c m4private.h macro.c module.c output.c \
			  path.c symtab.c syntax.c utility.c
libm4_la_LIBADD		= $(top_builddir)/gnulib/m4/libgnu.la \
			  $(LIBLTDL) $(LTLIBINTL) $(LIBADD_DL)

module.o module.lo: module.c pathconf.h
pathconf.h: Makefile
	@echo "#define MODULE_PATH \"$(libexecdir)/$(PACKAGE)\"" > pathconf.h

EXTRA_DIST	= system_.h
# This file needs to be regenerated at configure time.
dist-hook:
	rm -f $(distdir)/system.h

pkginclude_HEADERS	= hash.h system.h
include_HEADERS		= m4module.h

CLEANFILES		= pathconf.h
MAINTAINERCLEANFILES	= Makefile.in

## Makefile.am -- Process this file with automake to produce Makefile.in
##
## Copyright 2000, 2001 Free Software Foundation
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

CLEANFILES		= pathconf.h
DISTCLEANFILES		= obstack.h
MAINTAINERCLEANFILES	= Makefile.in

MODULE_PATH		= $(pkglibexecdir)
pkgincdir		= $(includedir)/@PACKAGE@

LDFLAGS 	= -no-undefined -export-dynamic
INCLUDES	= -I$(top_builddir) -I$(top_srcdir) \
		  -I$(top_builddir)/src -I$(top_srcdir)/src $(INTLINCL)

include_HEADERS	= m4module.h
noinst_HEADERS	= m4private.h
pkginc_HEADERS	= error.h hash.h ltdl.h regex.h system.h $(OBSTACK_H)
EXTRA_HEADERS	= system-h.in gnu-obstack.h

EXTRA_DIST = $(EXTRA_HEADERS) obstack.c

lib_LTLIBRARIES		= libm4.la
libm4_la_SOURCES 	= builtin.c debug.c error.c hash.c \
			  input.c ltdl.c macro.c module.c output.c \
			  path.c regex.c symtab.c syntax.c utility.c
libm4_la_LIBADD		= $(LTLIBOBJS) $(LIBADD_DL)

module.o module.lo: $(srcdir)/module.c pathconf.h
pathconf.h: Makefile
	@echo "#define MODULE_PATH \"${MODULE_PATH}\"" > pathconf.h

# This file needs to be regenerated at configure time.
dist-hook:
	rm -f $(distdir)/system.h

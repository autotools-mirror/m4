## Makefile.am -- Process this file with automake to produce Makefile.in
##
## Copyright (C) 2000, 2001, 2003 Free Software Foundation, Inc.
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

BUILT_SOURCES		= $(OBSTACK_H)

CLEANFILES		= pathconf.h
MOSTLYCLEANFILES	= obstack.h
MAINTAINERCLEANFILES	= Makefile.in

MODULE_PATH		= $(pkglibexecdir)
pkgincdir		= $(includedir)/@PACKAGE@

AM_LDFLAGS 	= -no-undefined -export-dynamic
INCLUDES	= -I$(top_builddir) -I$(top_srcdir) \
		  -I$(top_builddir)/src -I$(top_srcdir)/src $(INTLINCL)

include_HEADERS	= m4module.h
noinst_HEADERS	= m4private.h
pkginc_HEADERS	= error.h hash.h ltdl.h regex.h system.h $(OBSTACK_H)
EXTRA_HEADERS	= system_.h obstack_.h

EXTRA_DIST = $(EXTRA_HEADERS) obstack.c

lib_LTLIBRARIES		= libm4.la
libm4_la_SOURCES 	= builtin.c debug.c error.c hash.c \
			  input.c ltdl.c m4.c macro.c module.c output.c \
			  path.c regex.c symtab.c syntax.c utility.c
libm4_la_LIBADD		= $(LTLIBOBJS) $(LTLIBINTL) $(LIBADD_DL)

module.o module.lo: $(srcdir)/module.c pathconf.h
pathconf.h: Makefile
	@echo "#define MODULE_PATH \"${MODULE_PATH}\"" > pathconf.h

# This file needs to be regenerated at configure time.
dist-hook:
	rm -f $(distdir)/system.h


# ------------------------------------------------------------ #
# Taken from gnulib/modules/stdbool:Makefile.am.  Do not edit. #
# ------------------------------------------------------------ #

BUILT_SOURCES += $(STDBOOL_H)
EXTRA_DIST += stdbool_.h

# We need the following in order to create an <stdbool.h> when the system
# doesn't have one that works.
all-local $(lib_OBJECTS): $(STDBOOL_H)
stdbool.h: stdbool_.h
	sed -e 's/@''HAVE__BOOL''@/$(HAVE__BOOL)/g' < $(srcdir)/stdbool_.h > $@-t
	mv $@-t $@
MOSTLYCLEANFILES += stdbool.h stdbool.h-t

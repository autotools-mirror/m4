## Makefile.am -- Process this file with automake to produce Makefile.in
##
## Copyright (C) 2001, 2002 Free Software Foundation
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
## Written by Akim Demaille <akim@freefriends.org>

MAINTAINERCLEANFILES =				\
$(srcdir)/Makefile.in				\
$(srcdir)/ltmain.sh				\
$(srcdir)/texinfo.tex				\
$(ACLOCAL_MACROS)				\
$(STANDARD_TOOLS)

ACLOCAL_MACROS =				\
$(srcdir)/codeset.m4				\
$(srcdir)/gettext.m4				\
$(srcdir)/glibc21.m4				\
$(srcdir)/iconv.m4				\
$(srcdir)/intdiv0.m4				\
$(srcdir)/inttypes-pri.m4			\
$(srcdir)/inttypes.m4				\
$(srcdir)/inttypes_h.m4				\
$(srcdir)/isc-posix.m4				\
$(srcdir)/lcmessage.m4				\
$(srcdir)/lib-ld.m4  				\
$(srcdir)/lib-link.m4				\
$(srcdir)/lib-prefix.m4				\
$(srcdir)/progtest.m4				\
$(srcdir)/stdint_h.m4				\
$(srcdir)/uintmax_t.m4				\
$(srcdir)/ulonglong.m4

STANDARD_TOOLS =				\
$(srcdir)/compile				\
$(srcdir)/config.guess				\
$(srcdir)/config.rpath				\
$(srcdir)/config.sub				\
$(srcdir)/depcomp				\
$(srcdir)/install-sh				\
$(srcdir)/mdate-sh				\
$(srcdir)/missing				\
$(srcdir)/mkinstalldirs

SPECIFIC_MACROS =				\
$(srcdir)/debug.m4				\
$(srcdir)/error.m4				\
$(srcdir)/gmp.m4				\
$(srcdir)/gnu-obstack.m4			\
$(srcdir)/regex.m4				\
$(srcdir)/stackovf.m4

SPECIFIC_TOOLS =				\
$(srcdir)/mkstamp

spy:
	@echo $($(SPIED))

EXTRA_DIST = $(ACLOCAL_MACROS) $(SPECIFIC_MACROS) $(SPECIFIC_TOOLS)

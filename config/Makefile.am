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
Makefile.in					\
ltmain.sh					\
texinfo.tex					\
$(ACLOCAL_MACROS)				\
$(STANDARD_TOOLS)

ACLOCAL_MACROS =				\
codeset.m4					\
gettext.m4					\
glibc21.m4					\
iconv.m4					\
intdiv0.m4					\
inttypes-pri.m4					\
inttypes.m4					\
inttypes_h.m4					\
isc-posix.m4					\
lcmessage.m4					\
lib-ld.m4  					\
lib-link.m4					\
lib-prefix.m4					\
progtest.m4					\
stdint_h.m4					\
uintmax_t.m4					\
ulonglong.m4

STANDARD_TOOLS =				\
compile						\
config.guess					\
config.rpath					\
config.sub					\
depcomp						\
install-sh					\
mdate-sh					\
missing						\
mkinstalldirs

SPECIFIC_MACROS =				\
debug.m4					\
error.m4					\
gmp.m4						\
m4-obstack.m4					\
regex.m4					\
stackovf.m4

SPECIFIC_TOOLS =				\
mkstamp

spy:
	@echo $($(SPIED))

EXTRA_DIST = $(ACLOCAL_MACROS) $(SPECIFIC_MACROS) $(SPECIFIC_TOOLS)

## Makefile.am -- Process this file with automake to produce Makefile.in
##
## Copyright 2001 Free Software Foundation
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
amversion.m4					\
auxdir.m4					\
ccstdc.m4					\
codeset.m4					\
cond.m4						\
depend.m4					\
depout.m4					\
dmalloc.m4					\
gettext.m4					\
glibc21.m4					\
header.m4					\
iconv.m4					\
init.m4						\
install-sh.m4					\
isc-posix.m4					\
lcmessage.m4					\
libtool.m4					\
ltdl.m4						\
make.m4						\
missing.m4					\
options.m4					\
progtest.m4					\
protos.m4					\
sanity.m4					\
strip.m4

STANDARD_TOOLS =				\
compile						\
config.guess					\
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
gnu-obstack.m4					\
stackovf.m4

spy:
	@echo $($(SPIED))

EXTRA_DIST = mkstamp $(SPECIFIC_MACROS) $(ACLOCAL_MACROS)

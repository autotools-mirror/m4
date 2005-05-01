## This file is part of GNU m4
## Copyright (C) 2000, 2001, 2004 Free Software Foundation, Inc.
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
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
## 02110-1301  USA

info_TEXINFOS = m4.texinfo

dist_man_MANS = m4.1

EXTRA_DIST = helptoman.pl

# Automake 1.4 does not support TAGS_FILES directly.
TAGS_FILES = $(info_TEXINFOS)
TAGS_DEPENDENCIES = $(TAGS_FILES)
ETAGS_ARGS = --language=none --regex='/@node \([^,]*\)/\1/' $(TAGS_FILES)

MAINTAINERCLEANFILES = Makefile.in m4.1

PERL = @PERL@

%.1: helptoman.pl $(top_builddir)/src/$*
	@if test -f $(top_builddir)/src/$*; then \
	  echo "Updating the \`man' page \`$@'"; \
	  $(PERL) $(srcdir)/helptoman.pl $(top_builddir)/src/$* > $@-tmp; \
	  cmp -s $@-tmp $(srcdir)/$@ || cp $@-tmp $(srcdir)/$@; \
	  rm -f $@-tmp; \
	else \
	  echo "WARNING: The \`man' page \`$@' cannot be updated yet."; \
	  echo "         Retry once the program executable will be ready."; \
	fi

## This file is part of GNU m4
## Copyright 2000 Free Software Foundation, Inc.
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
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
## 02111-1307  USA

info_TEXINFOS = m4.texinfo

man_MANS = m4.1

EXTRA_DIST = helptoman.pl # $(MANS)

MAINTAINERCLEANFILES = Makefile.in mdate-sh texinfo.tex m4.1

PERL = @PERL@

%.1: helptoman.pl ../src/$*
	@if test -f ../src/$*; then \
	  echo "Updating the \`man' page \`$@'"; \
	  $(PERL) $(srcdir)/helptoman.pl ../src/$* > $@-tmp; \
	  cmp -s $@-tmp $(srcdir)/$@ || cp $@-tmp $(srcdir)/$@; \
	  rm -f $@-tmp; \
	else \
	  echo "WARNING: The \`man' page \`$@' cannot be updated yet."; \
	  echo "         Retry once the program executable will be ready."; \
	fi


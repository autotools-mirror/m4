## Makefile.am -- Process this file with automake to produce Makefile.in
##
## Copyright (C) 2000, 2003 Free Software Foundation
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

EXTRA_DIST	= bootstrap config/mkstamp

## There is currently no means with Automake not to run aclocal.
ACLOCAL_AMFLAGS = -I gnulib/acm4 -I acm4

SUBDIRS			= po gnulib libltdl m4 modules src tests examples doc .
MAINTAINERCLEANFILES	= ABOUT-NLS COPYING INSTALL Makefile.in aclocal.m4 \
			  config-h.in configure stamp-h.in \
			  po/cat-id-tbl.c po/m4.pot po/stamp-cat-id

# Make sure config.status is regenerated when the version timestamp changes
MKSTAMP = $(SHELL) $(top_srcdir)/config/mkstamp
CONFIG_STATUS_DEPENDENCIES = stamp-vcl
stamp-vcl: vcl.tmp $(top_srcdir)/ChangeLog
vcl.tmp:
	@dir=.; test -f $$dir/stamp-vcl || dir=$(srcdir); \
	set `$(MKSTAMP) < $(top_srcdir)/ChangeLog`; \
	echo "$$1" > vcl.tmp; \
	cmp -s vcl.tmp $$dir/stamp-vcl \
## If mkstamp does not match $(srcdir)/stamp-vcl, we still put the new one
## in the current dir, incase $(srcdir) is not writable.  The dir selection
## at the top of this rule takes care of prefering the right one on
## subsequent runs.
	  || (echo "Updating stamp-vcl"; cp vcl.tmp ./stamp-vcl)
	-@rm -f vcl.tmp

##                                                                      ##
##                   --- RULES FOR THE MAINTAINER ---                   ##
##           You shouldn't need to touch anything below here.           ##
##              (mostly copied from automake/Makefile.am)               ##
##                                                                      ##

CVS	= cvs # set it to `:' to avoid CVS operations at dist time

# Tag before making distribution.  Also, don't make a distribution if
# checks fail.  Also, make sure the NEWS file is up-to-date.
cvs-dist: distcheck # maintainer-check
	@if sed '1,2d;3q' $(srcdir)/NEWS | grep -e $(VERSION)" > dev/null; \
	  echo "NEWS not updated not releasing" 1>&2; \
	  exit 1;
	fi
	cd $(srcdir) && \
	  $(CVS) -q tag `echo "Release-$(VERSION)" | sed 's/\./-/g'`
	$(MAKE) dist

cvs-diff:
	thisver=`echo "Release-$(VERSION)" | sed 's/\./-/g'`; \
	if test -z "$$OLDVERSION"; then \
	  prevno=`echo "$(VERSION)" - 0.01 | bc | sed 's/^\./0./'`; \
	else prevno="$$OLDVERSION"; fi; \
	prevver=Release-`echo $$prevno | sed 's/\./-/g'`; \
	cvs -f rdiff -c -r $$prevver -r $$thisver $(PACKAGE) \
	    > $(PACKAGE)-$$prevno-$(VERSION).diff

## This file is part of GNU m4
## Copyright 2000, 2001 Free Software Foundation, Inc.
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

# The next line is needed by `others.at:esyscmd'
#  Ty Coon, President of Vice

EXTRA_DIST = mkconfig.sh foo incl.m4 \
	defs $(TESTS) $(OTHER_FILES) $(TESTSUITE_SOURCES) $(TESTSUITE)

MAINTAINERCLEANFILES = Makefile.in generated.at config.m4 config.sh \
	$(TESTSUITE)

OTHER_FILES = \
	iso8859.m4 \
	stackovf.test

TESTSUITE_SOURCES = testsuite.at others.at generated.at
TESTSUITE = testsuite

check-local: atconfig $(TESTSUITE)
	$(SHELL) $(TESTSUITE)

$(srcdir)/generated.at: $(srcdir)/generate.awk $(top_srcdir)/doc/m4.texinfo
	rm -f $@
	$(AWK) -f $(srcdir)/generate.awk $(top_srcdir)/doc/m4.texinfo >$@t
	mv $@t $@

# FIXME: Autom4te is not complete yet, and lacks a means to find
# the lib files.
AUTOM4TE = autom4te
AUTOTEST = $(AUTOM4TE) -I /usr/local/share/autoconf autotest/general.m4
$(srcdir)/$(TESTSUITE): $(TESTSUITE_SOURCES)
	$(AUTOTEST) -I $(srcdir) testsuite.at -o $@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@

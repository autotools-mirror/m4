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

EXTRA_DIST = generate.awk atlocal.in m4 \
	$(OTHER_FILES) $(TESTSUITE_AT) $(TESTSUITE)

DISTCLEANFILES       = atconfig atlocal
MAINTAINERCLEANFILES = Makefile.in generated.at $(TESTSUITE)

OTHER_FILES = \
	iso8859.m4 incl.m4 foo \
	stackovf.test

TESTSUITE_AT = testsuite.at others.at generated.at
TESTSUITE = testsuite

check-local: atconfig $(TESTSUITE)
	$(SHELL) $(srcdir)/$(TESTSUITE)

clean-local:
	$(srcdir)/testsuite --clean

$(srcdir)/generated.at: $(srcdir)/generate.awk $(top_srcdir)/doc/m4.texinfo
	rm -f $@ $@t
	$(AWK) -f $(srcdir)/generate.awk $(top_srcdir)/doc/m4.texinfo >$@t
	mv $@t $@

# FIXME: Autom4te is not complete yet, and lacks a means to find
# the lib files.
AUTOM4TE = autom4te
AUTOTEST = $(AUTOM4TE) -I /usr/local/share/autoconf autotest/autotest.m4
$(srcdir)/$(TESTSUITE): $(TESTSUITE_AT)
	$(AUTOTEST) -I $(srcdir) testsuite.at -o $@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@

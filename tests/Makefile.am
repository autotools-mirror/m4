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

# The next line is needed by `generated.at:esyscmd'
#  Ty Coon, President of Vice

EXTRA_DIST = generate.awk atlocal.in \
	$(OTHER_FILES) $(TESTSUITE_AT) $(TESTSUITE)

DISTCLEANFILES       = atconfig atlocal package.m4 m4
MAINTAINERCLEANFILES = Makefile.in generated.at $(TESTSUITE)

OTHER_FILES = \
	iso8859.m4 stackovf.test

TESTSUITE_AT = \
	testsuite.at \
	macros.at builtins.at options.at others.at modules.at generated.at
TESTSUITE = testsuite

check-local: atconfig $(TESTSUITE)
	$(SHELL) $(srcdir)/$(TESTSUITE)

clean-local:
	$(srcdir)/testsuite --clean

# Using variables so that this snippet is not too wide and can
# be used as is in Texinfo @example/@end example.
m4_texinfo  = $(top_srcdir)/doc/m4.texinfo
generate    = $(AWK) -f $(srcdir)/generate.awk
$(srcdir)/generated.at: $(srcdir)/generate.awk $(m4_texinfo)
	$(generate) $(m4_texinfo) >$@t
	mv $@t $@

AUTOM4TE = autom4te
AUTOTEST = $(AUTOM4TE) --language=autotest
$(srcdir)/$(TESTSUITE): $(TESTSUITE_AT)
	$(AUTOTEST) -I $(srcdir) testsuite.at -o $@

# Run the test suite on the *installed* tree.
installcheck-local:
	$(SHELL) $(srcdir)/$(TESTSUITE) AUTOTEST_PATH=$(exec_prefix)/bin

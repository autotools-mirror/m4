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

# The next line is needed by `./esyscmd.1.test'
#  Ty Coon, President of Vice
# The previous line is needed by `./esyscmd.1.test'

EXTRA_DIST = get-them run-test mkconfig.sh foo incl.m4 stamp-TESTS \
	defs $(TESTS) $(OTHER_FILES)

MAINTAINERCLEANFILES = Makefile.in $(GENERATED_TESTS) config.m4 config.sh \
	stamp-TESTS

TESTS = $(GENERATED_TESTS) $(OTHER_TESTS)

GENERATED_TESTS = \
	generated-tests/argument.1.test \
	generated-tests/argument.2.test \
	generated-tests/argument.3.test \
	generated-tests/argument.4.test \
	generated-tests/changeco.1.test \
	generated-tests/changeco.2.test \
	generated-tests/changequ.1.test \
	generated-tests/changequ.2.test \
	generated-tests/changequ.3.test \
	generated-tests/changesy.1.test \
	generated-tests/changesy.2.test \
	generated-tests/changesy.3.test \
	generated-tests/changesy.4.test \
	generated-tests/changesy.5.test \
	generated-tests/changesy.6.test \
	generated-tests/changesy.7.test \
	generated-tests/changesy.8.test \
	generated-tests/cleardiv.1.test \
	generated-tests/cleardiv.2.test \
	generated-tests/define.1.test \
	generated-tests/define.2.test \
	generated-tests/defn.1.test \
	generated-tests/defn.2.test \
	generated-tests/divert.1.test \
	generated-tests/divert.2.test \
	generated-tests/divnum.1.test \
	generated-tests/dnl.1.test \
	generated-tests/dumpdef.1.test \
	generated-tests/errprint.1.test \
	generated-tests/errprint.2.test \
	generated-tests/esyscmd.1.test \
	generated-tests/eval.1.test \
	generated-tests/eval.2.test \
	generated-tests/format.1.test \
	generated-tests/ifdef.1.test \
	generated-tests/ifelse.1.test \
	generated-tests/ifelse.2.test \
	generated-tests/include.1.test \
	generated-tests/include.2.test \
	generated-tests/include.3.test \
	generated-tests/incr.1.test \
	generated-tests/index.1.test \
	generated-tests/indir.1.test \
	generated-tests/len.1.test \
	generated-tests/loops.1.test \
	generated-tests/loops.2.test \
	generated-tests/m4exit.1.test \
	generated-tests/m4wrap.1.test \
	generated-tests/patsubst.1.test \
	generated-tests/patsubst.2.test \
	generated-tests/pseudoar.1.test \
	generated-tests/pseudoar.2.test \
	generated-tests/pseudoar.3.test \
	generated-tests/pseudoar.4.test \
	generated-tests/pseudoar.5.test \
	generated-tests/pushdef.1.test \
	generated-tests/pushdef.2.test \
	generated-tests/regexp.1.test \
	generated-tests/regexp.2.test \
	generated-tests/substr.1.test \
	generated-tests/symbols.1.test \
	generated-tests/sysval.1.test \
	generated-tests/trace.1.test \
	generated-tests/translit.1.test \
	generated-tests/undefine.1.test \
	generated-tests/undivert.1.test \
	generated-tests/undivert.2.test \
	generated-tests/undivert.3.test

OTHER_TESTS = \
	other-tests/changeword.test \
	other-tests/discard-comments.test \
	other-tests/stackovf.test \
	other-tests/capitalize.test \
	other-tests/comments.test \
	other-tests/ddivert.test \
	other-tests/debug.test \
	other-tests/esyscmd.test \
	other-tests/exp.test \
	other-tests/foreach.test \
	other-tests/forloop.test \
	other-tests/fstab.test \
	other-tests/gmp.test \
	other-tests/hanoi.test \
	other-tests/import-environment.test \
	other-tests/include.test \
	other-tests/indir.test \
	other-tests/iso8859.test \
	other-tests/misc.test \
	other-tests/modfreeze.test \
	other-tests/modpath1.test \
	other-tests/modpath2.test \
	other-tests/modpath3.test \
	other-tests/modpath4.test \
	other-tests/modtest.test \
	other-tests/multiquotes.test \
	other-tests/patsubst.test \
	other-tests/pushpop.test \
	other-tests/regexp.test \
	other-tests/reverse.test \
	other-tests/shadow.test \
	other-tests/sync-lines.test \
	other-tests/sysv-args.test \
	other-tests/trace.test \
	other-tests/translit.test \
	other-tests/undivert.test \
	other-tests/unload.test \
	other-tests/wrap.test

OTHER_FILES = \
	other-tests/frozen.m4 \
	other-tests/gmp.m4 \
	other-tests/iso8859.m4 \
	other-tests/import-environment.m4 \
	other-tests/discard-comments.m4 \
	other-tests/sync-lines.m4 \
	other-tests/unfrozen.m4

check: stamp-TESTS

stamp-TESTS: $(srcdir)/get-them $(top_srcdir)/doc/m4.texinfo
	cd $(srcdir) \
	&& AWK=$(AWK) ./get-them ../doc/m4.texinfo \
	&& > $@

clean-local:
	rm -rf testSubDir

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
	other-tests/gmp.test \
	other-tests/iso8859.test \
	other-tests/sync-lines.test \
	other-tests/import-environment.test \
	other-tests/discard-comments.test \
	other-tests/stackovf.test

OTHER_FILES = \
	other-tests/gmp.m4 \
	other-tests/iso8859.m4 \
	other-tests/import-environment.m4 \
	other-tests/discard-comments.m4 \
	other-tests/sync-lines.m4

all: stamp-TESTS

stamp-TESTS: $(srcdir)/get-them $(top_srcdir)/doc/m4.texinfo
	cd $(srcdir) \
	&& AWK=$(AWK) $(srcdir)/get-them $(top_srcdir)/doc/m4.texinfo \
	&& > $@

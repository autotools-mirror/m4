# The next line is needed by `./esyscmd.1.test'
#  Ty Coon, President of Vice
# The previous line is needed by `./esyscmd.1.test'

EXTRA_DIST = get-them run-test foo incl.m4 stamp-TESTS \
	defs $(GENERATED_TESTS)

TESTS = $(GENERATED_TESTS) $(OTHER_TESTS)

GENERATED_TESTS = \
	gentest/argument.1.test \
	gentest/argument.2.test \
	gentest/argument.3.test \
	gentest/argument.4.test \
	gentest/changeco.1.test \
	gentest/changeco.2.test \
	gentest/changequ.1.test \
	gentest/changequ.2.test \
	gentest/changequ.3.test \
	gentest/changesy.1.test \
	gentest/changesy.2.test \
	gentest/changesy.3.test \
	gentest/changesy.4.test \
	gentest/changesy.5.test \
	gentest/changesy.6.test \
	gentest/changesy.7.test \
	gentest/cleardiv.1.test \
	gentest/cleardiv.2.test \
	gentest/define.1.test \
	gentest/define.2.test \
	gentest/defn.1.test \
	gentest/defn.2.test \
	gentest/divert.1.test \
	gentest/divert.2.test \
	gentest/divnum.1.test \
	gentest/dnl.1.test \
	gentest/dumpdef.1.test \
	gentest/errprint.1.test \
	gentest/errprint.2.test \
	gentest/esyscmd.1.test \
	gentest/eval.1.test \
	gentest/eval.2.test \
	gentest/format.1.test \
	gentest/ifdef.1.test \
	gentest/ifelse.1.test \
	gentest/ifelse.2.test \
	gentest/include.1.test \
	gentest/include.2.test \
	gentest/include.3.test \
	gentest/incr.1.test \
	gentest/index.1.test \
	gentest/indir.1.test \
	gentest/len.1.test \
	gentest/loops.1.test \
	gentest/loops.2.test \
	gentest/m4exit.1.test \
	gentest/m4wrap.1.test \
	gentest/patsubst.1.test \
	gentest/patsubst.2.test \
	gentest/pseudoar.1.test \
	gentest/pseudoar.2.test \
	gentest/pseudoar.3.test \
	gentest/pseudoar.4.test \
	gentest/pseudoar.5.test \
	gentest/pushdef.1.test \
	gentest/pushdef.2.test \
	gentest/regexp.1.test \
	gentest/regexp.2.test \
	gentest/substr.1.test \
	gentest/sysval.1.test \
	gentest/trace.1.test \
	gentest/translit.1.test \
	gentest/undefine.1.test \
	gentest/undivert.1.test \
	gentest/undivert.2.test \
	gentest/undivert.3.test

OTHER_TESTS = \
	../examples/comments.test \
	../examples/ddivert.test \
	../examples/debug.test \
	../examples/iso8859.test \
	../examples/reverse.test \
	../examples/sysv-args.test \
	../examples/wrap.test


all: stamp-TESTS

stamp-TESTS: $(srcdir)/get-them $(top_srcdir)/doc/m4.texinfo
	cd $(srcdir) \
	&& AWK=$(AWK) $(srcdir)/get-them $(top_srcdir)/doc/m4.texinfo \
	&& > $@

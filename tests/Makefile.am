# The next line is needed by `./esyscmd.1.test'
#  Ty Coon, President of Vice
# The previous line is needed by `./esyscmd.1.test'

EXTRA_DIST = get-them run-test foo incl.m4 stamp-TESTS \
	defs $(GENERATED_TESTS)

TESTS = $(GENERATED_TESTS) $(OTHER_TESTS)

GENERATED_TESTS = \
	gentest/argument.1.m4 \
	gentest/argument.2.m4 \
	gentest/argument.3.m4 \
	gentest/argument.4.m4 \
	gentest/changeco.1.m4 \
	gentest/changeco.2.m4 \
	gentest/changequ.1.m4 \
	gentest/changequ.2.m4 \
	gentest/changequ.3.m4 \
	gentest/changesy.1.m4 \
	gentest/changesy.2.m4 \
	gentest/changesy.3.m4 \
	gentest/changesy.4.m4 \
	gentest/changesy.5.m4 \
	gentest/changesy.6.m4 \
	gentest/changesy.7.m4 \
	gentest/cleardiv.1.m4 \
	gentest/cleardiv.2.m4 \
	gentest/define.1.m4 \
	gentest/define.2.m4 \
	gentest/defn.1.m4 \
	gentest/defn.2.m4 \
	gentest/divert.1.m4 \
	gentest/divert.2.m4 \
	gentest/divnum.1.m4 \
	gentest/dnl.1.m4 \
	gentest/dumpdef.1.m4 \
	gentest/errprint.1.m4 \
	gentest/errprint.2.m4 \
	gentest/esyscmd.1.m4 \
	gentest/eval.1.m4 \
	gentest/eval.2.m4 \
	gentest/format.1.m4 \
	gentest/ifdef.1.m4 \
	gentest/ifelse.1.m4 \
	gentest/ifelse.2.m4 \
	gentest/include.1.m4 \
	gentest/include.2.m4 \
	gentest/include.3.m4 \
	gentest/incr.1.m4 \
	gentest/index.1.m4 \
	gentest/indir.1.m4 \
	gentest/len.1.m4 \
	gentest/loops.1.m4 \
	gentest/loops.2.m4 \
	gentest/m4exit.1.m4 \
	gentest/m4wrap.1.m4 \
	gentest/patsubst.1.m4 \
	gentest/patsubst.2.m4 \
	gentest/pseudoar.1.m4 \
	gentest/pseudoar.2.m4 \
	gentest/pseudoar.3.m4 \
	gentest/pseudoar.4.m4 \
	gentest/pseudoar.5.m4 \
	gentest/pushdef.1.m4 \
	gentest/pushdef.2.m4 \
	gentest/regexp.1.m4 \
	gentest/regexp.2.m4 \
	gentest/substr.1.m4 \
	gentest/sysval.1.m4 \
	gentest/trace.1.m4 \
	gentest/translit.1.m4 \
	gentest/undefine.1.m4 \
	gentest/undivert.1.m4 \
	gentest/undivert.2.m4 \
	gentest/undivert.3.m4

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

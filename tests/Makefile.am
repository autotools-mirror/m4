# The next line is needed by `./esyscmd.1.test'
#  Ty Coon, President of Vice
# The previous line is needed by `./esyscmd.1.test'

EXTRA_DIST = get-them foo incl.m4 stamp-TESTS $(TESTS) defs m4wrap.1.test

TESTS = $(OTHER_TESTS) $(GENERATED_TESTS)

GENERATED_TESTS = \
	argument.1.test argument.2.test argument.3.test argument.4.test \
	changeco.1.test changeco.2.test changequ.1.test changequ.2.test \
	changequ.3.test changesy.1.test changesy.2.test changesy.3.test \
	changesy.4.test changesy.5.test changesy.6.test changesy.7.test \
	cleardiv.1.test cleardiv.2.test define.1.test define.2.test \
	defn.1.test defn.2.test divert.1.test divert.2.test divnum.1.test \
	dnl.1.test dumpdef.1.test errprint.1.test errprint.2.test \
	esyscmd.1.test eval.1.test eval.2.test format.1.test ifdef.1.test \
	ifelse.1.test ifelse.2.test include.1.test include.2.test \
	include.3.test incr.1.test index.1.test indir.1.test len.1.test \
	loops.1.test loops.2.test m4exit.1.test  patsubst.1.test \
	patsubst.2.test pseudoar.1.test pseudoar.2.test pseudoar.3.test \
	pseudoar.4.test pseudoar.5.test pushdef.1.test pushdef.2.test \
	regexp.1.test regexp.2.test substr.1.test sysval.1.test trace.1.test \
	translit.1.test undefine.1.test undivert.1.test undivert.2.test \
	undivert.3.test

OTHER_TESTS =

all: stamp-TESTS

stamp-TESTS: $(srcdir)/get-them $(top_srcdir)/doc/m4.texinfo
	cd $(srcdir) \
	&& rm -f *.[0-9].test \
	&& AWK=$(AWK) $(srcdir)/get-them $(top_srcdir)/doc/m4.texinfo \
	&& > $@

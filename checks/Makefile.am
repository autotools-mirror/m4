EXTRA_DIST = check-them foo get-them incl.m4 stamp-checks $(CHECKS)

CHECKS = 01.define 02.arguments 03.arguments 04.arguments 05.arguments \
	06.pseudo_argu 07.pseudo_argu 08.pseudo_argu 09.pseudo_argu \
	10.pseudo_argu 11.undefine 12.defn 13.pushdef 14.pushdef 15.indir \
	16.ifdef 17.ifelse 18.ifelse 19.loops 20.loops 21.dumpdef 22.trace \
	23.dnl 24.changequote 25.changequote 26.changequote 27.changecom \
	28.changecom 29.m4wrap 30.include 31.include 32.include 33.divert \
	34.divert 35.undivert 36.undivert 37.undivert 38.divnum 39.cleardiv \
	40.cleardiv 41.len 42.index 43.regexp 44.regexp 45.substr 46.translit \
	47.patsubst 48.patsubst 49.format 50.incr 51.eval 52.eval 53.esyscmd \
	54.sysval 55.errprint 56.errprint 57.m4exit

all: stamp-checks

stamp-checks: get-them ../doc/m4.texinfo
	rm -f $(srcdir)/$(CHECKS)
	cd $(srcdir) && AWK=$(AWK) ./get-them ../doc/m4.texinfo
	touch $(srcdir)/stamp-checks

check: stamp-checks
	PATH=`pwd`/../src:$$PATH; export PATH; \
	cd $(srcdir) && ./check-them $(CHECKS)



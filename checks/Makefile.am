EXTRA_DIST = check-them foo get-them incl.m4 stamp-checks $(CHECKS)
	
CHECKS = 01.define 02.define 03.arguments 04.arguments 05.arguments \
	06.arguments 07.pseudo_argu 08.pseudo_argu 09.pseudo_argu \
	10.pseudo_argu 11.pseudo_argu 12.undefine 13.defn 14.defn \
	15.pushdef 16.pushdef 17.indir 18.ifdef 19.ifelse 20.ifelse \
	21.loops 22.loops 23.dumpdef 24.trace 25.dnl 26.changequote \
	27.changequote 28.changequote 29.changecom 30.changecom \
	31.changesynta 32.changesynta 33.changesynta 34.changesynta \
	35.changesynta 36.changesynta 37.changesynta 38.m4wrap \
	39.include 40.include 41.include 42.divert 43.divert 44.undivert \
	45.undivert 46.undivert 47.divnum 48.cleardiv 49.cleardiv 50.len \
	51.index 52.regexp 53.regexp 54.substr 55.translit 56.patsubst \
	57.patsubst 58.format 59.incr 60.eval 61.eval 62.esyscmd \
	63.sysval 64.errprint 65.errprint 66.m4exit


all: stamp-checks

stamp-checks: get-them ../doc/m4.texinfo
	rm -f $(srcdir)/$(CHECKS)
	cd $(srcdir) && AWK=$(AWK) ./get-them ../doc/m4.texinfo
	touch $(srcdir)/stamp-checks

check: stamp-checks
	PATH=`pwd`/../src:$$PATH; export PATH; \
	cd $(srcdir) && ./check-them $(CHECKS)



EXTRA_DIST = check-them foo get-them incl.m4 stamp-checks $(CHECKS)

CHECKS = ??.*

all: stamp-checks

stamp-checks: get-them ../doc/m4.texinfo
	rm -f $(srcdir)/$(CHECKS)
	cd $(srcdir) && AWK=$(AWK) ./get-them ../doc/m4.texinfo
	touch $(srcdir)/stamp-checks

check: stamp-checks
	PATH=`pwd`/../src:$$PATH; export PATH; \
	cd $(srcdir) && ./check-them $(CHECKS)



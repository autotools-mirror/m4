AUTOMAKE_OPTIONS = 1.3b gnits

SUBDIRS = acm4 intl po lib src doc tests examples # modules

# These are excluded otherwise
EXTRA_DIST = modules/Makefile.am modules/Makefile.in \
	modules/test.c modules/test.m4 \
	modules/time.c modules/time.m4 \
	modules/README

# Tag before making distribution.  Also, don't make a distribution if
# checks fail.  Also, make sure the NEWS file is up-to-date.
cvs-dist: # distcheck # maintainer-check
	cvs -q tag `echo "Release-$(VERSION)" | sed 's/\./-/g'`
	$(MAKE) dist

cvs-diff:
	thisver=`echo "Release-$(VERSION)" | sed 's/\./-/g'`; \
	if test -z "$$OLDVERSION"; then \
	  prevno=`echo "$(VERSION)" - 0.01 | bc | sed 's/^\./0./'`; \
	else prevno="$$OLDVERSION"; fi; \
	prevver=Release-`echo $$prevno | sed 's/\./-/g'`; \
	cvs -f rdiff -c -r $$prevver -r $$thisver $(PACKAGE) \
	    > $(PACKAGE)-$$prevno-$(VERSION).diff

AUTOMAKE_OPTIONS = 1.4 gnits

ACLOCAL_AMFLAGS=-I acm4

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = ABOUT-NLS COPYING INSTALL Makefile.in aclocal.m4 \
	config.guess config.h.in config.sub configure install-sh \
	ltconfig ltmain.sh missing mkinstalldirs stamp-h.in \
	po/cat-id-tbl.c po/m4.pot po/stamp-cat-id

SUBDIRS =      acm4 intl po lib libltdl src doc tests examples @MODULES_DIR@
DIST_SUBDIRS = acm4 intl po lib libltdl src doc tests examples modules

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

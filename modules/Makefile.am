# To get libtool and Automake to handle this correctly it is necessary
# to pretend we are compiling shared libraries (which we are) and then
# change the names when installing.

pkglibexec_LTLIBRARIES = libtest.la libtime.la libstdlib.la

EXTRA_DIST = test.m4 time.m4 stdlib.m4

INCLUDES = -I$(srcdir)/../src -I$(srcdir)/../lib

libtest_la_SOURCES = test.c
libtime_la_SOURCES = time.c
libstdlib_la_SOURCES = stdlib.c

LIBS = 
LDFLAGS = @DLLDFLAGS@

install-pkglibexecLTLIBRARIES: $(pkglibexec_LTLIBRARIES)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkglibexecdir)
	@list='$(pkglibexec_LTLIBRARIES)'; for p in $$list; do \
          module=`echo "$$p" | sed -e 's/^lib\(.*\)\.la/\1/'`; \
	  lib=.libs/lib$$module.so.0.0.0; \
	  if test -f $$lib; then \
	    echo "${INSTALL} $$lib $(DESTDIR)$(pkglibexecdir)/$$module"; \
	    ${INSTALL} $$lib $(DESTDIR)$(pkglibexecdir)/$$module; \
	  else :; fi; \
	done

uninstall-pkglibexecLTLIBRARIES: $(pkglibexec_LTLIBRARIES)
	@$(NORMAL_INSTALL)
	@list='$(pkglibexec_LTLIBRARIES)'; for p in $$list; do \
          module=`echo "$$p" | sed -e 's/^lib\(.*\)\.la/\1/'`; \
	  if test -f $$lib; then \
	    echo "rm -f $(DESTDIR)$(pkglibexecdir)/$$module"; \
	    rm -f $(DESTDIR)$(pkglibexecdir)/$$module; \
	  else :; fi; \
	done

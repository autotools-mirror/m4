EXTRA_DIST = $(ACINCLUDES_M4)

ACINCLUDES_M4 = error.m4 gettext.m4 regex.m4

ACINCLUDE_M4 = $(top_srcdir)/acinclude.m4

all: $(ACINCLUDE_M4)

$(ACINCLUDE_M4): $(ACINCLUDES_M4)
	cat $(ACINCLUDES_M4) > $@
	cd $(top_srcdir) && $(ACLOCAL)
#	cd $(top_srcdir) && $(MAKE)

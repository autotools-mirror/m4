info_TEXINFOS = m4.texinfo

man_MANS = m4.1

EXTRA_DIST = helptoman.pl # $(MANS)

MAINTAINERCLEANFILES = Makefile.in mdate-sh texinfo.tex m4.1

PERL = @PERL@

%.1: helptoman.pl ../src/$*
	@if test -f ../src/$*; then \
	  echo "Updating the \`man' page \`$@'"; \
	  $(PERL) $(srcdir)/helptoman.pl ../src/$* > $@-tmp; \
	  cmp -s $@-tmp $(srcdir)/$@ || cp $@-tmp $(srcdir)/$@; \
	  rm -f $@-tmp; \
	else \
	  echo "WARNING: The \`man' page \`$@' cannot be updated yet."; \
	  echo "         Retry once the program executable will be ready."; \
	fi


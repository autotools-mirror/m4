EXTRA_DIST = $(pkgdata_DATA) $(TESTS) defs mktests.sh $(WEB_SITE)

MAINTAINERCLEANFILES = Makefile.in

WEB_SITE = \
	WWW/Makefile \
	WWW/_footer.htm \
	WWW/_header.htm \
	WWW/man/Makefile \
	WWW/m4lib/bugs.m4 \
	WWW/m4lib/changelog.m4 \
	WWW/m4lib/download.m4 \
	WWW/m4lib/features.m4 \
	WWW/m4lib/feedback.m4 \
	WWW/m4lib/html.m4 \
	WWW/m4lib/index.m4 \
	WWW/m4lib/layout.m4 \
	WWW/m4lib/lists.m4 \
	WWW/m4lib/menu.m4 \
	WWW/m4lib/modules.m4 \
	WWW/m4lib/news.m4 \
	WWW/m4lib/readme.m4 \
	WWW/m4lib/setup.m4 \
	WWW/m4lib/test.m4 \
	WWW/m4lib/thanks.m4 \
	WWW/m4lib/thissite.m4 \
	WWW/m4lib/tmpl.m4 \
	WWW/m4lib/todo.m4 \
	WWW/m4lib/uses.m4 \
	WWW/m4lib/visions.m4 \
	WWW/m4lib/whatis.m4
	
pkgdata_DATA = \
	capitalize.m4 \
	comments.m4 \
	ddivert.m4 \
	debug.m4 \
	esyscmd.m4 \
	exp.m4 \
	foreach.m4 \
	forloop.m4 \
	fstab.m4 \
	hanoi.m4 \
	incl-test.m4 \
	include.m4 \
	indir.m4 \
	misc.m4 \
	multiquotes.m4 \
	patsubst.m4 \
	pushpop.m4 \
	regexp.m4 \
	reverse.m4 \
	sysv-args.m4 \
	trace.m4 \
	translit.m4 \
	undivert.incl \
	undivert.m4 \
	wrap.m4

TESTS =	capitalize.test \
	comments.test \
	ddivert.test \
	debug.test \
	esyscmd.test \
	exp.test \
	foreach.test \
	forloop.test \
	fstab.test \
	hanoi.test \
	include.test \
	indir.test \
	misc.test \
	multiquotes.test \
	patsubst.test \
	pushpop.test \
	regexp.test \
	reverse.test \
	sysv-args.test \
	trace.test \
	translit.test \
	undivert.test \
	wrap.test


all:

defs: $(top_srcdir)/tests/defs
	cp $(top_srcdir)/tests/defs $@
	chmod 444 $@

clean-local:
	rm -rf testSubDir

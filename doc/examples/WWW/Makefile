M4	= ./m4/src/m4
M4OPTS	= -Im4lib

VPATH	= .:m4lib

HTM	= _footer.htm _header.htm bugs.htm changelog.htm download.htm \
	  features.htm feedback.htm index.htm lists.htm modules.htm \
	  news.htm readme.htm thanks.htm thissite.htm todo.htm uses.htm \
	  visions.htm whatis.htm

M4LIB	= m4lib/bugs.m4 m4lib/changelog.m4 m4lib/download.m4 \
	  m4lib/features.m4 m4lib/feedback.m4 m4lib/html.m4 \
	  m4lib/index.m4 m4lib/layout.m4 m4lib/lists.m4 m4lib/menu.m4 \
	  m4lib/modules.m4 m4lib/news.m4 m4lib/readme.m4 \
	  m4lib/setup.m4 m4lib/test.m4 m4lib/thanks.m4 \
	  m4lib/thissite.m4 m4lib/tmpl.m4 m4lib/todo.m4 m4lib/uses.m4 \
	  m4lib/visions.m4 m4lib/whatis.m4

all: ./m4 $(HTM)

./m4:
	ln -s ../.. m4

%.htm: %.m4
	@$(M4) $(M4OPTS) $< >new.htm && \
	if cmp new.htm $@ >/dev/null 2>&1; then \
		rm new.htm; \
		echo "$@ has not changed"; \
	else \
		 echo "$@ updated"; \
		mv new.htm $@; \
	fi

.FORCE:
$(HTM): .FORCE $(M4LIB)

changelog.htm: m4/ChangeLog
readme.htm: m4/README
todo.htm: m4/TODO
news.htm: m4/NEWS
modules.htm: m4/modules/README

.PHONY: man
man:
	cd man; $(MAKE)

M4	= m4
M4OPTS	= -Im4lib

VPATH	= .:m4lib

all: *.htm

%.htm: %.m4
	@$(M4) $(M4OPTS) $< >new.htm && \
	if cmp -s new.htm $@; then \
		rm new.htm; \
		echo "$@ has not changed"; \
	else \
		 echo "$@ updated"; \
		mv new.htm $@; \
	fi

*.htm: m4lib/*.m4

changelog.htm: m4/ChangeLog
readme.htm: m4/README
todo.htm: m4/TODO
news.htm: m4/NEWS
modules.htm: m4/modules/README

.PHONY: man
man:
	cd man; $(MAKE)

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

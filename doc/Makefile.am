## Makefile.am - template for generating Makefile via Automake
##
## Copyright (C) 2009-2010, 2013 Free Software Foundation, Inc.
##
## This file is part of GNU M4.
##
## GNU M4 is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## GNU M4 is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

MAINTAINERCLEANFILES =
EXTRA_DIST =
config_aux_dir = build-aux

info_TEXINFOS	= m4.texi
m4_TEXINFOS	= regexprops-generic.texi fdl-1.3.texi gpl-3.0.texi
dist_man_MANS	= $(srcdir)/m4.1
EXTRA_DIST	+= gendocs_template
MAINTAINERCLEANFILES += gendocs_template
HELP2MAN	= $(SHELL) $(top_srcdir)/$(config_aux_dir)/missing --run help2man

# Build the man page once in the srcdir, rather than in every VPATH build
# dir, to match how automake builds info pages.  This is safe for 'make
# distcheck' since it is distributed pre-built.
$(srcdir)/m4.1: ../.version $(srcdir)/../src/main.c
	@echo "Updating the \`man' page \`$@'"; \
	$(HELP2MAN) --name="macro processor" --source=FSF \
	  --info-page=m4 --output=$@ ../src/m4$(EXEEXT)

TAGS_FILES	= $(infos_TEXINFOS)
TAGS_DEPENDENCIES = $(TAGS_FILES)
ETAGS_ARGS	= --language=none --regex='/@node \([^,]*\)/\1/' $(TAGS_FILES)

MAINTAINERCLEANFILES += $(dist_man_MANS)

## This file is part of GNU m4
## Copyright 2000 Free Software Foundation, Inc.
##  
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or 
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
## 
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
## 02111-1307  USA
##
## Written by Gary V. Vaughan <gary@gnu.org>

MAINTAINERCLEANFILES = Makefile.in

testsubdir = testSubDir

EXTRA_DIST = frozen.m4 modtest.m4 shadow.m4 time.m4 time2.m4 stdlib.m4 \
	 unfrozen.m4 $(TESTS) defs format.c

TESTS = modtest.test shadow.test modfreeze.test \
	modpath1.test modpath2.test modpath3.test modpath4.test \
	unload.test

AM_CPPFLAGS = @LIBM4_DLL_IMPORT@
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/m4 $(INCLTDL) $(INTLINCL)
LIBS = $(top_builddir)/m4/libm4.la
LDFLAGS = -no-undefined

if WITH_MODULES
pkglibexec_LTLIBRARIES	= gnu.la load.la m4.la traditional.la \
			  modtest.la perl.la shadow.la stdlib.la time.la
TESTS_ENVIRONMENT	= WITH_MODULES=1
endif

gnu_la_SOURCES = gnu.c
gnu_la_LDFLAGS = -module

load_la_SOURCES = load.c
load_la_LDFLAGS = -module

m4_la_SOURCES = m4.c
m4_la_LDFLAGS = -module

traditional_la_SOURCES = traditional.c
traditional_la_LDFLAGS = -module

modtest_la_SOURCES = modtest.c
modtest_la_LDFLAGS = -module

perl_la_SOURCES = perl.c
perl_la_CFLAGS = `perl -MExtUtils::Embed -e ccopts`
perl_la_LDFLAGS = -module `perl -MExtUtils::Embed -e ldopts`

shadow_la_SOURCES = shadow.c
shadow_la_LDFLAGS = -module

stdlib_la_SOURCES = stdlib.c
stdlib_la_LDFLAGS = -module

time_la_SOURCES = time.c
time_la_LDFLAGS = -module

perlxsi.c:
	perl -MExtUtils::Embed -e xsinit -- -o perlxsi.c

perl.lo: perl.c perlxsi.c
	@echo '$(LTCOMPILE) $(perl_la_CFLAGS) -c $<'; \
	$(LTCOMPILE) $(perl_la_CFLAGS) -c $<

distclean-local:
	rm -rf $(testsubdir)

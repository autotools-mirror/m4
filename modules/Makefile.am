## This file is part of GNU m4
## Copyright (C) 2000, 2001, 2003 Free Software Foundation, Inc.
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

testsubdir		= testSubDir
pkgmodincludedir	= $(includedir)/$(PACKAGE)/modules
pkglibexecdir		= $(libexecdir)/$(PACKAGE)

ETAGS_ARGS = --regex='/M4BUILTIN_HANDLER[ \t]*(\([^)]*\)/\1/'

EXTRA_DIST = modtest.m4 shadow.m4 time.m4 time2.m4 stdlib.m4 format.c

AM_CPPFLAGS	= -I$(top_builddir) -I$(top_srcdir) \
		  -I$(top_builddir)/gnulib -I$(top_srcdir)/gnulib \
		  -I$(top_builddir)/m4 -I$(top_srcdir)/m4 $(INTLINCL)
AM_LDFLAGS 	= -no-undefined -module
LIBS		= $(top_builddir)/m4/libm4.la

pkgmodinclude_HEADERS	= m4.h
pkglibexec_LTLIBRARIES	= gnu.la load.la m4.la traditional.la \
			  modtest.la shadow.la import.la stdlib.la time.la

if USE_GMP
pkglibexec_LTLIBRARIES += mpeval.la
EXTRA_mpeval_la_SOURCES	= evalparse.c
mpeval_la_LIBADD	= $(LIBADD_GMP)
endif

EXTRA_gnu_la_SOURCES	= format.c
EXTRA_m4_la_SOURCES	= evalparse.c


## Disabled for now.  It is too fragile to be useful.
## pkglibexec_LTLIBRARIES += perl.la
##perl_la_CFLAGS		= `perl -MExtUtils::Embed -e ccopts`
##perl_la_LDFLAGS		= `perl -MExtUtils::Embed -e ldopts`
##BUILT_SOURCES 		= perlxsi.c
##DISTCLEANFILES 		= $(BUILT_SOURCES)
##perlxsi.c:
##	perl -MExtUtils::Embed -e xsinit -- -o perlxsi.c

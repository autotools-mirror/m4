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

DISTCLEANFILES = perlxsi.c
MAINTAINERCLEANFILES = Makefile.in

testsubdir = testSubDir
pkgmodincdir = $(includedir)/@PACKAGE@/modules

ETAGS_ARGS = --regex='/M4BUILTIN_HANDLER[ \t]*(\([^)]*\)/\1/'

EXTRA_DIST = modtest.m4 shadow.m4 time.m4 time2.m4 stdlib.m4 format.c

INCLUDES	= -I$(top_builddir) -I$(top_srcdir) \
		  -I$(top_builddir)/m4 -I$(top_srcdir)/m4 $(INTLINCL)
LIBS = $(top_builddir)/m4/libm4.la
AM_LDFLAGS = -no-undefined

pkgmodinc_HEADERS = m4.h
pkglibexec_LTLIBRARIES	= gnu.la load.la m4.la \
			  traditional.la perl.la \
			  modtest.la shadow.la import.la stdlib.la time.la

if USE_GMP
pkglibexec_LTLIBRARIES += mpeval.la
endif

gnu_la_SOURCES       = gnu.c
EXTRA_gnu_la_SOURCES = format.c
gnu_la_LDFLAGS       = -module

load_la_SOURCES = load.c
load_la_LDFLAGS = -module

m4_la_SOURCES		= m4.c
EXTRA_m4_la_SOURCES	= evalparse.c
m4_la_LDFLAGS		= -module

mpeval_la_SOURCES	= mpeval.c
EXTRA_mpeval_la_SOURCES	= evalparse.c
mpeval_la_LDFLAGS	= -module
mpeval_la_LIBADD	= $(LIBADD_GMP)

traditional_la_SOURCES = traditional.c
traditional_la_LDFLAGS = -module

modtest_la_SOURCES = modtest.c
modtest_la_LDFLAGS = -module

import_la_SOURCES = import.c
import_la_LDFLAGS = -module

perl_la_SOURCES = perl.c
perl_la_CFLAGS = `perl -MExtUtils::Embed -e ccopts`
perl_la_LDFLAGS = -module `perl -MExtUtils::Embed -e ldopts`

shadow_la_SOURCES = shadow.c
shadow_la_LDFLAGS = -module

stdlib_la_SOURCES = stdlib.c
stdlib_la_LDFLAGS = -module

time_la_SOURCES = time.c
time_la_LDFLAGS = -module

BUILT_SOURCES = perlxsi.c
perlxsi.c:
	perl -MExtUtils::Embed -e xsinit -- -o perlxsi.c

## This file is part of GNU m4
## Copyright (C) 2000, 2003, 2004 Free Software Foundation, Inc.
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

AM_CPPFLAGS	= -I$(top_builddir) -I$(top_srcdir) \
		  -I$(top_builddir)/libltdl -I$(top_srcdir)/libltdl \
		  -I$(top_builddir)/gnu -I$(top_srcdir)/gnu \
		  -I$(top_builddir)/m4 -I$(top_srcdir)/m4 $(INTLINCL)

bin_PROGRAMS		= m4
m4_SOURCES		= $(top_srcdir)/gnu/getopt.c		\
			  $(top_srcdir)/gnu/getopt1.c		\
			  $(top_srcdir)/gnu/getopt.h		\
			  $(top_srcdir)/gnu/version-etc.c	\
			  $(top_srcdir)/gnu/version-etc.h	\
			  main.c m4.h freeze.c stackovf.c
m4_LDADD		= $(top_builddir)/m4/libm4.la
m4_LDFLAGS		= -no-undefined -export-dynamic $(DLPREOPEN)

CLEANFILES		= m4$(EXEEXT)
MAINTAINERCLEANFILES	= Makefile.in

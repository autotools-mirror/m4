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

CLEANFILES = m4$(EXEEXT)
MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = $(LIBM4_DLL_IMPORT)
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/m4 $(INCLTDL) $(INTLINCL)

noinst_HEADERS = getopt.h

bin_PROGRAMS = m4
m4_SOURCES = main.c m4.h eval.c evalmp.c freeze.c stackovf.c
EXTRA_m4_SOURCES = numb.c numb.h getopt.c getopt1.c


m4_LDADD = $(M4OBJS) $(top_builddir)/m4/libm4.la $(LIBADD_DL) $(INTLLIBS)
m4_LDFLAGS = -no-undefined -export-dynamic $(DLPREOPEN)

$(bin_PROGRAMS): $(M4OBJS)

## Makefile.am -- Process this file with automake to produce Makefile.in
##
## Copyright (C) 2003 Free Software Foundation
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
## along with this program; see the file COPYING.  If not, write to
## the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
## Boston, MA 02111-1307, USA.
##
## Written by Gary V. Vaughan <gary@gnu.org>

gnulib_macrodir	= $(srcdir)/m4
gnulib_srcdir	= $(srcdir)/lib

GNULIB_SRCS	=				\
$(gnulib_srcdir)/obstack.c			\
$(gnulib_srcdir)/obstack.h			\
$(gnulib_srcdir)/stdbool_.h

GNULIB_MACROS	=				\
$(gnulib_macrodir)/obstack.m4			\
$(gnulib_macrodir)/onceonly_2_57.m4		\
$(gnulib_macrodir)/stdbool.m4

EXTRA_DIST	= $(GNULIB_SRCS) $(GNULIB_MACROS)
# Customize maint.mk.                           -*- makefile -*-
# Copyright (C) 2003-2010 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Used in maint.mk's web-manual rule
manual_title = GNU macro processor

# Tests not to run as part of "make syntax-check".
# M4 intentionally uses a coding style that compiles under C++.
# sc_proper_name_utf8_requires_ICONV doesn't work with non-recursive Makefile
# sc_po_check assumes a directory layout that we don't quite provide
local-checks-to-skip = sc_cast_of_x_alloc_return_value \
	sc_proper_name_utf8_requires_ICONV \
	sc_po_check

# PRAGMA_SYSTEM_HEADER includes #, which does not work through a
# Makefile variable, so we exempt it.
_makefile_at_at_check_exceptions = ' && !/PRAGMA_SYSTEM_HEADER/'

# Hash of NEWS contents, to ensure we don't add entries to wrong section.
old_NEWS_hash = 0ef74b67f86e4f4ec20b67f02d9b1124

# Always use longhand copyrights.
update-copyright-env = \
  UPDATE_COPYRIGHT_USE_INTERVALS=0 \
  UPDATE_COPYRIGHT_MAX_LINE_LENGTH=72

# Indent only with spaces.
sc_prohibit_tab_based_indentation:
 @re='^ *    '                                               \
 msg='TAB in indentation; use only spaces'                   \
   $(_prohibit_regexp)

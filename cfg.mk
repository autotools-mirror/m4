# Customize maint.mk.                           -*- makefile -*-
# Copyright (C) 2003-2011, 2013 Free Software Foundation, Inc.

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

# Always use shorthand copyrights.
update-copyright-env = \
  UPDATE_COPYRIGHT_USE_INTERVALS=1 \
  UPDATE_COPYRIGHT_MAX_LINE_LENGTH=72

# Tests not to run as part of "make syntax-check".
# M4 intentionally uses a coding style that compiles under C++.
# sc_proper_name_utf8_requires_ICONV doesn't work with non-recursive Makefile
# sc_po_check assumes a directory layout that we don't quite provide
local-checks-to-skip = sc_cast_of_x_alloc_return_value \
	sc_proper_name_utf8_requires_ICONV \
	sc_po_check \
	sc_bindtextdomain

# PRAGMA_SYSTEM_HEADER includes #, which does not work through a
# Makefile variable, so we exempt it.
_makefile_at_at_check_exceptions = ' && !/PRAGMA_SYSTEM_HEADER/'

# Hash of NEWS contents, to ensure we don't add entries to wrong section.
old_NEWS_hash = bbccada98ce08092a9f24b508c399051

# Indent only with spaces.
sc_prohibit_tab_based_indentation:
 @re='^ *    '                                               \
 msg='TAB in indentation; use only spaces'                   \
   $(_prohibit_regexp)

# List all syntax-check exemptions:
exclude_file_name_regexp--sc_cast_of_argument_to_free = ^m4/m4private.h$
exclude_file_name_regexp--sc_prohibit_always_true_header_tests = \
  ^Makefile.am$$
exclude_file_name_regexp--sc_prohibit_strncpy = ^m4/path.c$$
exclude_file_name_regexp--sc_prohibit_tab_based_indentation = \
  (^(GNU)?Makefile(\.am)?|\.mk|^HACKING|^ChangeLog.*)$$
exclude_file_name_regexp--sc_require_config_h = \
  ^modules/(evalparse|format)\.c$$
exclude_file_name_regexp--sc_require_config_h_first = \
  ^modules/(evalparse|format)\.c$$
exclude_file_name_regexp--update_copyright = \
  ^(doc/m4\.texi|ltdl/m4/gnulib-cache.m4)$$

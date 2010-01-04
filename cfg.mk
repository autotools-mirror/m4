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
local-checks-to-skip = sc_cast_of_x_alloc_return_value

# Hash of NEWS contents, to ensure we don't add entries to wrong section.
old_NEWS_hash = b63892a79436f9f3cd05e10c3c4657ef

# Always use longhand copyrights.
update-copyright-env = \
  UPDATE_COPYRIGHT_USE_INTERVALS=0 \
  UPDATE_COPYRIGHT_MAX_LINE_LENGTH=72

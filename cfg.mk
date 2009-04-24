# Customize maint.mk.                           -*- makefile -*-
# Copyright (C) 2003-2009 Free Software Foundation, Inc.

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

# Use alpha.gnu.org for alpha and beta releases.
# Use ftp.gnu.org for major releases.
gnu_ftp_host-alpha = alpha.gnu.org
gnu_ftp_host-beta = alpha.gnu.org
gnu_ftp_host-major = ftp.gnu.org
gnu_rel_host = $(gnu_ftp_host-$(RELEASE_TYPE))

# Used in maint.mk's web-manual rule
manual_title = GNU macro processor

url_dir_list = ftp://$(gnu_rel_host)/gnu/m4

# The GnuPG ID of the key used to sign the tarballs.
gpg_key_ID = F4850180

# Tests not to run as part of "make syntax-check".
# M4 intentionally uses a coding style that compiles under C++.
local-checks-to-skip = sc_cast_of_x_alloc_return_value

# Our files include "m4.h", which in turn includes <config.h> first.
config_h_header = "m4\.h"

# Hash of NEWS contents, to ensure we don't add entries to wrong section.
old_NEWS_hash = 0330971054cd4fb4e94b85fe367980f2

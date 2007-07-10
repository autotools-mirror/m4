/* GNU m4 -- A simple macro processor
   Copyright (C) 2000, 2006, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

#include <config.h>

/* Build using only the exported interfaces, unless NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

/* Rename exported symbols for dlpreload()ing.  */
#define m4_macro_table		traditional_LTX_m4_macro_table

/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  /* name		text */
#if UNIX
  { "unix",		"" },
#elif W32_NATIVE
  { "windows",		"" },
#elif OS2
  { "os2",		"" },
#else
# warning Platform macro not provided
#endif
  { "__traditional__",	"" },
  { NULL,		NULL },
};

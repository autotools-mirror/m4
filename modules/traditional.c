/* GNU m4 -- A simple macro processor
   Copyright (C) 2000, 2006, 2007, 2008, 2010 Free Software Foundation,
   Inc.

   This file is part of GNU M4.

   GNU M4 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU M4 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#define m4_macro_table          traditional_LTX_m4_macro_table

/* A table for mapping m4 symbol names to simple expansion text. */
const m4_macro m4_macro_table[] =
{
  /* name               text    min     max */
#if UNIX
  { "unix",             "",     0,      0 },
#elif W32_NATIVE
  { "windows",          "",     0,      0 },
#elif OS2
  { "os2",              "",     0,      0 },
#else
# warning Platform macro not provided
#endif
  { "__traditional__",  "",     0,      0 },
  { NULL,               NULL,   0,      0 },
};

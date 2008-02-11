/* GNU m4 -- A simple macro processor
   Copyright (C) 1999, 2000, 2001, 2003, 2004, 2006, 2007, 2008 Free
   Software Foundation, Inc.

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
#define m4_builtin_table	modtest_LTX_m4_builtin_table
#define m4_macro_table		modtest_LTX_m4_macro_table

#define export_test		modtest_LTX_export_test

extern bool export_test (const char *foo);

/*	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions					\
  BUILTIN (test,	false,	false,	false,	0,	0)

#define BUILTIN(handler, macros, blind, side, min, max) M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN

m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)	\
  { CONC(builtin_, handler), STR(handler),		\
    ((macros ? M4_BUILTIN_GROKS_MACRO : 0)		\
     | (blind ? M4_BUILTIN_BLIND : 0)			\
     | (side ? M4_BUILTIN_SIDE_EFFECT : 0)),		\
    min, max },

  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};

m4_macro m4_macro_table[] =
{
  /* name		text		min	max */
  { "__test__",		"`modtest'",	0,	0 },
  { "onearg",		"$1",		1,	1 },
  { "manyargs",		"$@",		0,	SIZE_MAX },
  { NULL,		NULL,		0,	0 },
};



/**
 * modtest()
 **/
M4INIT_HANDLER (modtest)
{
  const char *s = "Test module loaded.\n";

  /* Don't depend on OBS so that the traces are the same when used
     directly, or via a frozen file.  */
  fputs (s, stderr);
}


/**
 * modtest()
 **/
M4FINISH_HANDLER (modtest)
{
  const char *s = "Test module unloaded.\n";

  /* Don't depend on OBS so that the traces are the same when used
     directly, or via a frozen file.  */
  fputs (s, stderr);
}


/**
 * test()
 **/
M4BUILTIN_HANDLER (test)
{
  const char *s = "Test module called.";

  assert (obs != 0);
  obstack_grow (obs, s, strlen(s));
}


/**
 * export_test()
 **/
bool
export_test (const char *foo)
{
  if (foo)
    xfprintf (stderr, "%s\n", foo);
  return (bool) (foo != 0);
}

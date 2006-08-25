/* GNU m4 -- A simple macro processor
   Copyright (C) 1999, 2000, 2001, 2003, 2004, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <m4module.h>

#include <assert.h>

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
  /* name		text */
  { "__test__",		"`modtest'" },
  { NULL,		NULL },
};



/**
 * modtest()
 **/
M4INIT_HANDLER (modtest)
{
  const char *s = "Test module loaded.";

  /* Don't depend on OBS so that the traces are the same when used
     directly, or via a frozen file.  */
  fprintf (stderr, "%s\n", s);
}


/**
 * modtest()
 **/
M4FINISH_HANDLER (modtest)
{
  const char *s = "Test module unloaded.";

  /* Don't depend on OBS so that the traces are the same when used
     directly, or via a frozen file.  */
  fprintf (stderr, "%s\n", s);
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
    fprintf (stderr, "%s\n", foo);
  return (bool) (foo != 0);
}

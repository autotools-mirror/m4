/* GNU m4 -- A simple macro processor
   Copyright 1999, 2000, 2001, 2003 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307  USA
*/

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <m4module.h>

#include <assert.h>

#define m4_builtin_table	modtest_LTX_m4_builtin_table
#define m4_macro_table		modtest_LTX_m4_macro_table

#define export_test		modtest_LTX_export_test


/*		function	macros	blind minargs maxargs */
#define builtin_functions					\
	BUILTIN (test,		false,	false,	1,	1)

#define BUILTIN(handler, macros,  blind, min, max) M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN

m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, min, max)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind, min, max },

  builtin_functions
#undef BUILTIN

  { 0, 0, false, false, 0, 0 },
};

m4_macro m4_macro_table[] =
{
  /* name		text */
  { "__test__",		"`modtest'" },
  { 0,			0 },
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

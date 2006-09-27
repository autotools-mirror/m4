/* GNU m4 -- A simple macro processor
   Copyright (C) 2003, 2006 Free Software Foundation, Inc.

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

#include <config.h>

#include <m4module.h>

#include <assert.h>

#define m4_builtin_table	import_LTX_m4_builtin_table

/*	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions					\
  BUILTIN (import,	false,	false,	false,	0,	1)	\
  BUILTIN (symbol_fail,	false,	false,	false,	0,	1)	\
  BUILTIN (module_fail,	false,	false,	false,	0,	1)	\

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



typedef bool export_test_func (const char *);
typedef bool no_such_func (const char *);

/**
 * import()
 **/
M4BUILTIN_HANDLER (import)
{
  M4_MODULE_IMPORT (modtest, export_test);

  const char *s = "`import'::`import' called.";

  assert (obs != 0);
  obstack_grow (obs, s, strlen(s));

  if (export_test && export_test (M4ARG (1)))
    fputs ("TRUE\n", stderr);
}

/**
 * symbol_fail()
 **/
M4BUILTIN_HANDLER (symbol_fail)
{
  M4_MODULE_IMPORT (modtest, no_such);

  const char *s = "`import'::`symbol_fail' called.";

  assert (obs != 0);
  obstack_grow (obs, s, strlen(s));

  if (no_such && no_such (M4ARG (1)))
    fputs ("TRUE\n", stderr);
}

/**
 * module_fail()
 **/
M4BUILTIN_HANDLER (module_fail)
{
  M4_MODULE_IMPORT (no_such, no_such);

  const char *s = "`import'::`module_fail' called.";

  assert (obs != 0);
  obstack_grow (obs, s, strlen(s));

  if (no_such && no_such (M4ARG (1)))
    fputs ("TRUE\n", stderr);
}

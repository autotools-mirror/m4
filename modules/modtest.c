/* GNU m4 -- A simple macro processor
   Copyright (C) 1999, 2000 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <m4module.h>

#define m4_builtin_table	modtest_LTX_m4_builtin_table
#define m4_macro_table		modtest_LTX_m4_macro_table
#define m4_init_module		modtest_LTX_m4_init_module

m4_macro m4_macro_table[] =
{
  /* name		text */
  { "__test__",		"`modtest'" },
  { 0,			0 },
};

/*		function	macros	blind */
#define builtin_functions			\
	BUILTIN (test,		FALSE,	FALSE)	

#define BUILTIN(handler, macros,  blind)	M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN

m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind },

  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE },
};

void
m4_init_module (struct obstack *obs)
{
  char *s = "Test module loaded.";

  if (obs != 0)
    obstack_grow (obs, s, strlen(s));
}



/*-------.
| test() |
`-------*/
M4BUILTIN_HANDLER (test)
{
  char *s = "Test module called.";
  obstack_grow (obs, s, strlen(s));
}

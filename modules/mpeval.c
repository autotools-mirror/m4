/* GNU m4 -- A simple macro processor
   Copyright 2000 Free Software Foundation, Inc.
  
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

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	mpeval_LTX_m4_builtin_table
#define m4_macro_table		mpeval_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind */
#define builtin_functions			\
	BUILTIN(mpeval,		FALSE,	TRUE )


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind)	M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind },
  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE },
};


/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  /* name			text */
  { "__m4_gmp__",		"" },
  { 0, 0 },
};



M4BUILTIN_HANDLER (mpeval)
{
  do_eval(obs, argc, argv, mp_evaluate);
}

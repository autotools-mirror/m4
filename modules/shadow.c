/* GNU m4 -- A simple macro processor
   Copyright (C) 1999, 2000, 2006 Free Software Foundation, Inc.

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

#define LTDL_MODULE_NAME shadow

#define m4_builtin_table	shadow_LTX_m4_builtin_table
#define m4_macro_table		shadow_LTX_m4_macro_table

/*	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions			\
  BUILTIN (shadow,	false,	false,	false,	0,	-1 )	\
  BUILTIN (test,	false,	false,	false,	0,	-1 )	\


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
  { "__test__",		"`shadow'" },
  { NULL, NULL },
};



M4INIT_HANDLER (shadow)
{
  const lt_dlinfo *info = 0;
  const char *s = "Shadow module loaded.";

  if (handle)
    info = lt_dlgetinfo (handle);

  /* Only display the message on first load.  */
  if (obs && info && (info->ref_count == 1))
    obstack_grow (obs, s, strlen (s));
}



/**
 * shadow()
 **/
M4BUILTIN_HANDLER (shadow)
{
  const char *s = "Shadow::`shadow' called.";
  obstack_grow (obs, s, strlen (s));
}

/**
 * test()
 **/
M4BUILTIN_HANDLER (test)
{
  const char *s = "Shadow::`test' called.";
  obstack_grow (obs, s, strlen (s));
}

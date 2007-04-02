/* GNU m4 -- A simple macro processor
   Copyright (C) 1999, 2000, 2006, 2007 Free Software Foundation, Inc.

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

/* Build using only the exported interfaces, unless NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

#undef PACKAGE
#include "perlxsi.c"			/* Perl stuff */
#undef try
#undef _

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	perl_LTX_m4_builtin_table
#define m4_macro_table		perl_LTX_m4_macro_table

/*	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions					\
  BUILTIN (perleval,	false,	false,	false,	0,	-1  )	\


#define BUILTIN(handler, macros, blind, side, min, max)  M4BUILTIN(handler)
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

/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  /* name			text */
  { "__perleval__",		"" },
  { NULL, NULL },
};



static PerlInterpreter *my_perl;

M4INIT_HANDLER (perl)
{
  const lt_dlinfo *info = 0;
  char *embedding[] = { "", "-e", "0" };

  if (handle)
    info = lt_dlgetinfo (handle);

  /* Start up a perl parser, when loaded for the first time.  */
  if (info && (info->ref_count == 1))
    {
      my_perl = perl_alloc ();
      perl_construct (my_perl);

      perl_parse (my_perl, xs_init, 3, embedding, NULL);
      perl_run (my_perl);
    }
}

M4FINISH_HANDLER (perl)
{
  const lt_dlinfo *info = 0;

  if (handle)
    info = lt_dlgetinfo (handle);

  /* Recycle the perl parser, when unloaded for the last time.  */
  if (info && (info->ref_count == 1))
    {
      perl_destruct (my_perl);
      perl_free (my_perl);
    }
}



/**
 * perleval([PERLCODE], [...])
 **/
M4BUILTIN_HANDLER (perleval)
{
  SV *val;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
	obstack_1grow (obs, ',');

      val = perl_eval_pv(M4ARG(i), true);

      m4_shipout_string(context, obs, SvPV(val,PL_na), 0, false);
    }
}

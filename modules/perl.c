/* GNU m4 -- A simple macro processor
   Copyright (C) 1999, 2000, 2006, 2007, 2008, 2010 Free Software
   Foundation, Inc.

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

#undef PACKAGE
#include "perlxsi.c"                    /* Perl stuff */
#undef try
#undef _

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table        perl_LTX_m4_builtin_table
#define m4_macro_table          perl_LTX_m4_macro_table

/*         function     macros  blind   side    minargs maxargs */
#define builtin_functions                                       \
  BUILTIN (perleval,    false,  false,  false,  0,      -1  )   \


#define BUILTIN(handler, macros, blind, side, min, max)  M4BUILTIN (handler)
  builtin_functions
#undef BUILTIN

const m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)                 \
  M4BUILTIN_ENTRY (handler, #handler, macros, blind, side, min, max)

  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};

/* A table for mapping m4 symbol names to simple expansion text. */
const m4_macro m4_macro_table[] =
{
  /* name               text    min     max */
  { "__perleval__",     "",     0,      0 },
  { NULL,               NULL,   0,      0 },
};



static PerlInterpreter *my_perl;

M4INIT_HANDLER (perl)
{
  const lt_dlinfo *info = 0;
  char *embedding[] = { "", "-e", "0" };

  if (module)
    info = lt_dlgetinfo (module);

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

  if (module)
    info = lt_dlgetinfo (module);

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
  size_t i;

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
        obstack_1grow (obs, ',');

      val = perl_eval_pv (M4ARG (i), true);

      m4_shipout_string (context, obs, SvPV (val, PL_na), SIZE_MAX, false);
    }
}

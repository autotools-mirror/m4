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

#include "perlxsi.c"			/* Perl stuff */
#undef _

#include <m4module.h>			/* These are obligatory */

#define m4_builtin_table	perl_LTX_m4_builtin_table
#define m4_init_module		perl_LTX_m4_init_module
#define m4_finish_module	perl_LTX_m4_finish_module

/*		function	macros	blind */
#define builtin_functions			\
	BUILTIN (perleval,	FALSE,	FALSE)	

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

static PerlInterpreter *my_perl;

extern void xs_init (void);

void
m4_init_module (struct obstack *obs)
{
  char *embedding[] = { "", "-e", "0" };

  my_perl = perl_alloc ();
  perl_construct (my_perl);

  perl_parse (my_perl, xs_init, 3, embedding, NULL);
  perl_run (my_perl);
}

void
m4_finish_module(void)
{
  perl_destruct (my_perl);
  perl_free (my_perl);
}



/*----------------------------.
| perleval([PERLCODE], [...]) |
`----------------------------*/
M4BUILTIN_HANDLER (perleval)
{
  SV *val;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
	obstack_1grow (obs, ',');

      val = perl_eval_pv(M4ARG(i), TRUE);

      m4_shipout_string(obs, SvPV(val,na), 0, FALSE);
    }
}

/* GNU m4 -- A simple macro processor
   Copyright (C) 1998 Free Software Foundation, Inc.
  
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

#include <m4.h>				/* These are obligatory */
#include <builtin.h>

#define m4_macro_table		test_LTX_m4_macro_table
#define m4_init_module		test_LTX_m4_init_module
#define m4_finish_module	test_LTX_m4_finish_module

module_init_t m4_init_module;		/* initialisation function */
module_finish_t m4_finish_module;	/* cleanup function */

DECLARE(test);				/* declare test as implementing
					   a builtin */

#undef DECLARE

/* The table of builtins defined by this module - just one */

builtin m4_macro_table[] =
{
  /* name		GNUext	macros	blind	function */
  { "test",		FALSE,	FALSE,	FALSE,	test },
  { 0,			FALSE,	FALSE,	FALSE,	0 },
};

void
m4_init_module(struct obstack *obs)
{
  char *s = "Test module loaded.";
  obstack_grow (obs, s, strlen(s));
}

void
m4_finish_module(void)
{
  return;
}

/* The functions for builtins can be static */
static void
test (struct obstack *obs, int argc, token_data **argv)
{
  char *s = "Test module called";
  obstack_grow (obs, s, strlen(s));
}

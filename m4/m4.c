/* GNU m4 -- A simple macro processor
   Copyright 1989, 1990, 1991, 1992, 1993, 1994, 2001
   Free Software Foundation, Inc.

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

#include "m4private.h"

m4 *
m4_create (void)
{
  m4 *context = XCALLOC (m4, 1);

  context->symtab = m4_symtab_create (0, &context->no_gnu_extensions);
  context->syntax = m4_syntax_create ();

  context->debug_file	 = stderr;
  obstack_init (&context->trace_messages);

  context->nesting_limit = M4_DEFAULT_NESTING_LIMIT;

  return context;
}

void
m4_delete (m4 *context)
{
  assert (context);

  if (context->symtab)
    m4_symtab_delete (context->symtab);

  if (context->syntax)
    m4_syntax_delete (context->syntax);

  if (context->debug_file)
    fclose (context->debug_file);

  obstack_free (&context->trace_messages, NULL);

  xfree (context);
}



/* Use the preprocessor to generate the repetitive bit twiddling functions
   for us.  */
#undef m4_get_symbol_table
#undef m4_get_syntax_table
#undef m4_get_warning_status_opt
#undef m4_get_no_gnu_extensions_opt
#undef m4_get_nesting_limit_opt
#undef m4_get_debug_level_opt
#undef m4_get_max_debug_arg_length_opt
#undef m4_get_prefix_builtins_opt
#undef m4_get_suppress_warnings_opt
#undef m4_get_discard_comments_opt
#undef m4_get_interactive_opt
#undef m4_get_sync_output_opt
#undef m4_get_posixly_correct_opt


#define M4FIELD(type, base, field)					\
	type CONC(m4_get_, base) (m4 *context)				\
	{								\
	  assert (context);						\
	  return context->field;					\
	}
m4_context_field_table
#undef M4FIELD

#define M4FIELD(type, base, field)					\
	type CONC(m4_set_, base) (m4 *context, type value)		\
	{								\
	  assert (context);						\
	  return context->field = value;				\
	}
m4_context_field_table
#undef M4FIELD

#define M4OPT_BIT(bit, base) 						\
	boolean CONC(m4_get_, base) (m4 *context)			\
	{								\
	  assert (context);						\
	  return BIT_TEST (context->opt_flags, (bit));			\
	}
m4_context_opt_bit_table
#undef M4OPT_BIT

#define M4OPT_BIT(bit, base) 						\
	boolean CONC(m4_set_, base) (m4 *context, boolean value)	\
	{								\
	  assert (context);						\
	  if (value)							\
	     BIT_SET   (context->opt_flags, (bit));			\
	  else								\
	     BIT_RESET (context->opt_flags, (bit));			\
	  return value;							\
	}
m4_context_opt_bit_table
#undef M4OPT_BIT

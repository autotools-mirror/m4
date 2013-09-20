/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 2001, 2004, 2006-2008, 2010, 2013 Free
   Software Foundation, Inc.

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

#include "bitrotate.h"
#include "m4private.h"

#define DEFAULT_NESTING_LIMIT	1024
#define DEFAULT_NAMEMAP_SIZE    61

static size_t
hashfn (const void *ptr)
{
  const char *s = (const char *) ptr;
  size_t val = DEFAULT_NAMEMAP_SIZE;
  while (*s)
    val = rotl_sz (val, 7) + to_uchar (*s++);
  return val;
}


m4 *
m4_create (void)
{
  m4 *context = (m4 *) xzalloc (sizeof *context);

  context->symtab = m4_symtab_create (0);
  context->syntax = m4_syntax_create ();

  context->namemap =
    m4_hash_new (DEFAULT_NAMEMAP_SIZE, hashfn, (m4_hash_cmp_func *) strcmp);

  context->debug_file	 = stderr;
  obstack_init (&context->trace_messages);

  context->nesting_limit = DEFAULT_NESTING_LIMIT;
  context->debug_level = M4_DEBUG_TRACE_INITIAL;
  context->max_debug_arg_length = SIZE_MAX;

  context->search_path =
    (m4__search_path_info *) xzalloc (sizeof *context->search_path);
  m4__include_init (context);

  return context;
}

void
m4_delete (m4 *context)
{
  size_t i;
  assert (context);

  if (context->symtab)
    m4_symtab_delete (context->symtab);

  if (context->syntax)
    m4_syntax_delete (context->syntax);

  /* debug_file should have been reset to stdout or stderr, both of
     which are closed later.  */
  assert (context->debug_file == stderr || context->debug_file == stdout);

  obstack_free (&context->trace_messages, NULL);

  if (context->search_path)
    {
      m4__search_path *path = context->search_path->list;

      while (path)
        {
          m4__search_path *stale = path;
          path = path->next;

          DELETE (stale->dir); /* Cast away const.  */
          free (stale);
        }
      free (context->search_path);
    }

  for (i = 0; i < context->stacks_count; i++)
    {
      assert (context->arg_stacks[i].refcount == 0
              && context->arg_stacks[i].argcount == 0);
      if (context->arg_stacks[i].args)
        {
          obstack_free (context->arg_stacks[i].args, NULL);
          free (context->arg_stacks[i].args);
          obstack_free (context->arg_stacks[i].argv, NULL);
          free (context->arg_stacks[i].argv);
        }
    }
  free (context->arg_stacks);

  free (context);
}



/* Use the preprocessor to generate the repetitive bit twiddling functions
   for us.  Note the additional paretheses around the expanded function
   name to protect against macro expansion from the fast macros used to
   replace these functions when NDEBUG is defined.  */
#define M4FIELD(type, base, field)					\
        type (CONC(m4_get_, base)) (m4 *context)			\
        {								\
          assert (context);						\
          return context->field;					\
        }
m4_context_field_table
#undef M4FIELD

#define M4FIELD(type, base, field)					\
        type (CONC(m4_set_, base)) (m4 *context, type value)		\
        {								\
          assert (context);						\
          return context->field = value;				\
        }
m4_context_field_table
#undef M4FIELD

#define M4OPT_BIT(bit, base)						\
        bool (CONC(m4_get_, base)) (m4 *context)			\
        {								\
          assert (context);						\
          return BIT_TEST (context->opt_flags, (bit));			\
        }
m4_context_opt_bit_table
#undef M4OPT_BIT

#define M4OPT_BIT(bit, base)						\
        bool (CONC(m4_set_, base)) (m4 *context, bool value)		\
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

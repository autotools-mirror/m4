/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1999-2000, 2005-2008, 2010, 2013 Free
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

/* Code for all builtin macros, initialisation of symbol table, and
   expansion of user defined macros.  */

#include <config.h>

#include "m4private.h"

/* Comparison function, for use in bsearch, which compares NAME
   against the name of BUILTIN.  */
static int
compare_builtin_name_CB (const void *name, const void *b)
{
  const m4__builtin *builtin = (const m4__builtin *) b;
  return strcmp ((const char *) name, builtin->builtin.name);
}

/* Find the builtin which has NAME.  If MODULE is not NULL, then
   search only in MODULE's builtin table.  The result is a malloc'd
   symbol value, suitable for use in the symbol table or for an
   argument to m4_push_builtin.  */
m4_symbol_value * M4_GNUC_PURE
m4_builtin_find_by_name (m4 *context, m4_module *module, const char *name)
{
  m4_module *cur = module ? module : m4_module_next (context, NULL);
  m4__builtin *bp;

  do
    {
      bp = (m4__builtin *) bsearch (name, cur->builtins, cur->builtins_len,
                                    sizeof *bp, compare_builtin_name_CB);
      if (bp)
        {
          m4_symbol_value *token = (m4_symbol_value *) xzalloc (sizeof *token);
          m4__set_symbol_value_builtin (token, bp);
          return token;
        }
    }
  while (!module && (cur = m4_module_next (context, cur)));

  return NULL;
}

/* Find the builtin which has FUNC.  If MODULE argument is supplied
   then search only in MODULE's builtin table.  The result is a
   malloc'd symbol value, suitable for use in the symbol table or for
   an argument to m4_push_builtin.  */
m4_symbol_value * M4_GNUC_PURE
m4_builtin_find_by_func (m4 *context, m4_module *module, m4_builtin_func *func)
{
  m4_module *cur = module ? module : m4_module_next (context, NULL);
  size_t i;

  do
    {
      for (i = 0; i < cur->builtins_len; i++)
        if (cur->builtins[i].builtin.func == func)
          {
            m4_symbol_value *token =
              (m4_symbol_value *) xzalloc (sizeof *token);
            m4__set_symbol_value_builtin (token, &cur->builtins[i]);
            return token;
          }
    }
  while (!module && (cur = m4_module_next (context, cur)));

  return 0;
}

/* Print a representation of FUNC to OBS, optionally including the
   MODULE it came from.  If FLATTEN, output QUOTES around an empty
   string; if CHAIN, append the builtin to the chain; otherwise print
   the name of FUNC.  */
void
m4__builtin_print (m4_obstack *obs, const m4__builtin *func, bool flatten,
                   m4__symbol_chain **chain, const m4_string_pair *quotes,
                   bool module)
{
  assert (func);
  if (flatten)
    {
      if (quotes)
        {
          obstack_grow (obs, quotes->str1, quotes->len1);
          obstack_grow (obs, quotes->str2, quotes->len2);
        }
      module = false;
    }
  else if (chain)
    m4__append_builtin (obs, func, NULL, chain);
  else
    {
      obstack_1grow (obs, '<');
      obstack_grow (obs, func->builtin.name, strlen (func->builtin.name));
      obstack_1grow (obs, '>');
    }
  if (module)
    {
      const char *text = m4_get_module_name (func->module);
      obstack_1grow (obs, '{');
      obstack_grow (obs, text, strlen (text));
      obstack_1grow (obs, '}');
    }
}

/* GNU m4 -- A simple macro processor
   Copyright 1989-1994, 1999, 2000 Free Software Foundation, Inc.

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

/* Code for all builtin macros, initialisation of symbol table, and
   expansion of user defined macros.  */

#include "m4.h"
#include "m4private.h"

extern FILE *popen ();
extern int pclose ();



/* Find the builtin, which has NAME.  If BP argument is supplied
   then search only in table BP.  */
const m4_builtin *
m4_builtin_find_by_name (const m4_builtin *bp, const char *name)
{
  lt_dlhandle	handle	= NULL;

  while ((handle = lt_dlhandle_next (handle)))
    {
      m4_builtin *builtin = m4_module_builtins (handle);

      if (builtin && (bp == NULL || bp == builtin))
	{
	  for (; builtin->name != NULL; builtin++)
	    if (strcmp (builtin->name, name) == 0)
	      return builtin;
	}
    }

  return NULL;
}

const m4_builtin *
m4_builtin_find_by_func (const m4_builtin *bp, m4_builtin_func *func)
{
  lt_dlhandle	handle	= NULL;

  while ((handle = lt_dlhandle_next (handle)))
    {
      m4_builtin *builtin = m4_module_builtins (handle);

      if (builtin && (bp == NULL || bp == builtin))
	{
	  for (; builtin->name != NULL; builtin++)
	    if (builtin->func == func)
	      return builtin;
	}
    }

  return NULL;
}

m4_symbol *
m4_symbol_token (const char *name, m4_data_t type, m4_token *token,
		 m4_symbol *(*getter) (const char *name),
		 m4_symbol *(*setter) (m4_symbol *, m4_token *))
{
  m4_symbol *symbol;

  assert (name);
  assert (!token || (TOKEN_TYPE (token) == type));
  assert (getter);
  assert (setter);

  symbol = (*getter) (name);

  if (symbol)
    {
      m4_token empty;

      if (!token)
	{
	  bzero (&empty, sizeof (m4_token));
	  TOKEN_TYPE (&empty) = type;
	  switch (type)
	    {
	    case M4_TOKEN_TEXT:
	      TOKEN_TEXT (&empty) = "";
	      break;

	    case M4_TOKEN_FUNC:
	    case M4_TOKEN_VOID:
	      break;
	    }

	  token = &empty;
	}

      (*setter) (symbol, token);
    }

  return symbol;
}

void
m4_builtin_table_install (lt_dlhandle handle, const m4_builtin *table)
{
  const m4_builtin *bp;
  m4_token token;

  assert (handle);
  assert (table);

  bzero (&token, sizeof (m4_token));
  TOKEN_TYPE (&token)		= M4_TOKEN_FUNC;
  TOKEN_HANDLE (&token)		= handle;

  for (bp = table; bp->name != NULL; bp++)
    {
      int flags = 0;
      char *name;

      if (prefix_all_builtins)
	{
	  static const char prefix[] = "m4_";
	  size_t len = strlen (prefix) + strlen (bp->name);

	  name = (char *) xmalloc (1+ len);
	  snprintf (name, 1+ len, "%s%s", prefix, bp->name);
	}
      else
	name = (char *) bp->name;

      if (bp->groks_macro_args) BIT_SET (flags, TOKEN_MACRO_ARGS_BIT);
      if (bp->blind_if_no_args) BIT_SET (flags, TOKEN_BLIND_ARGS_BIT);

      TOKEN_FUNC (&token)	= bp->func;
      TOKEN_FLAGS (&token)	= flags;
      TOKEN_MIN_ARGS (&token)	= bp->min_args;
      TOKEN_MAX_ARGS (&token)	= bp->max_args;

      m4_builtin_pushdef (name, &token);

      if (prefix_all_builtins)
	xfree (name);
    }
}

void
m4_macro_table_install (lt_dlhandle handle, const m4_macro *table)
{
  const m4_macro *mp;
  m4_token token;

  bzero (&token, sizeof (m4_token));
  TOKEN_TYPE (&token)		= M4_TOKEN_TEXT;
  TOKEN_HANDLE (&token)		= handle;

  for (mp = table; mp->name != NULL; mp++)
    {
      TOKEN_TEXT (&token)	= (char *) mp->value;
      m4_macro_pushdef (mp->name, &token);
    }
}

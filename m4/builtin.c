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

#define M4_ARG_SIGNATURE_DEFAULT_SIZE 7

static m4_hash *m4_arg_signature_parse (const char *name, const char *param);



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
  const char *openp  = NULL;
  const char *params = NULL;
  m4_symbol *symbol;
  size_t len;

  assert (name);
  assert (!token || (TOKEN_TYPE (token) == type));
  assert (getter);
  assert (setter);

  if (!no_gnu_extensions)
    {
      /* If name contains an open paren, then parse before that paren
	 as the actual name, and the rest as a formal parameter list.  */
      len = 0;
      for (openp = name; *openp && !M4_IS_OPEN (*openp); ++openp)
	++len;

      if (*openp)
	{
	  name   = xstrzdup (name, len);
	  params = 1+ openp;
	}
    }

  /* Get a symbol table entry for the name.  */
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

      if (params)
	{
	  /* Make a hash table to map formal parameter names to
	     argv offsets, and store that in the symbol's token.  */
	  TOKEN_ARG_SIGNATURE (token) = m4_arg_signature_parse (name, params);
	}

      (*setter) (symbol, token);
    }

  /* If we split name on open paren, free the copied substring.  */
  if (params)
    xfree ((char *) name);

  return symbol;
}

static m4_hash *
m4_arg_signature_parse (const char *name, const char *params)
{
  m4_hash *arg_signature;
  const char *commap;
  int index;

  assert (params);

  arg_signature = m4_hash_new (M4_ARG_SIGNATURE_DEFAULT_SIZE,
			m4_hash_string_hash, m4_hash_string_cmp);

  for (index = 1; *params && !M4_IS_CLOSE (*params); ++index)
    {
      size_t len = 0;

      /* Skip leading whitespace.  */
      while (M4_IS_SPACE (*params))
	++params;

      for (commap = params; *commap && M4_IS_IDENT (*commap); ++commap)
	++len;

      /* Skip trailing whitespace.  */
      while (M4_IS_SPACE (*commap))
	++commap;

      if (!M4_IS_COMMA (*commap) && !M4_IS_CLOSE (*commap))
	M4ERROR ((EXIT_FAILURE, 0,
		  _("Error: %s: syntax error in parameter list at char `%c'"),
		  name, *commap));

      /* Skip parameter delimiter.  */
      if (M4_IS_COMMA (*commap))
	++commap;

      if (len)
	{
	  struct m4_token_arg *arg = XCALLOC (struct m4_token_arg, 1);

	  TOKEN_ARG_INDEX (arg) = index;

	  m4_hash_insert (arg_signature, xstrzdup (params, len), arg);

	  params = commap;
	}
    }

  if (!M4_IS_CLOSE (*commap))
    M4WARN ((warning_status, 0,
	     _("Warning: %s: unterminated parameter list"), name));

  return arg_signature;
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

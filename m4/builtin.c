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
m4_builtin_find_by_name (bp, name)
     const m4_builtin *bp;
     const char *name;
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
m4_builtin_find_by_func (bp, func)
     const m4_builtin *bp;
     m4_builtin_func *func;
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


/* Install a builtin macro with name NAME, bound to the C function given in
   BP.  MODE is SYMBOL_INSERT or SYMBOL_PUSHDEF.  TRACED defines whether
   NAME is to be traced.  */
void
m4_builtin_define (handle, name, bp, mode, traced)
     const lt_dlhandle handle;
     const char	*name;
     const m4_builtin *bp;
     m4_symbol_lookup mode;
     boolean traced;
{
  m4_symbol *symbol;

  symbol = m4_lookup_symbol (name, mode);
  if (symbol)
    {
      if (SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
        xfree (SYMBOL_TEXT (symbol));

      SYMBOL_HANDLE (symbol)		= handle;
      SYMBOL_TYPE (symbol)		= M4_TOKEN_FUNC;
      SYMBOL_MACRO_ARGS (symbol)	= bp->groks_macro_args;
      SYMBOL_BLIND_NO_ARGS (symbol)	= bp->blind_if_no_args;
      SYMBOL_FUNC (symbol)		= bp->func;
      SYMBOL_TRACED (symbol)		= traced;
    }
}

void
m4_builtin_table_install (handle, table)
     const lt_dlhandle handle;
     const m4_builtin *table;
{
  const m4_builtin *bp;
  char *string;

  for (bp = table; bp->name != NULL; bp++)
    if (prefix_all_builtins)
      {
	string = (char *) xmalloc (strlen (bp->name) + 4);
	strcpy (string, "m4_");
	strcat (string, bp->name);
	m4_builtin_define (handle, string, bp, M4_SYMBOL_PUSHDEF, FALSE);
	free (string);
      }
    else
      m4_builtin_define (handle, bp->name, bp, M4_SYMBOL_PUSHDEF, FALSE);
}

/* Define a predefined or user-defined macro, with name NAME, and expansion
   TEXT.  MODE destinguishes between the "define" and the "pushdef" case.
   It is also used from main ().  */
void
m4_macro_define (handle, name, text, mode)
     const lt_dlhandle handle;
     const char *name;
     const char *text;
     m4_symbol_lookup mode;
{
  m4_symbol *symbol;

  symbol = m4_lookup_symbol (name, mode);
  if (symbol)
    {
      if (SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
        xfree (SYMBOL_TEXT (symbol));

      SYMBOL_HANDLE (symbol) 		= handle;
      SYMBOL_TYPE (symbol) 		= M4_TOKEN_TEXT;
      SYMBOL_MACRO_ARGS (symbol)	= FALSE;
      SYMBOL_BLIND_NO_ARGS (symbol)	= FALSE;
      SYMBOL_TEXT (symbol) 		= xstrdup (text);
      /* Do not reset SYMBOL_TRACED as it means that --trace would be
	 usable only for existing macros.  m4_lookup_symbol takes care
	 of its proper initialization.  */
    }
}

void
m4_macro_table_install (handle, table)
     const lt_dlhandle handle;
     const m4_macro *table;
{
  const m4_macro *mp;

  for (mp = table; mp->name != NULL; mp++)
    m4_macro_define (handle, mp->name, mp->value, M4_SYMBOL_PUSHDEF);
}

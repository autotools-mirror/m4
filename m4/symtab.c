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

/* This file handles all the low level work around the symbol table.  The
   symbol table is a simple chained hash table.  Each symbol is described
   by a struct symbol, which is placed in the hash table based upon the
   symbol name.  Symbols that hash to the same entry in the table are
   kept on a list, sorted by name.  As a special case, to facilitate the
   "pushdef" and "popdef" builtins, a symbol can be several times in the
   symbol table, one for each definition.  Since the name is the same,
   all the entries for the symbol will be on the same list, and will
   also, because the list is sorted, be adjacent.  All the entries for a
   name are simply ordered on the list by age.  The current definition
   will then always be the first found.  */

#include "m4private.h"

/* Initialise the symbol table, by allocating the necessary storage, and
   zeroing all the entries.  */

/* Pointer to symbol table.  */
m4_symbol **m4_symtab;

void
m4_symtab_init ()
{
  int i;
  m4_symbol **symbol;

  symbol = m4_symtab = XMALLOC (m4_symbol *, hash_table_size);

  for (i = hash_table_size; --i >= 0;)
    *symbol++ = NULL;
}

/* Return a hashvalue for a string, from GNU-emacs.  */
static int
hash (const char *s)
{
  register int val = 0;

  register const char *ptr = s;
  register char ch;

  while ((ch = *ptr++) != '\0')
    {
      if (ch >= 0140)
	ch -= 40;
      val = ((val << 3) + (val >> 28) + ch);
    };
  val = (val < 0) ? -val : val;
  return val % hash_table_size;
}

/* Free all storage associated with a symbol.  */
static void
free_symbol (m4_symbol *symbol)
{
  if (SYMBOL_NAME (symbol))
    xfree (SYMBOL_NAME (symbol));
  if (SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    xfree (SYMBOL_TEXT (symbol));
  xfree ((VOID *) symbol);
}

/* Search in, and manipulation of the symbol table, are all done by
   m4_lookup_symbol ().  It basically hashes NAME to a list in the symbol
   table, and searched this list for the first occurence of a symbol with
   the name.

   The MODE parameter determines what m4_lookup_symbol () will do.  It can
   either just do a lookup, do a lookup and insert if not present, do an
   insertion even if the name is already in the list, delete the first
   occurrence of the name on the list or delete all occurences of the name
   on the list.  */
m4_symbol *
m4_lookup_symbol (const char *name, m4_symbol_lookup mode)
{
  if (mode != M4_SYMBOL_IGNORE)
    {
      int cmp = 1;
      int h = hash (name);
      m4_symbol *symbol = m4_symtab[h];
      m4_symbol **spp, *prev;

      for (prev = NULL; symbol != NULL; prev = symbol, symbol = symbol->next)
	{
	  cmp = strcmp (SYMBOL_NAME (symbol), name);
	  if (cmp >= 0)
	    break;
	}

      /* If just searching, return status of search.  */

      if (mode == M4_SYMBOL_LOOKUP)
	return cmp == 0 ? symbol : NULL;

      /* Symbol not found.  */

      spp = (prev != NULL) ?  &prev->next : &m4_symtab[h];

      switch (mode)
	{

	case M4_SYMBOL_INSERT:

	  /* Return the symbol, if the name was found in the table.
	     Otherwise, just insert the name, and return the new symbol.  */

	  if (cmp == 0 && symbol != NULL)
	    return symbol;
	  /* Fall through.  */

	case M4_SYMBOL_PUSHDEF:

	  /* Insert a name in the symbol table.  If there is already a symbol
	     with the name, insert this in front of it, and mark the old
	     symbol as "shadowed".  */

	  symbol = XMALLOC (m4_symbol, 1);
	  SYMBOL_TYPE (symbol) = M4_TOKEN_VOID;
	  SYMBOL_TRACED (symbol) = SYMBOL_SHADOWED (symbol) = FALSE;
	  SYMBOL_NAME (symbol) = xstrdup (name);

	  SYMBOL_NEXT (symbol) = *spp;
	  (*spp) = symbol;

	  if (mode == M4_SYMBOL_PUSHDEF && cmp == 0)
	    {
	      SYMBOL_SHADOWED (SYMBOL_NEXT (symbol)) = TRUE;
	      SYMBOL_TRACED (symbol) = SYMBOL_TRACED (SYMBOL_NEXT (symbol));
	    }
	  return symbol;

	case M4_SYMBOL_DELETE:

	  /* Delete all occurences of symbols with NAME.  */

	  if (cmp != 0 || symbol == NULL)
	    return NULL;
	  do
	    {
	      *spp = SYMBOL_NEXT (symbol);
	      free_symbol (symbol);
	      symbol = *spp;
	    }
	  while (symbol != NULL && strcmp (name, SYMBOL_NAME (symbol)) == 0);
	  return NULL;

	case M4_SYMBOL_POPDEF:

	  /* Delete the first occurence of a symbol with NAME.  */

	  if (cmp != 0 || symbol == NULL)
	    return NULL;
	  if (SYMBOL_NEXT (symbol) != NULL && cmp == 0)
	    SYMBOL_SHADOWED (SYMBOL_NEXT (symbol)) = FALSE;
	  *spp = SYMBOL_NEXT (symbol);
	  free_symbol (symbol);
	  return NULL;

	default:
	  M4ERROR ((warning_status, 0,
		    _("INTERNAL ERROR: Illegal mode to m4_symbol_lookup ()")));
	  abort ();
	}
    }

  return NULL;
}

/* The following function removes from the symbol table, every symbol
   that references a function in the given builtin table.  */
void
m4_remove_table_reference_symbols (builtins, macros)
     m4_builtin *builtins;
     m4_macro *macros;
{
  /* FIXME:  This can be reimplemented to work much faster now that each
     symtab entry carries a reference to its defining module.  */
  m4_symbol *symbol;
  int h;

  /* Look in each bucket of the hashtable... */
  for (h = 0; h < hash_table_size; h++)
    {
      m4_symbol *prev = NULL;
      m4_symbol *next = NULL;

      /* And each symbol in each hash bucket... */
      for (symbol = m4_symtab[h]; symbol != NULL; prev = symbol, symbol = next)
	{
	  next = SYMBOL_NEXT (symbol);

	  switch (SYMBOL_TYPE (symbol))
	    {
	    case M4_TOKEN_FUNC:
	      /* For symbol functions referencing a function in
		 BUILTIN_TABLE.  */
	      {
		const m4_builtin *bp;
		for (bp = builtins; symbol && bp && bp->name != NULL; ++bp)
		  if (SYMBOL_FUNC (symbol) == bp->func)
		    {
		      if (prev)
			SYMBOL_NEXT (prev) = SYMBOL_NEXT (symbol);
		      else
			m4_symtab[h] = SYMBOL_NEXT (symbol);

		      /* Unshadow any symbol that this one had shadowed. */
		      if (SYMBOL_NEXT (symbol) != NULL)
			SYMBOL_SHADOWED (SYMBOL_NEXT (symbol)) = FALSE;

		      free_symbol (symbol);

		      /* Maintain the loop invariant. */
		      symbol = prev;
		    }
	      }
	      break;

	    case M4_TOKEN_TEXT:
	      /* For symbol macros referencing a value in
		 PREDEFINED_TABLE.  */
	      {
		const m4_macro *mp;
		for (mp = macros; symbol && mp && mp->name; mp++)
		  if ((strcmp(SYMBOL_NAME (symbol), mp->name) == 0)
		      && (strcmp (SYMBOL_TEXT (symbol), mp->value) == 0))
		    {
		      if (prev)
			SYMBOL_NEXT (prev) = SYMBOL_NEXT (symbol);
		      else
			m4_symtab[h] = SYMBOL_NEXT (symbol);

		      /* Unshadow any symbol that this one had shadowed. */
		      if (SYMBOL_NEXT (symbol) != NULL)
			SYMBOL_SHADOWED (SYMBOL_NEXT (symbol)) = FALSE;

		      free_symbol (symbol);

		      /* Maintain the loop invariant. */
		      symbol = prev;
		    }
	      }
	      break;

	    default:
	      /*NOWORK*/
	      break;
	    }
	}
    }
}

/* The following function is used for the cases, where we want to do
   something to each and every symbol in the table.  The function
   hack_all_symbols () traverses the symbol table, and calls a specified
   function FUNC for each symbol in the table.  FUNC is called with a
   pointer to the symbol, and the DATA argument.  */
void
m4_hack_all_symbols (func, data)
     m4_hack_symbol *func;
     const char *data;
{
  int h;
  m4_symbol *symbol;

  for (h = 0; h < hash_table_size; h++)
    for (symbol = m4_symtab[h]; symbol != NULL; symbol = SYMBOL_NEXT (symbol))
      (*func) (symbol, data);
}

#ifdef DEBUG_SYM

static void symtab_debug M4_PARAMS((void));
static void symtab_print_list M4_PARAMS((int i));

static void
symtab_debug ()
{
  m4_token_data_t t;
  m4_token_data td;
  const char *text;
  m4_symbol *s;
  int delete;

  while ((t = next_token (&td)) != NULL)
    {
      if (t != M4_TOKEN_WORD)
	continue;
      text = M4_TOKEN_DATA_TEXT (&td);
      if (*text == '_')
	{
	  delete = 1;
	  text++;
	}
      else
	delete = 0;

      s = m4_lookup_symbol (text, M4_SYMBOL_LOOKUP);

      if (s == NULL)
	printf (_("Name `%s' is unknown\n"), text);

      if (delete)
	(void) m4_lookup_symbol (text, M4_SYMBOL_DELETE);
      else
	(void) m4_lookup_symbol (text, M4_SYMBOL_INSERT);
    }
  m4_hack_all_symbols (symtab_print_list, "foo");
}

static void
symtab_print_list (i)
     int i;
{
  m4_symbol *symbol;

  printf ("Symbol dump #%d:\n", i);
  for (symbol = m4_symtab[i]; symbol != NULL; symbol = symbol->next)
    printf ("\tname %s, addr 0x%x, next 0x%x, flags%s%s\n",
	   SYMBOL_NAME (symbol), symbol, symbol->next,
	   SYMBOL_TRACED (symbol) ? " traced" : "",
	   SYMBOL_SHADOWED (symbol) ? " shadowed" : "");
}

#endif /* DEBUG_SYM */

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

static size_t	m4_symtab_hash		(const void *key);
static int	m4_symtab_cmp		(const void *key, const void *try);
static void	m4_token_data_delete	(m4_token_data *data);
static void	m4_symbol_pop		(m4_symbol *symbol);
static void	m4_symbol_del		(m4_symbol *symbol);


/* Pointer to symbol table.  */
m4_hash *m4_symtab = 0;



void
m4_symtab_init (void)
{
  m4_symtab = m4_hash_new (m4_symtab_hash, m4_symtab_cmp);
}

/* Return a hashvalue for a string, from GNU-emacs.  */
size_t
m4_symtab_hash (const void *key)
{
  int val = 0;
  const char *ptr = (const char *) key;
  char ch;

  while ((ch = *ptr++) != '\0')
    {
      if (ch >= 0140)
	ch -= 40;
      val = ((val << 3) + (val >> 28) + ch);
    };
  val = (val < 0) ? -val : val;
  return val;
}

int
m4_symtab_cmp (const void *key, const void *try)
{
  return (strcmp ((const char *) key, (const char *) try));
}



void
m4_token_data_delete (m4_token_data *data)
{
  assert (data);
  assert (M4_TOKEN_DATA_NEXT (data) == 0);

  if (M4_TOKEN_DATA_TYPE (data) == M4_TOKEN_TEXT)
    XFREE (M4_TOKEN_DATA_TEXT (data));

  XFREE (data);
}

void
m4_symbol_pop (m4_symbol *symbol)
{
  m4_token_data *stale;

  assert (symbol);
  assert (M4_SYMBOL_DATA_NEXT (symbol));

  stale				= M4_SYMBOL_DATA (symbol);
  M4_SYMBOL_DATA (symbol)	= M4_TOKEN_DATA_NEXT (stale);

#ifndef NDEBUG
  M4_TOKEN_DATA_NEXT (stale) = 0;
#endif
  m4_token_data_delete (stale);
}

/* Free all storage associated with a symbol.  */
void
m4_symbol_del (m4_symbol *symbol)
{
  assert (symbol);

  while (M4_SYMBOL_DATA_NEXT (symbol))
    m4_symbol_pop (symbol);

  assert (M4_SYMBOL_DATA_NEXT (symbol) == 0);

  if (M4_SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    XFREE (M4_SYMBOL_TEXT (symbol));

  XFREE (M4_SYMBOL_DATA (symbol));
  XFREE (symbol);
}


/* Dispatch on all the following 5 functions.

   Search in, and manipulation of the symbol table, are all done by
   m4_lookup_symbol ().  It basically hashes NAME to a list in the
   symbol table, and searched this list for the first occurence of a
   symbol with the name.

   The MODE parameter determines what m4_lookup_symbol () will do.  It
   can either just do a lookup, do a lookup and insert if not present,
   do an insertion even if the name is already in the list, delete the
   first occurrence of the name on the list or delete all occurences
   of the name on the list.  */
m4_symbol *
m4_lookup_symbol (const char *name, m4_symbol_lookup_t mode)
{
  switch (mode)
    {
    case M4_SYMBOL_INSERT:
      return m4_symbol_insert (name);

    case M4_SYMBOL_PUSHDEF:
      return m4_symbol_pushdef (name);
    }

  assert (0);
  /*NOTREACHED*/
  return 0;
}


/* Return the symbol associated to NAME, or else NULL.  */
m4_symbol *
m4_symbol_lookup (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  /* If just searching, return status of search.  */
  return psymbol ? *psymbol : 0;
}


/* Push a new slot associted to NAME, and return it.  */
m4_symbol *
m4_symbol_pushdef (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  /* Insert a name in the symbol table.  If there is already a symbol
     with the name, push the new value on top of the value stack for
     this symbol.  */
  m4_symbol *symbol    = 0;
  m4_token_data *value = XCALLOC (m4_token_data, 1);

  if (psymbol)
    {
      symbol = *psymbol;
      M4_TOKEN_DATA_NEXT (value) = M4_SYMBOL_DATA (symbol);
    }
  else
    symbol = XCALLOC (m4_symbol, 1);

  M4_SYMBOL_DATA (symbol)	= value;
  M4_SYMBOL_TYPE (symbol)	= M4_TOKEN_VOID;

  if (!psymbol)
    m4_hash_insert (m4_symtab, xstrdup (name), symbol);

  return symbol;
}


/* Return a slot associated to NAME, creating it if needed.  */
m4_symbol *
m4_symbol_insert (const char *name)
{
  m4_symbol *res = m4_symbol_lookup (name);
  if (res)
    return res;
  else
    m4_symbol_pushdef (name);
}


/* Remove the topmost definition associated to NAME.  */
void
m4_symbol_popdef (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  assert (psymbol);

  if (M4_SYMBOL_DATA_NEXT (*psymbol))
    m4_symbol_pop (*psymbol);
  else
    {
      xfree (m4_hash_remove (m4_symtab, name));
      m4_symbol_del (*psymbol);
    }
}


/* Remove all the definitions associated with NAME.  */
void
m4_symbol_delete (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  assert (psymbol);
  xfree (m4_hash_remove (m4_symtab, name));
  m4_symbol_del (*psymbol);
}


/* Remove every symbol that references the given module handle from
   the symbol table.  */
void
m4_remove_table_reference_symbols (lt_dlhandle handle)
{
  m4_hash_iterator *place = 0;

  assert (handle);

  while ((place = m4_hash_iterator_next (m4_symtab, place)))
    {
      m4_symbol *symbol = (m4_symbol *) m4_hash_iterator_value (place);
      m4_token_data *data = M4_SYMBOL_DATA (symbol);

      /* Purge any shadowed references.  */
      while (M4_TOKEN_DATA_NEXT (data))
	{
	  m4_token_data *next = M4_TOKEN_DATA_NEXT (data);

	  if (M4_TOKEN_DATA_HANDLE (next) == handle)
	    {
	      M4_TOKEN_DATA_NEXT (data) = M4_TOKEN_DATA_NEXT (next);
	      m4_token_data_delete (next);
	    }
	  else
	    data = next;
	}

      /* Purge live reference.  */
      if (M4_SYMBOL_HANDLE (symbol) == handle)
	{
	  if (M4_SYMBOL_DATA_NEXT (symbol))
	    m4_symbol_pop (symbol);
	  else
	    {
	      xfree (m4_hash_remove (m4_symtab, m4_hash_iterator_key (place)));
	      m4_symbol_del (symbol);
	    }
	}
    }
}

/* The following function is used for the cases, where we want to do
   something to each and every symbol in the table.  The function
   hack_all_symbols () traverses the symbol table, and calls a specified
   function FUNC for each symbol in the table.  FUNC is called with a
   pointer to the symbol, and the DATA argument.  */
int
m4_symtab_apply (m4_symtab_apply_func *func, void *data)
{
  int result = 0;
  m4_hash_iterator *place = 0;

  assert (func);

  while ((place = m4_hash_iterator_next (m4_symtab, place)))
    {
      const char *name	= (const char *) m4_hash_iterator_key (place);
      m4_symbol *symbol = (m4_symbol *) m4_hash_iterator_value (place);
      result = (*func) (name, symbol, data);

      if (result != 0)
	break;
    }

  return result;
}



#ifdef DEBUG_SYM

static int symtab_print_list (const char *name, m4_symbol *symbol, void *ignored);

static void
symtab_debug (void)
{
  m4_token_data_t t;
  m4_token_data td;
  const char *text;
  m4_symbol *s;
  int delete;

  while ((t = m4_next_token (&td)) != NULL)
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

      s = m4_symbol_lookup (text);

      if (s == NULL)
	printf (_("Name `%s' is unknown\n"), text);

      if (delete)
	(void) m4_symbol_delete (text);
      else
	(void) m4_symbol_insert (text);
    }
  m4_symtab_apply (symtab_print_list, 0);
}

static int
symtab_print_list (const char *name, m4_symbol *symbol, void *ignored)
{
  printf ("\tname %s, addr %#x, flags: %s\n",
	  name, (unsigned) symbol,
	  M4_SYMBOL_TRACED (symbol) ? " traced" : "<none>");
  return 0;
}

#endif /* DEBUG_SYM */

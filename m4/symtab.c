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

#define DEBUG_SYM		/* Define this to see runtime debug info.  */
#undef DEBUG_SYM

/* This file handles all the low level work around the symbol table.  The
   symbol table is an abstract hash table type implemented in hash.c.  Each
   symbol is represented by `struct m4_symbol', which is stored in the hash
   table keyed by the symbol name.  As a special case, to facilitate the
   "pushdef" and "popdef" builtins, the value stored against each key us a
   stack of `struct m4_symbol'. All the value entries for a symbol name are
   simply ordered on the stack by age.  The most recently pushed definition
   will then always be the first found.  */

static size_t	m4_symtab_hash		(const void *key);
static int	m4_symtab_cmp		(const void *key, const void *try);
static void	m4_token_data_delete	(m4_symbol *data);
static void	m4_symbol_pop		(m4_symbol **psymbol);
static void	m4_symbol_del		(m4_symbol **psymbol);
static int	m4_symbol_destroy	(const char *name, m4_symbol *symbol,
					 void *data);


/* Pointer to symbol table.  */
m4_hash *m4_symtab = 0;



void
m4_symtab_init (void)
{
  m4_symtab = m4_hash_new (m4_symtab_hash, m4_symtab_cmp);
}

int
m4_symbol_destroy (const char *name, m4_symbol *symbol, void *data)
{
  m4_symbol_delete (name);
  return 0;
}

void
m4_symtab_exit (void)
{
  m4_symtab_apply (m4_symbol_destroy, NULL);
  m4_hash_delete (m4_symtab);
  m4_hash_exit ();
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
m4_token_data_delete (m4_symbol *data)
{
  assert (data);
  assert (M4_SYMBOL_NEXT (data) == 0);

  if (M4_SYMBOL_TYPE (data) == M4_TOKEN_TEXT)
    XFREE (M4_SYMBOL_TEXT (data));

  XFREE (data);
}


void
m4_symbol_pop (m4_symbol **psymbol)
{
  m4_symbol *stale;

  assert (psymbol);
  assert (*psymbol);

  stale	 = *psymbol;
  *psymbol = M4_SYMBOL_NEXT (stale);

#ifndef NDEBUG
  M4_SYMBOL_NEXT (stale) = 0;
#endif
  m4_token_data_delete (stale);
}


/* Free all storage associated with a symbol.  */
void
m4_symbol_del (m4_symbol **psymbol)
{
  assert (psymbol);
  assert (*psymbol);

  while (*psymbol)
    m4_symbol_pop (psymbol);
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
  m4_symbol *symbol         = XCALLOC (m4_symbol, 1);;
  M4_SYMBOL_TYPE (symbol)	= M4_TOKEN_VOID;

  if (psymbol)
    {
      M4_SYMBOL_NEXT (symbol) = *psymbol;
      *psymbol = symbol;
    }
  else
    m4_hash_insert (m4_symtab, xstrdup (name), symbol);

  return symbol;
}


/* Return a slot associated to NAME, creating it if needed.  */
m4_symbol *
m4_symbol_define (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);
  if (psymbol)
    return *psymbol;
  else
    return m4_symbol_pushdef (name);
}


/* Remove the topmost definition associated to NAME.  */
void
m4_symbol_popdef (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  assert (psymbol);

  if (M4_SYMBOL_NEXT (*psymbol))
    m4_symbol_pop (psymbol);
  else
    {
      xfree (m4_hash_remove (m4_symtab, name));
      m4_symbol_del (psymbol);
    }
}


/* Remove all the definitions associated with NAME.  */
void
m4_symbol_delete (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  assert (psymbol);

#ifdef DEBUG_SYM
  M4_DEBUG_MESSAGE1("symbol %s recycled.", name);
#endif

  xfree (m4_hash_remove (m4_symtab, name));
  m4_symbol_del (psymbol);
}


void
m4_symbol_builtin (m4_symbol *symbol, lt_dlhandle handle,
		   const m4_builtin *bp)
{
  assert (symbol);
  assert (handle);
  assert (bp);

  if (M4_SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    xfree (M4_SYMBOL_TEXT (symbol));

  M4_SYMBOL_HANDLE (symbol)		= handle;
  M4_SYMBOL_TYPE (symbol)		= M4_TOKEN_FUNC;
  M4_SYMBOL_MACRO_ARGS (symbol)		= bp->groks_macro_args;
  M4_SYMBOL_BLIND_NO_ARGS (symbol)	= bp->blind_if_no_args;
  M4_SYMBOL_FUNC (symbol)		= bp->func;
  /* Do not reset M4_SYMBOL_TRACED as it means that --trace would be
     usable only for existing macros.  m4_symbol_lookup takes care
     of its proper initialisation.  */
}

void
m4_symbol_macro (m4_symbol *symbol, lt_dlhandle handle, const char *text)
{
  assert (symbol);

  if (M4_SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    xfree (M4_SYMBOL_TEXT (symbol));

  M4_SYMBOL_HANDLE (symbol) 		= handle;
  M4_SYMBOL_TYPE (symbol) 		= M4_TOKEN_TEXT;
  M4_SYMBOL_MACRO_ARGS (symbol)		= FALSE;
  M4_SYMBOL_BLIND_NO_ARGS (symbol)	= FALSE;
  M4_SYMBOL_TEXT (symbol) 		= xstrdup (text);
  /* Do not reset M4_SYMBOL_TRACED as it means that --trace would be
     usable only for existing macros.  m4_symbol_lookup takes care
     of its proper initialisation.  */
}

/* Remove every symbol that references the given module handle from
   the symbol table.  */
void
m4_symtab_remove_module_references (lt_dlhandle handle)
{
  m4_hash_iterator *place = 0;

  assert (handle);

  /* Traverse each symbol name in the hash table.  */
  while ((place = m4_hash_iterator_next (m4_symtab, place)))
    {
      m4_symbol **psymbol = (m4_symbol **) m4_hash_iterator_value (place);
      m4_symbol *current  = *psymbol;

      /* Examine each value in the value stack associated with this
	 symbol name.  */
      while (M4_SYMBOL_NEXT (current))
	{
	  m4_symbol *next = M4_SYMBOL_NEXT (current);

	  if (M4_SYMBOL_HANDLE (next) == handle)
	    {
	      M4_SYMBOL_NEXT (current) = M4_SYMBOL_NEXT (next);
	      m4_token_data_delete (next);
	    }
	  else
	    current = next;
	}

      /* Purge live reference.
         This must be performed after the shadowed values have been weeded
         so that the symbol key can be removed from the hash table if this
         last value matches too.  */
      if (M4_SYMBOL_HANDLE (*psymbol) == handle)
	{
	  if (!M4_SYMBOL_NEXT (*psymbol))
	    xfree (m4_hash_remove (m4_symtab, m4_hash_iterator_key (place)));
	  m4_symbol_pop (psymbol);
	}
    }
}

/* The following function is used for the cases where we want to do
   something to each and every symbol in the table.  The function
   m4_symtab_apply () traverses the symbol table, and calls a specified
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
      const char *name	  = (const char *) m4_hash_iterator_key (place);
      m4_symbol **psymbol = (m4_symbol **) m4_hash_iterator_value (place);
      result = (*func) (name, *psymbol, data);

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
  m4_token_t t;
  m4_symbol tokenbuf;
  const char *text;
  m4_symbol *s;
  int delete;

  while ((t = m4_next_token (&tokenbuf)) != M4_TOKEN_EOF)
    {
      if (t != M4_TOKEN_WORD)
	continue;
      text = M4_SYMBOL_TEXT (&tokenbuf);
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
	(void) m4_symbol_define (text);
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

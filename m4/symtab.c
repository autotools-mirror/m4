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
   "pushdef" and "popdef" builtins, the value stored against each key is a
   stack of `struct m4_symbol'. All the value entries for a symbol name are
   simply ordered on the stack by age.  The most recently pushed definition
   will then always be the first found.

   Also worthy of mention is the way traced symbols are managed:  the trace
   bit is associated with a particular symbol name.  If a symbol is
   undefined and then redefined, it does not lose its trace bit (in GNU
   mode).  This is achieved by not removing traced symbol names from the
   symbol table, even if their value stack is empty.  That way, when the
   name is given a new value, it is pushed onto the empty stack, and the
   trace bit attached to the name was never lost.  There is a small amount
   of fluff in these functions to make sure that such symbols (with empty
   value stacks) are invisible to the users of this module.  */

static size_t	m4_symtab_hash		(const void *key);
static int	m4_symtab_cmp		(const void *key, const void *try);
static int	m4_symbol_destroy	(const char *name, m4_symbol *symbol,
					 void *data);

/* Pointer to symbol table.  */
m4_hash *m4_symtab = 0;




/* -- SYMBOL TABLE MANAGEMENT --

   These functions are used to manage a symbol table as a whole.  */

void
m4_symtab_init (void)
{
  m4_symtab = m4_hash_new (m4_symtab_hash, m4_symtab_cmp);
}

/* The following function is used for the cases where we want to do
   something to each and every symbol in the table.  This function
   traverses the symbol table, and calls a specified function FUNC for
   each symbol in the table.  FUNC is called with a pointer to the
   symbol, and the DATA argument.  */
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
      m4_symbol *symbol = (m4_symbol *) m4_hash_iterator_value (place);
      m4_token *data = SYMBOL_TOKEN (symbol);

      /* For symbols that have token data... */
      if (data)
	{
	  /* Purge any shadowed references.  */
	  while (TOKEN_NEXT (data))
	    {
	      m4_token *next = TOKEN_NEXT (data);

	      if (TOKEN_HANDLE (next) == handle)
		{
		  TOKEN_NEXT (data) = TOKEN_NEXT (next);

		  if (TOKEN_TYPE (next) == M4_TOKEN_TEXT)
		    XFREE (TOKEN_TEXT (next));
		  XFREE (next);
		}
	      else
		data = next;
	    }

	  /* Purge the live reference if necessary.  */
	  if (SYMBOL_HANDLE (symbol) == handle)
	    m4_symbol_popdef (m4_hash_iterator_key (place));
	}
    }
}

/* Release all of the memory used by the symbol table.  */
void
m4_symtab_exit (void)
{
  m4_symtab_apply (m4_symbol_destroy, NULL);
  m4_hash_delete  (m4_symtab);
  m4_hash_exit    ();
}

/* This callback is used exclusively by m4_symtab_exit(), to cleanup
   the memory used by the symbol table.  As such, the trace bit is reset
   on every symbol so that m4_symbol_popdef() doesn't try to preserve
   the table entry.  */
static int
m4_symbol_destroy (const char *name, m4_symbol *symbol, void *data)
{
  SYMBOL_TRACED (symbol) = FALSE;

  while (m4_hash_lookup (m4_symtab, name))
    m4_symbol_popdef (name);

  return 0;
}

/* Return a hash value for a string, from GNU Emacs.  */
static size_t
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

/* Comparison function for hash keys -- used by the underlying
   hash table ADT when searching for a key match during name lookup.  */
static int
m4_symtab_cmp (const void *key, const void *try)
{
  return (strcmp ((const char *) key, (const char *) try));
}




/* -- SYMBOL MANAGEMENT --

   The following functions manipulate individual symbols within
   an existing table.  */

/* Return the symbol associated to NAME, or else NULL.  */
m4_symbol *
m4_symbol_lookup (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  /* If just searching, return status of search -- if only an empty
     struct is returned, that is treated as a failed lookup.  */
  return (psymbol && SYMBOL_TOKEN (*psymbol)) ? *psymbol : 0;
}


/* Insert NAME into the symbol table.  If there is already a symbol
   associated with NAME, push the new value on top of the value stack
   for this symbol.  Otherwise create a new association.  */
m4_symbol *
m4_symbol_pushdef (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4_symtab, name);

  m4_symbol *symbol = 0;
  m4_token *value   = XCALLOC (m4_token, 1);

  if (psymbol)
    {
      symbol = *psymbol;
      TOKEN_NEXT (value) = SYMBOL_TOKEN (symbol);
    }
  else
    symbol = XCALLOC (m4_symbol, 1);

  SYMBOL_TOKEN (symbol)	= value;
  SYMBOL_TYPE (symbol)	= M4_TOKEN_VOID;

  if (!psymbol)
    m4_hash_insert (m4_symtab, xstrdup (name), symbol);

  return symbol;
}


/* Return the symbol associated with NAME in the symbol table, creating
   a new symbol if necessary.  */
m4_symbol *
m4_symbol_define (const char *name)
{
  m4_symbol *symbol = m4_symbol_lookup (name);

  if (symbol)
    return symbol;

  return m4_symbol_pushdef (name);
}


/* Pop the topmost value stack entry from the symbol associated with
   NAME, deleting it from the table entirely if that was the last
   remaining value in the stack.  */
void
m4_symbol_popdef (const char *name)
{
  m4_symbol **psymbol	= (m4_symbol **) m4_hash_lookup (m4_symtab, name);
  m4_token  *stale	= NULL;

  assert (psymbol);
  assert (*psymbol);

  stale = SYMBOL_TOKEN (*psymbol);

  if (stale)
    {
      SYMBOL_TOKEN (*psymbol) = TOKEN_NEXT (stale);

      if (TOKEN_TYPE (stale) == M4_TOKEN_TEXT)
	XFREE (TOKEN_TEXT (stale));
      XFREE (stale);
    }

  /* Only remove the hash table entry if the last value in the
     symbol value stack was successfully removed.  */
  if (!SYMBOL_TOKEN (*psymbol))
    if (no_gnu_extensions || !SYMBOL_TRACED (*psymbol))
      {
	XFREE (*psymbol);
	xfree (m4_hash_remove (m4_symtab, name));
      }
}


/* Pop all values from the symbol associated with NAME.  */
void
m4_symbol_delete (const char *name)
{
  while (m4_symbol_lookup (name))
    m4_symbol_popdef (name);
}



/* Set the type and value of a symbol according to the passed
   arguments.  This function is usually passed a newly pushdef()d symbol
   that is already interned in the symbol table.  The traced bit should
   be appropriately set by the caller.  */
void
m4_symbol_builtin (m4_symbol *symbol, lt_dlhandle handle,
		   m4_builtin_func *func, int flags,
		   int min_args, int max_args)
{
  assert (symbol);
  assert (handle);
  assert (func);

  if (SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    xfree (SYMBOL_TEXT (symbol));

  SYMBOL_TYPE (symbol)		= M4_TOKEN_FUNC;
  SYMBOL_FUNC (symbol)		= func;
  SYMBOL_HANDLE (symbol)	= handle;
  SYMBOL_FLAGS (symbol)		= flags;
  SYMBOL_MIN_ARGS (symbol)	= min_args;
  SYMBOL_MAX_ARGS (symbol)	= max_args;
}

/* ...and similarly for macro valued symbols.  */
void
m4_symbol_macro (m4_symbol *symbol, lt_dlhandle handle,
		 const char *text, int flags, int min_args, int max_args)
{
  assert (symbol);

  if (SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    xfree (SYMBOL_TEXT (symbol));

  SYMBOL_TYPE (symbol) 		= M4_TOKEN_TEXT;
  SYMBOL_TEXT (symbol) 		= xstrdup (text);
  SYMBOL_HANDLE (symbol) 	= handle;
  SYMBOL_FLAGS (symbol)		= flags;
  SYMBOL_MIN_ARGS (symbol)	= min_args;
  SYMBOL_MAX_ARGS (symbol)	= max_args;
}




#ifdef DEBUG_SYM

static int symtab_print_list (const char *name, m4_symbol *symbol, void *ignored);
static void symtab_dump (void);

static void
symtab_dump (void)
{
  m4_hash_iterator *place = 0;

  while ((place = m4_hash_iterator_next (m4_symtab, place)))
    {
      const char   *symbol_name	= (const char *) m4_hash_iterator_key (place);
      m4_symbol	   *symbol	= m4_hash_iterator_value (place);
      m4_token	   *token	= SYMBOL_TOKEN (symbol);
      int	    flags	= token ? SYMBOL_FLAGS (symbol) : 0;
      lt_dlhandle   handle	= token ? SYMBOL_HANDLE (symbol) : 0;
      const char   *module_name	= handle ? m4_module_name (handle) : "NONE";
      const m4_builtin *bp;

      fprintf (stderr, "%10s: (%d%s) %s=",
	       module_name, flags,
	       SYMBOL_TRACED (symbol) ? "!" : "", symbol_name);

      if (!token)
	fputs ("<!UNDEFINED!>", stderr);
      else
	switch (SYMBOL_TYPE (symbol))
	  {
	  case M4_TOKEN_TEXT:
	    fputs (SYMBOL_TEXT (symbol), stderr);
	    break;

	  case M4_TOKEN_FUNC:
	    bp = m4_builtin_find_by_func (m4_module_builtins (handle),
					SYMBOL_FUNC (symbol));
	    fprintf (stderr, "<%s>",
		     bp ? bp->name : "!ERROR!");
	    break;
	  case M4_TOKEN_VOID:
	    fputs ("<!VOID!>", stderr);
	    break;
	}
      fputc ('\n', stderr);
    }
}

static void
symtab_debug (void)
{
  m4_token_t t;
  m4_token token;
  const char *text;
  m4_symbol *s;
  int delete;

  while ((t = m4_next_token (&token)) != M4_TOKEN_EOF)
    {
      if (t != M4_TOKEN_WORD)
	continue;
      text = TOKEN_TEXT (&token);
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
  printf ("\tname %s, addr %#x, flags: %d %s\n",
	  name, (unsigned) symbol,
	  SYMBOL_FLAGS (symbol),
	  SYMBOL_TRACED (symbol) ? " traced" : "<none>");
  return 0;
}

#endif /* DEBUG_SYM */

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
   stack of `struct m4_token'. All the value entries for a symbol name are
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

#define M4_SYMTAB_DEFAULT_SIZE 2047

static int	symbol_destroy		(m4_hash *hash, const void *name,
					 void *symbol, void *ignored);
static int	arg_destroy		(m4_hash *hash, const void *name,
					 void *arg, void *ignored);

/* Pointer to symbol table.  */
m4_hash *m4__symtab = 0;




/* -- SYMBOL TABLE MANAGEMENT --

   These functions are used to manage a symbol table as a whole.  */

void
m4__symtab_init (void)
{
  m4__symtab = m4_hash_new (M4_SYMTAB_DEFAULT_SIZE,
			    m4_hash_string_hash, m4_hash_string_cmp);
}

/* Remove every symbol that references the given module handle from
   the symbol table.  */
void
m4__symtab_remove_module_references (lt_dlhandle handle)
{
  m4_hash_iterator *place = 0;

  assert (handle);

   /* Traverse each symbol name in the hash table.  */
  while ((place = m4_hash_iterator_next (m4__symtab, place)))
    {
      m4_symbol *symbol = (m4_symbol *) m4_hash_iterator_value (place);
      m4_token  *data   = SYMBOL_TOKEN (symbol);

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
m4__symtab_exit (void)
{
  m4_hash_apply  (m4__symtab, symbol_destroy, NULL);
  m4_hash_delete (m4__symtab);
  m4_hash_exit   ();
}

/* This callback is used exclusively by m4__symtab_exit(), to cleanup
   the memory used by the symbol table.  As such, the trace bit is reset
   on every symbol so that m4_symbol_popdef() doesn't try to preserve
   the table entry.  */
static int
symbol_destroy (m4_hash *hash, const void *name, void *symbol, void *ignored)
{
  char *key = xstrdup ((char *) name);

  SYMBOL_TRACED ((m4_symbol *) symbol) = FALSE;

  while (key && m4_hash_lookup (hash, key))
    m4_symbol_popdef (key);

  XFREE (key);

  return 0;
}



/* -- SYMBOL MANAGEMENT --

   The following functions manipulate individual symbols within
   an existing table.  */

/* Return the symbol associated to NAME, or else NULL.  */
m4_symbol *
m4_symbol_lookup (const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4__symtab, name);

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
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (m4__symtab, name);

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
    m4_hash_insert (m4__symtab, xstrdup (name), symbol);

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
  m4_symbol **psymbol	= (m4_symbol **) m4_hash_lookup (m4__symtab, name);
  m4_token  *stale	= NULL;

  assert (psymbol);
  assert (*psymbol);

  stale = SYMBOL_TOKEN (*psymbol);

  if (stale)
    {
      SYMBOL_TOKEN (*psymbol) = TOKEN_NEXT (stale);

      if (TOKEN_ARG_SIGNATURE (stale))
	{
	  m4_hash_apply (TOKEN_ARG_SIGNATURE (stale), arg_destroy, NULL);
	  m4_hash_delete (TOKEN_ARG_SIGNATURE (stale));
	}
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
	xfree (m4_hash_remove (m4__symtab, name));
      }
}


/* Callback used by m4_symbol_popdef () to release the memory used
   by values in the arg_signature hash.  */
static int
arg_destroy (m4_hash *hash, const void *name, void *arg, void *ignored)
{
  struct m4_token_arg *token_arg = (struct m4_token_arg *) arg;

  assert (name);
  assert (hash);

  if (TOKEN_ARG_DEFAULT (token_arg))
    XFREE (TOKEN_ARG_DEFAULT (token_arg));
  xfree (token_arg);
  xfree (m4_hash_remove (hash, (const char *) name));

  return 0;
}


/* Set the type and value of a symbol according to the passed
   arguments.  This function is usually passed a newly pushdef()d symbol
   that is already interned in the symbol table.  The traced bit should
   be appropriately set by the caller.  */
m4_symbol *
m4__symbol_set_builtin (m4_symbol *symbol, m4_token *token)
{
  assert (symbol);
  assert (token);
  assert (TOKEN_FUNC (token));
  assert (TOKEN_HANDLE (token));
  assert (TOKEN_TYPE (token) == M4_TOKEN_FUNC);

  if (SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    xfree (SYMBOL_TEXT (symbol));

  SYMBOL_TYPE (symbol)		= TOKEN_TYPE (token);
  SYMBOL_FUNC (symbol)		= TOKEN_FUNC (token);
  SYMBOL_HANDLE (symbol)	= TOKEN_HANDLE (token);
  SYMBOL_FLAGS (symbol)		= TOKEN_FLAGS (token);
  SYMBOL_ARG_SIGNATURE (symbol)	= TOKEN_ARG_SIGNATURE (token);
  SYMBOL_MIN_ARGS (symbol)	= TOKEN_MIN_ARGS (token);
  SYMBOL_MAX_ARGS (symbol)	= TOKEN_MAX_ARGS (token);

  return symbol;
}

/* ...and similarly for macro valued symbols.  */
m4_symbol *
m4__symbol_set_macro (m4_symbol *symbol, m4_token *token)
{
  assert (symbol);
  assert (TOKEN_TEXT (token));
  assert (TOKEN_TYPE (token) == M4_TOKEN_TEXT);

  if (SYMBOL_TYPE (symbol) == M4_TOKEN_TEXT)
    xfree (SYMBOL_TEXT (symbol));

  SYMBOL_TYPE (symbol)		= TOKEN_TYPE (token);
  SYMBOL_TEXT (symbol) 		= xstrdup (TOKEN_TEXT (token));
  SYMBOL_HANDLE (symbol) 	= TOKEN_HANDLE (token);
  SYMBOL_FLAGS (symbol)		= TOKEN_FLAGS (token);
  SYMBOL_ARG_SIGNATURE (symbol)	= TOKEN_ARG_SIGNATURE (token);
  SYMBOL_MIN_ARGS (symbol)	= TOKEN_MIN_ARGS (token);
  SYMBOL_MAX_ARGS (symbol)	= TOKEN_MAX_ARGS (token);

  return symbol;
}




#ifdef DEBUG_SYM

static int  symtab_print_list (const void *name, void *symbol, void *ignored);
static void symtab_debug      (void)
static void symtab_dump	      (void);

static void
symtab_dump (void)
{
  m4_hash_iterator *place = 0;

  while ((place = m4_hash_iterator_next (m4__symtab, place)))
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
  m4__token_type type;
  m4_token token;
  const char *text;
  m4_symbol *symbol;
  int delete;

  while ((type = m4_next_token (&token)) != M4_TOKEN_EOF)
    {
      if (type != M4_TOKEN_WORD)
	continue;
      text = TOKEN_TEXT (&token);
      if (*text == '_')
	{
	  delete = 1;
	  text++;
	}
      else
	delete = 0;

      symbol = m4_symbol_lookup (text);

      if (symbol == NULL)
	printf (_("Name `%s' is unknown\n"), text);

      if (delete)
	(void) m4_symbol_delete (text);
      else
	(void) m4_symbol_define (text);
    }
  m4_symtab_apply (symtab_print_list, NULL);
}

static int
symtab_print_list (const void *name, void *symbol, void *ignored)
{
  printf ("\tname %s, addr %#x, flags: %d %s\n",
	  (char *) name, (unsigned) symbol,
	  SYMBOL_FLAGS ((m4_symbol *) symbol),
	  SYMBOL_TRACED ((m4_symbol *) symbol) ? " traced" : "<none>");
  return 0;
}

#endif /* DEBUG_SYM */

/* Define these functions at the end, so that calls in the file use the
   faster macro version from m4private.h.  */
#undef m4_symtab_apply
int
m4_symtab_apply (m4_symtab_apply_func *func, void *userdata)
{
  return m4_hash_apply (m4__symtab, (m4_hash_apply_func *) func, userdata);
}

/* Pop all values from the symbol associated with NAME.  */
#undef m4_symbol_delete
void
m4_symbol_delete (const char *name)
{
  while (m4_symbol_lookup (name))
    m4_symbol_popdef (name);
}

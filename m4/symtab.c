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

#define M4_SYMTAB_DEFAULT_SIZE		2047
#define M4_ARG_SIGNATURE_DEFAULT_SIZE	7

static m4_symbol *symtab_fetch		(m4_symtab*, const char *);
static void	  symbol_popval		(m4_symbol *symbol);
static int	  symbol_destroy	(m4_hash *hash, const void *name,
					 void *symbol, void *ignored);
static int	  arg_destroy		(m4_hash *hash, const void *name,
					 void *arg, void *ignored);
static m4_hash *  arg_signature_parse	(const char *name, const char *param);



/* -- SYMBOL TABLE MANAGEMENT --

   These functions are used to manage a symbol table as a whole.  */

m4_symtab *
m4_symtab_create (size_t size)
{
  return (m4_symtab *) m4_hash_new (size ? size : M4_SYMTAB_DEFAULT_SIZE,
				    m4_hash_string_hash, m4_hash_string_cmp);
}

void
m4_symtab_delete (m4_symtab *symtab)
{
  assert (symtab);

  m4_hash_apply  ((m4_hash *) symtab, symbol_destroy, NULL);
  m4_hash_delete ((m4_hash *) symtab);
}

static m4_symbol *
symtab_fetch (m4_symtab *symtab, const char *name)
{
  m4_symbol **psymbol;
  m4_symbol *symbol;

  assert (symtab);
  assert (name);

  psymbol = (m4_symbol **) m4_hash_lookup ((m4_hash *) symtab, name);
  if (psymbol)
    {
      symbol = *psymbol;
    }
  else
    {
      symbol = XCALLOC (m4_symbol, 1);
      m4_hash_insert ((m4_hash *) symtab, xstrdup (name), symbol);
    }

  return symbol;
}

/* Remove every symbol that references the given module handle from
   the symbol table.  */
void
m4__symtab_remove_module_references (m4_symtab *symtab, lt_dlhandle handle)
{
  m4_hash_iterator *place = 0;

  assert (handle);

   /* Traverse each symbol name in the hash table.  */
  while ((place = m4_hash_iterator_next (symtab, place)))
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
	    m4_symbol_popdef (symtab, m4_hash_iterator_key (place));
	}
    }
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
    m4_symbol_popdef ((m4_symtab *) hash, key);

  XFREE (key);

  return 0;
}



/* -- SYMBOL MANAGEMENT --

   The following functions manipulate individual symbols within
   an existing table.  */

/* Return the symbol associated to NAME, or else NULL.  */
m4_symbol *
m4_symbol_lookup (m4_symtab *symtab, const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup ((m4_hash *) symtab,
						       name);

  /* If just searching, return status of search -- if only an empty
     struct is returned, that is treated as a failed lookup.  */
  return (psymbol && SYMBOL_TOKEN (*psymbol)) ? *psymbol : 0;
}


/* Insert NAME into the symbol table.  If there is already a symbol
   associated with NAME, push the new VALUE on top of the value stack
   for this symbol.  Otherwise create a new association.  */
m4_symbol *
m4_symbol_pushdef (m4_symtab *symtab, const char *name, m4_token *value)
{
  m4_symbol *symbol;

  assert (symtab);
  assert (name);
  assert (value);

  symbol 		= symtab_fetch (symtab, name);
  TOKEN_NEXT (value)	= SYMBOL_TOKEN (symbol);
  SYMBOL_TOKEN (symbol)	= value;

  assert (SYMBOL_TOKEN (symbol));

  return symbol;
}

/* Return the symbol associated with NAME in the symbol table, creating
   a new symbol if necessary.  In either case set the symbol's VALUE.  */
m4_symbol *
m4_symbol_define (m4_symtab *symtab, const char *name, m4_token *value)
{
  m4_symbol *symbol;

  assert (symtab);
  assert (name);
  assert (value);

  symbol = symtab_fetch (symtab, name);
  if (SYMBOL_TOKEN (symbol))
    symbol_popval (symbol);

  TOKEN_NEXT (value)	= SYMBOL_TOKEN (symbol);
  SYMBOL_TOKEN (symbol)	= value;

  assert (SYMBOL_TOKEN (symbol));

  return symbol;
}

void
m4_set_symbol_traced (m4_symtab *symtab, const char *name)
{
  m4_symbol *symbol;

  assert (symtab);
  assert (name);

  symbol = symtab_fetch (symtab, name);

  SYMBOL_TRACED (symbol) = TRUE;
}


/* Pop the topmost value stack entry from the symbol associated with
   NAME, deleting it from the table entirely if that was the last
   remaining value in the stack.  */
void
m4_symbol_popdef (m4_symtab *symtab, const char *name)
{
  m4_symbol **psymbol	= (m4_symbol **) m4_hash_lookup ((m4_hash *) symtab,
							 name);
  assert (psymbol);
  assert (*psymbol);

  symbol_popval (*psymbol);

  /* Only remove the hash table entry if the last value in the
     symbol value stack was successfully removed.  */
  if (!SYMBOL_TOKEN (*psymbol))
    if (no_gnu_extensions || !SYMBOL_TRACED (*psymbol))
      {
	XFREE (*psymbol);
	xfree (m4_hash_remove ((m4_hash *) symtab, name));
      }
}


static void
symbol_popval (m4_symbol *symbol)
{
  m4_token  *stale;

  assert (symbol);

  stale = SYMBOL_TOKEN (symbol);

  if (stale)
    {
      SYMBOL_TOKEN (symbol) = TOKEN_NEXT (stale);

      if (TOKEN_ARG_SIGNATURE (stale))
	{
	  m4_hash_apply (TOKEN_ARG_SIGNATURE (stale), arg_destroy, NULL);
	  m4_hash_delete (TOKEN_ARG_SIGNATURE (stale));
	}
      if (TOKEN_TYPE (stale) == M4_TOKEN_TEXT)
	XFREE (TOKEN_TEXT (stale));
      XFREE (stale);
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

#if 0
m4_symbol *
m4_symbol_push_token (m4_symtab *symtab, const char *name, m4_token *token)
{
  m4_token *next;

  assert (symtab);
  assert (name);
  assert (token);

  /* If it's a function, it must have come from a module.  */
  assert ((TOKEN_TYPE (token) != M4_TOKEN_FUNC) || TOKEN_HANDLE (token));

#if M4PARAMS
  const char *openp  = NULL;
  const char *params = NULL;

  if (!no_gnu_extensions)
    {
      /* If name contains an open paren, then parse before that paren
	 as the actual name, and the rest as a formal parameter list.  */
      size_t len = 0;
      for (openp = name; *openp && !M4_IS_OPEN (*openp); ++openp)
	++len;

      if (*openp)
	{
	  name   = xstrzdup (name, len);
	  params = 1+ openp;
	}
    }

  if (params)
    {
      /* Make a hash table to map formal parameter names to
	 argv offsets, and store that in the symbol's token.  */
      TOKEN_ARG_SIGNATURE (token) = arg_signature_parse (name, params);
    }
#endif

  SYMBOL_TOKEN (symbol) = token;

#if M4PARAMS
  /* If we split name on open paren, free the copied substring.  */
  if (params)
    xfree ((char *) name);
#endif

  return symbol;
}
#endif

void
m4_token_copy (m4_token *dest, m4_token *src)
{
  m4_token *next;

  assert (dest);
  assert (src);

  if (TOKEN_TYPE (dest) == M4_TOKEN_TEXT)
    xfree (TOKEN_TEXT (dest));

#if M4PARAMS
  if (TOKEN_ARG_SIGNATURE (dest))
    {
      m4_hash_apply (TOKEN_ARG_SIGNATURE (dest), arg_destroy, NULL);
      m4_hash_delete (TOKEN_ARG_SIGNATURE (dest));
    }
#endif

  /* Copy the token contents over, being careful to preserve
     the next pointer.  */
  next = TOKEN_NEXT (dest);
  bcopy (src, dest, sizeof (m4_token));
  TOKEN_NEXT (dest) = next;

  /* Caller is supposed to free text token strings, so we have to
     copy the string not just its address in that case.  */
  if (TOKEN_TYPE (src) == M4_TOKEN_TEXT)
    TOKEN_TEXT (dest) = xstrdup (TOKEN_TEXT (src));

#if M4PARAMS
  if (TOKEN_ARG_SIGNATURE (src))
    TOKEN_ARG_SIGNATURE (dest) = m4_hash_dup (TOKEN_ARG_SIGNATURE (token));
#endif
}

#if M4PARAMS
static m4_hash *
arg_signature_parse (const char *name, const char *params)
{
  m4_hash *arg_signature;
  const char *commap;
  int offset;

  assert (params);

  arg_signature = m4_hash_new (M4_ARG_SIGNATURE_DEFAULT_SIZE,
			m4_hash_string_hash, m4_hash_string_cmp);

  for (offset = 1; *params && !M4_IS_CLOSE (*params); ++offset)
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

	  TOKEN_ARG_INDEX (arg) = offset;

	  m4_hash_insert (arg_signature, xstrzdup (params, len), arg);

	  params = commap;
	}
    }

  if (!M4_IS_CLOSE (*commap))
    M4WARN ((warning_status, 0,
	     _("Warning: %s: unterminated parameter list"), name));

  return arg_signature;
}
#endif



#ifdef DEBUG_SYM

static int  symtab_print_list (m4_hash *hash, const void *name, void *symbol,
			       void *ignored);
static void symtab_debug      (m4_symtab *symtab);
static void symtab_dump	      (m4_symtab *symtab);

static void
symtab_dump (m4_symtab *symtab)
{
  m4_hash_iterator *place = 0;

  while ((place = m4_hash_iterator_next ((m4_hash *) symtab, place)))
    {
      const char   *symbol_name	= (const char *) m4_hash_iterator_key (place);
      m4_symbol	   *symbol	= m4_hash_iterator_value (place);
      m4_token	   *token	= SYMBOL_TOKEN (symbol);
      int	    flags	= token ? SYMBOL_FLAGS (symbol) : 0;
      lt_dlhandle   handle	= token ? SYMBOL_HANDLE (symbol) : 0;
      const char   *module_name	= handle ? m4_get_module_name (handle) : "NONE";
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
	    bp = m4_builtin_find_by_func (m4_get_module_builtin_table (handle),
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
symtab_debug (m4_symtab *symtab)
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

      symbol = m4_symbol_lookup (symtab, text);

      if (symbol == NULL)
	printf (_("Name `%s' is unknown\n"), text);

      if (delete)
	m4_symbol_delete (symtab, text);
      else
	symtab_fetch (symtab, text);
    }
  m4_symtab_apply (symtab, symtab_print_list, NULL);
}

static int
symtab_print_list (m4_hash *hash, const void *name, void *symbol,
		   void *ignored)
{
  printf ("\tname %s, addr %#x, flags: %d %s\n",
	  (char *) name, (unsigned) symbol,
	  SYMBOL_FLAGS ((m4_symbol *) symbol),
	  SYMBOL_TRACED ((m4_symbol *) symbol) ? " traced" : "<none>");
  return 0;
}

#endif /* DEBUG_SYM */

/* Define these functions at the end, so that calls in the file use the
   faster macro version from m4module.h.  */
#undef m4_symtab_apply
int
m4_symtab_apply (m4_symtab *symtab, m4_symtab_apply_func *func, void *userdata)
{
  return m4_hash_apply ((m4_hash *) symtab, (m4_hash_apply_func *) func,
			userdata);
}

/* Pop all values from the symbol associated with NAME.  */
#undef m4_symbol_delete
void
m4_symbol_delete (m4_symtab *symtab, const char *name)
{
  while (m4_symbol_lookup (symtab, name))
    m4_symbol_popdef (symtab, name);
}

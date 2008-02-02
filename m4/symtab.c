/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2001, 2005, 2006,
   2007, 2008 Free Software Foundation, Inc.

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

#include <config.h>

#include "m4private.h"

/* Define this to see runtime debug info.  Implied by DEBUG.  */
/*#define DEBUG_SYM */

/* This file handles all the low level work around the symbol table.  The
   symbol table is an abstract hash table type implemented in hash.c.  Each
   symbol is represented by `struct m4_symbol', which is stored in the hash
   table keyed by the symbol name.  As a special case, to facilitate the
   "pushdef" and "popdef" builtins, the value stored against each key is a
   stack of `m4_symbol_value'. All the value entries for a symbol name are
   simply ordered on the stack by age.  The most recently pushed definition
   will then always be the first found.

   Also worthy of mention is the way traced symbols are managed: the
   trace bit is associated with a particular symbol name.  If a symbol
   is undefined and then redefined, it does not lose its trace bit.
   This is achieved by not removing traced symbol names from the
   symbol table, even if their value stack is empty.  That way, when
   the name is given a new value, it is pushed onto the empty stack,
   and the trace bit attached to the name was never lost.  There is a
   small amount of fluff in these functions to make sure that such
   symbols (with empty value stacks) are invisible to the users of
   this module.  */

#define M4_SYMTAB_DEFAULT_SIZE		2047

struct m4_symbol_table {
  m4_hash *table;
};

static m4_symbol *symtab_fetch		(m4_symbol_table*, const char *);
static void	  symbol_popval		(m4_symbol *symbol);
static void *	  symbol_destroy_CB	(m4_symbol_table *symtab,
					 const char *name,
					 m4_symbol *symbol, void *ignored);
static void *	  arg_destroy_CB	(m4_hash *hash, const void *name,
					 void *arg, void *ignored);
static void *	  arg_copy_CB		(m4_hash *src, const void *name,
					 void *arg, m4_hash *dest);


/* -- SYMBOL TABLE MANAGEMENT --

   These functions are used to manage a symbol table as a whole.  */

m4_symbol_table *
m4_symtab_create (size_t size)
{
  m4_symbol_table *symtab = xmalloc (sizeof *symtab);

  symtab->table = m4_hash_new (size ? size : M4_SYMTAB_DEFAULT_SIZE,
			       m4_hash_string_hash, m4_hash_string_cmp);
  return symtab;
}

void
m4_symtab_delete (m4_symbol_table *symtab)
{
  assert (symtab);
  assert (symtab->table);

  m4_symtab_apply (symtab, true, symbol_destroy_CB, NULL);
  m4_hash_delete (symtab->table);
  free (symtab);
}

/* For every symbol in SYMTAB, execute the callback FUNC with the name
   and value of the symbol being visited, and the opaque parameter
   USERDATA.  Skip undefined symbols that are placeholders for
   traceon, unless INCLUDE_TRACE is true.  If FUNC returns non-NULL,
   abort the iteration and return the same result; otherwise return
   NULL when iteration completes.  */
void *
m4_symtab_apply (m4_symbol_table *symtab, bool include_trace,
		 m4_symtab_apply_func *func, void *userdata)
{
  m4_hash_iterator *place  = NULL;
  void *	    result = NULL;

  assert (symtab);
  assert (symtab->table);
  assert (func);

  while ((place = m4_get_hash_iterator_next (symtab->table, place)))
    {
      m4_symbol *symbol = m4_get_hash_iterator_value (place);
      if (symbol->value || include_trace)
	result = func (symtab, (const char *) m4_get_hash_iterator_key (place),
		       symbol, userdata);

      if (result != NULL)
	{
	  m4_free_hash_iterator (symtab->table, place);
	  break;
	}
    }

  return result;
}

/* Ensure that NAME exists in the table, creating an entry if needed.  */
static m4_symbol *
symtab_fetch (m4_symbol_table *symtab, const char *name)
{
  m4_symbol **psymbol;
  m4_symbol *symbol;

  assert (symtab);
  assert (name);

  psymbol = (m4_symbol **) m4_hash_lookup (symtab->table, name);
  if (psymbol)
    {
      symbol = *psymbol;
    }
  else
    {
      symbol = xzalloc (sizeof *symbol);
      m4_hash_insert (symtab->table, xstrdup (name), symbol);
    }

  return symbol;
}

/* Remove every symbol that references the given module from
   the symbol table.  */
void
m4__symtab_remove_module_references (m4_symbol_table *symtab,
				     m4_module *module)
{
  m4_hash_iterator *place = 0;

  assert (module);

   /* Traverse each symbol name in the hash table.  */
  while ((place = m4_get_hash_iterator_next (symtab->table, place)))
    {
      m4_symbol *symbol = (m4_symbol *) m4_get_hash_iterator_value (place);
      m4_symbol_value *data = m4_get_symbol_value (symbol);

      /* For symbols that have token data... */
      if (data)
	{
	  /* Purge any shadowed references.  */
	  while (VALUE_NEXT (data))
	    {
	      m4_symbol_value *next = VALUE_NEXT (data);

	      if (VALUE_MODULE (next) == module)
		{
		  VALUE_NEXT (data) = VALUE_NEXT (next);

		  assert (next->type != M4_SYMBOL_PLACEHOLDER);
		  m4_symbol_value_delete (next);
		}
	      else
		data = next;
	    }

	  /* Purge the live reference if necessary.  */
	  if (SYMBOL_MODULE (symbol) == module)
	    m4_symbol_popdef (symtab, m4_get_hash_iterator_key (place));
	}
    }
}


/* This callback is used exclusively by m4_symtab_delete(), to cleanup
   the memory used by the symbol table.  As such, the trace bit is reset
   on every symbol so that m4_symbol_popdef() doesn't try to preserve
   the table entry.  */
static void *
symbol_destroy_CB (m4_symbol_table *symtab, const char *name,
		   m4_symbol *symbol, void *ignored)
{
  char *key = xstrdup ((char *) name);

  symbol->traced = false;

  while (key && m4_hash_lookup (symtab->table, key))
    m4_symbol_popdef (symtab, key);

  free (key);

  return NULL;
}



/* -- SYMBOL MANAGEMENT --

   The following functions manipulate individual symbols within
   an existing table.  */

/* Return the symbol associated to NAME, or else NULL.  */
m4_symbol *
m4_symbol_lookup (m4_symbol_table *symtab, const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (symtab->table, name);

  /* If just searching, return status of search -- if only an empty
     struct is returned, that is treated as a failed lookup.  */
  return (psymbol && m4_get_symbol_value (*psymbol)) ? *psymbol : NULL;
}


/* Insert NAME into the symbol table.  If there is already a symbol
   associated with NAME, push the new VALUE on top of the value stack
   for this symbol.  Otherwise create a new association.  */
m4_symbol *
m4_symbol_pushdef (m4_symbol_table *symtab, const char *name,
		   m4_symbol_value *value)
{
  m4_symbol *symbol;

  assert (symtab);
  assert (name);
  assert (value);

  symbol		= symtab_fetch (symtab, name);
  VALUE_NEXT (value)	= m4_get_symbol_value (symbol);
  symbol->value		= value;

  assert (m4_get_symbol_value (symbol));

  return symbol;
}

/* Return the symbol associated with NAME in the symbol table, creating
   a new symbol if necessary.  In either case set the symbol's VALUE.  */
m4_symbol *
m4_symbol_define (m4_symbol_table *symtab,
		  const char *name, m4_symbol_value *value)
{
  m4_symbol *symbol;

  assert (symtab);
  assert (name);
  assert (value);

  symbol = symtab_fetch (symtab, name);
  if (m4_get_symbol_value (symbol))
    symbol_popval (symbol);

  VALUE_NEXT (value) = m4_get_symbol_value (symbol);
  symbol->value      = value;

  assert (m4_get_symbol_value (symbol));

  return symbol;
}

/* Pop the topmost value stack entry from the symbol associated with
   NAME, deleting it from the table entirely if that was the last
   remaining value in the stack.  */
void
m4_symbol_popdef (m4_symbol_table *symtab, const char *name)
{
  m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (symtab->table, name);

  assert (psymbol);
  assert (*psymbol);

  symbol_popval (*psymbol);

  /* Only remove the hash table entry if the last value in the
     symbol value stack was successfully removed.  */
  if (!m4_get_symbol_value (*psymbol) && !m4_get_symbol_traced (*psymbol))
    {
      DELETE (*psymbol);
      free (m4_hash_remove (symtab->table, name));
    }
}

/* Remove the top-most value from SYMBOL's stack.  */
static void
symbol_popval (m4_symbol *symbol)
{
  m4_symbol_value  *stale;

  assert (symbol);

  stale = m4_get_symbol_value (symbol);

  if (stale)
    {
      symbol->value = VALUE_NEXT (stale);
      m4_symbol_value_delete (stale);
    }
}

/* Remove VALUE from the symbol table, and mark it as deleted.  If no
   expansions are pending, reclaim its resources.  */
void
m4_symbol_value_delete (m4_symbol_value *value)
{
  if (VALUE_PENDING (value) > 0)
    BIT_SET (VALUE_FLAGS (value), VALUE_DELETED_BIT);
  else
    {
      if (VALUE_ARG_SIGNATURE (value))
	{
	  m4_hash_apply (VALUE_ARG_SIGNATURE (value), arg_destroy_CB, NULL);
	  m4_hash_delete (VALUE_ARG_SIGNATURE (value));
	}
      switch (value->type)
	{
	case M4_SYMBOL_TEXT:
	  free ((char *) m4_get_symbol_value_text (value));
	  break;
	case M4_SYMBOL_PLACEHOLDER:
	  free ((char *) m4_get_symbol_value_placeholder (value));
	  break;
	case M4_SYMBOL_VOID:
	case M4_SYMBOL_FUNC:
	  break;
	default:
	  assert (!"m4_symbol_value_delete");
	  abort ();
	}
      free (value);
    }
}

m4_symbol *
m4_symbol_rename (m4_symbol_table *symtab, const char *name,
		  const char *newname)
{
  m4_symbol *symbol	= NULL;
  m4_symbol **psymbol;

  assert (symtab);
  assert (name);
  assert (newname);

  /* Use a low level hash fetch, so we can save the symbol value when
     removing the symbol name from the symbol table.  */
  psymbol = (m4_symbol **) m4_hash_lookup (symtab->table, name);

  if (psymbol)
    {
      symbol = *psymbol;

      /* Remove the old name from the symbol table.  */
      free (m4_hash_remove (symtab->table, name));
      assert (!m4_hash_lookup (symtab->table, name));

      m4_hash_insert (symtab->table, xstrdup (newname), *psymbol);
    }
  /* else
       NAME does not name a symbol in symtab->table!  */

  return symbol;
}


/* Callback used by m4_symbol_popdef () to release the memory used
   by values in the arg_signature hash.  */
static void *
arg_destroy_CB (m4_hash *hash, const void *name, void *arg, void *ignored)
{
  struct m4_symbol_arg *token_arg = (struct m4_symbol_arg *) arg;

  assert (name);
  assert (hash);

  if (SYMBOL_ARG_DEFAULT (token_arg))
    DELETE (SYMBOL_ARG_DEFAULT (token_arg));
  free (token_arg);
  free (m4_hash_remove (hash, (const char *) name));

  return NULL;
}

void
m4_symbol_value_copy (m4_symbol_value *dest, m4_symbol_value *src)
{
  m4_symbol_value *next;

  assert (dest);
  assert (src);

  switch (dest->type)
    {
    case M4_SYMBOL_TEXT:
      free ((char *) m4_get_symbol_value_text (dest));
      break;
    case M4_SYMBOL_PLACEHOLDER:
      free ((char *) m4_get_symbol_value_placeholder (dest));
      break;
    case M4_SYMBOL_VOID:
    case M4_SYMBOL_FUNC:
      break;
    default:
      assert (!"m4_symbol_value_delete");
      abort ();
    }

  if (VALUE_ARG_SIGNATURE (dest))
    {
      m4_hash_apply (VALUE_ARG_SIGNATURE (dest), arg_destroy_CB, NULL);
      m4_hash_delete (VALUE_ARG_SIGNATURE (dest));
    }

  /* Copy the value contents over, being careful to preserve
     the next pointer.  */
  next = VALUE_NEXT (dest);
  memcpy (dest, src, sizeof (m4_symbol_value));
  VALUE_NEXT (dest) = next;

  /* Caller is supposed to free text token strings, so we have to
     copy the string not just its address in that case.  */
  switch (src->type)
    {
    case M4_SYMBOL_TEXT:
      {
	size_t len = m4_get_symbol_value_len (src);
	unsigned int age = m4_get_symbol_value_quote_age (src);
	m4_set_symbol_value_text (dest,
				  xmemdup (m4_get_symbol_value_text (src),
					   len + 1), len, age);
      }
      break;
    case M4_SYMBOL_FUNC:
      /* Nothing further to do.  */
      break;
    case M4_SYMBOL_PLACEHOLDER:
      m4_set_symbol_value_placeholder (dest,
				       xstrdup (m4_get_symbol_value_placeholder
						(src)));
      break;
    case M4_SYMBOL_COMP:
      {
	m4__symbol_chain *chain = src->u.u_c.chain;
	size_t len = 0;
	char *str;
	char *p;
	while (chain)
	  {
	    /* TODO for now, only text links are supported.  */
	    assert (chain->type == M4__CHAIN_STR);
	    len += chain->u.u_s.len;
	    chain = chain->next;
	  }
	p = str = xcharalloc (len + 1);
	chain = src->u.u_c.chain;
	while (chain)
	  {
	    memcpy (p, chain->u.u_s.str, chain->u.u_s.len);
	    p += chain->u.u_s.len;
	    chain = chain->next;
	  }
	*p = '\0';
	m4_set_symbol_value_text (dest, str, len, 0);
      }
      break;
    default:
      assert (!"m4_symbol_value_copy");
      abort ();
    }
  if (VALUE_ARG_SIGNATURE (src))
    VALUE_ARG_SIGNATURE (dest) = m4_hash_dup (VALUE_ARG_SIGNATURE (src),
					      arg_copy_CB);
}

static void *
arg_copy_CB (m4_hash *src, const void *name, void *arg, m4_hash *dest)
{
  m4_hash_insert ((m4_hash *) dest, name, arg);
  return NULL;
}

/* Set the tracing status of the symbol NAME to TRACED.  This takes a
   name, rather than a symbol, since we hide macros that are traced
   but otherwise undefined from normal lookups, but still can affect
   their tracing status.  Return true iff the macro was previously
   traced.  */
bool
m4_set_symbol_name_traced (m4_symbol_table *symtab, const char *name,
			   bool traced)
{
  m4_symbol *symbol;
  bool result;

  assert (symtab);
  assert (name);

  if (traced)
    symbol = symtab_fetch (symtab, name);
  else
    {
      m4_symbol **psymbol = (m4_symbol **) m4_hash_lookup (symtab->table,
							   name);
      if (!psymbol)
	return false;
      symbol = *psymbol;
    }

  result = symbol->traced;
  symbol->traced = traced;
  if (!traced && !m4_get_symbol_value (symbol))
    {
      /* Free an undefined entry once it is no longer traced.  */
      assert (result);
      free (symbol);
      free (m4_hash_remove (symtab->table, name));
    }

  return result;
}

/* Grow OBS with a text representation of VALUE.  If QUOTES, then use
   it to surround a text definition.  If MAXLEN, then truncate text
   definitions to *MAXLEN, and adjust by how many characters are
   printed.  If MODULE, then include which module defined a builtin.
   Return true if the output was truncated.  QUOTES and MODULE do not
   count against the truncation length.  */
bool
m4_symbol_value_print (m4_symbol_value *value, m4_obstack *obs,
		       const m4_string_pair *quotes, size_t *maxlen,
		       bool module)
{
  const char *text;
  const m4_builtin *bp;
  m4__symbol_chain *chain;
  size_t len = maxlen ? *maxlen : SIZE_MAX;
  bool result = false;

  switch (value->type)
    {
    case M4_SYMBOL_TEXT:
      if (m4_shipout_string_trunc (obs, m4_get_symbol_value_text (value),
				   m4_get_symbol_value_len (value), quotes,
				   &len))
	result = true;
      break;
    case M4_SYMBOL_FUNC:
      bp = m4_get_symbol_value_builtin (value);
      obstack_1grow (obs, '<');
      obstack_grow (obs, bp->name, strlen (bp->name));
      obstack_1grow (obs, '>');
      break;
    case M4_SYMBOL_PLACEHOLDER:
      text = m4_get_symbol_value_placeholder (value);
      obstack_1grow (obs, '<');
      obstack_1grow (obs, '<');
      obstack_grow (obs, text, strlen (text));
      obstack_1grow (obs, '>');
      obstack_1grow (obs, '>');
      break;
    case M4_SYMBOL_COMP:
      chain = value->u.u_c.chain;
      if (quotes)
	obstack_grow (obs, quotes->str1, quotes->len1);
      while (chain && !result)
	{
	  switch (chain->type)
	    {
	    case M4__CHAIN_STR:
	      if (m4_shipout_string_trunc (obs, chain->u.u_s.str,
					   chain->u.u_s.len, NULL, &len))
		result = true;
	      break;
	    case M4__CHAIN_ARGV:
	      if (m4_arg_print (obs, chain->u.u_a.argv, chain->u.u_a.index,
				NULL, &len, module))
		result = true;
	      break;
	    default:
	      assert (!"m4_symbol_value_print");
	      abort ();
	    }
	    chain = chain->next;
	  }
      if (quotes)
	obstack_grow (obs, quotes->str2, quotes->len2);
      assert (!module);
      break;
    default:
      assert (!"m4_symbol_value_print");
      abort ();
    }

  if (module && VALUE_MODULE (value))
    {
      obstack_1grow (obs, '{');
      text = m4_get_module_name (VALUE_MODULE (value));
      obstack_grow (obs, text, strlen (text));
      obstack_1grow (obs, '}');
    }
  if (maxlen)
    *maxlen = len;
  return result;
}

/* Grow OBS with a text representation of SYMBOL.  If QUOTES, then use
   it to surround each text definition.  If STACK, then append all
   pushdef'd values, rather than just the top.  If ARG_LENGTH is less
   than SIZE_MAX, then truncate text definitions to that length.  If
   MODULE, then include which module defined a builtin.  QUOTES and
   MODULE do not count toward truncation.  */
void
m4_symbol_print (m4_symbol *symbol, m4_obstack *obs,
		 const m4_string_pair *quotes, bool stack, size_t arg_length,
		 bool module)
{
  m4_symbol_value *value;
  size_t len = arg_length;

  assert (symbol);
  assert (obs);

  value = m4_get_symbol_value (symbol);
  m4_symbol_value_print (value, obs, quotes, &len, module);
  if (stack)
    {
      value = VALUE_NEXT (value);
      while (value)
	{
	  obstack_1grow (obs, ',');
	  obstack_1grow (obs, ' ');
	  len = arg_length;
	  m4_symbol_value_print (value, obs, quotes, &len, module);
	  value = VALUE_NEXT (value);
	}
    }
}


/* Define these functions at the end, so that calls in the file use the
   faster macro version from m4module.h.  */

/* Pop all values from the symbol associated with NAME.  */
#undef m4_symbol_delete
void
m4_symbol_delete (m4_symbol_table *symtab, const char *name)
{
  while (m4_symbol_lookup (symtab, name))
    m4_symbol_popdef (symtab, name);
}

#undef m4_get_symbol_traced
bool
m4_get_symbol_traced (m4_symbol *symbol)
{
  assert (symbol);
  return symbol->traced;
}

#undef m4_symbol_value_create
m4_symbol_value *
m4_symbol_value_create (void)
{
  return xzalloc (sizeof (m4_symbol_value));
}

#undef m4_symbol_value_groks_macro
bool
m4_symbol_value_groks_macro (m4_symbol_value *value)
{
  assert (value);
  return BIT_TEST (value->flags, VALUE_MACRO_ARGS_BIT);
}

#undef m4_get_symbol_value
m4_symbol_value *
m4_get_symbol_value (m4_symbol *symbol)
{
  assert (symbol);
  return symbol->value;
}

#undef m4_is_symbol_value_text
bool
m4_is_symbol_value_text (m4_symbol_value *value)
{
  assert (value);
  return (value->type == M4_SYMBOL_TEXT);
}

#undef m4_is_symbol_value_func
bool
m4_is_symbol_value_func (m4_symbol_value *value)
{
  assert (value);
  return (value->type == M4_SYMBOL_FUNC);
}

#undef m4_is_symbol_value_placeholder
bool
m4_is_symbol_value_placeholder (m4_symbol_value *value)
{
  assert (value);
  return (value->type == M4_SYMBOL_PLACEHOLDER);
}

#undef m4_is_symbol_value_void
bool
m4_is_symbol_value_void (m4_symbol_value *value)
{
  assert (value);
  return (value->type == M4_SYMBOL_VOID);
}

#undef m4_get_symbol_value_text
const char *
m4_get_symbol_value_text (m4_symbol_value *value)
{
  assert (value && value->type == M4_SYMBOL_TEXT);
  return value->u.u_t.text;
}

#undef m4_get_symbol_value_len
size_t
m4_get_symbol_value_len (m4_symbol_value *value)
{
  assert (value && value->type == M4_SYMBOL_TEXT);
  return value->u.u_t.len;
}

#undef m4_get_symbol_value_quote_age
unsigned int
m4_get_symbol_value_quote_age (m4_symbol_value *value)
{
  assert (value && value->type == M4_SYMBOL_TEXT);
  return value->u.u_t.quote_age;
}

#undef m4_get_symbol_value_func
m4_builtin_func *
m4_get_symbol_value_func (m4_symbol_value *value)
{
  assert (value && value->type == M4_SYMBOL_FUNC);
  return value->u.builtin->func;
}

#undef m4_get_symbol_value_builtin
const m4_builtin *
m4_get_symbol_value_builtin (m4_symbol_value *value)
{
  assert (value && value->type == M4_SYMBOL_FUNC);
  return value->u.builtin;
}

#undef m4_get_symbol_value_placeholder
const char *
m4_get_symbol_value_placeholder (m4_symbol_value *value)
{
  assert (value && value->type == M4_SYMBOL_PLACEHOLDER);
  return value->u.u_t.text;
}

#undef m4_set_symbol_value_text
void
m4_set_symbol_value_text (m4_symbol_value *value, const char *text, size_t len,
			  unsigned int quote_age)
{
  assert (value && text);
  /* In practice, it is easier to debug when we guarantee a
     terminating NUL, even when there are embedded NULs.  */
  assert (!text[len]);

  value->type = M4_SYMBOL_TEXT;
  value->u.u_t.text = text;
  value->u.u_t.len = len;
  value->u.u_t.quote_age = quote_age;
}

#undef m4_set_symbol_value_builtin
void
m4_set_symbol_value_builtin (m4_symbol_value *value, const m4_builtin *builtin)
{
  assert (value);
  assert (builtin);

  value->type   = M4_SYMBOL_FUNC;
  value->u.builtin = builtin;
}

#undef m4_set_symbol_value_placeholder
void
m4_set_symbol_value_placeholder (m4_symbol_value *value, const char *text)
{
  assert (value);
  assert (text);

  value->type = M4_SYMBOL_PLACEHOLDER;
  value->u.u_t.text = text;
  value->u.u_t.len = SIZE_MAX; /* len is not tracked for placeholders.  */
}


#ifdef DEBUG_SYM

static void *dump_symbol_CB	(m4_symbol_table *symtab, const char *name,
				 m4_symbol *symbol, void *userdata);
static M4_GNUC_UNUSED void *
symtab_dump (m4_symbol_table *symtab)
{
  return m4_symtab_apply (symtab, true, dump_symbol_CB, NULL);
}

static void *
dump_symbol_CB (m4_symbol_table *symtab, const char *name,
		m4_symbol *symbol, void *ignored)
{
  m4_symbol_value *value	= m4_get_symbol_value (symbol);
  int		   flags	= value ? SYMBOL_FLAGS (symbol) : 0;
  m4_module *      module	= value ? SYMBOL_MODULE (symbol) : NULL;
  const char *     module_name	= module ? m4_get_module_name (module) : "NONE";

  xfprintf (stderr, "%10s: (%d%s) %s=", module_name, flags,
	    m4_get_symbol_traced (symbol) ? "!" : "", name);

  if (!value)
    fputs ("<!UNDEFINED!>", stderr);
  else if (m4_is_symbol_value_void (value))
    fputs ("<!VOID!>", stderr);
  else
    {
      m4_obstack obs;
      obstack_init (&obs);
      m4_symbol_value_print (value, &obs, false, NULL, NULL, SIZE_MAX, true);
      xfprintf (stderr, "%s", (char *) obstack_finish (&obs));
      obstack_free (&obs, NULL);
    }
  fputc ('\n', stderr);
  return NULL;
}
#endif /* DEBUG_SYM */

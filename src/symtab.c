/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2003, 2006, 2007,
   2008, 2009 Free Software Foundation, Inc.

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

#include "m4.h"

#include "hash.h"

#ifdef DEBUG_SYM
/* When evaluating hash table performance, this profiling code shows
   how many collisions were encountered.  */

struct profile
{
  int entry; /* Number of times lookup_symbol called with this mode.  */
  int comparisons; /* Number of times strcmp was called.  */
  int misses; /* Number of times strcmp did not return 0.  */
  long long bytes; /* Number of bytes compared.  */
};

static struct profile profiles[5];
static symbol_lookup current_mode;
static unsigned long long hash_entry;
static unsigned long long comparator_entry;
static size_t current_size;
static unsigned int resizes;

/* On exit, show a profile of symbol table performance.  */
static void
show_profile (void)
{
  int i;
  FILE *f = fopen ("/dev/tty", "w");
  for (i = 0; i < 5; i++)
    {
      xfprintf(f, "m4: lookup mode %d called %d times, %d compares, "
	       "%d misses, %lld bytes\n",
	       i, profiles[i].entry, profiles[i].comparisons,
	       profiles[i].misses, profiles[i].bytes);
    }
  xfprintf(f, "m4: %llu hash callbacks, %llu compare callbacks, "
	   "%zu buckets, %u resizes\n",
	   hash_entry, comparator_entry, current_size, resizes - 1);
  fclose (f);
}

/* Like memcmp (S1, S2, L), but also track profiling statistics.  */
static int
profile_memcmp (const char *s1, const char *s2, size_t l)
{
  int i = 0;
  int result;
  while (l && *s1 == *s2)
    {
      s1++;
      s2++;
      i++;
      l--;
    }
  result = l ? (unsigned char) *s1 - (unsigned char) *s2 : 0;
  profiles[current_mode].comparisons++;
  if (result != 0)
    profiles[current_mode].misses++;
  profiles[current_mode].bytes += i;
  return result;
}

# define memcmp profile_memcmp
#endif /* DEBUG_SYM */


/* Pointer to symbol table.  */
static Hash_table *symtab;

/*--------------------------------------------------.
| Return a hashvalue for a string S of length LEN.  |
`--------------------------------------------------*/
static size_t
hash (const char *s, size_t len)
{
  size_t val = len;

  /* This algorithm was originally borrowed from GNU Emacs, but has
     been modified to allow embedded NUL.  */
  while (len--)
    val = (val << 7) + (val >> (sizeof val * CHAR_BIT - 7)) + to_uchar (*s++);
  return val;
}

/*----------------------------------------------------.
| Wrap our hash inside signature expected by hash.h.  |
`----------------------------------------------------*/
static size_t
symtab_hasher (const void *entry, size_t buckets)
{
#ifdef DEBUG_SYM
  hash_entry++;
  if (buckets != current_size)
    {
      resizes++;
      current_size = buckets;
    }
#endif /* DEBUG_SYM */
  const symbol *sym = (const symbol *) entry;
  return hash (SYMBOL_NAME (sym), SYMBOL_NAME_LEN (sym)) % buckets;
}

/*----------------------------------------------.
| Compare two hash table entries for equality.  |
`----------------------------------------------*/
static bool
symtab_comparator (const void *entry_a, const void *entry_b)
{
#ifdef DEBUG_SYM
  comparator_entry++;
#endif /* DEBUG_SYM */
  const symbol *sym_a = (const symbol *) entry_a;
  const symbol *sym_b = (const symbol *) entry_b;
  return (SYMBOL_NAME_LEN (sym_a) == SYMBOL_NAME_LEN (sym_b)
	  && memcmp (SYMBOL_NAME (sym_a), SYMBOL_NAME (sym_b),
		     SYMBOL_NAME_LEN (sym_a)) == 0);
}

/*---------------------------.
| Reclaim an entry on exit.  |
`---------------------------*/
static void
symtab_free_entry (void *entry)
{
  symbol *sym = (symbol *) entry;
  while (sym->stack != sym)
    {
      symbol *old = sym->stack;
      sym->stack = old->stack;
      assert (!SYMBOL_PENDING_EXPANSIONS (old));
      free_symbol (old);
    }
  assert (!SYMBOL_PENDING_EXPANSIONS (sym));
  free_symbol (sym);
}

/*--------------------------------------------------------------.
| Initialize the symbol table, with SIZE as a hint for expected |
| number of entries.					        |
`--------------------------------------------------------------*/
void
symtab_init (size_t size)
{
  symtab = hash_initialize (size, NULL, symtab_hasher, symtab_comparator,
			    symtab_free_entry);
  if (!symtab)
    xalloc_die ();

#ifdef DEBUG_SYM
  atexit (show_profile); /* Ignore failure, since this is debug code.  */
#endif /* DEBUG_SYM */
}

/*------------------------.
| Clean up entire table.  |
`------------------------*/
void
symtab_free (void)
{
  hash_free (symtab);
}

/*--------------------------------------------.
| Free all storage associated with a symbol.  |
`--------------------------------------------*/

void
free_symbol (symbol *sym)
{
  if (SYMBOL_PENDING_EXPANSIONS (sym) > 0)
    SYMBOL_DELETED (sym) = true;
  else
    {
      free (SYMBOL_NAME (sym));
      if (SYMBOL_TYPE (sym) == TOKEN_TEXT)
	free (SYMBOL_TEXT (sym));
      free (sym);
    }
}

/*-------------------------------------------------------------------.
| Searches and manipulation of the symbol table are all done by      |
| lookup_symbol ().  It basically hashes NAME, of length LEN, to a   |
| list in the symbol table, and searches this list for the first     |
| occurrence of a symbol with the name.                              |
|                                                                    |
| The MODE parameter determines what lookup_symbol () will do.  It   |
| can either just do a lookup, do a lookup and insert if not         |
| present, do an insertion even if the name is already in the list,  |
| delete the first occurrence of the name on the list, or delete all |
| occurrences of the name on the list.  The return value when        |
| requesting deletion is non-NULL if deletion occurred, but must not |
| be dereferenced.                                                   |
`-------------------------------------------------------------------*/

symbol *
lookup_symbol (const char *name, size_t len, symbol_lookup mode)
{
  symbol *sym;
  symbol *entry;
  symbol tmp;

#if DEBUG_SYM
  current_mode = mode;
  profiles[mode].entry++;
#endif /* DEBUG_SYM */

  tmp.name = (char *) name;
  tmp.len = len;
  entry = (symbol *) hash_lookup (symtab, &tmp);

  switch (mode)
    {
    case SYMBOL_LOOKUP:
      return entry ? entry->stack : NULL;

    case SYMBOL_INSERT:

      /* If the name was found in the table, check whether it is still in
	 use by a pending expansion.  If so, replace the table element with
	 a new one; if not, just return the symbol.  If not found, just
	 insert the name, and return the new symbol.  */

      if (entry)
	{
	  sym = entry->stack;
	  if (SYMBOL_PENDING_EXPANSIONS (sym) > 0)
	    {
	      symbol *old = sym;
	      SYMBOL_DELETED (old) = true;

	      sym = (symbol *) xmalloc (sizeof *sym);
	      SYMBOL_TYPE (sym) = TOKEN_VOID;
	      SYMBOL_TRACED (sym) = SYMBOL_TRACED (old);
	      SYMBOL_NAME (sym) = xmemdup0 (name, len);
	      SYMBOL_NAME_LEN (sym) = len;
	      SYMBOL_MACRO_ARGS (sym) = false;
	      SYMBOL_BLIND_NO_ARGS (sym) = false;
	      SYMBOL_DELETED (sym) = false;
	      SYMBOL_PENDING_EXPANSIONS (sym) = 0;

	      if (old == entry)
		{
		  old = (symbol *) hash_delete (symtab, entry);
		  assert (entry == old);
		  sym->stack = sym;
		  entry = (symbol *) hash_insert (symtab, sym);
		  if (entry)
		    assert (sym == entry);
		  else
		    xalloc_die ();
		}
	      else
		{
		  entry->stack = sym;
		  sym->stack = old->stack;
		}
	      old->stack = NULL;
	    }
	  return sym;
	}
      /* Fall through.  */

    case SYMBOL_PUSHDEF:

      /* Insert a name in the symbol table.  If there is already a
	 symbol with the name, add it to the pushdef stack.  Since the
	 hash table does not allow the insertion of duplicates, the
	 pushdef stack is a circular chain; the hash entry is the
	 oldest entry, which points to the newest entry; all other
	 entries point to the next older entry.  */
      sym = (symbol *) xmalloc (sizeof *sym);
      SYMBOL_TYPE (sym) = TOKEN_VOID;
      SYMBOL_TRACED (sym) = false;
      SYMBOL_NAME (sym) = xmemdup0 (name, len);
      SYMBOL_NAME_LEN (sym) = len;
      SYMBOL_MACRO_ARGS (sym) = false;
      SYMBOL_BLIND_NO_ARGS (sym) = false;
      SYMBOL_DELETED (sym) = false;
      SYMBOL_PENDING_EXPANSIONS (sym) = 0;

      if (entry)
	{
	  assert (mode == SYMBOL_PUSHDEF);
	  sym->stack = entry->stack;
	  entry->stack = sym;
	  SYMBOL_TRACED (sym) = SYMBOL_TRACED (sym->stack);
	}
      else
	{
	  sym->stack = sym;
	  entry = (symbol *) hash_insert (symtab, sym);
	  if (entry)
	    assert (sym == entry);
	  else
	    xalloc_die ();
	}
      return sym;

    case SYMBOL_DELETE:
    case SYMBOL_POPDEF:

      /* Delete occurrences of symbols with NAME.  SYMBOL_DELETE kills
	 all definitions, SYMBOL_POPDEF kills only the first.
	 However, if the last instance of a symbol is marked for
	 tracing, reinsert a placeholder in the table.  And if the
	 definition is still in use, let the caller free the memory
	 after it is done with the symbol.  */

      if (!entry
	  || (SYMBOL_TYPE (entry) == TOKEN_VOID && entry->stack == entry
	      && SYMBOL_TRACED (entry)))
	return NULL;
      {
	bool traced = false;
	symbol *result = sym = entry->stack;
	if (sym != entry && mode == SYMBOL_POPDEF)
	  {
	    SYMBOL_TRACED (sym->stack) = SYMBOL_TRACED (sym);
	    entry->stack = sym->stack;
	    sym->stack = NULL;
	    free_symbol (sym);
	  }
	else
	  {
	    traced = SYMBOL_TRACED (sym);
	    while (sym != entry)
	      {
		symbol *old = sym;
		sym = sym->stack;
		old->stack = NULL;
		free_symbol (old);
	      }
	    sym = (symbol *) hash_delete (symtab, entry);
	    assert (sym == entry);
	    sym->stack = NULL;
	    free_symbol (sym);
	  }
	if (traced)
	  {
	    sym = (symbol *) xmalloc (sizeof *sym);
	    SYMBOL_TYPE (sym) = TOKEN_VOID;
	    SYMBOL_TRACED (sym) = true;
	    SYMBOL_NAME (sym) = xmemdup0 (name, len);
	    SYMBOL_NAME_LEN (sym) = len;
	    SYMBOL_MACRO_ARGS (sym) = false;
	    SYMBOL_BLIND_NO_ARGS (sym) = false;
	    SYMBOL_DELETED (sym) = false;
	    SYMBOL_PENDING_EXPANSIONS (sym) = 0;

	    sym->stack = sym;
	    entry = (symbol *) hash_insert (symtab, sym);
	    if (entry)
	      assert (sym == entry);
	    else
	      xalloc_die ();
	  }
	return result;
      }

    default:
      assert (!"symbol_lookup");
      abort ();
    }
}

/*-----------------------------------------------------------------.
| The following function is used for the cases where we want to do |
| something to each and every symbol in the table.  The function   |
| hack_all_symbols () traverses the symbol table, and calls a	   |
| specified function FUNC for each symbol in the table.  FUNC is   |
| called with a pointer to the symbol, and the DATA argument.	   |
|								   |
| FUNC may safely call lookup_symbol with mode SYMBOL_POPDEF or	   |
| SYMBOL_LOOKUP, but any other mode can break the iteration.	   |
`-----------------------------------------------------------------*/

void
hack_all_symbols (hack_symbol *func, void *data)
{
  symbol *sym = (symbol *) hash_get_first (symtab);
  symbol *next;

  while (sym)
    {
      /* We allow func to call SYMBOL_POPDEF, which can invalidate
	 sym, so we must grab the next element to traverse before
	 calling func.  */
      next = (symbol *) hash_get_next (symtab, sym);
      func (sym->stack, data);
      sym = next;
    }
}

#ifdef DEBUG_SYM

static void symtab_print_list (int i);

static void M4_GNUC_UNUSED
symtab_debug (void)
{
  token_data td;
  const char *text;
  symbol *s;
  int delete;
  size_t len;
  int line;
  static int i;

  while (next_token (&td, &line, NULL, false, NULL) == TOKEN_WORD)
    {
      text = TOKEN_DATA_TEXT (&td);
      len = TOKEN_DATA_LEN (&td);
      if (*text == '_')
	{
	  delete = 1;
	  text++;
	  len--;
	}
      else
	delete = 0;

      s = lookup_symbol (text, len, SYMBOL_LOOKUP);

      if (s == NULL)
	xprintf ("Name `%s' is unknown\n", text);

      if (delete)
	lookup_symbol (text, len, SYMBOL_DELETE);
      else
	lookup_symbol (text, len, SYMBOL_INSERT);
    }
  symtab_print_list (i++);
}

static void
symtab_print_list (int i)
{
  symbol *sym = (symbol *) hash_get_first (symtab);
  symbol *stack;

  xprintf ("Symbol dump #%d:\n", i);
  while (sym)
    {
      stack = sym->stack;
      do
	{
	  xprintf ("\tname %s, len %zu, addr %p, "
		   "stack %p, flags%s%s, pending %d\n",
		   SYMBOL_NAME (stack), SYMBOL_NAME_LEN (stack),
		   stack, stack->stack, SYMBOL_TRACED (stack) ? " traced" : "",
		   SYMBOL_DELETED (stack) ? " deleted" : "",
		   SYMBOL_PENDING_EXPANSIONS (stack));
	  stack = stack->stack;
	}
      while (stack != sym);
      sym = (symbol *) hash_get_next (symtab, sym);
    }
}

#endif /* DEBUG_SYM */

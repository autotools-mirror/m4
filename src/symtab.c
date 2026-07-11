/* GNU m4 -- A simple macro processor

   Copyright (C) 1989-1994, 2003, 2006-2014, 2016-2017, 2020-2026 Free
   Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/* This file handles all the low level work around the symbol table.  The
   symbol table is a simple chained hash table.  Each symbol is described
   by a struct symbol, which is placed in the hash table based upon the
   symbol name.  Symbols that hash to the same entry in the table are
   kept on a list, sorted by hash.  As a special case, to facilitate the
   "pushdef" and "popdef" builtins, a symbol can be several times in the
   symbol table, one for each definition.  Since the name is the same,
   all the entries for the symbol will be on the same list, and will
   also, because the list is sorted, be adjacent.  All the entries for a
   name are simply ordered on the list by age.  The current definition
   will then always be the first found.  */

#include "m4.h"
#include <limits.h>
#include <stdbit.h>
#include <stdckdint.h>

#ifdef DEBUG_SYM
/* When evaluating hash table performance, this profiling code shows
   how many collisions were encountered.  */

struct profile
{
  intmax_t entry;               /* Number of times lookup_symbol called with
                                   this mode.  */
  intmax_t allocations;         /* Number of times a symbol is malloc'd.  */
  intmax_t hits;                /* Number of times a symbol is found.  */
  intmax_t checks;              /* Number of times a hash is checked.  */
  intmax_t comparisons;         /* Number of times strcmp was called.  */
  intmax_t misses;              /* Number of times strcmp did not return 0.  */
  intmax_t bytes_hashed;        /* Number of bytes hashed.  */
  intmax_t bytes_compared;      /* Number of bytes compared.  */
};

static struct profile profiles[5];
static symbol_lookup current_mode;

/* On exit, show a profile of symbol table performance.  */
static void
show_profile (void)
{
  int i;
  for (i = 0; i < 5; i++)
    {
      xfprintf (stderr, "m4debug: lookup mode %d called %jd times, %jd hits:\n"
                "m4debug:  symbols: %jd allocs, %jd checks, %jd bytes hashed\n"
                "m4debug:  str: %jd compares, %jd misses, %jd bytes compared\n",
                i, profiles[i].entry, profiles[i].hits,
                profiles[i].allocations, profiles[i].checks,
                profiles[i].bytes_hashed, profiles[i].comparisons,
                profiles[i].misses, profiles[i].bytes_compared);
    }
}

/* Like strcmp (S1, S2), but also track profiling statistics.  */
static int
profile_strcmp (const char *s1, const char *s2)
{
  idx_t i = 0;
  int result;
  for (; s1[i] && s1[i] == s2[i]; i++)
    continue;
  result = to_uchar (s1[i]) - to_uchar (s2[i]);
  profiles[current_mode].comparisons++;
  if (result != 0)
    profiles[current_mode].misses++;
  profiles[current_mode].bytes_compared += i + 1;
  return result;
}

# define strcmp profile_strcmp
#endif /* DEBUG_SYM */


/*------------------------------------------------------------------.
| Initialize the symbol table, by allocating the necessary storage, |
| and zeroing all the entries.                                      |
`------------------------------------------------------------------*/

/* Pointer to symbol table.  */
static symbol **symtab;

void
symtab_init (void)
{
  symbol **s = xicalloc (hash_table_size, sizeof *s);
  symtab = s;

#ifdef DEBUG_SYM
  {
    int e = atexit (show_profile);
    if (e != 0)
      M4ERROR ((warning_status, 0,
                "INTERNAL ERROR: unable to show symtab profile"));
  }
#endif /* DEBUG_SYM */
}

/*----------------------------------.
| Return a hashvalue for a string.  |
`----------------------------------*/

static size_t ATTRIBUTE_PURE
hash (const char *s)
{
  size_t val = 0;
  for (; *s; s++)
    ckd_add (&val, stdc_rotate_left (val, 7), +*s);
  return val;
}

/*--------------------------------------------.
| Free all storage associated with a symbol.  |
`--------------------------------------------*/

void
free_symbol (symbol *sym)
{
  if (SYMBOL_PENDING_EXPANSIONS (sym) > 0)
    {
      SYMBOL_DELETED (sym) = true;
      if (SYMBOL_STACK (sym))
        {
          SYMBOL_NAME (sym) = ximemdup0 (SYMBOL_NAME (sym),
                                         SYMBOL_NAME_LEN (sym));
          SYMBOL_STACK (sym) = NULL;
        }
    }
  else
    {
      if (SYMBOL_STACK (sym) == NULL)
        free (SYMBOL_NAME (sym));
      if (SYMBOL_TYPE (sym) == TOKEN_TEXT)
        free (SYMBOL_TEXT (sym));
      free (sym);
    }
}

/*-------------------------------------------------------------------.
| Search in, and manipulation of the symbol table, are all done by   |
| lookup_symbol ().  It basically hashes NAME to a list in the       |
| symbol table, and searches this list for the first occurrence of a |
| symbol with the name.                                              |
|                                                                    |
| The MODE parameter determines what lookup_symbol () will do.  It   |
| can either just do a lookup, do a lookup and insert if not         |
| present, do an insertion even if the name is already in the list,  |
| delete the first occurrence of the name on the list, or delete all |
| occurrences of the name on the list.                               |
`-------------------------------------------------------------------*/

symbol *
lookup_symbol (const char *name, idx_t len, symbol_lookup mode)
{
  size_t h;
  int cmp = 1;
  symbol *sym, *prev;
  symbol **spp;

#if DEBUG_SYM
  current_mode = mode;
  profiles[mode].entry++;
  profiles[mode].bytes_hashed += len;
#endif /* DEBUG_SYM */

  h = hash (name);
  sym = symtab[h % hash_table_size];

  for (prev = NULL; sym != NULL; prev = sym, sym = sym->next)
    {
#ifdef DEBUG_SYM
      profiles[mode].checks++;
#endif
      cmp = (h > sym->hash) - (h < sym->hash);
      if (cmp == 0)
        cmp = strcmp (SYMBOL_NAME (sym), name);
      if (cmp >= 0)
        break;
    }

  /* If just searching, return status of search.  */

#ifdef DEBUG_SYM
  if (cmp == 0)
    profiles[mode].hits++;
#endif
  if (mode == SYMBOL_LOOKUP)
    return cmp == 0 ? sym : NULL;

  /* Symbol not found.  */

  spp = (prev != NULL) ? &prev->next : &symtab[h % hash_table_size];

  switch (mode)
    {

    case SYMBOL_INSERT:

      /* If the name was found in the table, check whether it is still in
         use by a pending expansion.  If so, replace the table element with
         a new one; if not, just return the symbol.  If not found, just
         insert the name, and return the new symbol.  */

      if (cmp == 0 && sym != NULL)
        {
          if (SYMBOL_PENDING_EXPANSIONS (sym) > 0)
            {
              symbol *old = sym;
              SYMBOL_DELETED (old) = true;

#ifdef DEBUG_SYM
              profiles[mode].allocations++;
#endif
              sym = (symbol *) xmalloc (sizeof (symbol));
              set_token_data_void (symbol_token_data (sym));
              SYMBOL_TRACED (sym) = SYMBOL_TRACED (old);
              sym->hash = h;
              SYMBOL_NAME (sym) = SYMBOL_NAME (old);
              SYMBOL_NAME_LEN (sym) = SYMBOL_NAME_LEN (old);
              SYMBOL_MACRO_ARGS (sym) = false;
              SYMBOL_BLIND_NO_ARGS (sym) = false;
              SYMBOL_DELETED (sym) = false;
              SYMBOL_PENDING_EXPANSIONS (sym) = 0;

              SYMBOL_STACK (sym) = SYMBOL_STACK (old);
              SYMBOL_STACK (old) = sym;
              sym->next = old->next;
              old->next = NULL;
              *spp = sym;
            }
          return sym;
        }
      FALLTHROUGH;

    case SYMBOL_PUSHDEF:

      /* Insert a name in the symbol table.  If there is already a symbol
         with the name, insert this in front of it.  */

#ifdef DEBUG_SYM
      profiles[mode].allocations++;
#endif
      sym = (symbol *) xmalloc (sizeof (symbol));
      set_token_data_void (symbol_token_data (sym));
      SYMBOL_TRACED (sym) = false;
      sym->hash = h;
      SYMBOL_MACRO_ARGS (sym) = false;
      SYMBOL_BLIND_NO_ARGS (sym) = false;
      SYMBOL_DELETED (sym) = false;
      SYMBOL_PENDING_EXPANSIONS (sym) = 0;

      SYMBOL_STACK (sym) = NULL;
      sym->next = *spp;
      *spp = sym;

      if (mode == SYMBOL_PUSHDEF && cmp == 0)
        {
          SYMBOL_STACK (sym) = sym->next;
          sym->next = SYMBOL_STACK (sym)->next;
          SYMBOL_STACK (sym)->next = NULL;
          SYMBOL_TRACED (sym) = SYMBOL_TRACED (SYMBOL_STACK (sym));
          SYMBOL_NAME (sym) = SYMBOL_NAME (SYMBOL_STACK (sym));
          SYMBOL_NAME_LEN (sym) = SYMBOL_NAME_LEN (SYMBOL_STACK (sym));
        }
      else
        {
          SYMBOL_NAME (sym) = ximemdup0 (name, len);
          SYMBOL_NAME_LEN (sym) = len;
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

      if (cmp != 0)
        return NULL;
      if (sym == NULL)
        return NULL;
      {
        bool traced = false;
        symbol *next;
        if (SYMBOL_STACK (sym) != NULL && mode == SYMBOL_POPDEF)
          {
            SYMBOL_TRACED (SYMBOL_STACK (sym)) = SYMBOL_TRACED (sym);
            SYMBOL_STACK (sym)->next = sym->next;
            *spp = SYMBOL_STACK (sym);
          }
        else
          {
            traced = SYMBOL_TRACED (sym);
            *spp = sym->next;
          }
        do
          {
            next = SYMBOL_STACK (sym);
            free_symbol (sym);
            sym = next;
          }
        while (next != NULL && mode == SYMBOL_DELETE);
        if (traced)
          {
#ifdef DEBUG_SYM
            profiles[mode].allocations++;
#endif
            sym = (symbol *) xmalloc (sizeof (symbol));
            set_token_data_void (symbol_token_data (sym));
            SYMBOL_TRACED (sym) = true;
            sym->hash = h;
            SYMBOL_NAME (sym) = ximemdup0 (name, len);
            SYMBOL_NAME_LEN (sym) = len;
            SYMBOL_MACRO_ARGS (sym) = false;
            SYMBOL_BLIND_NO_ARGS (sym) = false;
            SYMBOL_DELETED (sym) = false;
            SYMBOL_PENDING_EXPANSIONS (sym) = 0;

            SYMBOL_STACK (sym) = NULL;
            sym->next = *spp;
            *spp = sym;
          }
      }
      return NULL;

    case SYMBOL_LOOKUP:
    default:
      M4ERROR ((warning_status, 0,
                "INTERNAL ERROR: invalid mode to symbol_lookup ()"));
      abort ();
    }
}

/*-----------------------------------------------------------------.
| The following function is used for the cases where we want to do |
| something to each and every symbol in the table.  The function   |
| hack_all_symbols () traverses the symbol table, and calls a      |
| specified function FUNC for each symbol in the table.  FUNC is   |
| called with a pointer to the symbol, and the DATA argument.      |
|                                                                  |
| FUNC may safely call lookup_symbol with mode SYMBOL_POPDEF or    |
| SYMBOL_LOOKUP, but any other mode can break the iteration.       |
`-----------------------------------------------------------------*/

void
hack_all_symbols (hack_symbol *func, void *data)
{
  for (idx_t h = 0; h < hash_table_size; h++)
    {
      /* We allow func to call SYMBOL_POPDEF, which can invalidate
         sym, so we must grab the next element to traverse before
         calling func.  */
      symbol *next;
      for (symbol *sym = symtab[h]; sym != NULL; sym = next)
        {
          next = sym->next;
          func (sym, data);
        }
    }
}

#ifdef DEBUG_SYM

static void symtab_print_list (intmax_t i);

static void MAYBE_UNUSED
symtab_debug (void)
{
  token_data td;
  const char *text;
  symbol *s;
  static intmax_t i;
  idx_t len;

  while (next_token (&td, NULL) == TOKEN_WORD)
    {
      enum symbol_lookup mode;
      text = TOKEN_DATA_TEXT (&td);
      len = TOKEN_DATA_LEN (&td);
      if (*text == '_')
        {
          mode = SYMBOL_DELETE;
          text++;
          len--;
        }
      else
        mode = SYMBOL_INSERT;

      s = lookup_symbol (text, len, SYMBOL_LOOKUP);

      if (s == NULL)
        xprintf ("Name %s is unknown\n", squote (text));

      lookup_symbol (text, len, mode);
    }
  symtab_print_list (i++);
}

static void
symtab_print_list (intmax_t i)
{
  xprintf ("Symbol dump #%jd:\n", i);
  for (idx_t h = 0; h < hash_table_size; h++)
    for (symbol *bucket = symtab[h]; bucket != NULL; bucket = bucket->next)
      for (symbol *sym = bucket; sym; sym = sym->stack)
        xprintf ("\tname %s, len %td, hash %zu, bucket %td, addr %p, "
                 "stack %p, next %p, flags%s%s, pending %jd\n",
                 SYMBOL_NAME (sym), SYMBOL_NAME_LEN (sym),
                 sym->hash, h, sym, SYMBOL_STACK (sym),
                 sym->next,
                 SYMBOL_TRACED (sym) ? " traced" : "",
                 SYMBOL_DELETED (sym) ? " deleted" : "",
                 SYMBOL_PENDING_EXPANSIONS (sym));
}

#endif /* DEBUG_SYM */

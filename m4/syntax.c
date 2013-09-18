/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 2002, 2004, 2006-2010, 2013 Free Software
   Foundation, Inc.

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
/*#define DEBUG_SYNTAX */

/* THE SYNTAX TABLE

   The input is read character by character and grouped together
   according to a syntax table.  The character groups are (definitions
   are all in m4module.h, those marked with a * are not yet in use):

   Basic (all characters fall in one of these mutually exclusive bins)
   M4_SYNTAX_IGNORE     *Character to be deleted from input as if not present
   M4_SYNTAX_OTHER      Any character with no special meaning to m4
   M4_SYNTAX_SPACE      Whitespace (ignored when leading macro arguments)
   M4_SYNTAX_OPEN       Open list of macro arguments
   M4_SYNTAX_CLOSE      Close list of macro arguments
   M4_SYNTAX_COMMA      Separates macro arguments
   M4_SYNTAX_ACTIVE     This character is a macro name by itself
   M4_SYNTAX_ESCAPE     Use this character to prefix all macro names

   M4_SYNTAX_ALPHA      Alphabetic characters (can start macro names)
   M4_SYNTAX_NUM        Numeric characters (can form macro names)

   M4_SYNTAX_LQUOTE     A single character left quote
   M4_SYNTAX_BCOMM      A single character begin comment delimiter

   Attribute (these are context sensitive, and exist in addition to basic)
   M4_SYNTAX_RQUOTE     A single character right quote
   M4_SYNTAX_ECOMM      A single character end comment delimiter
   M4_SYNTAX_DOLLAR     Indicates macro argument in user macros
   M4_SYNTAX_LBRACE     *Indicates start of extended macro argument
   M4_SYNTAX_RBRACE     *Indicates end of extended macro argument

   Besides adding new facilities, the use of a syntax table will reduce
   the number of calls to next_token ().  Now groups of OTHER, NUM and
   SPACE characters can be returned as a single token, since next_token
   () knows they have no special syntactical meaning to m4.  This is,
   however, only possible if only single character quotes comments
   comments are used, because otherwise the quote and comment characters
   will not show up in the syntax-table.

   Having a syntax table allows new facilities.  The new builtin
   "changesyntax" allows the user to change the category of any
   character.

   By default, '\n' is both ECOMM and SPACE, depending on the context.
   Hence we have basic categories (mutually exclusive, can introduce a
   context, and can be empty sets), and attribute categories
   (additive, only recognized in context, and will never be empty).

   The precedence as implemented by next_token () is:

   M4_SYNTAX_IGNORE     *Filtered out below next_token ()
   M4_SYNTAX_ESCAPE     Reads macro name iff set, else next character
   M4_SYNTAX_ALPHA      Reads M4_SYNTAX_ALPHA and M4_SYNTAX_NUM as macro name
   M4_SYNTAX_LQUOTE     Reads all until balanced M4_SYNTAX_RQUOTE
   M4_SYNTAX_BCOMM      Reads all until M4_SYNTAX_ECOMM

   M4_SYNTAX_OTHER  }   Reads all M4_SYNTAX_OTHER, M4_SYNTAX_NUM
   M4_SYNTAX_NUM    }

   M4_SYNTAX_SPACE      Reads all M4_SYNTAX_SPACE, depending on buffering
   M4_SYNTAX_ACTIVE     Returns a single char as a macro name

   M4_SYNTAX_OPEN   }   Returned as a single char
   M4_SYNTAX_CLOSE  }
   M4_SYNTAX_COMMA  }

   M4_SYNTAX_RQUOTE and M4_SYNTAX_ECOMM are context-sensitive, and
   close out M4_SYNTAX_LQUOTE and M4_SYNTAX_BCOMM, respectively.
   Also, M4_SYNTAX_DOLLAR, M4_SYNTAX_LBRACE, and M4_SYNTAX_RBRACE are
   context-sensitive, only mattering when expanding macro definitions.

   There are several optimizations that can be performed depending on
   known states of the syntax table.  For example, when searching for
   quotes, if there is only a single start quote and end quote
   delimiter, we can use memchr2 and search a word at a time, instead
   of performing a table lookup a byte at a time.  The is_single_*
   flags track whether quotes and comments have a single delimiter
   (always the case if changequote/changecom were used, and
   potentially the case after changesyntax).  Since we frequently need
   to access quotes, we store the oldest valid quote outside the
   lookup table; the suspect flag tracks whether a cleanup pass is
   needed to restore our invariants.  On the other hand, coalescing
   multiple M4_SYNTAX_OTHER bytes could form a delimiter, so many
   optimizations must be disabled if a multi-byte delimiter exists;
   this is handled by m4__safe_quotes.  Meanwhile, quotes and comments
   can be disabled if the leading delimiter is length 0.  */

static int add_syntax_attribute         (m4_syntax_table *, char, int);
static int remove_syntax_attribute      (m4_syntax_table *, char, int);
static void set_quote_age               (m4_syntax_table *, bool, bool);

m4_syntax_table *
m4_syntax_create (void)
{
  m4_syntax_table *syntax = (m4_syntax_table *) xzalloc (sizeof *syntax);
  int ch;

  /* Set up default table.  This table never changes during operation,
     and contains no context attributes.  */
  for (ch = UCHAR_MAX + 1; --ch >= 0; )
    switch (ch)
      {
      case '(':
        syntax->orig[ch] = M4_SYNTAX_OPEN;
        break;
      case ')':
        syntax->orig[ch] = M4_SYNTAX_CLOSE;
        break;
      case ',':
        syntax->orig[ch] = M4_SYNTAX_COMMA;
        break;
      case '`':
        syntax->orig[ch] = M4_SYNTAX_LQUOTE;
        break;
      case '#':
        syntax->orig[ch] = M4_SYNTAX_BCOMM;
        break;
      default:
        if (isspace (ch))
          syntax->orig[ch] = M4_SYNTAX_SPACE;
        else if (isalpha (ch) || ch == '_')
          syntax->orig[ch] = M4_SYNTAX_ALPHA;
        else if (isdigit (ch))
          syntax->orig[ch] = M4_SYNTAX_NUM;
        else
          syntax->orig[ch] = M4_SYNTAX_OTHER;
      }

  /* Set up current table to match default.  */
  m4_reset_syntax (syntax);
  syntax->cached_simple.str1 = syntax->cached_lquote;
  syntax->cached_simple.len1 = 1;
  syntax->cached_simple.str2 = syntax->cached_rquote;
  syntax->cached_simple.len2 = 1;
  return syntax;
}

void
m4_syntax_delete (m4_syntax_table *syntax)
{
  assert (syntax);

  free (syntax->quote.str1);
  free (syntax->quote.str2);
  free (syntax->comm.str1);
  free (syntax->comm.str2);
  free (syntax);
}

int
m4_syntax_code (char ch)
{
  int code;

  switch (ch)
    {
      /* Sorted according to the order of M4_SYNTAX_* in m4module.h.  */
      /* FIXME - revisit the ignore syntax attribute.  */
    case 'I': case 'i': code = M4_SYNTAX_IGNORE; break;
      /* Basic categories.  */
    case '@':           code = M4_SYNTAX_ESCAPE; break;
    case 'W': case 'w': code = M4_SYNTAX_ALPHA;  break;
    case 'L': case 'l': code = M4_SYNTAX_LQUOTE; break;
    case 'B': case 'b': code = M4_SYNTAX_BCOMM;  break;
    case 'A': case 'a': code = M4_SYNTAX_ACTIVE; break;
    case 'D': case 'd': code = M4_SYNTAX_NUM;    break;
    case 'S': case 's': code = M4_SYNTAX_SPACE;  break;
    case '(':           code = M4_SYNTAX_OPEN;   break;
    case ')':           code = M4_SYNTAX_CLOSE;  break;
    case ',':           code = M4_SYNTAX_COMMA;  break;
    case 'O': case 'o': code = M4_SYNTAX_OTHER;  break;
      /* Context categories.  */
    case '$':           code = M4_SYNTAX_DOLLAR; break;
    case '{':           code = M4_SYNTAX_LBRACE; break;
    case '}':           code = M4_SYNTAX_RBRACE; break;
    case 'R': case 'r': code = M4_SYNTAX_RQUOTE; break;
    case 'E': case 'e': code = M4_SYNTAX_ECOMM;  break;

    default: code = -1;  break;
    }

  return code;
}



/* Functions to manipulate the syntax table.  */
static int
add_syntax_attribute (m4_syntax_table *syntax, char ch, int code)
{
  int c = to_uchar (ch);
  if (code & M4_SYNTAX_MASKS)
    {
      syntax->table[c] |= code;
      syntax->suspect = true;
    }
  else
    {
      if ((code & (M4_SYNTAX_SUSPECT)) != 0
          || m4_has_syntax (syntax, c, M4_SYNTAX_SUSPECT))
        syntax->suspect = true;
      syntax->table[c] = ((syntax->table[c] & M4_SYNTAX_MASKS) | code);
    }

#ifdef DEBUG_SYNTAX
  xfprintf(stderr, "Set syntax %o %c = %04X\n", c, isprint(c) ? c : '-',
           syntax->table[c]);
#endif

  return syntax->table[c];
}

static int
remove_syntax_attribute (m4_syntax_table *syntax, char ch, int code)
{
  int c = to_uchar (ch);
  assert (code & M4_SYNTAX_MASKS);
  syntax->table[c] &= ~code;
  syntax->suspect = true;

#ifdef DEBUG_SYNTAX
  xfprintf(stderr, "Unset syntax %o %c = %04X\n", c, isprint(c) ? c : '-',
           syntax->table[c]);
#endif

  return syntax->table[c];
}

/* Add the set CHARS of length LEN to syntax category CODE, removing
   them from whatever category they used to be in.  */
static void
add_syntax_set (m4_syntax_table *syntax, const char *chars, size_t len,
                int code)
{
  while (len--)
    add_syntax_attribute (syntax, *chars++, code);
}

/* Remove the set CHARS of length LEN from syntax category CODE,
   adding them to category M4_SYNTAX_OTHER instead.  */
static void
subtract_syntax_set (m4_syntax_table *syntax, const char *chars, size_t len,
                     int code)
{
  while (len--)
    {
      char ch = *chars++;
      if ((code & M4_SYNTAX_MASKS) != 0)
        remove_syntax_attribute (syntax, ch, code);
      else if (m4_has_syntax (syntax, ch, code))
        add_syntax_attribute (syntax, ch, M4_SYNTAX_OTHER);
    }
}

/* Make the set CHARS of length LEN become syntax category CODE,
   removing CHARS from any other categories, and sending all bytes in
   the category but not in CHARS to category M4_SYNTAX_OTHER
   instead.  */
static void
set_syntax_set (m4_syntax_table *syntax, const char *chars, size_t len,
                int code)
{
  int ch;
  /* Explicit set of characters to install with this category; all
     other characters that used to have the category get reset to
     OTHER.  */
  for (ch = UCHAR_MAX + 1; --ch >= 0; )
    {
      if ((code & M4_SYNTAX_MASKS) != 0)
        remove_syntax_attribute (syntax, ch, code);
      else if (m4_has_syntax (syntax, ch, code))
        add_syntax_attribute (syntax, ch, M4_SYNTAX_OTHER);
    }
  while (len--)
    {
      ch = *chars++;
      add_syntax_attribute (syntax, ch, code);
    }
}

/* Reset syntax category CODE to its default state, sending all other
   characters in the category back to their default state.  */
static void
reset_syntax_set (m4_syntax_table *syntax, int code)
{
  int ch;
  for (ch = UCHAR_MAX + 1; --ch >= 0; )
    {
      /* Reset the category back to its default state.  All other
         characters that used to have this category get reset to
         their default state as well.  */
      if (code == M4_SYNTAX_RQUOTE)
        {
          if (ch == '\'')
            add_syntax_attribute (syntax, ch, code);
          else
            remove_syntax_attribute (syntax, ch, code);
        }
      else if (code == M4_SYNTAX_ECOMM)
        {
          if (ch == '\n')
            add_syntax_attribute (syntax, ch, code);
          else
            remove_syntax_attribute (syntax, ch, code);
        }
      else if (code == M4_SYNTAX_DOLLAR)
        {
          if (ch == '$')
            add_syntax_attribute (syntax, ch, code);
          else
            remove_syntax_attribute (syntax, ch, code);
        }
      else if (code == M4_SYNTAX_LBRACE)
        {
          if (ch == '{')
            add_syntax_attribute (syntax, ch, code);
          else
            remove_syntax_attribute (syntax, ch, code);
        }
      else if (code == M4_SYNTAX_RBRACE)
        {
          if (ch == '}')
            add_syntax_attribute (syntax, ch, code);
          else
            remove_syntax_attribute (syntax, ch, code);
        }
      else if (syntax->orig[ch] == code || m4_has_syntax (syntax, ch, code))
        add_syntax_attribute (syntax, ch, syntax->orig[ch]);
    }
}

/* Reset the syntax table to its default state.  */
void
m4_reset_syntax (m4_syntax_table *syntax)
{
  /* Restore the default syntax, which has known quote and comment
     properties.  */
  memcpy (syntax->table, syntax->orig, sizeof syntax->orig);

  free (syntax->quote.str1);
  free (syntax->quote.str2);
  free (syntax->comm.str1);
  free (syntax->comm.str2);

  /* The use of xmemdup0 is exploited by input.c.  */
  syntax->quote.str1 = xmemdup0 (DEF_LQUOTE, 1);
  syntax->quote.len1 = 1;
  syntax->quote.str2 = xmemdup0 (DEF_RQUOTE, 1);
  syntax->quote.len2 = 1;
  syntax->comm.str1 = xmemdup0 (DEF_BCOMM, 1);
  syntax->comm.len1 = 1;
  syntax->comm.str2 = xmemdup0 (DEF_ECOMM, 1);
  syntax->comm.len2 = 1;
  syntax->dollar = '$';

  add_syntax_attribute (syntax, syntax->quote.str2[0], M4_SYNTAX_RQUOTE);
  add_syntax_attribute (syntax, syntax->comm.str2[0], M4_SYNTAX_ECOMM);
  add_syntax_attribute (syntax, '$', M4_SYNTAX_DOLLAR);
  add_syntax_attribute (syntax, '{', M4_SYNTAX_LBRACE);
  add_syntax_attribute (syntax, '}', M4_SYNTAX_RBRACE);

  syntax->is_single_quotes = true;
  syntax->is_single_comments = true;
  syntax->is_single_dollar = true;
  syntax->is_macro_escaped = false;
  set_quote_age (syntax, true, false);
}

/* Alter the syntax for category KEY, according to ACTION: '+' to add,
   '-' to subtract, '=' to set, or '\0' to reset.  The array CHARS of
   length LEN describes the characters to modify; it is ignored if
   ACTION is '\0'.  Return -1 if KEY is invalid, otherwise return the
   syntax category matching KEY.  */
int
m4_set_syntax (m4_syntax_table *syntax, char key, char action,
               const char *chars, size_t len)
{
  int code;

  assert (syntax && chars);
  code = m4_syntax_code (key);
  if (code < 0)
    {
      return -1;
    }
  syntax->suspect = false;
  switch (action)
    {
    case '+':
      add_syntax_set (syntax, chars, len, code);
      break;
    case '-':
      subtract_syntax_set (syntax, chars, len, code);
      break;
    case '=':
      set_syntax_set (syntax, chars, len, code);
      break;
    case '\0':
      assert (!len);
      reset_syntax_set (syntax, code);
      break;
    default:
      assert (false);
    }

  /* Check for any cleanup needed.  */
  if (syntax->suspect)
    {
      int ch;
      int lquote = -1;
      int rquote = -1;
      int bcomm = -1;
      int ecomm = -1;
      bool single_quote_possible = true;
      bool single_comm_possible = true;
      int dollar = -1;
      if (m4_has_syntax (syntax, syntax->quote.str1[0], M4_SYNTAX_LQUOTE))
        {
          assert (syntax->quote.len1 == 1);
          lquote = to_uchar (syntax->quote.str1[0]);
        }
      if (m4_has_syntax (syntax, syntax->quote.str2[0], M4_SYNTAX_RQUOTE))
        {
          assert (syntax->quote.len2 == 1);
          rquote = to_uchar (syntax->quote.str2[0]);
        }
      if (m4_has_syntax (syntax, syntax->comm.str1[0], M4_SYNTAX_BCOMM))
        {
          assert (syntax->comm.len1 == 1);
          bcomm = to_uchar (syntax->comm.str1[0]);
        }
      if (m4_has_syntax (syntax, syntax->comm.str2[0], M4_SYNTAX_ECOMM))
        {
          assert (syntax->comm.len2 == 1);
          ecomm = to_uchar (syntax->comm.str2[0]);
        }
      syntax->is_single_dollar = false;
      syntax->is_macro_escaped = false;
      /* Find candidates for each category.  */
      for (ch = UCHAR_MAX + 1; --ch >= 0; )
        {
          if (m4_has_syntax (syntax, ch, M4_SYNTAX_LQUOTE))
            {
              if (lquote == -1)
                lquote = ch;
              else if (lquote != ch)
                single_quote_possible = false;
            }
          if (m4_has_syntax (syntax, ch, M4_SYNTAX_RQUOTE))
            {
              if (rquote == -1)
                rquote = ch;
              else if (rquote != ch)
                single_quote_possible = false;
            }
          if (m4_has_syntax (syntax, ch, M4_SYNTAX_BCOMM))
            {
              if (bcomm == -1)
                bcomm = ch;
              else if (bcomm != ch)
                single_comm_possible = false;
            }
          if (m4_has_syntax (syntax, ch, M4_SYNTAX_ECOMM))
            {
              if (ecomm == -1)
                ecomm = ch;
              else if (ecomm != ch)
                single_comm_possible = false;
            }
          if (m4_has_syntax (syntax, ch, M4_SYNTAX_DOLLAR))
            {
              if (dollar == -1)
                {
                  syntax->dollar = dollar = ch;
                  syntax->is_single_dollar = true;
                }
              else
                syntax->is_single_dollar = false;
            }
          if (m4_has_syntax (syntax, ch, M4_SYNTAX_ESCAPE))
            syntax->is_macro_escaped = true;
        }
      /* Disable multi-character delimiters if we discovered
         delimiters.  */
      if (!single_quote_possible)
        syntax->is_single_quotes = false;
      if (!single_comm_possible)
        syntax->is_single_comments = false;
      if ((1 < syntax->quote.len1 || 1 < syntax->quote.len2)
          && (!syntax->is_single_quotes || lquote != -1 || rquote != -1))
        {
          if (syntax->quote.len1)
            {
              syntax->quote.len1 = lquote == to_uchar (syntax->quote.str1[0]);
              syntax->quote.str1[syntax->quote.len1] = '\0';
            }
          if (syntax->quote.len2)
            {
              syntax->quote.len2 = rquote == to_uchar (syntax->quote.str2[0]);
              syntax->quote.str2[syntax->quote.len2] = '\0';
            }
        }
      if ((1 < syntax->comm.len1 || 1 < syntax->comm.len2)
          && (!syntax->is_single_comments || bcomm != -1 || ecomm != -1))
        {
          if (syntax->comm.len1)
            {
              syntax->comm.len1 = bcomm == to_uchar (syntax->comm.str1[0]);
              syntax->comm.str1[syntax->comm.len1] = '\0';
            }
          if (syntax->comm.len2)
            {
              syntax->comm.len2 = ecomm == to_uchar (syntax->comm.str2[0]);
              syntax->comm.str2[syntax->comm.len2] = '\0';
            }
        }
      /* Update the strings.  */
      if (lquote != -1)
        {
          if (single_quote_possible)
            syntax->is_single_quotes = true;
          if (syntax->quote.len1)
            assert (syntax->quote.len1 == 1);
          else
            {
              free (syntax->quote.str1);
              syntax->quote.str1 = xcharalloc (2);
              syntax->quote.str1[1] = '\0';
              syntax->quote.len1 = 1;
            }
          syntax->quote.str1[0] = lquote;
          if (rquote == -1)
            {
              rquote = '\'';
              add_syntax_attribute (syntax, rquote, M4_SYNTAX_RQUOTE);
            }
          if (!syntax->quote.len2)
            {
              free (syntax->quote.str2);
              syntax->quote.str2 = xcharalloc (2);
            }
          syntax->quote.str2[0] = rquote;
          syntax->quote.str2[1] = '\0';
          syntax->quote.len2 = 1;
        }
      if (bcomm != -1)
        {
          if (single_comm_possible)
            syntax->is_single_comments = true;
          if (syntax->comm.len1)
            assert (syntax->comm.len1 == 1);
          else
            {
              free (syntax->comm.str1);
              syntax->comm.str1 = xcharalloc (2);
              syntax->comm.str1[1] = '\0';
              syntax->comm.len1 = 1;
            }
          syntax->comm.str1[0] = bcomm;
          if (ecomm == -1)
            {
              ecomm = '\n';
              add_syntax_attribute (syntax, ecomm, M4_SYNTAX_ECOMM);
            }
          if (!syntax->comm.len2)
            {
              free (syntax->comm.str2);
              syntax->comm.str2 = xcharalloc (2);
            }
          syntax->comm.str2[0] = ecomm;
          syntax->comm.str2[1] = '\0';
          syntax->comm.len2 = 1;
        }
    }
  set_quote_age (syntax, false, true);
  m4__quote_uncache (syntax);
  return code;
}


/* Functions for setting quotes and comment delimiters.  Used by
   m4_changecom () and m4_changequote ().  Both functions override the
   syntax table to maintain compatibility.  */

/* Set the quote delimiters to LQ and RQ, with respective lengths
   LQ_LEN and RQ_LEN.  Pass NULL if the argument was not present, to
   distinguish from an explicit empty string.  */
void
m4_set_quotes (m4_syntax_table *syntax, const char *lq, size_t lq_len,
               const char *rq, size_t rq_len)
{
  int ch;

  assert (syntax);

  /* POSIX states that with 0 arguments, the default quotes are used.
     POSIX XCU ERN 112 states that behavior is implementation-defined
     if there was only one argument, or if there is an empty string in
     either position when there are two arguments.  We allow an empty
     left quote to disable quoting, but a non-empty left quote will
     always create a non-empty right quote.  See the texinfo for what
     some other implementations do.  */
  if (!lq)
    {
      lq = DEF_LQUOTE;
      lq_len = 1;
      rq = DEF_RQUOTE;
      rq_len = 1;
    }
  else if (!rq || (lq_len && !rq_len))
    {
      rq = DEF_RQUOTE;
      rq_len = 1;
    }

  if (syntax->quote.len1 == lq_len && syntax->quote.len2 == rq_len
      && memcmp (syntax->quote.str1, lq, lq_len) == 0
      && memcmp (syntax->quote.str2, rq, rq_len) == 0)
    return;

  free (syntax->quote.str1);
  free (syntax->quote.str2);
  /* The use of xmemdup0 is exploited by input.c.  */
  syntax->quote.str1 = xmemdup0 (lq, lq_len);
  syntax->quote.len1 = lq_len;
  syntax->quote.str2 = xmemdup0 (rq, rq_len);
  syntax->quote.len2 = rq_len;

  /* changequote overrides syntax_table, but be careful when it is
     used to select a start-quote sequence that is effectively
     disabled.  */
  syntax->is_single_quotes = true;
  for (ch = UCHAR_MAX + 1; --ch >= 0; )
    {
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_LQUOTE))
        add_syntax_attribute (syntax, ch,
                              (syntax->orig[ch] == M4_SYNTAX_LQUOTE
                               ? M4_SYNTAX_OTHER : syntax->orig[ch]));
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_RQUOTE))
        remove_syntax_attribute (syntax, ch, M4_SYNTAX_RQUOTE);
    }

  if (!m4_has_syntax (syntax, *syntax->quote.str1,
                      (M4_SYNTAX_IGNORE | M4_SYNTAX_ESCAPE | M4_SYNTAX_ALPHA
                       | M4_SYNTAX_NUM)))
    {
      if (syntax->quote.len1 == 1)
        add_syntax_attribute (syntax, syntax->quote.str1[0], M4_SYNTAX_LQUOTE);
      if (syntax->quote.len2 == 1)
        add_syntax_attribute (syntax, syntax->quote.str2[0], M4_SYNTAX_RQUOTE);
    }
  set_quote_age (syntax, false, false);
}

/* Set the comment delimiters to BC and EC, with respective lengths
   BC_LEN and EC_LEN.  Pass NULL if the argument was not present, to
   distinguish from an explicit empty string.  */
void
m4_set_comment (m4_syntax_table *syntax, const char *bc, size_t bc_len,
                const char *ec, size_t ec_len)
{
  int ch;

  assert (syntax);

  /* POSIX requires no arguments to disable comments, and that one
     argument use newline as the close-comment.  POSIX XCU ERN 131
     states that empty arguments invoke implementation-defined
     behavior.  We allow an empty begin comment to disable comments,
     and a non-empty begin comment will always create a non-empty end
     comment.  See the texinfo for what some other implementations
     do.  */
  if (!bc)
    {
      bc = ec = "";
      bc_len = ec_len = 0;
    }
  else if (!ec || (bc_len && !ec_len))
    {
      ec = DEF_ECOMM;
      ec_len = 1;
    }

  if (syntax->comm.len1 == bc_len && syntax->comm.len2 == ec_len
      && memcmp (syntax->comm.str1, bc, bc_len) == 0
      && memcmp (syntax->comm.str2, ec, ec_len) == 0)
    return;

  free (syntax->comm.str1);
  free (syntax->comm.str2);
  /* The use of xmemdup0 is exploited by input.c.  */
  syntax->comm.str1 = xmemdup0 (bc, bc_len);
  syntax->comm.len1 = bc_len;
  syntax->comm.str2 = xmemdup0 (ec, ec_len);
  syntax->comm.len2 = ec_len;

  /* changecom overrides syntax_table, but be careful when it is used
     to select a start-comment sequence that is effectively
     disabled.  */
  syntax->is_single_comments = true;
  for (ch = UCHAR_MAX + 1; --ch >= 0; )
    {
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_BCOMM))
        add_syntax_attribute (syntax, ch,
                              (syntax->orig[ch] == M4_SYNTAX_BCOMM
                               ? M4_SYNTAX_OTHER : syntax->orig[ch]));
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_ECOMM))
        remove_syntax_attribute (syntax, ch, M4_SYNTAX_ECOMM);
    }
  if (!m4_has_syntax (syntax, *syntax->comm.str1,
                      (M4_SYNTAX_IGNORE | M4_SYNTAX_ESCAPE | M4_SYNTAX_ALPHA
                       | M4_SYNTAX_NUM | M4_SYNTAX_LQUOTE)))
    {
      if (syntax->comm.len1 == 1)
        add_syntax_attribute (syntax, syntax->comm.str1[0], M4_SYNTAX_BCOMM);
      if (syntax->comm.len2 == 1)
        add_syntax_attribute (syntax, syntax->comm.str2[0], M4_SYNTAX_ECOMM);
    }
  set_quote_age (syntax, false, false);
}

/* Call this when changing anything that might impact the quote age,
   so that m4__quote_age and m4__safe_quotes will reflect the change.
   If RESET, changesyntax was reset to its default stage; if CHANGE,
   arbitrary syntax has changed; otherwise, just quotes or comment
   delimiters have changed.  */
static void
set_quote_age (m4_syntax_table *syntax, bool reset, bool change)
{
  /* Multi-character quotes are inherently unsafe, since concatenation
     of individual characters can result in a quote delimiter,
     consider:

     define(echo,``$1'')define(a,A)changequote(<[,]>)echo(<[]]><[>a]>)
     => A]> (not ]>a)

   Also, unquoted close delimiters are unsafe, consider:

     define(echo,``$1'')define(a,A)echo(`a''`a')
     => aA' (not a'a)

   Duplicated start and end quote delimiters, as well as comment
   delimiters that overlap with quote delimiters or active characters,
   also present a problem, consider:

     define(echo,$*)echo(a,a,a`'define(a,A)changecom(`,',`,'))
     => A,a,A (not A,A,A)

   The impact of arbitrary changesyntax is difficult to characterize.
   So if things are in their default state, we use 0 for the upper 16
   bits of quote_age; otherwise we increment syntax_age for each
   changesyntax, but saturate it at 0xffff rather than wrapping
   around.  Perhaps a cache of other frequently used states is
   warranted, if changesyntax becomes more popular.

   Perhaps someday we will fix $@ expansion to use the current
   settings of the comma category, or even allow multi-character
   argument separators via changesyntax.  Until then, we use a literal
   `,' in $@ expansion, therefore we must insist that `,' be an
   argument separator for quote_age to be non-zero.

   Rather than check every token for an unquoted delimiter, we merely
   encode current_quote_age to 0 when things are unsafe, and non-zero
   when safe (namely, the syntax_age in the upper 16 bits, coupled
   with the 16-bit value composed of the single-character start and
   end quote delimiters).  There may be other situations which are
   safe even when this algorithm sets the quote_age to zero, but at
   least a quote_age of zero always produces correct results (although
   it may take more time in doing so).  */

  unsigned short local_syntax_age;
  if (reset)
    local_syntax_age = 0;
  else if (change && syntax->syntax_age < 0xffff)
    local_syntax_age = ++syntax->syntax_age;
  else
    local_syntax_age = syntax->syntax_age;
  if (local_syntax_age < 0xffff && syntax->is_single_quotes
      && syntax->quote.len1 == 1 && syntax->quote.len2 == 1
      && !m4_has_syntax (syntax, *syntax->quote.str1,
                         (M4_SYNTAX_ALPHA | M4_SYNTAX_NUM | M4_SYNTAX_OPEN
                          | M4_SYNTAX_COMMA | M4_SYNTAX_CLOSE
                          | M4_SYNTAX_SPACE))
      && !m4_has_syntax (syntax, *syntax->quote.str2,
                         (M4_SYNTAX_ALPHA | M4_SYNTAX_NUM | M4_SYNTAX_OPEN
                          | M4_SYNTAX_COMMA | M4_SYNTAX_CLOSE
                          | M4_SYNTAX_SPACE))
      && *syntax->quote.str1 != *syntax->quote.str2
      && (!syntax->comm.len1
          || (*syntax->comm.str1 != *syntax->quote.str2
              && !m4_has_syntax (syntax, *syntax->comm.str1,
                                 (M4_SYNTAX_OPEN | M4_SYNTAX_COMMA
                                  | M4_SYNTAX_CLOSE))))
      && m4_has_syntax (syntax, ',', M4_SYNTAX_COMMA))
    {
      syntax->quote_age = ((local_syntax_age << 16)
                           | ((*syntax->quote.str1 & 0xff) << 8)
                           | (*syntax->quote.str2 & 0xff));
    }
  else
    syntax->quote_age = 0;
}

/* Interface for caching frequently used quote pairs, independently of
   the current quote delimiters (for example, consider a text macro
   expansion that includes several copies of $@), and using AGE for
   optimization.  If QUOTES is NULL, don't use quoting.  If OBS is
   non-NULL, AGE should be the current quote age, and QUOTES should be
   m4_get_syntax_quotes; the return value will be a cached quote pair,
   where the pointer is valid at least as long as OBS is not reset,
   but whose contents are only guaranteed until the next changequote
   or quote_cache.  Otherwise, OBS is NULL, AGE should be the same as
   before, and QUOTES should be a previously returned cache value;
   used to refresh the contents of the result.  */
const m4_string_pair *
m4__quote_cache (m4_syntax_table *syntax, m4_obstack *obs, unsigned int age,
                 const m4_string_pair *quotes)
{
  /* Implementation - if AGE is non-zero, then the implementation of
     set_quote_age guarantees that we can recreate the return value on
     the fly; so we use static storage, and the contents must be used
     immediately.  If AGE is zero, then we must copy QUOTES onto OBS,
     but we might as well cache that copy.  */
  if (!quotes)
    return NULL;
  if (age)
    {
      *syntax->cached_lquote = (age >> 8) & 0xff;
      *syntax->cached_rquote = age & 0xff;
      return &syntax->cached_simple;
    }
  if (!obs)
    return quotes;
  assert (quotes == &syntax->quote);
  if (!syntax->cached_quote)
    {
      assert (obstack_object_size (obs) == 0);
      syntax->cached_quote = (m4_string_pair *) obstack_copy (obs, quotes,
                                                              sizeof *quotes);
      syntax->cached_quote->str1 = (char *) obstack_copy0 (obs, quotes->str1,
                                                           quotes->len1);
      syntax->cached_quote->str2 = (char *) obstack_copy0 (obs, quotes->str2,
                                                           quotes->len2);
    }
  return syntax->cached_quote;
}


/* Define these functions at the end, so that calls in the file use the
   faster macro version from m4module.h.  */
#undef m4_get_syntax_lquote
const char *
m4_get_syntax_lquote (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->quote.str1;
}

#undef m4_get_syntax_rquote
const char *
m4_get_syntax_rquote (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->quote.str2;
}

#undef m4_get_syntax_quotes
const m4_string_pair *
m4_get_syntax_quotes (m4_syntax_table *syntax)
{
  assert (syntax);
  return &syntax->quote;
}

#undef m4_is_syntax_single_quotes
bool
m4_is_syntax_single_quotes (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_single_quotes;
}

#undef m4_get_syntax_bcomm
const char *
m4_get_syntax_bcomm (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->comm.str1;
}

#undef m4_get_syntax_ecomm
const char *
m4_get_syntax_ecomm (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->comm.str2;
}

#undef m4_get_syntax_comments
const m4_string_pair *
m4_get_syntax_comments (m4_syntax_table *syntax)
{
  assert (syntax);
  return &syntax->comm;
}

#undef m4_is_syntax_single_comments
bool
m4_is_syntax_single_comments (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_single_comments;
}

#undef m4_is_syntax_single_dollar
bool
m4_is_syntax_single_dollar (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_single_dollar;
}

#undef m4_is_syntax_macro_escaped
bool
m4_is_syntax_macro_escaped (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_macro_escaped;
}

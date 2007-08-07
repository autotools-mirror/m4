/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2002, 2004, 2006, 2007
   Free Software Foundation, Inc.

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
   are all in m4.h, those marked with a * are not yet in use):

   M4_SYNTAX_IGNORE	*Character to be deleted from input as if not present
   M4_SYNTAX_OTHER	Any character with no special meaning to m4
   M4_SYNTAX_SPACE	Whitespace (ignored when leading macro arguments)
   M4_SYNTAX_OPEN	Open list of macro arguments
   M4_SYNTAX_CLOSE	Close list of macro arguments
   M4_SYNTAX_COMMA	Separates macro arguments
   M4_SYNTAX_DOLLAR	Indicates macro argument in user macros
   M4_SYNTAX_LBRACE	Indicates start of extended macro argument
   M4_SYNTAX_RBRACE	Indicates end of extended macro argument
   M4_SYNTAX_ACTIVE	This character is a macro name by itself
   M4_SYNTAX_ESCAPE	Use this character to prefix all macro names

   M4_SYNTAX_ALPHA	Alphabetic characters (can start macro names)
   M4_SYNTAX_NUM	Numeric characters (can form macro names)

   M4_SYNTAX_LQUOTE	A single characters left quote
   M4_SYNTAX_BCOMM	A single characters begin comment delimiter

   (These are bit masks)
   M4_SYNTAX_RQUOTE	A single characters right quote
   M4_SYNTAX_ECOMM	A single characters end comment delimiter

   Besides adding new facilities, the use of a syntax table will reduce
   the number of calls to next_token ().  Now groups of OTHER, NUM and
   SPACE characters can be returned as a single token, since next_token
   () knows they have no special syntactical meaning to m4.  This is,
   however, only possible if only single character quotes comments
   comments are used, because otherwise the quote and comment characters
   will not show up in the syntax-table.

   Having a syntax table allows new facilities.  The new builtin
   "changesyntax" allows the the user to change the category of any
   character.

   Default '\n' is both ECOMM and SPACE, depending on the context.  To
   solve the problem of quotes and comments that have diffent syntax
   code based on the context, the RQUOTE and ECOMM codes are bit
   masks to add to an ordinary code.  If a character is made a quote it
   will be recognised if the basis code does not have precedence.

   When changing quotes and comment delimiters only the bits are
   removed, and the characters are therefore reverted to its old
   category code.

   The precedence as implemented by next_token () is:

   M4_SYNTAX_IGNORE	*Filtered out below next_token ()
   M4_SYNTAX_ESCAPE	Reads macro name iff set, else next character
   M4_SYNTAX_ALPHA	Reads M4_SYNTAX_ALPHA and M4_SYNTAX_NUM as macro name
   M4_SYNTAX_LQUOTE	Reads all until balanced M4_SYNTAX_RQUOTE
   M4_SYNTAX_BCOMM	Reads all until M4_SYNTAX_ECOMM

   M4_SYNTAX_OTHER  }	Reads all M4_SYNTAX_OTHER, M4_SYNTAX_NUM
   M4_SYNTAX_NUM    }	M4_SYNTAX_DOLLAR, M4_SYNTAX_LBRACE, M4_SYNTAX_RBRACE
   M4_SYNTAX_DOLLAR }
   M4_SYNTAX_LBRACE }
   M4_SYNTAX_RBRACE }

   M4_SYNTAX_SPACE	Reads all M4_SYNTAX_SPACE, depending on buffering
   M4_SYNTAX_ACTIVE	Returns a single char as a macro name

   M4_SYNTAX_OPEN   }	Returned as a single char
   M4_SYNTAX_CLOSE  }
   M4_SYNTAX_COMMA  }

   The $, {, and } are not really a part of m4's input syntax, because a
   a string is parsed equally whether there is a $ or not.  These characters
   are instead used during user macro expansion.

   M4_SYNTAX_RQUOTE and M4_SYNTAX_ECOMM do not start tokens.  */

static bool	check_is_single_quotes		(m4_syntax_table *);
static bool	check_is_single_comments	(m4_syntax_table *);
static bool	check_is_macro_escaped		(m4_syntax_table *);
static int	add_syntax_attribute		(m4_syntax_table *, int, int);
static int	remove_syntax_attribute		(m4_syntax_table *, int, int);

m4_syntax_table *
m4_syntax_create (void)
{
  m4_syntax_table *syntax = xzalloc (sizeof *syntax);
  int ch;

  /* Set up default table.  This table never changes during operation.  */
  for (ch = 256; --ch >= 0;)
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
      case '$':
	syntax->orig[ch] = M4_SYNTAX_DOLLAR;
	break;
      case '{':
	syntax->orig[ch] = M4_SYNTAX_LBRACE;
	break;
      case '}':
	syntax->orig[ch] = M4_SYNTAX_RBRACE;
	break;
      case '`':
	syntax->orig[ch] = M4_SYNTAX_LQUOTE;
	break;
      case '#':
	syntax->orig[ch] = M4_SYNTAX_BCOMM;
	break;
      case '\0':
	/* FIXME - revisit the ignore syntax attribute.  */
	/* syntax->orig[ch] = M4_SYNTAX_IGNORE; */
	/* break; */
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
  m4_set_syntax (syntax, '\0', '\0', NULL);
  return syntax;
}

void
m4_syntax_delete (m4_syntax_table *syntax)
{
  assert (syntax);

  free (syntax->lquote.string);
  free (syntax->rquote.string);
  free (syntax->bcomm.string);
  free (syntax->ecomm.string);
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
    case 'I': case 'i':	code = M4_SYNTAX_IGNORE; break;
    case '@':		code = M4_SYNTAX_ESCAPE; break;
    case 'W': case 'w':	code = M4_SYNTAX_ALPHA;  break;
    case 'L': case 'l':	code = M4_SYNTAX_LQUOTE; break;
    case 'B': case 'b':	code = M4_SYNTAX_BCOMM;  break;
    case 'O': case 'o':	code = M4_SYNTAX_OTHER;  break;
    case 'D': case 'd':	code = M4_SYNTAX_NUM;    break;
    case '$':		code = M4_SYNTAX_DOLLAR; break;
    case '{':		code = M4_SYNTAX_LBRACE; break;
    case '}':		code = M4_SYNTAX_RBRACE; break;
    case 'S': case 's':	code = M4_SYNTAX_SPACE;  break;
    case 'A': case 'a':	code = M4_SYNTAX_ACTIVE; break;
    case '(':		code = M4_SYNTAX_OPEN;   break;
    case ')':		code = M4_SYNTAX_CLOSE;  break;
    case ',':		code = M4_SYNTAX_COMMA;  break;

    case 'R': case 'r':	code = M4_SYNTAX_RQUOTE; break;
    case 'E': case 'e':	code = M4_SYNTAX_ECOMM;  break;

    default: code = -1;  break;
    }

  return code;
}



/* Functions to manipulate the syntax table.  */
static int
add_syntax_attribute (m4_syntax_table *syntax, int ch, int code)
{
  if (code & M4_SYNTAX_MASKS)
    syntax->table[ch] |= code;
  else
    syntax->table[ch] = (syntax->table[ch] & M4_SYNTAX_MASKS) | code;

#ifdef DEBUG_SYNTAX
  fprintf(stderr, "Set syntax %o %c = %04X\n",
	  ch, isprint(ch) ? ch : '-',
	  syntax->table[ch]);
#endif

  return syntax->table[ch];
}

static int
remove_syntax_attribute (m4_syntax_table *syntax, int ch, int code)
{
  assert (code & M4_SYNTAX_MASKS);
  syntax->table[ch] &= ~code;

#ifdef DEBUG_SYNTAX
  fprintf(stderr, "Unset syntax %o %c = %04X\n",
	  ch, isprint(ch) ? ch : '-',
	  syntax->table[ch]);
#endif

  return syntax->table[ch];
}

static void
add_syntax_set (m4_syntax_table *syntax, const char *chars, int code)
{
  int ch;

  if (*chars == '\0')
    return;

  if (code == M4_SYNTAX_ESCAPE)
    syntax->is_macro_escaped = true;

  /* Adding doesn't affect single-quote or single-comment.  */

  while ((ch = to_uchar (*chars++)))
    add_syntax_attribute (syntax, ch, code);
}

static void
subtract_syntax_set (m4_syntax_table *syntax, const char *chars, int code)
{
  int ch;

  if (*chars == '\0')
    return;

  while ((ch = to_uchar (*chars++)))
    {
      if ((code & M4_SYNTAX_MASKS) != 0)
	remove_syntax_attribute (syntax, ch, code);
      else if (m4_has_syntax (syntax, ch, code))
	add_syntax_attribute (syntax, ch, M4_SYNTAX_OTHER);
    }

  /* Check for any cleanup needed.  */
  switch (code)
    {
    case M4_SYNTAX_ESCAPE:
      if (syntax->is_macro_escaped)
	check_is_macro_escaped (syntax);
      break;

    case M4_SYNTAX_LQUOTE:
    case M4_SYNTAX_RQUOTE:
      if (syntax->is_single_quotes)
	check_is_single_quotes (syntax);
      break;

    case M4_SYNTAX_BCOMM:
    case M4_SYNTAX_ECOMM:
      if (syntax->is_single_comments)
	check_is_single_comments (syntax);
      break;

    default:
      break;
    }
}

static void
set_syntax_set (m4_syntax_table *syntax, const char *chars, int code)
{
  int ch;
  /* Explicit set of characters to install with this category; all
     other characters that used to have the category get reset to
     OTHER.  */
  for (ch = 256; --ch >= 0; )
    {
      if (code == M4_SYNTAX_RQUOTE || code == M4_SYNTAX_ECOMM)
	remove_syntax_attribute (syntax, ch, code);
      else if (m4_has_syntax (syntax, ch, code))
	add_syntax_attribute (syntax, ch, M4_SYNTAX_OTHER);
    }
  while ((ch = to_uchar (*chars++)))
    add_syntax_attribute (syntax, ch, code);

  /* Check for any cleanup needed.  */
  check_is_macro_escaped (syntax);
  check_is_single_quotes (syntax);
  check_is_single_comments (syntax);
}

static void
reset_syntax_set (m4_syntax_table *syntax, int code)
{
  int ch;
  for (ch = 256; --ch >= 0; )
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
      else if (syntax->orig[ch] == code || m4_has_syntax (syntax, ch, code))
	add_syntax_attribute (syntax, ch, syntax->orig[ch]);
    }
  check_is_macro_escaped (syntax);
  check_is_single_quotes (syntax);
  check_is_single_comments (syntax);
}

int
m4_set_syntax (m4_syntax_table *syntax, char key, char action,
	       const char *chars)
{
  int code;

  assert (syntax);
  assert (chars || key == '\0');

  if (key == '\0')
    {
      /* Restore the default syntax, which has known quote and comment
	 properties.  */
      memcpy (syntax->table, syntax->orig, sizeof syntax->orig);

      free (syntax->lquote.string);
      free (syntax->rquote.string);
      free (syntax->bcomm.string);
      free (syntax->ecomm.string);

      syntax->lquote.string	= xstrdup (DEF_LQUOTE);
      syntax->lquote.length	= strlen (syntax->lquote.string);
      syntax->rquote.string	= xstrdup (DEF_RQUOTE);
      syntax->rquote.length	= strlen (syntax->rquote.string);
      syntax->bcomm.string	= xstrdup (DEF_BCOMM);
      syntax->bcomm.length	= strlen (syntax->bcomm.string);
      syntax->ecomm.string	= xstrdup (DEF_ECOMM);
      syntax->ecomm.length	= strlen (syntax->ecomm.string);

      add_syntax_attribute (syntax, to_uchar (syntax->rquote.string[0]),
			    M4_SYNTAX_RQUOTE);
      add_syntax_attribute (syntax, to_uchar (syntax->ecomm.string[0]),
			    M4_SYNTAX_ECOMM);

      syntax->is_single_quotes		= true;
      syntax->is_single_comments	= true;
      syntax->is_macro_escaped		= false;
      return 0;
    }

  code = m4_syntax_code (key);
  if (code < 0)
    {
      return -1;
    }
  switch (action)
    {
    case '+':
      add_syntax_set (syntax, chars, code);
      break;
    case '-':
      subtract_syntax_set (syntax, chars, code);
      break;
    case '=':
      set_syntax_set (syntax, chars, code);
      break;
    case '\0':
      reset_syntax_set (syntax, code);
      break;
    default:
      assert (false);
    }
  return code;
}

static bool
check_is_single_quotes (m4_syntax_table *syntax)
{
  int ch;
  int lquote = -1;
  int rquote = -1;

  if (! syntax->is_single_quotes)
    return false;
  assert (syntax->lquote.length == 1 && syntax->rquote.length == 1);

  if (m4_has_syntax (syntax, to_uchar (*syntax->lquote.string),
		     M4_SYNTAX_LQUOTE)
      && m4_has_syntax (syntax, to_uchar (*syntax->rquote.string),
			M4_SYNTAX_RQUOTE))
    return true;

  /* The most recent action invalidated our current lquote/rquote.  If
     we still have exactly one character performing those roles based
     on the syntax table, then update lquote/rquote accordingly.
     Otherwise, keep lquote/rquote, but we no longer have single
     quotes.  */
  for (ch = 256; --ch >= 0; )
    {
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_LQUOTE))
	{
	  if (lquote == -1)
	    lquote = ch;
	  else
	    {
	      syntax->is_single_quotes = false;
	      break;
	    }
	}
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_RQUOTE))
	{
	  if (rquote == -1)
	    rquote = ch;
	  else
	    {
	      syntax->is_single_quotes = false;
	      break;
	    }
	}
    }
  if (lquote == -1 || rquote == -1)
    syntax->is_single_quotes = false;
  else if (syntax->is_single_quotes)
    {
      *syntax->lquote.string = lquote;
      *syntax->rquote.string = rquote;
    }
  return syntax->is_single_quotes;
}

static bool
check_is_single_comments (m4_syntax_table *syntax)
{
  int ch;
  int bcomm = -1;
  int ecomm = -1;

  if (! syntax->is_single_comments)
    return false;
  assert (syntax->bcomm.length == 1 && syntax->ecomm.length == 1);

  if (m4_has_syntax (syntax, to_uchar (*syntax->bcomm.string),
		     M4_SYNTAX_BCOMM)
      && m4_has_syntax (syntax, to_uchar (*syntax->ecomm.string),
			M4_SYNTAX_ECOMM))
    return true;

  /* The most recent action invalidated our current bcomm/ecomm.  If
     we still have exactly one character performing those roles based
     on the syntax table, then update bcomm/ecomm accordingly.
     Otherwise, keep bcomm/ecomm, but we no longer have single
     comments.  */
  for (ch = 256; --ch >= 0; )
    {
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_BCOMM))
	{
	  if (bcomm == -1)
	    bcomm = ch;
	  else
	    {
	      syntax->is_single_comments = false;
	      break;
	    }
	}
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_ECOMM))
	{
	  if (ecomm == -1)
	    ecomm = ch;
	  else
	    {
	      syntax->is_single_comments = false;
	      break;
	    }
	}
    }
  if (bcomm == -1 || ecomm == -1)
    syntax->is_single_comments = false;
  else if (syntax->is_single_comments)
    {
      *syntax->bcomm.string = bcomm;
      *syntax->ecomm.string = ecomm;
    }
  return syntax->is_single_comments;
}

static bool
check_is_macro_escaped (m4_syntax_table *syntax)
{
  int ch;

  syntax->is_macro_escaped = false;
  for (ch = 256; --ch >= 0; )
    if (m4_has_syntax (syntax, ch, M4_SYNTAX_ESCAPE))
      {
	syntax->is_macro_escaped = true;
	break;
      }

  return syntax->is_macro_escaped;
}



/* Functions for setting quotes and comment delimiters.  Used by
   m4_changecom () and m4_changequote ().  Both functions override the
   syntax table to maintain compatibility.  */
void
m4_set_quotes (m4_syntax_table *syntax, const char *lq, const char *rq)
{
  int ch;

  assert (syntax);

  free (syntax->lquote.string);
  free (syntax->rquote.string);

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
      rq = DEF_RQUOTE;
    }
  else if (!rq || (*lq && !*rq))
    rq = DEF_RQUOTE;

  syntax->lquote.string = xstrdup (lq);
  syntax->lquote.length = strlen (syntax->lquote.string);
  syntax->rquote.string = xstrdup (rq);
  syntax->rquote.length = strlen (syntax->rquote.string);

  /* changequote overrides syntax_table, but be careful when it is
     used to select a start-quote sequence that is effectively
     disabled.  */

  syntax->is_single_quotes
    = (syntax->lquote.length == 1 && syntax->rquote.length == 1
       && !m4_has_syntax (syntax, to_uchar (*syntax->lquote.string),
			  (M4_SYNTAX_IGNORE | M4_SYNTAX_ESCAPE
			   | M4_SYNTAX_ALPHA | M4_SYNTAX_NUM)));

  for (ch = 256; --ch >= 0;)
    {
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_LQUOTE))
	add_syntax_attribute (syntax, ch,
			      (syntax->orig[ch] == M4_SYNTAX_LQUOTE
			       ? M4_SYNTAX_OTHER : syntax->orig[ch]));
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_RQUOTE))
	remove_syntax_attribute (syntax, ch, M4_SYNTAX_RQUOTE);
    }

  if (syntax->is_single_quotes)
    {
      add_syntax_attribute (syntax, to_uchar (syntax->lquote.string[0]),
			    M4_SYNTAX_LQUOTE);
      add_syntax_attribute (syntax, to_uchar (syntax->rquote.string[0]),
			    M4_SYNTAX_RQUOTE);
    }

  if (syntax->is_macro_escaped)
    check_is_macro_escaped (syntax);
}

void
m4_set_comment (m4_syntax_table *syntax, const char *bc, const char *ec)
{
  int ch;

  assert (syntax);

  free (syntax->bcomm.string);
  free (syntax->ecomm.string);

  /* POSIX requires no arguments to disable comments, and that one
     argument use newline as the close-comment.  POSIX XCU ERN 131
     states that empty arguments invoke implementation-defined
     behavior.  We allow an empty begin comment to disable comments,
     and a non-empty begin comment will always create a non-empty end
     comment.  See the texinfo for what some other implementations
     do.  */
  if (!bc)
    bc = ec = "";
  else if (!ec || (*bc && !*ec))
    ec = DEF_ECOMM;

  syntax->bcomm.string = xstrdup (bc);
  syntax->bcomm.length = strlen (syntax->bcomm.string);
  syntax->ecomm.string = xstrdup (ec);
  syntax->ecomm.length = strlen (syntax->ecomm.string);

  /* changecom overrides syntax_table, but be careful when it is used
     to select a start-comment sequence that is effectively
     disabled.  */

  syntax->is_single_comments
    = (syntax->bcomm.length == 1 && syntax->ecomm.length == 1
       && !m4_has_syntax (syntax, to_uchar (*syntax->bcomm.string),
			  (M4_SYNTAX_IGNORE | M4_SYNTAX_ESCAPE
			   | M4_SYNTAX_ALPHA | M4_SYNTAX_NUM
			   | M4_SYNTAX_LQUOTE)));

  for (ch = 256; --ch >= 0;)
    {
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_BCOMM))
	add_syntax_attribute (syntax, ch,
			      (syntax->orig[ch] == M4_SYNTAX_BCOMM
			       ? M4_SYNTAX_OTHER : syntax->orig[ch]));
      if (m4_has_syntax (syntax, ch, M4_SYNTAX_ECOMM))
	remove_syntax_attribute (syntax, ch, M4_SYNTAX_ECOMM);
    }
  if (syntax->is_single_comments)
    {
      add_syntax_attribute (syntax, to_uchar (syntax->bcomm.string[0]),
			    M4_SYNTAX_BCOMM);
      add_syntax_attribute (syntax, to_uchar (syntax->ecomm.string[0]),
			    M4_SYNTAX_ECOMM);
    }

  if (syntax->is_macro_escaped)
    check_is_macro_escaped (syntax);
}



/* Define these functions at the end, so that calls in the file use the
   faster macro version from m4module.h.  */
#undef m4_get_syntax_lquote
const char *
m4_get_syntax_lquote (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->lquote.string;
}

#undef m4_get_syntax_rquote
const char *
m4_get_syntax_rquote (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->rquote.string;
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
  return syntax->bcomm.string;
}

#undef m4_get_syntax_ecomm
const char *
m4_get_syntax_ecomm (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->ecomm.string;
}

#undef m4_is_syntax_single_comments
bool
m4_is_syntax_single_comments (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_single_comments;
}

#undef m4_is_syntax_macro_escaped
bool
m4_is_syntax_macro_escaped (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_macro_escaped;
}

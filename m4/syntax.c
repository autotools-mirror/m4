/* GNU m4 -- A simple macro processor
   Copyright 1989, 90, 91, 92, 93, 94, 2002 Free Software Foundation, Inc.

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

#define DEBUG_SYNTAX
#undef DEBUG_SYNTAX

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
   M4_SYNTAX_DOLLAR	*Indicates macro argument in user macros
   M4_SYNTAX_ACTIVE	This character is a macro name by itself
   M4_SYNTAX_ESCAPE	Use this character to prefix all macro names
   M4_SYNTAX_ASSIGN	Used to assign defaults in parameter lists

   M4_SYNTAX_ALPHA	Alphabetic characters (can start macro names)
   M4_SYNTAX_NUM	Numeric characters
   M4_SYNTAX_ALNUM	Alphanumeric characters (can form macro names)

   (These are bit masks)
   M4_SYNTAX_LQUOTE	A single characters left quote
   M4_SYNTAX_RQUOTE	A single characters right quote
   M4_SYNTAX_BCOMM	A single characters begin comment delimiter
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
   code based on the context, the [LR]QUOTE and [BE]COMM codes are bit
   masks to add to an ordinary code.  If a character is made a quote it
   will be recognised if the basis code does not have precedence.

   When changing quotes and comment delimiters only the bits are
   removed, and the characters are therefore reverted to its old
   category code.

   The precedence as implemented by next_token () is:

   M4_SYNTAX_IGNORE	*Filtered out below next_token ()
   M4_SYNTAX_BCOMM	Reads all until M4_SYNTAX_ECOMM
   M4_SYNTAX_ESCAPE	Reads macro name iff set, else next
   M4_SYNTAX_ALPHA	Reads macro name
   M4_SYNTAX_LQUOTE	Reads all until balanced M4_SYNTAX_RQUOTE

   M4_SYNTAX_OTHER  }	Reads all M4_SYNTAX_OTHER, M4_SYNTAX_NUM
   M4_SYNTAX_NUM    }	and M4_SYNTAX_DOLLAR
   M4_SYNTAX_DOLLAR }

   M4_SYNTAX_SPACE	Reads all M4_SYNTAX_SPACE
   M4_SYNTAX_ACTIVE	Returns a single char as a word
   the rest		Returned as a single char

   The $ is not really a part of m4's input syntax in the sense that a
   string is parsed equally whether there is a $ or not.  The character
   $ is used by convention in user macros.  */

static bool check_is_macro_escaped (m4_syntax_table *syntax);
static int add_syntax_attribute	   (m4_syntax_table *syntax, int ch, int code);
static int remove_syntax_attribute (m4_syntax_table *syntax, int ch, int code);

m4_syntax_table *
m4_syntax_create (void)
{
  m4_syntax_table *syntax = XCALLOC (m4_syntax_table, 1);
  int ch;

  for (ch = 256; --ch > 0;)
    {
      if (ch == '(')
	add_syntax_attribute (syntax, ch, M4_SYNTAX_OPEN);
      else if (ch == ')')
	add_syntax_attribute (syntax, ch, M4_SYNTAX_CLOSE);
      else if (ch == ',')
	add_syntax_attribute (syntax, ch, M4_SYNTAX_COMMA);
      else if (ch == '$')
	add_syntax_attribute (syntax, ch, M4_SYNTAX_DOLLAR);
      else if (ch == '=')
	add_syntax_attribute (syntax, ch, M4_SYNTAX_ASSIGN);
      else if (isspace (ch))
	add_syntax_attribute (syntax, ch, M4_SYNTAX_SPACE);
      else if (isalpha (ch) || ch == '_')
	add_syntax_attribute (syntax, ch, M4_SYNTAX_ALPHA);
      else if (isdigit (ch))
	add_syntax_attribute (syntax, ch, M4_SYNTAX_NUM);
      else
	add_syntax_attribute (syntax, ch, M4_SYNTAX_OTHER);
    }
  /* add_syntax_attribute(syntax, 0, M4_SYNTAX_IGNORE); */

  /* Default quotes and comment delimiters are always one char */
  syntax->lquote.string		= xstrdup (DEF_LQUOTE);
  syntax->lquote.length		= strlen (syntax->lquote.string);
  syntax->rquote.string		= xstrdup (DEF_RQUOTE);
  syntax->rquote.length		= strlen (syntax->rquote.string);
  syntax->bcomm.string		= xstrdup (DEF_BCOMM);
  syntax->bcomm.length		= strlen (syntax->bcomm.string);
  syntax->ecomm.string		= xstrdup (DEF_ECOMM);
  syntax->ecomm.length		= strlen (syntax->ecomm.string);

  syntax->is_single_quotes	= true;
  syntax->is_single_comments	= true;
  syntax->is_macro_escaped	= false;

  add_syntax_attribute (syntax, syntax->lquote.string[0], M4_SYNTAX_LQUOTE);
  add_syntax_attribute (syntax, syntax->rquote.string[0], M4_SYNTAX_RQUOTE);
  add_syntax_attribute (syntax, syntax->bcomm.string[0], M4_SYNTAX_BCOMM);
  add_syntax_attribute (syntax, syntax->ecomm.string[0], M4_SYNTAX_ECOMM);

  return syntax;
}

void
m4_syntax_delete (m4_syntax_table *syntax)
{
  assert (syntax);

  XFREE (syntax->lquote.string);
  XFREE (syntax->rquote.string);
  XFREE (syntax->bcomm.string);
  XFREE (syntax->ecomm.string);
  xfree (syntax);
}

int
m4_syntax_code (char ch)
{
  int code;

  switch (ch)
    {
    case 'I': case 'i': code = M4_SYNTAX_IGNORE; break;
    case 'O': case 'o': code = M4_SYNTAX_OTHER;  break;
    case 'S': case 's': code = M4_SYNTAX_SPACE;  break;
    case 'W': case 'w': code = M4_SYNTAX_ALPHA;  break;
    case 'D': case 'd': code = M4_SYNTAX_NUM;    break;

    case '(': code = M4_SYNTAX_OPEN;   break;
    case ')': code = M4_SYNTAX_CLOSE;  break;
    case ',': code = M4_SYNTAX_COMMA;  break;
    case '=': code = M4_SYNTAX_ASSIGN; break;
    case '@': code = M4_SYNTAX_ESCAPE; break;
    case '$': code = M4_SYNTAX_DOLLAR; break;

    case 'L': case 'l': code = M4_SYNTAX_LQUOTE; break;
    case 'R': case 'r': code = M4_SYNTAX_RQUOTE; break;
    case 'B': case 'b': code = M4_SYNTAX_BCOMM;  break;
    case 'E': case 'e': code = M4_SYNTAX_ECOMM;  break;
    case 'A': case 'a': code = M4_SYNTAX_ACTIVE;  break;

    default: code = -1;  break;
    }

  return code;
}



/* Functions for setting quotes and comment delimiters.  Used by
   m4_changecom () and m4_changequote ().  Both functions overrides the
   syntax table to maintain compatibility.  */
void
m4_set_quotes (m4_syntax_table *syntax, const char *lq, const char *rq)
{
  int ch;

  assert (syntax);

  for (ch = 256; --ch >= 0;)	/* changequote overrides syntax_table */
    if (m4_has_syntax (syntax, ch, M4_SYNTAX_LQUOTE|M4_SYNTAX_RQUOTE))
      remove_syntax_attribute (syntax, ch, M4_SYNTAX_LQUOTE|M4_SYNTAX_RQUOTE);

  xfree (syntax->lquote.string);
  xfree (syntax->rquote.string);

  syntax->lquote.string = xstrdup (lq ? lq : DEF_LQUOTE);
  syntax->lquote.length = strlen (syntax->lquote.string);
  syntax->rquote.string = xstrdup (rq ? rq : DEF_RQUOTE);
  syntax->rquote.length = strlen (syntax->rquote.string);

  syntax->is_single_quotes = (syntax->lquote.length == 1
			      && syntax->rquote.length == 1);

  if (syntax->is_single_quotes)
    {
      add_syntax_attribute (syntax, syntax->lquote.string[0], M4_SYNTAX_LQUOTE);
      add_syntax_attribute (syntax, syntax->rquote.string[0], M4_SYNTAX_RQUOTE);
    }

  if (syntax->is_macro_escaped)
    check_is_macro_escaped (syntax);
}

const char *
m4_get_syntax_lquote (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->lquote.string;
}

const char *
m4_get_syntax_rquote (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->rquote.string;
}

void
m4_set_comment (m4_syntax_table *syntax, const char *bc, const char *ec)
{
  int ch;

  assert (syntax);

  for (ch = 256; --ch >= 0;)	/* changecom overrides syntax_table */
    if (m4_has_syntax (syntax, ch, M4_SYNTAX_BCOMM|M4_SYNTAX_ECOMM))
      remove_syntax_attribute (syntax, ch, M4_SYNTAX_BCOMM|M4_SYNTAX_ECOMM);

  xfree (syntax->bcomm.string);
  xfree (syntax->ecomm.string);

  syntax->bcomm.string = xstrdup (bc ? bc : DEF_BCOMM);
  syntax->bcomm.length = strlen (syntax->bcomm.string);
  syntax->ecomm.string = xstrdup (ec ? ec : DEF_ECOMM);
  syntax->ecomm.length = strlen (syntax->ecomm.string);

  syntax->is_single_comments = (syntax->bcomm.length == 1
				&& syntax->ecomm.length == 1);

  if (syntax->is_single_comments)
    {
      add_syntax_attribute (syntax, syntax->bcomm.string[0], M4_SYNTAX_BCOMM);
      add_syntax_attribute (syntax, syntax->ecomm.string[0], M4_SYNTAX_ECOMM);
    }

  if (syntax->is_macro_escaped)
    check_is_macro_escaped (syntax);
}

const char *
m4_get_syntax_bcomm (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->bcomm.string;
}

const char *
m4_get_syntax_ecomm (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->ecomm.string;
}

bool
m4_is_syntax_single_quotes (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_single_quotes;
}

bool
m4_is_syntax_single_comments (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_single_comments;
}

bool
m4_is_syntax_macro_escaped (m4_syntax_table *syntax)
{
  assert (syntax);
  return syntax->is_macro_escaped;
}



/* Functions to manipulate the syntax table.  */
static int
add_syntax_attribute (m4_syntax_table *syntax, int ch, int code)
{
  if (code & M4_SYNTAX_MASKS)
    syntax->table[ch] |= code;
  else
    syntax->table[ch] = code;

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
  if (code & M4_SYNTAX_MASKS)
    syntax->table[ch] &= ~code;

#ifdef DEBUG_SYNTAX
  fprintf(stderr, "Unset syntax %o %c = %04X\n",
	  ch, isprint(ch) ? ch : '-',
	  syntax->table[ch]);
#endif

  return syntax->table[ch];
}

int
m4_set_syntax (m4_syntax_table *syntax, char key, const unsigned char *chars)
{
  int ch, code;

  assert (syntax);

  code = m4_syntax_code (key);

  if ((code < 0) && (key != '\0'))
    {
      return -1;
    }

  if (*chars != '\0')
    while ((ch = *chars++))
      add_syntax_attribute (syntax, ch, code);
  else
    for (ch = 256; --ch > 0; )
      add_syntax_attribute (syntax, ch, code);

  if (syntax->is_macro_escaped || code == M4_SYNTAX_ESCAPE)
    check_is_macro_escaped (syntax);

  return code;
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

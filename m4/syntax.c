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
   M4_SYNTAX_ACTIVE	This caracter is a macro name by itself

   M4_SYNTAX_ESCAPE	Use this character to prefix all macro names
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

   M4_SYNTAX_OTHER	and M4_SYNTAX_NUM
			Reads all M4_SYNTAX_OTHER and M4_SYNTAX_NUM
   M4_SYNTAX_SPACE	Reads all M4_SYNTAX_SPACE
   M4_SYNTAX_ACTIVE	Returns a single char as a word
   the rest		Returned as a single char

   M4_SYNTAX_DOLLAR is not currently used.  The character $ is treated as a
   M4_SYNTAX_OTHER.  It could be done, but it will slow next_token () down
   a bit.  The $ is not really a part of m4's input syntax in the sense
   that a string is parsed equally whether there is a $ or not.  The
   character $ is used by convention in user macros.  */

static	void  check_use_macro_escape	(void);
static	void  set_syntax_internal	(int code, int ch);
static	void  unset_syntax_attribute	(int code, int ch);

/* TRUE iff strlen(rquote) == strlen(lquote) == 1 */
boolean m4__single_quotes;

/* TRUE iff strlen(bcomm) == strlen(ecomm) == 1 */
boolean m4__single_comments;

/* TRUE iff some character has M4_SYNTAX_ESCAPE */
boolean m4__use_macro_escape;

void
m4_syntax_init (void)
{
  int ch;

  for (ch = 256; --ch > 0;)
    {
      if (ch == '(')
	set_syntax_internal (M4_SYNTAX_OPEN, ch);
      else if (ch == ')')
	set_syntax_internal (M4_SYNTAX_CLOSE, ch);
      else if (ch == ',')
	set_syntax_internal (M4_SYNTAX_COMMA, ch);
      else if (isspace (ch))
	set_syntax_internal (M4_SYNTAX_SPACE, ch);
      else if (isalpha (ch) || ch == '_')
	set_syntax_internal (M4_SYNTAX_ALPHA, ch);
      else if (isdigit (ch))
	set_syntax_internal (M4_SYNTAX_NUM, ch);
      else
	set_syntax_internal (M4_SYNTAX_OTHER, ch);
    }
  /* set_syntax_internal(M4_SYNTAX_IGNORE, 0); */

  /* Default quotes and comment delimiters are always one char */
  set_syntax_internal (M4_SYNTAX_LQUOTE, lquote.string[0]);
  set_syntax_internal (M4_SYNTAX_RQUOTE, rquote.string[0]);
  set_syntax_internal (M4_SYNTAX_BCOMM, bcomm.string[0]);
  set_syntax_internal (M4_SYNTAX_ECOMM, ecomm.string[0]);
}

void
m4_syntax_exit (void)
{
  return;
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
    case '@': code = M4_SYNTAX_ESCAPE; break;
#if 0				/* not yet used */
    case '$': code = M4_SYNTAX_DOLLAR; break;
#endif

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
   syntax_table to maintain compatibility.  */
void
m4_set_quotes (const char *lq, const char *rq)
{
  int ch;
  for (ch = 256; --ch >= 0;)	/* changequote overrides syntax_table */
    if (M4_IS_LQUOTE (ch) || M4_IS_RQUOTE (ch))
      unset_syntax_attribute (M4_SYNTAX_LQUOTE | M4_SYNTAX_RQUOTE, ch);

  xfree (lquote.string);
  xfree (rquote.string);

  lquote.string = xstrdup (lq ? lq : DEF_LQUOTE);
  lquote.length = strlen (lquote.string);
  rquote.string = xstrdup (rq ? rq : DEF_RQUOTE);
  rquote.length = strlen (rquote.string);

  m4__single_quotes = (lquote.length == 1 && rquote.length == 1);

  if (m4__single_quotes)
    {
      set_syntax_internal (M4_SYNTAX_LQUOTE, lquote.string[0]);
      set_syntax_internal (M4_SYNTAX_RQUOTE, rquote.string[0]);
    }

  if (m4__use_macro_escape)
    check_use_macro_escape ();
}

void
m4_set_comment (const char *bc, const char *ec)
{
  int ch;
  for (ch = 256; --ch >= 0;)	/* changecom overrides syntax_table */
    if (M4_IS_BCOMM (ch) || M4_IS_ECOMM (ch))
      unset_syntax_attribute (M4_SYNTAX_BCOMM | M4_SYNTAX_ECOMM, ch);

  xfree (bcomm.string);
  xfree (ecomm.string);

  bcomm.string = xstrdup (bc ? bc : DEF_BCOMM);
  bcomm.length = strlen (bcomm.string);
  ecomm.string = xstrdup (ec ? ec : DEF_ECOMM);
  ecomm.length = strlen (ecomm.string);

  m4__single_comments = (bcomm.length == 1 && ecomm.length == 1);

  if (m4__single_comments)
    {
      set_syntax_internal (M4_SYNTAX_BCOMM, bcomm.string[0]);
      set_syntax_internal (M4_SYNTAX_ECOMM, ecomm.string[0]);
    }

  if (m4__use_macro_escape)
    check_use_macro_escape ();
}

/* Functions to manipulate the syntax table.  */
static void
set_syntax_internal (int code, int ch)
{
  if (code & M4_SYNTAX_MASKS)
    m4_syntax_table[ch] |= code;
  else
    m4_syntax_table[ch] = code;

#ifdef DEBUG_SYNTAX
  fprintf(stderr, "Set syntax %o %c = %04X\n",
	  ch, isprint(ch) ? ch : '-',
	  m4_syntax_table[ch]);
#endif
}

static void
unset_syntax_attribute (int code, int ch)
{
  if (code & M4_SYNTAX_MASKS)
    m4_syntax_table[ch] &= ~code;

#ifdef DEBUG_SYNTAX
  fprintf(stderr, "Unset syntax %o %c = %04X\n",
	  ch, isprint(ch) ? ch : '-',
	  m4_syntax_table[ch]);
#endif
}

void
m4_set_syntax (char key, const unsigned char *chars)
{
  int ch, code;

  code = m4_syntax_code (key);

  if ((code < 0) && (key != '\0'))
    {
      M4ERROR ((warning_status, 0,
		_("Undefined syntax code %c"), key));
      return;
    }

  if (*chars != '\0')
    while ((ch = *chars++))
      set_syntax_internal (code, ch);
  else
    for (ch = 256; --ch > 0; )
      set_syntax_internal (code, ch);

  if (m4__use_macro_escape || code == M4_SYNTAX_ESCAPE)
    check_use_macro_escape();
}

static void
check_use_macro_escape (void)
{
  int ch;

  m4__use_macro_escape = FALSE;
  for (ch = 256; --ch >= 0; )
    if (M4_IS_ESCAPE (ch))
      m4__use_macro_escape = TRUE;
}

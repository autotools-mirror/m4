/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

/* This module handles frozen files.  */

#include <config.h>

#include "m4.h"

#include "binary-io.h"

static	void  produce_mem_dump		(FILE *, const char *, size_t);
static	void  produce_resyntax_dump	(m4 *, FILE *);
static	void  produce_syntax_dump	(FILE *, m4_syntax_table *, char);
static	void  produce_module_dump	(FILE *, lt_dlhandle);
static	void  produce_symbol_dump	(m4 *, FILE *, m4_symbol_table *);
static	void *dump_symbol_CB		(m4_symbol_table *, const char *,
					 m4_symbol *, void *);
static	void  issue_expect_message	(m4 *, int);
static	int   decode_char		(FILE *);


/* Dump an ASCII-encoded representation of LEN bytes at MEM to FILE.
   MEM may contain embedded NUL characters.  */
static void
produce_mem_dump (FILE *file, const char *mem, size_t len)
{
  while (len--)
    {
      int ch = to_uchar (*mem++);
      switch (ch)
	{
	case '\a': putc ('\\', file); putc ('a', file); break;
	case '\b': putc ('\\', file); putc ('b', file); break;
	case '\f': putc ('\\', file); putc ('f', file); break;
	case '\n': putc ('\\', file); putc ('n', file); break;
	case '\r': putc ('\\', file); putc ('r', file); break;
	case '\t': putc ('\\', file); putc ('t', file); break;
	case '\v': putc ('\\', file); putc ('v', file); break;
	case '\\': putc ('\\', file); putc ('\\', file); break;
	default:
	  if (ch >= 0x7f || ch < 0x20)
	    {
	      int digit = ch / 16;
	      ch %= 16;
	      digit += digit > 9 ? 'a' - 10 : '0';
	      ch += ch > 9 ? 'a' - 10 : '0';
	      putc ('\\', file);
	      putc ('x', file);
	      putc (digit, file);
	    }
	  putc (ch, file);
	  break;
	}
    }
}


/* Produce the 'R14\nPOSIX_EXTENDED\n' frozen file dump of the current
   default regular expression syntax.  Note that it would be a little
   faster to use the encoded syntax in this format as used by re_compile(),
   but the representation of RE_SYNTAX_POSIX_EXTENDED may change in
   future (or alternative) implementations of re_compile, so we use an
   unencoded representation here.  */

static void
produce_resyntax_dump (m4 *context, FILE *file)
{
  int code = m4_get_regexp_syntax_opt (context);

  /* Don't dump default syntax code (`0' for GNU_EMACS).  */
  if (code)
    {
      const char *resyntax = m4_regexp_syntax_decode (code);

      if (!resyntax)
	m4_error (context, EXIT_FAILURE, 0,
		  _("invalid regexp syntax code `%d'"), code);

      /* No need to use produce_mem_dump, since we know all resyntax
	 names are already ASCII-encoded.  */
      fprintf (file, "R%zu\n%s\n", strlen (resyntax), resyntax);
    }
}

static void
produce_syntax_dump (FILE *file, m4_syntax_table *syntax, char ch)
{
  char buf[256];
  int code = m4_syntax_code (ch);
  int count = 0;
  int i;

  for (i = 0; i < 256; ++i)
    if (m4_has_syntax (syntax, i, code) && code != syntax->orig[i])
      buf[count++] = i;

  /* If code falls in M4_SYNTAX_MASKS, then we must treat it
     specially, since it will not be found in syntax->orig.  */
  if (count == 1
      && ((code == M4_SYNTAX_RQUOTE && *buf == *DEF_RQUOTE)
	  || (code == M4_SYNTAX_ECOMM && *buf == *DEF_ECOMM)))
    return;

  if (count || (code & M4_SYNTAX_MASKS))
    {
      fprintf (file, "S%c%d\n", ch, count);
      produce_mem_dump (file, buf, count);
      fputc ('\n', file);
    }
}

/* The modules must be dumped in the order in which they will be
   reloaded from the frozen file.  libltdl stores handles in a push
   down stack, so we need to dump them in the reverse order to that.  */
static void
produce_module_dump (FILE *file, lt_dlhandle handle)
{
  const char *name = m4_get_module_name (handle);
  size_t len = strlen (name);

  handle = m4__module_next (handle);
  if (handle)
    produce_module_dump (file, handle);

  fprintf (file, "M%zu\n", len);
  produce_mem_dump (file, name, len);
  fputc ('\n', file);
}

/* Process all entries in one bucket, from the last to the first.
   This order ensures that, at reload time, pushdef's will be
   executed with the oldest definitions first.  */
static void
produce_symbol_dump (m4 *context, FILE *file, m4_symbol_table *symtab)
{
  if (m4_symtab_apply (symtab, dump_symbol_CB, file))
    assert (false);
}

static void *
dump_symbol_CB (m4_symbol_table *symtab, const char *symbol_name,
		m4_symbol *symbol, void *userdata)
{
  lt_dlhandle   handle		= SYMBOL_HANDLE (symbol);
  const char   *module_name	= handle ? m4_get_module_name (handle) : NULL;
  FILE *	file		= (FILE *) userdata;
  size_t	symbol_len	= strlen (symbol_name);
  size_t	module_len	= module_name ? strlen (module_name) : 0;

  if (m4_is_symbol_text (symbol))
    {
      const char *text = m4_get_symbol_text (symbol);
      size_t text_len = strlen (text);
      fprintf (file, "T%zu,%zu", symbol_len, text_len);
      if (handle)
	fprintf (file, ",%zu", module_len);
      fputc ('\n', file);

      produce_mem_dump (file, symbol_name, symbol_len);
      produce_mem_dump (file, text, text_len);
      if (handle)
	produce_mem_dump (file, module_name, module_len);
      fputc ('\n', file);
    }
  else if (m4_is_symbol_func (symbol))
    {
      const m4_builtin *bp = m4_get_symbol_builtin (symbol);
      size_t bp_len;
      if (bp == NULL)
	assert (!"INTERNAL ERROR: builtin not found in builtin table!");
      bp_len = strlen (bp->name);

      fprintf (file, "F%zu,%zu", symbol_len, bp_len);
      if (handle)
	fprintf (file, ",%zu", module_len);
      fputc ('\n', file);

      produce_mem_dump (file, symbol_name, symbol_len);
      produce_mem_dump (file, bp->name, bp_len);
      if (handle)
	produce_mem_dump (file, module_name, module_len);
      fputc ('\n', file);
    }
  else if (m4_is_symbol_placeholder (symbol))
    ; /* Nothing to do for a builtin we couldn't reload earlier.  */
  else
    assert (!"INTERNAL ERROR: bad token data type in produce_symbol_dump ()");

  return NULL;
}

/* Produce a frozen state to the given file NAME. */
void
produce_frozen_state (m4 *context, const char *name)
{
  FILE *file = fopen (name, O_BINARY ? "wb" : "w");
  const char *str;

  if (!file)
    {
      m4_error (context, 0, errno, _("cannot open `%s'"), name);
      return;
    }

  /* Write a recognizable header.  */

  fprintf (file, "# This is a frozen state file generated by GNU %s %s\n",
	   PACKAGE, VERSION);
  fputs ("V2\n", file);

  /* Dump quote delimiters.  */

  if (strcmp (m4_get_syntax_lquote (M4SYNTAX), DEF_LQUOTE)
      || strcmp (m4_get_syntax_rquote (M4SYNTAX), DEF_RQUOTE))
    {
      fprintf (file, "Q%zu,%zu\n", M4SYNTAX->lquote.length,
	       M4SYNTAX->rquote.length);
      produce_mem_dump (file, M4SYNTAX->lquote.string,
			M4SYNTAX->lquote.length);
      produce_mem_dump (file, M4SYNTAX->rquote.string,
                        M4SYNTAX->rquote.length);
      fputc ('\n', file);
    }

  /* Dump comment delimiters.  */

  if (strcmp (m4_get_syntax_bcomm (M4SYNTAX), DEF_BCOMM)
      || strcmp (m4_get_syntax_ecomm (M4SYNTAX), DEF_ECOMM))
    {
      fprintf (file, "C%zu,%zu\n", M4SYNTAX->bcomm.length,
	       M4SYNTAX->ecomm.length);
      produce_mem_dump (file, M4SYNTAX->bcomm.string, M4SYNTAX->bcomm.length);
      produce_mem_dump (file, M4SYNTAX->ecomm.string, M4SYNTAX->ecomm.length);
      fputc ('\n', file);
    }

  /* Dump regular expression syntax.  */

  produce_resyntax_dump (context, file);

  /* Dump syntax table.  */

  str = "I@WLBOD${}SA(),RE";
  while (*str)
    produce_syntax_dump (file, M4SYNTAX, *str++);

  /* Dump all loaded modules.  */
  produce_module_dump (file, m4__module_next (0));

  /* Dump all symbols.  */
  produce_symbol_dump (context, file, M4SYMTAB);

  /* Let diversions be issued from output.c module, its cleaner to have this
     piece of code there.  */
  m4_freeze_diversions (context, file);

  /* All done.  */

  fputs ("# End of frozen state file\n", file);
  fclose (file);
}

/* Issue a message saying that some character is an EXPECTED character. */
static void
issue_expect_message (m4 *context, int expected)
{
  if (expected == '\n')
    m4_error (context, EXIT_FAILURE, 0,
	      _("expecting line feed in frozen file"));
  else
    m4_error (context, EXIT_FAILURE, 0,
	      _("expecting character `%c' in frozen file"), expected);
}


/* Reload frozen state.  */

/* Read the next character from the IN stream.  Various escape
   sequences are converted, and returned.  EOF is returned if the end
   of file is reached whilst reading the character, or on an
   unrecognized escape sequence.  */

static int
decode_char (FILE *in)
{
  int ch = getc (in);
  int next;
  int value = 0;

  if (ch == '\\')
    {
      ch = getc (in);
      switch (ch)
	{
	case 'a': return '\a';
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	case '\\': return '\\';

	case 'x': case 'X':
	  next = getc (in);
	  if (next >= '0' && next <= '9')
	    ch = (next - '0') * 16;
	  else if (next >= 'a' && next <= 'f')
	    ch = (next - 'a' + 10) * 16;
	  else if (next >= 'A' && next <= 'F')
	    ch = (next - 'A' + 10) * 16;
	  else
	    return EOF;
	  next = getc (in);
	  if (next >= '0' && next <= '9')
	    ch += next - '0';
	  else if (next >= 'a' && next <= 'f')
	    ch += next - 'a' + 10;
	  else if (next >= 'A' && next <= 'F')
	    ch += next - 'A' + 10;
	  else
	    return EOF;
	  return ch;
	case '0': case '1': case '2': case '3':
	  value = ch - '0';
	  ch = getc (in);
	  /* fall through */
	case '4': case '5': case '6': case '7':
	  if (ch >= '0' && ch <= '7')
	    {
	      value = value * 8 + ch - '0';
	      ch = getc (in);
	    }
	  else
	    {
	      ungetc (ch, in);
	      return value;
	    }
	  if (ch >= '0' && ch <= '7')
	    value = value * 8 + ch - '0';
	  else
	    ungetc (ch, in);
	  return value;

	default:
	  return EOF;
	}
    }

  return ch;
}


/*  Reload state from the given file NAME.  We are seeking speed,
    here.  */

void
reload_frozen_state (m4 *context, const char *name)
{
  FILE *file;
  int version;
  int character;
  int operation;
  char syntax;
  char *string[3];
  size_t allocated[3];
  int number[3] = {0};

#define GET_CHARACTER						\
  (character = getc (file))

#define GET_NUMBER(Number)					\
  do								\
    {								\
      (Number) = 0;						\
      while (isdigit (character))				\
	{							\
	  (Number) = 10 * (Number) + character - '0';		\
	  GET_CHARACTER;					\
	}							\
    }								\
  while (0)

#define GET_STRING(File, Buf, BufSize, StrLen)			\
  do								\
    {								\
      size_t len = (StrLen);					\
      char *p;							\
      CHECK_ALLOCATION ((Buf), (BufSize), len);			\
      p = (Buf);						\
      while (len-- > 0)						\
	{							\
	  int ch = (version > 1 ? decode_char (File)		\
		    : getc (File));				\
	  if (ch == EOF)					\
	    m4_error (context, EXIT_FAILURE, 0,			\
		      _("premature end of frozen file"));	\
	  *p++ = ch;						\
	}							\
      *p = '\0';						\
    }								\
  while (0)

#define VALIDATE(Expected)					\
  do								\
    {								\
      if (character != (Expected))				\
	issue_expect_message (context, (Expected));		\
    }								\
  while (0)

#define CHECK_ALLOCATION(Where, Allocated, Needed)		\
  do								\
    {								\
      if ((Needed) + 1 > (Allocated))				\
	{							\
	  free (Where);						\
	  (Allocated) = (Needed) + 1;				\
	  (Where) = xmalloc (Allocated);			\
	}							\
    }								\
  while (0)

  /* Skip comments (`#' at beginning of line) and blank lines, setting
     character to the next directive or to EOF.  */

#define GET_DIRECTIVE						\
  do								\
    {								\
      GET_CHARACTER;						\
      if (character == '#')					\
	{							\
	  while (character != EOF && character != '\n')		\
	    GET_CHARACTER;					\
	  VALIDATE ('\n');					\
	}							\
    }								\
  while (character == '\n')

  file = m4_path_search (context, name, (char **)NULL);
  if (file == NULL)
    m4_error (context, EXIT_FAILURE, errno, _("cannot open `%s'"), name);

  allocated[0] = 100;
  string[0] = xmalloc (allocated[0]);
  allocated[1] = 100;
  string[1] = xmalloc (allocated[1]);
  allocated[2] = 100;
  string[2] = xmalloc (allocated[2]);

  /* Validate format version.  Accept both `1' (m4 1.3 and 1.4.x) and
     `2' (m4 2.0).  */
  GET_DIRECTIVE;
  VALIDATE ('V');
  GET_CHARACTER;
  GET_NUMBER (version);
  switch (version)
    {
    case 2:
      break;
    case 1:
      m4__module_open (context, "m4", NULL);
      if (m4_get_posixly_correct_opt (context))
	m4__module_open (context, "traditional", NULL);
      else
	m4__module_open (context, "gnu", NULL);
      /* Disable { and } categories, since ${11} was not supported in
         1.4.x.  */
      m4_set_syntax (M4SYNTAX, 'O', '+', "{}");
      break;
    default:
      if (version > 2)
	m4_error (context, EXIT_MISMATCH, 0,
		  _("frozen file version %d greater than max supported of 2"),
		  version);
      else
	m4_error (context, EXIT_FAILURE, 0,
		  _("ill-formed frozen file, version directive expected"));
    }
  VALIDATE ('\n');

  GET_DIRECTIVE;
  while (character != EOF)
    {
      switch (character)
	{
	default:
	  m4_error (context, EXIT_FAILURE, 0,
		    _("ill-formed frozen file, unknown directive %c"),
		    character);

	case 'F':
	  GET_CHARACTER;

	  /* Get string lengths. */

	  GET_NUMBER (number[0]);
	  VALIDATE (',');
	  GET_CHARACTER;
	  GET_NUMBER (number[1]);

	  if (character == ',')
	    {
	      if (version > 1)
		{
		  /* 'F' operator accepts an optional third argument for
		     format versions 2 or later.  */
		  GET_CHARACTER;
		  GET_NUMBER (number[2]);
		}
	      else
		/* 3 argument 'F' operations are invalid for format
		   version 1.  */
		m4_error (context, EXIT_FAILURE, 0, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'F');
	    }
	  else
	    {
	      number[2] = 0;
	    }

	  VALIDATE ('\n');


	  /* Get string contents.  */

	  GET_STRING (file, string[0], allocated[0], number[0]);
	  GET_STRING (file, string[1], allocated[1], number[1]);
	  GET_STRING (file, string[2], allocated[2], number[2]);
	  VALIDATE ('\n');

	  /* Enter a macro having a builtin function as a definition.  */
	  {
	    lt_dlhandle handle   = 0;
	    m4_symbol_value *token;

	    if (number[2] > 0)
	      handle = m4__module_find (string[2]);
	    token = m4_builtin_find_by_name (handle, string[1]);

	    if (token == NULL)
	      {
		token = xzalloc (sizeof *token);
		m4_set_symbol_value_placeholder (token, xstrdup (string[1]));
		VALUE_HANDLE (token) = handle;
		VALUE_MIN_ARGS (token) = 0;
		VALUE_MAX_ARGS (token) = -1;
	      }
	    m4_symbol_pushdef (M4SYMTAB, string[0], token);
	  }
	  break;

	case 'M':

	  /* Load a module, but *without* perturbing the symbol table.
	     Note that any expansion from loading the module which would
	     have been seen when loading it originally is discarded
	     when loading it from a frozen file. */

	  if (version < 2)
	    {
	      /* 'M' operator is not supported in format version 1. */
	      m4_error (context, EXIT_FAILURE, 0, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'M');
	    }

	  GET_CHARACTER;
	  GET_NUMBER (number[0]);
	  VALIDATE ('\n');
	  GET_STRING (file, string[0], allocated[0], number[0]);
	  VALIDATE ('\n');

	  m4__module_open (context, string[0], NULL);

	  break;

	case 'R':

	  if (version < 2)
	    {
	      /* 'R' operator is not supported in format version 1. */
	      m4_error (context, EXIT_FAILURE, 0, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'R');
	    }

	  GET_CHARACTER;
	  GET_NUMBER (number[0]);
	  VALIDATE ('\n');
	  GET_STRING (file, string[0], allocated[0], number[0]);
	  VALIDATE ('\n');

	  m4_set_regexp_syntax_opt (context,
				    m4_regexp_syntax_encode (string[0]));
	  if (m4_get_regexp_syntax_opt (context) < 0)
	    {
	      m4_error (context, EXIT_FAILURE, 0,
			_("unknown regexp syntax code `%s'"), string[0]);
	    }

	  break;

	case 'S':

	  if (version < 2)
	    {
	      /* 'S' operator is not supported in format version 1. */
	      m4_error (context, EXIT_FAILURE, 0, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'S');
	    }

	  GET_CHARACTER;
	  syntax = character;
	  GET_CHARACTER;
	  GET_NUMBER (number[0]);
	  VALIDATE ('\n');
	  GET_STRING (file, string[0], allocated[0], number[0]);

	  /* Syntax under M4_SYNTAX_MASKS is handled specially; all
	     other characters are additive.  */
	  if ((m4_set_syntax (M4SYNTAX, syntax,
			      (m4_syntax_code (syntax) & M4_SYNTAX_MASKS
			       ? '=' : '+'), string[0]) < 0)
	      && (syntax != '\0'))
	    {
	      m4_error (context, 0, 0,
			_("undefined syntax code %c"), syntax);
	    }
	  break;

	case 'C':
	case 'D':
	case 'Q':
	  operation = character;
	  GET_CHARACTER;

	  /* Get string lengths. */

	  if (operation == 'D' && character == '-')
	    {
	      /* Accept a negative diversion number.  */
	      GET_CHARACTER;
	      GET_NUMBER (number[0]);
	      number[0] = -number[0];
	    }
	  else
	    GET_NUMBER (number[0]);
	  VALIDATE (',');
	  GET_CHARACTER;
	  GET_NUMBER (number[1]);
	  VALIDATE ('\n');

	  /* Get string contents.  */
	  if (operation != 'D')
	    GET_STRING (file, string[0], allocated[0], number[0]);
	  GET_STRING (file, string[1], allocated[1], number[1]);
	  GET_CHARACTER;
	  VALIDATE ('\n');

	  /* Act according to operation letter.  */

	  switch (operation)
	    {
	    case 'C':

	      /* Change comment strings.  */

	      m4_set_comment (M4SYNTAX, string[0], string[1]);
	      break;

	    case 'D':

	      /* Select a diversion and add a string to it.  */

	      m4_make_diversion (context, number[0]);
	      if (number[1] > 0)
		m4_output_text (context, string[1], number[1]);
	      break;

	    case 'Q':

	      /* Change quote strings.  */

	      m4_set_quotes (M4SYNTAX, string[0], string[1]);
	      break;

	    default:

	      /* Cannot happen.  */

	      break;
	    }
	  break;

	case 'T':
	  GET_CHARACTER;

	  /* Get string lengths. */

	  GET_NUMBER (number[0]);
	  VALIDATE (',');
	  GET_CHARACTER;
	  GET_NUMBER (number[1]);

	  if (character == ',')
	    {
	      if (version > 1)
		{
		  /* 'T' operator accepts an optional third argument for
		     format versions 2 or later.  */
		  GET_CHARACTER;
		  GET_NUMBER (number[2]);
		}
	      else
		{
		  /* 3 argument 'T' operations are invalid for format
		     version 1.  */
		  m4_error (context, EXIT_FAILURE, 0, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'T');
		}
	    }
	  else
	    number[2] = 0;

	  VALIDATE ('\n');

	  /* Get string contents.  */
	  GET_STRING (file, string[0], allocated[0], number[0]);
	  GET_STRING (file, string[1], allocated[1], number[1]);
	  GET_STRING (file, string[2], allocated[2], number[2]);
	  VALIDATE ('\n');

	  /* Enter a macro having an expansion text as a definition.  */
	  {
	    m4_symbol_value *token = xzalloc (sizeof *token);
	    lt_dlhandle handle = 0;

	    if (number[2] > 0)
	      handle = m4__module_find (string[2]);

	    m4_set_symbol_value_text (token, xstrdup (string[1]));
	    VALUE_HANDLE (token)		= handle;
	    VALUE_MAX_ARGS (token)	= -1;

	    m4_symbol_pushdef (M4SYMTAB, string[0], token);
	  }
	  break;

	}
      GET_DIRECTIVE;
    }

  free (string[0]);
  free (string[1]);
  free (string[2]);
  fclose (file);

#undef GET_STRING
#undef GET_CHARACTER
#undef GET_NUMBER
#undef VALIDATE
#undef CHECK_ALLOCATION
#undef GET_DIRECTIVE
}

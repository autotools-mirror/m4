/* GNU m4 -- A simple macro processor
   Copyright 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.

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

/* This module handles frozen files.  */

#include "m4/system.h"
#include "m4.h"
#include "m4private.h"

static	int   decode_char	   (FILE *in);
static	void  issue_expect_message (int expected);
static	int   produce_char_dump    (char *buf, int ch);
static	void  produce_syntax_dump  (FILE *file, char ch, int mask);
static	void  produce_module_dump  (FILE *file, lt_dlhandle handle);
static	void  produce_symbol_dump  (FILE *file, m4_hash *hash);


/* Produce a frozen state to the given file NAME. */
static int
produce_char_dump (char *buf, int ch)
{
  char *p = buf;
  int digit;

  if (ch > 127 || ch < 32)
    {
      *p++  = '\\';

      digit = ch / 64;
      ch    = ch - (64 * digit);
      *p++  = digit + '0';

      digit = ch / 8;
      ch    = ch - (8 * digit);
      *p++  = digit + '0';

      *p++  = ch + '0';
    }
  else
    {
      switch (ch)
	{
	case '\\':
	  *p++ = '\\';
	  *p++ = '\\';
	  break;

	default:
	  *p++ = ch;
	  break;
	}
    }
  *p = '\0';

  return strlen (buf);
}

#define MAX_CHAR_LENGTH 4	/* '\377' -> 4 characters */

static void
produce_syntax_dump (FILE *file, char ch, int mask)
{
  char buf[1+ MAX_CHAR_LENGTH * sizeof (m4_syntax_table)];
  int code = m4_syntax_code (ch);
  int count = 0;
  int offset = 0;
  int i;

  /* FIXME:  Can't set the syntax of '\000' since that character marks
             the end of a string, and when passed to `set_syntax', tells
	     it to set the syntax of every table entry. */

  for (i = 1; i < 256; ++i)
    {
      if ((mask && ((m4_syntax_table[i] & mask) == code))
	  || (!mask && ((m4_syntax_table[i] & code) == code)))
	{
	  offset += produce_char_dump (buf + offset, i);
	  ++count;
	}
    }

  if (offset)
      fprintf (file, "S%c%d\n%s\n", ch, count, buf);
}

/* The modules must be dumped in the order in which they will be
   reloaded from the frozen file.  libltdl stores handles in a push
   down stack, so we need to dump them in the reverse order to that.  */
void
produce_module_dump (FILE *file, lt_dlhandle handle)
{
  lt_dlhandle pending = handle;
  const char *name = m4_module_name (pending);

  handle = lt_dlhandle_next (handle);
  if (handle)
    produce_module_dump (file, handle);

  fprintf (file, "M%lu\n", (unsigned long) strlen (name));
  fputs (name, file);
  fputc ('\n', file);
}

/* Process all entries in one bucket, from the last to the first.
   This order ensures that, at reload time, pushdef's will be
   executed with the oldest definitions first.  */
void
produce_symbol_dump (FILE *file, m4_hash *hash)
{
  m4_hash_iterator *place	= 0;

  while ((place = m4_hash_iterator_next (hash, place)))
    {
      const char   *symbol_name	= (const char *) m4_hash_iterator_key (place);
      m4_symbol	   *symbol	= m4_hash_iterator_value (place);
      lt_dlhandle   handle	= SYMBOL_HANDLE (symbol);
      const char   *module_name	= handle ? m4_module_name (handle) : NULL;
      const m4_builtin *bp;

      switch (SYMBOL_TYPE (symbol))
	{
	case M4_TOKEN_TEXT:
	  fprintf (file, "T%lu,%lu",
		   (unsigned long) strlen (symbol_name),
		   (unsigned long) strlen (SYMBOL_TEXT (symbol)));
	  if (handle)
	    fprintf (file, ",%lu", (unsigned long) strlen (module_name));
	  fputc ('\n', file);

	  fputs (symbol_name, file);
	  fputs (SYMBOL_TEXT (symbol), file);
	  if (handle)
	    fputs (module_name, file);
	  fputc ('\n', file);
	  break;

	case M4_TOKEN_FUNC:
	  bp = m4_builtin_find_by_func
	    	(m4_module_builtins (SYMBOL_HANDLE (symbol)),
		 SYMBOL_FUNC (symbol));

	  if (bp == NULL)
	    {
	      M4ERROR ((warning_status, 0,
			"INTERNAL ERROR: Builtin not found in builtin table!"));
	      abort ();
	    }

	  fprintf (file, "F%lu,%lu",
		   (unsigned long) strlen (symbol_name),
		   (unsigned long) strlen (bp->name));

	  if (handle)
	    fprintf (file, ",%lu",
		     (unsigned long) strlen (module_name));
	  fputc ('\n', file);

	  fputs (symbol_name, file);
	  fputs (bp->name, file);
	  if (handle)
	    fputs (module_name, file);
	  fputc ('\n', file);
	  break;

	default:
	  M4ERROR ((warning_status, 0,
		    "INTERNAL ERROR: Bad token data type in produce_symbol_dump ()"));
	  abort ();
	  break;
	}
    }
}

void
produce_frozen_state (m4 *context, const char *name)
{
  FILE *file;

  if (file = fopen (name, "w"), !file)
    {
      M4ERROR ((warning_status, errno, name));
      return;
    }

  /* Write a recognizable header.  */

  fprintf (file, "# This is a frozen state file generated by GNU %s %s\n",
	   PACKAGE, VERSION);
  fprintf (file, "V2\n");

  /* Dump quote delimiters.  */

  if (strcmp (lquote.string, DEF_LQUOTE)
      || strcmp (rquote.string, DEF_RQUOTE))
    {
      fprintf (file, "Q%lu,%lu\n",
	       (unsigned long) lquote.length,
	       (unsigned long) rquote.length);
      fputs (lquote.string, file);
      fputs (rquote.string, file);
      fputc ('\n', file);
    }

  /* Dump comment delimiters.  */

  if (strcmp (bcomm.string, DEF_BCOMM) || strcmp (ecomm.string, DEF_ECOMM))
    {
      fprintf (file, "C%lu,%lu\n",
	       (unsigned long) bcomm.length,
	       (unsigned long) ecomm.length);
      fputs (bcomm.string, file);
      fputs (ecomm.string, file);
      fputc ('\n', file);
    }

  /* Dump syntax table. */

  produce_syntax_dump (file, 'I', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, 'S', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, '(', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, ')', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, ',', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, '$', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, 'A', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, '@', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, 'O', M4_SYNTAX_VALUE);

  produce_syntax_dump (file, 'W', M4_SYNTAX_VALUE);
  produce_syntax_dump (file, 'D', M4_SYNTAX_VALUE);

  produce_syntax_dump (file, 'L', 0);
  produce_syntax_dump (file, 'R', 0);
  produce_syntax_dump (file, 'B', 0);
  produce_syntax_dump (file, 'E', 0);

  /* Dump all loaded modules.  */
  produce_module_dump (file, lt_dlhandle_next (0));

  /* Dump all symbols.  */
  produce_symbol_dump (file, M4SYMTAB);

  /* Let diversions be issued from output.c module, its cleaner to have this
     piece of code there.  */
  m4_freeze_diversions (file);

  /* All done.  */

  fputs ("# End of frozen state file\n", file);
  fclose (file);
}

/* Issue a message saying that some character is an EXPECTED character. */
static void
issue_expect_message (int expected)
{
  if (expected == '\n')
    M4ERROR ((EXIT_FAILURE, 0, _("Expecting line feed in frozen file")));
  else
    M4ERROR ((EXIT_FAILURE, 0, _("Expecting character `%c' in frozen file"),
	      expected));
}

/* Reload a frozen state from the given file NAME. */

/* Read the next character from the IN stream.  Octal characters of the
   form ``\nnn'' are converted, and returned.  EOF is returned if the end
   of file is reached whilst reading the character.  */

static int
decode_char (FILE *in)
{
  int ch = fgetc (in);

  if (ch == '\\')
    {
      ch = fgetc (in);
      if ((ch != EOF) && (ch != '\\'))
	{
	  int next;

	  /* first octal digit */
	  ch  = (ch - '0') * 64;

	  /* second octal digit */
	  next = fgetc (in);
	  if (next == EOF)
	    ch = EOF;
	  else
	    ch += 8 * (next - '0');

	  /* third octal digit */
	  next = fgetc (in);
	  if (next == EOF)
	    ch = EOF;
	  else
	    ch += (next - '0');
	}
    }

  return ch;
}


/* We are seeking speed, here.  */

void
reload_frozen_state (m4 *context, const char *name)
{
  FILE *file;
  int version;
  int character;
  int operation;
  char syntax;
  unsigned char *string[3];
  int allocated[3];
  int number[3];
  const m4_builtin *bp;

#define GET_CHARACTER \
  (character = getc (file))

#define GET_NUMBER(Number) \
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

#define GET_STRING(File, Buf, BufSize, StrLen) \
  do								\
    {								\
      CHECK_ALLOCATION((Buf), (BufSize), (StrLen));		\
      if ((StrLen) > 0)						\
	if (!fread ((Buf), (size_t) (StrLen), 1, (File)))	\
	    M4ERROR ((EXIT_FAILURE, 0, 				\
                      _("Premature end of frozen file")));	\
      (Buf)[(StrLen)] = '\0';					\
    }								\
  while (0)

#define VALIDATE(Expected) \
  do								\
    {								\
      if (character != (Expected))				\
	issue_expect_message ((Expected));			\
    }								\
  while (0)

#define CHECK_ALLOCATION(Where, Allocated, Needed) \
  do								\
    {								\
      if ((Needed) + 1 > (Allocated))				\
        {							\
	  free (Where);						\
	  (Allocated) = (Needed) + 1;				\
	  (Where) = xmalloc ((size_t) (Allocated));		\
	}							\
    }								\
  while (0)

  file = m4_path_search (name, (char **)NULL);
  if (file == NULL)
    M4ERROR ((EXIT_FAILURE, errno, _("Cannot open %s"), name));

  allocated[0] = 100;
  string[0] = xmalloc ((size_t) allocated[0]);
  allocated[1] = 100;
  string[1] = xmalloc ((size_t) allocated[1]);
  allocated[2] = 100;
  string[2] = xmalloc ((size_t) allocated[2]);

  while (GET_CHARACTER, character != EOF)
    switch (character)
      {
      default:
	M4ERROR ((EXIT_FAILURE, 0, _("Ill-formated frozen file")));

      case '\n':

	/* Skip empty lines.  */

	break;

      case '#':

	/* Comments are introduced by `#' at beginning of line, and are
	   ignored.  */

	while (character != EOF && character != '\n')
	  GET_CHARACTER;
	VALIDATE ('\n');
	break;

      case 'F':
	GET_CHARACTER;

	/* Get string lengths. */

	GET_NUMBER (number[0]);
	VALIDATE (',');
	GET_CHARACTER;
	GET_NUMBER (number[1]);

	if ((character == ',') && (version > 1))
	  {
	    /* 'F' operator accepts an optional third argument for
	       format versions 2 or later.  */
	    GET_CHARACTER;
	    GET_NUMBER (number[2]);
	  }
	else if (version > 1)
	  {
	    number[2] = 0;
	  }
	else
	  {
	    /* 3 argument 'F' operations are invalid for format version 1.  */
	    M4ERROR ((EXIT_FAILURE, 0, _("Ill-formated frozen file")));
	  }

	VALIDATE ('\n');


	/* Get string contents.  */

	GET_STRING (file, string[0], allocated[0], number[0]);
	GET_STRING (file, string[1], allocated[1], number[1]);
	if ((number[2] > 0)  && (version > 1))
	  GET_STRING (file, string[2], allocated[2], number[2]);
	VALIDATE ('\n');

	/* Enter a macro having a builtin function as a definition.  */
	{
	  lt_dlhandle handle = 0;
	  m4_builtin *bt = NULL;

	  if (number[2] > 0)
	    {
	      while ((handle = lt_dlhandle_next (handle)))
		if (strcmp (m4_module_name (handle), string[2]) == 0)
		  break;

	      if (handle)
		{
		  bt = m4_module_builtins (handle);
		}
	    }

	  if (bt)
	    bp = m4_builtin_find_by_name (bt, string[1]);

	  if (bp)
	    {
	      m4_token token;
	      int flags = 0;

	      if (bp->groks_macro_args)
		BIT_SET (flags, TOKEN_MACRO_ARGS_BIT);
	      if (bp->blind_if_no_args)
		BIT_SET (flags, TOKEN_BLIND_ARGS_BIT);

	      bzero (&token, sizeof (m4_token));
	      TOKEN_TYPE (&token)	= M4_TOKEN_FUNC;
	      TOKEN_FUNC (&token)	= bp->func;
	      TOKEN_HANDLE (&token)	= handle;
	      TOKEN_FLAGS (&token)	= flags;
	      TOKEN_MIN_ARGS (&token)	= bp->min_args;
	      TOKEN_MAX_ARGS (&token)	= bp->max_args;

	      m4_builtin_pushdef (context, string[0], &token);
	    }
	  else
	    M4ERROR ((warning_status, 0,
		      _("`%s' from frozen file not found in builtin table!"),
		      string[0]));
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
	    M4ERROR ((EXIT_FAILURE, 0, _("Ill-formated frozen file")));
	  }

	GET_CHARACTER;
	GET_NUMBER (number[0]);
	VALIDATE ('\n');
	GET_STRING (file, string[0], allocated[0], number[0]);
	VALIDATE ('\n');

	m4__module_open (context, string[0], NULL);

	break;

      case 'S':

	if (version < 2)
	  {
	    /* 'S' operator is not supported in format version 1. */
	    M4ERROR ((EXIT_FAILURE, 0, _("Ill-formated frozen file")));
	  }

	GET_CHARACTER;
	syntax = character;
	GET_CHARACTER;
	GET_NUMBER (number[0]);
	VALIDATE ('\n');

	CHECK_ALLOCATION(string[0], allocated[0], number[0]);
	if (number[0] > 0)
	  {
	    int i;

	    for (i = 0; i < number[0]; ++i)
	      {
		int ch = decode_char (file);

		if (ch < 0)
		  M4ERROR ((EXIT_FAILURE, 0,
			    _("Premature end of frozen file")));

		string[0][i] = (unsigned char) ch;
	      }
	  }
	string[0][number[0]] = '\0';

	m4_set_syntax (syntax, string[0]);
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

	    m4_set_comment (string[0], string[1]);
	    break;

	  case 'D':

	    /* Select a diversion and add a string to it.  */

	    m4_make_diversion (number[0]);
	    if (number[1] > 0)
	      m4_shipout_text (NULL, string[1], number[1]);
	    break;

	  case 'Q':

	    /* Change quote strings.  */

	    m4_set_quotes (string[0], string[1]);
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

	if ((character == ',') && (version > 1))
	  {
	    /* 'T' operator accepts an optional third argument for
	       format versions 2 or later.  */
	    GET_CHARACTER;
	    GET_NUMBER (number[2]);
	  }
	else if (version > 1)
	  {
	    number[2] = 0;
	  }
	else
	  {
	    /* 3 argument 'T' operations are invalid for format version 1.  */
	    M4ERROR ((EXIT_FAILURE, 0, _("Ill-formated frozen file")));
	  }

	VALIDATE ('\n');

	/* Get string contents.  */
	GET_STRING (file, string[0], allocated[0], number[0]);
	GET_STRING (file, string[1], allocated[1], number[1]);
	if ((number[2] > 0)  && (version > 1))
	  GET_STRING (file, string[2], allocated[2], number[2]);
	VALIDATE ('\n');

	/* Enter a macro having an expansion text as a definition.  */
	{
	  m4_token token;
	  lt_dlhandle handle = 0;

	  if (number[2] > 0)
	    while ((handle = lt_dlhandle_next (handle)))
	      if (strcmp (m4_module_name (handle), string[2]) == 0)
		break;

	  bzero (&token, sizeof (m4_token));
	  TOKEN_TYPE (&token)		= M4_TOKEN_TEXT;
	  TOKEN_TEXT (&token)		= string[1];
	  TOKEN_HANDLE (&token)		= handle;
	  TOKEN_MAX_ARGS (&token)	= -1;

	  m4_macro_pushdef (context, string[0], &token);
	}
	break;

      case 'V':

	/* Validate and save format version.  Only `1' and `2'
	   are acceptable for now.  */

	GET_CHARACTER;
	version = character - '0';
	if ((version < 1) || (version > 2))
	    issue_expect_message ('2');
	GET_CHARACTER;
	VALIDATE ('\n');
	break;

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
}

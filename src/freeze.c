/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.

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
static	void  produce_syntax_dump  (FILE *file, m4_syntax_table *syntax,
				    char ch);
static	void  produce_module_dump  (FILE *file, lt_dlhandle handle);
static	void  produce_symbol_dump  (m4 *context, FILE *file,
				    m4_symbol_table *symtab);
static	void *dump_symbol_CB	   (m4_symbol_table *symtab,
				    const char *symbol_name, m4_symbol *symbol,
				    void *userdata);


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
produce_syntax_dump (FILE *file, m4_syntax_table *syntax, char ch)
{
  char buf[1+ MAX_CHAR_LENGTH * sizeof (m4_syntax_table)];
  int code = m4_syntax_code (ch);
  int count = 0;
  int offset = 0;
  int i;

  /* FIXME:  Can't set the syntax of '\000' since that character marks
             the end of a string, and when passed to `m4_set_syntax', tells
	     it to set the syntax of every table entry. */

  for (i = 1; i < 256; ++i)
    {
      if (m4_has_syntax (syntax, i, code))
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
  const char *name = m4_get_module_name (pending);

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
produce_symbol_dump (m4 *context, FILE *file, m4_symbol_table *symtab)
{
  const char *errormsg = m4_symtab_apply (symtab, dump_symbol_CB, file);

  if (errormsg)
    {
      M4ERROR ((m4_get_warning_status_opt (context), 0, errormsg));
      abort ();
    }
}

static void *
dump_symbol_CB (m4_symbol_table *symtab, const char *symbol_name, m4_symbol *symbol,
		void *userdata)
{
  lt_dlhandle   handle		= SYMBOL_HANDLE (symbol);
  const char   *module_name	= handle ? m4_get_module_name (handle) : NULL;
  FILE *	file		= (FILE *) userdata;

  if (m4_is_symbol_text (symbol))
    {
      fprintf (file, "T%lu,%lu",
	       (unsigned long) strlen (symbol_name),
	       (unsigned long) strlen (m4_get_symbol_text (symbol)));
      if (handle)
	fprintf (file, ",%lu", (unsigned long) strlen (module_name));
      fputc ('\n', file);

      fputs (symbol_name, file);
      fputs (m4_get_symbol_text (symbol), file);
      if (handle)
	fputs (module_name, file);
      fputc ('\n', file);
    }
  else if (m4_is_symbol_func (symbol))
    {
      const m4_builtin *bp = m4_builtin_find_by_func (SYMBOL_HANDLE (symbol),
						m4_get_symbol_func (symbol));

      if (bp == NULL)
	return "INTERNAL ERROR: Builtin not found in builtin table!";

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
    }
  else
    return "INTERNAL ERROR: Bad token data type in produce_symbol_dump ()";

  return NULL;
}

void
produce_frozen_state (m4 *context, const char *name)
{
  FILE *file;

  if (file = fopen (name, "w"), !file)
    {
      M4ERROR ((m4_get_warning_status_opt (context), errno, name));
      return;
    }

  /* Write a recognizable header.  */

  fprintf (file, "# This is a frozen state file generated by GNU %s %s\n",
	   PACKAGE, VERSION);
  fprintf (file, "V2\n");

  /* Dump quote delimiters.  */

  if (strcmp (m4_get_syntax_lquote (M4SYNTAX), DEF_LQUOTE)
      || strcmp (m4_get_syntax_rquote (M4SYNTAX), DEF_RQUOTE))
    {
      fprintf (file, "Q%lu,%lu\n",
	       (unsigned long) context->syntax->lquote.length,
	       (unsigned long) context->syntax->rquote.length);
      fputs (context->syntax->lquote.string, file);
      fputs (context->syntax->rquote.string, file);
      fputc ('\n', file);
    }

  /* Dump comment delimiters.  */

  if (strcmp (m4_get_syntax_bcomm (M4SYNTAX), DEF_BCOMM)
      || strcmp (m4_get_syntax_ecomm (M4SYNTAX), DEF_ECOMM))
    {
      fprintf (file, "C%lu,%lu\n",
	       (unsigned long) context->syntax->bcomm.length,
	       (unsigned long) context->syntax->ecomm.length);
      fputs (context->syntax->bcomm.string, file);
      fputs (context->syntax->ecomm.string, file);
      fputc ('\n', file);
    }

  /* Dump syntax table. */

  produce_syntax_dump (file, M4SYNTAX, 'I');
  produce_syntax_dump (file, M4SYNTAX, 'S');
  produce_syntax_dump (file, M4SYNTAX, '(');
  produce_syntax_dump (file, M4SYNTAX, ')');
  produce_syntax_dump (file, M4SYNTAX, ',');
  produce_syntax_dump (file, M4SYNTAX, '$');
  produce_syntax_dump (file, M4SYNTAX, 'A');
  produce_syntax_dump (file, M4SYNTAX, '@');
  produce_syntax_dump (file, M4SYNTAX, 'O');

  produce_syntax_dump (file, M4SYNTAX, 'W');
  produce_syntax_dump (file, M4SYNTAX, 'D');

  produce_syntax_dump (file, M4SYNTAX, 'L');
  produce_syntax_dump (file, M4SYNTAX, 'R');
  produce_syntax_dump (file, M4SYNTAX, 'B');
  produce_syntax_dump (file, M4SYNTAX, 'E');

  /* Dump all loaded modules.  */
  produce_module_dump (file, lt_dlhandle_next (0));

  /* Dump all symbols.  */
  produce_symbol_dump (context, file, M4SYMTAB);

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

  file = m4_path_search (context, name, (char **)NULL);
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
	  lt_dlhandle handle   = 0;

	  if (number[2] > 0)
	    handle = lt_dlhandle_find (string[2]);

	  if (handle)
	    bp = m4_builtin_find_by_name (handle, string[1]);

	  if (bp)
	    {
	      m4_symbol_value *token = XCALLOC (m4_symbol_value, 1);

	      if (bp->groks_macro_args)
		BIT_SET (VALUE_FLAGS (token), VALUE_MACRO_ARGS_BIT);
	      if (bp->blind_if_no_args)
		BIT_SET (VALUE_FLAGS (token), VALUE_BLIND_ARGS_BIT);

	      m4_set_symbol_value_func (token, bp->func);
	      VALUE_HANDLE (token)	= handle;
	      VALUE_MIN_ARGS (token)	= bp->min_args;
	      VALUE_MAX_ARGS (token)	= bp->max_args;

	      m4_symbol_pushdef (M4SYMTAB, string[0], token);
	    }
	  else
	    M4ERROR ((m4_get_warning_status_opt (context), 0,
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

	if ((m4_set_syntax (context->syntax, syntax, string[0]) < 0)
	    && (syntax != '\0'))
	  {
	    M4ERROR ((m4_get_warning_status_opt (context), 0,
		      _("Undefined syntax code %c"), syntax));
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

	    m4_make_diversion (number[0]);
	    if (number[1] > 0)
	      m4_shipout_text (context, NULL, string[1], number[1]);
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
	  m4_symbol_value *token = XCALLOC (m4_symbol_value, 1);
	  lt_dlhandle handle = 0;

	  if (number[2] > 0)
	    handle = lt_dlhandle_find (string[2]);

	  m4_set_symbol_value_text (token, xstrdup (string[1]));
	  VALUE_HANDLE (token)		= handle;
	  VALUE_MAX_ARGS (token)	= -1;

	  m4_symbol_pushdef (M4SYMTAB, string[0], token);
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

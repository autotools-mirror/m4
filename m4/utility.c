/* GNU m4 -- A simple macro processor
   Copyright 1989-1994, 1998-1999 Free Software Foundation, Inc.

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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "m4.h"
#include "m4private.h"

static int dumpdef_cmp (const void *s1, const void *s2);

/* The name this program was run with. */
const char *program_name;

/* Exit code from last "syscmd" command.  */
int m4_sysval = 0;

/* Operate interactively (-e).  */
int interactive = 0;

/* Enable sync output for /lib/cpp (-s).  */
int sync_output = 0;

/* Debug (-d[flags]).  */
int debug_level = 0;

/* Disable GNU extensions (-G).  */
int no_gnu_extensions = 0;

/* Prefix all builtin functions by `m4_'.  */
int prefix_all_builtins = 0;

/* Max length of arguments in trace output (-lsize).  */
int max_debug_argument_length = 0;

/* Suppress warnings about missing arguments.  */
int suppress_warnings = 0;

/* If not zero, then value of exit status for warning diagnostics.  */
int warning_status = 0;

/* Artificial limit for expansion_level in macro.c.  */
int nesting_limit = 250;

/* If nonzero, comments are discarded in the token parser.  */
int discard_comments = 0;

/* input syntax table. */
unsigned short m4_syntax_table[256];

/* Quote chars.  */
m4_string rquote;
m4_string lquote;

/* Comment chars.  */
m4_string bcomm;
m4_string ecomm;


/* Addressable function versions of the macros defined in m4private.h.
   Since they are functions the caller does not need access to the
   internal data structure, so they are safe to export for use in
   external modules.  */
m4_token_t
m4_token_type (m4_token *name)
{
  return TOKEN_TYPE (name);
}

char *
m4_token_text (m4_token *name)
{
  return TOKEN_TEXT (name);
}

m4_builtin_func *
m4_token_func (m4_token *name)
{
  return TOKEN_FUNC (name);
}



/* Give friendly warnings if a builtin macro is passed an inappropriate
   number of arguments.  NAME is macro name for messages, ARGC is actual
   number of arguments, MIN is the minimum number of acceptable arguments,
   negative if not applicable, MAX is the maximum number, negative if not
   applicable.  */
boolean
m4_bad_argc (m4_token *token, int argc, int min, int max)
{
  if (min > 0 && argc < min)
    {
      M4WARN ((warning_status, 0,
	       _("Warning: %s: too few arguments: %d < %d"),
	       TOKEN_TEXT (token), argc, min));
      return TRUE;
    }

  if (max > 0 && argc > max)
    {
      M4WARN ((warning_status, 0,
	       _("Warning: %s: too many arguments (ignored): %d > %d"),
	       TOKEN_TEXT (token), argc, max));
      /* Return FALSE, otherwise it is not exactly `ignored'. */
      return FALSE;
    }

  return FALSE;
}

const char *
m4_skip_space (const char *arg)
{
  while (M4_IS_SPACE(*arg))
    arg++;
  return arg;
}

/* The function m4_numeric_arg () converts ARG to an int pointed to by
   VALUEP. If the conversion fails, print error message for macro MACRO.
   Return TRUE iff conversion succeeds.  */
boolean
m4_numeric_arg (m4_token *macro, const char *arg, int *valuep)
{
  char *endp;

  if (*arg == 0 || (*valuep = strtol (m4_skip_space (arg), &endp, 10),
		    *m4_skip_space (endp) != 0))
    {
      M4WARN ((warning_status, 0,
	       _("Warning: %s: non-numeric argument: %s"),
	       TOKEN_TEXT (macro), arg));
      return FALSE;
    }
  return TRUE;
}


/* Print ARGC arguments from the table ARGV to obstack OBS, separated by
   SEP, and quoted by the current quotes, if QUOTED is TRUE.  */
void
m4_dump_args (struct obstack *obs, int argc, m4_token **argv,
	      const char *sep, boolean quoted)
{
  int i;
  size_t len = strlen (sep);

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
	obstack_grow (obs, sep, len);

      m4_shipout_string (obs, M4ARG (i), 0, quoted);
    }
}

/* For "translit", ranges are allowed in the second and third argument.
   They are expanded in the following function, and the expanded strings,
   without any ranges left, are used to translate the characters of the
   first argument.  A single - (dash) can be included in the strings by
   being the first or the last character in the string.  If the first
   character in a range is after the first in the character set, the range
   is made backwards, thus 9-0 is the string 9876543210.  */
const char *
m4_expand_ranges (const char *s, struct obstack *obs)
{
  char from;
  char to;

  for (from = '\0'; *s != '\0'; from = *s++)
    {
      if (*s == '-' && from != '\0')
	{
	  to = *++s;
	  if (to == '\0')
	    {
              /* trailing dash */
              obstack_1grow (obs, '-');
              break;
	    }
	  else if (from <= to)
	    {
	      while (from++ < to)
		obstack_1grow (obs, from);
	    }
	  else
	    {
	      while (--from >= to)
		obstack_1grow (obs, from);
	    }
	}
      else
	obstack_1grow (obs, *s);
    }
  obstack_1grow (obs, '\0');
  return obstack_finish (obs);
}

/* qsort comparison routine, for sorting the table made in m4_dumpdef ().  */
static int
dumpdef_cmp (const void *s1, const void *s2)
{
  return strcmp (*(const char **) s1, *(const char **) s2);
}

/* The function dump_symbol () is for use by "dumpdef".  It builds up a
   table of all defined symbol names.  */
int
m4_dump_symbol (const char *name, m4_symbol *symbol, void *data)
{
  if (SYMBOL_TYPE (symbol) != M4_TOKEN_VOID)
    {
      struct m4_dump_symbol_data *symbol_data
	= (struct m4_dump_symbol_data *) data;

      obstack_blank (symbol_data->obs, sizeof (const char *));
      symbol_data->base = (const char **) obstack_base (symbol_data->obs);
      symbol_data->base[symbol_data->size++] = name;
    }

  return 0;
}

/* If there are no arguments, build a sorted list of all defined
   symbols, otherwise, only the specified symbols.  */
void
m4_dump_symbols (struct m4_dump_symbol_data *data, int argc,
		 m4_token **argv, boolean complain)
{
  data->base = (const char **) obstack_base (data->obs);
  data->size = 0;

  if (argc == 1)
    {
      m4_symtab_apply (m4_dump_symbol, data);
    }
  else
    {
      int i;
      m4_symbol *symbol;

      for (i = 1; i < argc; i++)
	{
	  symbol = m4_symbol_lookup (M4ARG (i));
	  if (symbol != NULL && SYMBOL_TYPE (symbol) != M4_TOKEN_VOID)
	    m4_dump_symbol (M4ARG (i), symbol, data);
	  else if (complain)
	    M4WARN ((warning_status, 0,
		     _("Warning: %s: undefined name: %s"),
		     M4ARG (0), M4ARG (i)));
	}
    }

  obstack_finish (data->obs);
  qsort ((void *) data->base, data->size, sizeof (const char *), dumpdef_cmp);
}

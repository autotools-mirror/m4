/* GNU m4 -- A simple macro processor
   Copyright 2000 Free Software Foundation, Inc.
  
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

#include "m4module.h"
#include "m4private.h"

#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifndef errno
M4_SCOPE int errno;
#endif

#include "regex.h"

#include "format.c"

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	gnu_LTX_m4_builtin_table
#define m4_macro_table		gnu_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind */
#define builtin_functions			\
	BUILTIN(__file__,	FALSE,	FALSE )	\
	BUILTIN(__line__,	FALSE,	FALSE )	\
	BUILTIN(builtin,	FALSE,	TRUE  )	\
	BUILTIN(changesyntax,	FALSE,	TRUE  )	\
	BUILTIN(debugmode,	FALSE,	FALSE )	\
	BUILTIN(debugfile,	FALSE,	FALSE )	\
	BUILTIN(esyscmd,	FALSE,	TRUE  )	\
	BUILTIN(format,		FALSE,	TRUE  )	\
	BUILTIN(indir,		FALSE,	FALSE )	\
	BUILTIN(patsubst,	FALSE,	TRUE  )	\
	BUILTIN(regexp,		FALSE,	TRUE  )	\
	BUILTIN(symbols,	FALSE,	FALSE )	\
	BUILTIN(syncoutput,	FALSE,  TRUE  )	\


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind)	M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind },
  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE },
};


/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  /* name			text */
#ifdef _WIN32
  { "__windows__",		"" },
#else
  { "__unix__",			"" },
#endif
  { "__gnu__",			"" },
  { "__m4_version__",		VERSION },

  { 0, 0 },
};

static void substitute M4_PARAMS((struct obstack *obs, const char *victim, const char *repl, struct re_registers *regs));


/*---------------------------------------------------------------------.
| The builtin "builtin" allows calls to builtin macros, even if their  |
| definition has been overridden or shadowed.  It is thus possible to  |
| redefine builtins, and still access their original definition.       |
`---------------------------------------------------------------------*/

M4BUILTIN_HANDLER (builtin)
{
  const m4_builtin *bp = NULL;
  const char *name = M4ARG (1);

  if (m4_bad_argc (argv[0], argc, 2, -1))
    return;

  bp = m4_builtin_find_by_name (NULL, name);

  if (bp == NULL)
    M4ERROR ((warning_status, 0,
	      _("Undefined name %s"), name));
  else
    (*bp->func) (obs, argc - 1, argv + 1);
}


/*------------------------------------------------------------------------.
| The builtin "indir" allows indirect calls to macros, even if their name |
| is not a proper macro name.  It is thus possible to define macros with  |
| ill-formed names for internal use in larger macro packages.  This macro |
| is not available in compatibility mode.				  |
`------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (indir)
{
  m4_symbol *symbol;
  const char *name = M4ARG (1);

  if (m4_bad_argc (argv[0], argc, 1, -1))
    return;

  symbol = m4_lookup_symbol (name, M4_SYMBOL_LOOKUP);
  if (symbol == NULL)
    M4ERROR ((warning_status, 0,
	      _("Undefined name `%s'"), name));
  else
    m4_call_macro (symbol, argc - 1, argv + 1, obs);
}

/*-------------------------------------------------------------------.
| Change the current input syntax.  The function set_syntax () lives |
| in input.c.  For compability reasons, this function is not called, |
| if not followed by an SYNTAX_OPEN.  Also, any changes to comment   |
| delimiters and quotes made here will be overridden by a call to    |
| `changecom' or `changequote'.					     |
`-------------------------------------------------------------------*/

M4BUILTIN_HANDLER (changesyntax)
{
  int i;

  if (m4_bad_argc (argv[0], argc, 1, -1))
    return;

  for (i = 1; i < argc; i++)
    {
      m4_set_syntax (*M4_TOKEN_DATA_TEXT (argv[i]),
		     m4_expand_ranges (M4_TOKEN_DATA_TEXT (argv[i])+1, obs));
    }
}

/*----------------------------------------------------------------------.
| On-the-fly control of the format of the tracing output.  It takes one |
| argument, which is a character string like given to the -d option, or |
| none in which case the debug_level is zeroed.			        |
`----------------------------------------------------------------------*/

M4BUILTIN_HANDLER (debugmode)
{
  int new_debug_level;
  int change_flag;

  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 1)
    debug_level = 0;
  else
    {
      if (M4ARG (1)[0] == '+' || M4ARG (1)[0] == '-')
	{
	  change_flag = M4ARG (1)[0];
	  new_debug_level = m4_debug_decode (M4ARG (1) + 1);
	}
      else
	{
	  change_flag = 0;
	  new_debug_level = m4_debug_decode (M4ARG (1));
	}

      if (new_debug_level < 0)
	M4ERROR ((warning_status, 0,
		  _("Debugmode: bad debug flags: `%s'"), M4ARG (1)));
      else
	{
	  switch (change_flag)
	    {
	    case 0:
	      debug_level = new_debug_level;
	      break;

	    case '+':
	      debug_level |= new_debug_level;
	      break;

	    case '-':
	      debug_level &= ~new_debug_level;
	      break;
	    }
	}
    }
}

/*-------------------------------------------------------------------------.
| Specify the destination of the debugging output.  With one argument, the |
| argument is taken as a file name, with no arguments, revert to stderr.   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (debugfile)
{
  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 1)
    m4_debug_set_output (NULL);
  else if (!m4_debug_set_output (M4ARG (1)))
    M4ERROR ((warning_status, errno,
	      _("Cannot set error file: %s"), M4ARG (1)));
}

/*--------------------------------------------------------------------------.
| Regular expression version of index.  Given two arguments, expand to the  |
| index of the first match of the second argument (a regexp) in the first.  |
| Expand to -1 if here is no match.  Given a third argument, is changes	    |
| the expansion to this argument.					    |
`--------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (regexp)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */
  const char *repl;		/* replacement string */

  struct re_pattern_buffer buf;	/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  const char *msg;		/* error message from re_compile_pattern */
  int startpos;			/* start position of match */
  int length;			/* length of first argument */

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  victim = M4_TOKEN_DATA_TEXT (argv[1]);
  regexp = M4_TOKEN_DATA_TEXT (argv[2]);

  buf.buffer = NULL;
  buf.allocated = 0;
  buf.fastmap = NULL;
  buf.translate = NULL;
  msg = re_compile_pattern (regexp, strlen (regexp), &buf);

  if (msg != NULL)
    {
      M4ERROR ((warning_status, 0,
		_("Bad regular expression `%s': %s"), regexp, msg));
      return;
    }

  length = strlen (victim);
  startpos = re_search (&buf, victim, length, 0, length, &regs);
  xfree (buf.buffer);

  if (startpos  == -2)
    {
      M4ERROR ((warning_status, 0,
		_("Error matching regular expression `%s'"), regexp));
      return;
    }

  if (argc == 3)
    m4_shipout_int (obs, startpos);
  else if (startpos >= 0)
    {
      repl = M4_TOKEN_DATA_TEXT (argv[3]);
      substitute (obs, victim, repl, &regs);
    }

  return;
}

/*--------------------------------------------------------------------------.
| Substitute all matches of a regexp occuring in a string.  Each match of   |
| the second argument (a regexp) in the first argument is changed to the    |
| third argument, with \& substituted by the matched text, and \N	    |
| substituted by the text matched by the Nth parenthesized sub-expression.  |
`--------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (patsubst)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */

  struct re_pattern_buffer buf;	/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  const char *msg;		/* error message from re_compile_pattern */
  int matchpos;			/* start position of match */
  int offset;			/* current match offset */
  int length;			/* length of first argument */

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  regexp = M4_TOKEN_DATA_TEXT (argv[2]);

  buf.buffer = NULL;
  buf.allocated = 0;
  buf.fastmap = NULL;
  buf.translate = NULL;
  msg = re_compile_pattern (regexp, strlen (regexp), &buf);

  if (msg != NULL)
    {
      M4ERROR ((warning_status, 0,
		_("Bad regular expression `%s': %s"), regexp, msg));
      if (buf.buffer != NULL)
	xfree (buf.buffer);
      return;
    }

  victim = M4_TOKEN_DATA_TEXT (argv[1]);
  length = strlen (victim);

  offset = 0;
  matchpos = 0;
  while (offset < length)
    {
      matchpos = re_search (&buf, victim, length,
			    offset, length - offset, &regs);
      if (matchpos < 0)
	{

	  /* Match failed -- either error or there is no match in the
	     rest of the string, in which case the rest of the string is
	     copied verbatim.  */

	  if (matchpos == -2)
	    M4ERROR ((warning_status, 0,
		      _("Error matching regular expression `%s'"), regexp));
	  else if (offset < length)
	    obstack_grow (obs, victim + offset, length - offset);
	  break;
	}

      /* Copy the part of the string that was skipped by re_search ().  */

      if (matchpos > offset)
	obstack_grow (obs, victim + offset, matchpos - offset);

      /* Handle the part of the string that was covered by the match.  */

      substitute (obs, victim, M4ARG (3), &regs);

      /* Update the offset to the end of the match.  If the regexp
	 matched a null string, advance offset one more, to avoid
	 infinite loops.  */

      offset = regs.end[0];
      if (regs.start[0] == regs.end[0])
	obstack_1grow (obs, victim[offset++]);
    }
  obstack_1grow (obs, '\0');

  xfree (buf.buffer);
  return;
}

/*-------------------------------------------------------------------------.
| Implementation of "symbols" itself.  It builds up a table of pointers to |
| symbols, sorts it and ships out the symbols name.			   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (symbols)
{
  struct m4_dump_symbol_data data;
  struct obstack data_obs;

  obstack_init (&data_obs);
  data.obs = &data_obs;
  m4_dump_symbols (&data, argc, argv, FALSE);

  for (; data.size > 0; --data.size, data.base++)
    {
      m4_shipout_string (obs, SYMBOL_NAME (data.base[0]), 0, TRUE);
      if (data.size > 1)
	obstack_1grow (obs, ',');
    }
  obstack_free (&data_obs, NULL);
}


/*------------------------------------------------------------------------.
| This contains macro which implements syncoutput() which takes one arg   |
|   1, on, yes - turn on sync lines                                       |
|   0, off, no - turn off sync lines                                      |
|   everything else is silently ignored                                   |
|                                                                         |
`------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (syncoutput)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  if (M4_TOKEN_DATA_TYPE (argv[1]) != M4_TOKEN_TEXT)
    return;

  if (M4_TOKEN_DATA_TEXT(argv[1])[0] == '0'
      || M4_TOKEN_DATA_TEXT(argv[1])[0] == 'n'
      || (M4_TOKEN_DATA_TEXT(argv[1])[0] == 'o'
	  && M4_TOKEN_DATA_TEXT(argv[1])[1] == 'f'))
    sync_output = 0;
  else if (M4_TOKEN_DATA_TEXT(argv[1])[0] == '1'
	   || M4_TOKEN_DATA_TEXT(argv[1])[0] == 'y'
	   || (M4_TOKEN_DATA_TEXT(argv[1])[0] == 'o'
	       && M4_TOKEN_DATA_TEXT(argv[1])[1] == 'n'))
    sync_output = 1;
}

M4BUILTIN_HANDLER (esyscmd)
{
  FILE *pin;
  int ch;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  m4_debug_flush_files ();
  pin = popen (M4ARG (1), "r");
  if (pin == NULL)
    {
      M4ERROR ((warning_status, errno,
		_("Cannot open pipe to command `%s'"), M4ARG (1)));
      m4_sysval = 0xff << 8;
    }
  else
    {
      while ((ch = getc (pin)) != EOF)
	obstack_1grow (obs, (char) ch);
      m4_sysval = pclose (pin);
    }
}

/*----------------------------------------------------------------------.
| Frontend for printf like formatting.  The function format () lives in |
| the file format.c.						        |
`----------------------------------------------------------------------*/

M4BUILTIN_HANDLER (format)
{
  format (obs, argc - 1, argv + 1);
}

M4BUILTIN_HANDLER (__file__)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_string (obs, m4_current_file, 0, TRUE);
}

M4BUILTIN_HANDLER (__line__)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;
  m4_shipout_int (obs, m4_current_line);
}

/*-------------------------------------------------------------------------.
| Function to perform substitution by regular expressions.  Used by the	   |
| builtins regexp and patsubst.  The changed text is placed on the	   |
| obstack.  The substitution is REPL, with \& substituted by this part of  |
| VICTIM matched by the last whole regular expression, taken from REGS[0], |
| and \N substituted by the text matched by the Nth parenthesized	   |
| sub-expression, taken from REGS[N].					   |
`-------------------------------------------------------------------------*/

static int substitute_warned = 0;

static void
substitute (struct obstack *obs, const char *victim, const char *repl,
	    struct re_registers *regs)
{
  register unsigned int ch;

  for (;;)
    {
      while ((ch = *repl++) != '\\')
	{
	  if (ch == '\0')
	    return;
	  obstack_1grow (obs, ch);
	}

      switch ((ch = *repl++))
	{
	case '0':
	  if (!substitute_warned)
	    {
	      M4ERROR ((warning_status, 0, _("\
WARNING: \\0 will disappear, use \\& instead in replacements")));
	      substitute_warned = 1;
	    }
	  /* Fall through.  */

	case '&':
	  obstack_grow (obs, victim + regs->start[0],
			regs->end[0] - regs->start[0]);
	  break;

	case '1': case '2': case '3': case '4': case '5': case '6':
	case '7': case '8': case '9': 
	  ch -= '0';
	  if (regs->end[ch] > 0)
	    obstack_grow (obs, victim + regs->start[ch],
			  regs->end[ch] - regs->start[ch]);
	  break;

	default:
	  obstack_1grow (obs, ch);
	  break;
	}
    }
}

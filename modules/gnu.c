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

#include <ctype.h>

#if HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#include <m4module.h>

#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifndef errno
int errno;
#endif

#ifdef NDEBUG
#  include "m4private.h"
#endif

#include "regex.h"

#define RE_SYNTAX_BRE RE_SYNTAX_EMACS

#define RE_SYNTAX_ERE \
  (/* Allow char classes. */					\
    RE_CHAR_CLASSES						\
  /* Anchors are OK in groups. */				\
  | RE_CONTEXT_INDEP_ANCHORS					\
  /* Be picky, `/^?/', for instance, makes no sense. */		\
  | RE_CONTEXT_INVALID_OPS					\
  /* Allow intervals with `{' and `}', forbid invalid ranges. */\
  | RE_INTERVALS | RE_NO_BK_BRACES | RE_NO_EMPTY_RANGES		\
  /* `(' and `)' are the grouping operators. */			\
  | RE_NO_BK_PARENS						\
  /* `|' is the alternation. */					\
  | RE_NO_BK_VBAR)

#include "format.c"


/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	gnu_LTX_m4_builtin_table
#define m4_macro_table		gnu_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind argmin  argmax */
#define builtin_functions			\
	BUILTIN(__file__,	FALSE,	FALSE,	1,	1  )	\
	BUILTIN(__line__,	FALSE,	FALSE,	1,	1  )	\
	BUILTIN(builtin,	FALSE,	TRUE,	2,	-1 )	\
	BUILTIN(changesyntax,	FALSE,	TRUE,	1,	-1 )	\
	BUILTIN(debugmode,	FALSE,	FALSE,	1,	2  )	\
	BUILTIN(debugfile,	FALSE,	FALSE,	1,	2  )	\
	BUILTIN(eregexp,	FALSE,	TRUE,	3,	4  )	\
	BUILTIN(epatsubst,	FALSE,	TRUE,	3,	4  )	\
	BUILTIN(esyscmd,	FALSE,	TRUE,	2,	2  )	\
	BUILTIN(format,		FALSE,	TRUE,	2,	-1 )	\
	BUILTIN(indir,		FALSE,	TRUE,	2,	-1 )	\
	BUILTIN(patsubst,	FALSE,	TRUE,	3,	4  )	\
	BUILTIN(regexp,		FALSE,	TRUE,	3,	4  )	\
	BUILTIN(symbols,	FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(syncoutput,	FALSE,  TRUE,	2,	2  )	\


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind, min, max)  M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, min, max)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind, min, max },
  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE, 0, 0 },
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
  { "__m4_version__",		VERSION/**/TIMESTAMP },

  { 0, 0 },
};

static void substitute (m4 *context, struct obstack *obs, const char *victim,
			const char *repl, struct re_registers *regs);
static void m4_patsubst_do (m4 *context, struct obstack *obs, int argc,
			    m4_symbol_value **argv, int syntax);


/* The builtin "builtin" allows calls to builtin macros, even if their
   definition has been overridden or shadowed.  It is thus possible to
   redefine builtins, and still access their original definition.  */

/**
 * builtin(MACRO, [...])
 **/
M4BUILTIN_HANDLER (builtin)
{
  const m4_builtin *bp = NULL;
  const char *name = M4ARG (1);

  bp = m4_builtin_find_by_name (NULL, name);

  if (bp == NULL)
    M4ERROR ((m4_get_warning_status_opt (context), 0,
	      _("Undefined name %s"), name));
  else
    (*bp->func) (context, obs, argc - 1, argv + 1);
}


/* The builtin "indir" allows indirect calls to macros, even if their name
   is not a proper macro name.  It is thus possible to define macros with
   ill-formed names for internal use in larger macro packages.  This macro
   is not available in compatibility mode.  */

/**
 * indir(MACRO, [...])
 **/
M4BUILTIN_HANDLER (indir)
{
  m4_symbol *symbol;
  const char *name = M4ARG (1);

  symbol = m4_symbol_lookup (M4SYMTAB, name);
  if (symbol == NULL)
    M4ERROR ((m4_get_warning_status_opt (context), 0,
	      _("Undefined name `%s'"), name));
  else
    m4_call_macro (context, symbol, obs, argc - 1, argv + 1);
}

/* Change the current input syntax.  The function set_syntax () lives
   in input.c.  For compability reasons, this function is not called,
   if not followed by a` SYNTAX_OPEN.  Also, any changes to comment
   delimiters and quotes made here will be overridden by a call to
   `changecom' or `changequote'.  */

/**
 * changesyntax(SYNTAX-SPEC, ...)
 **/
M4BUILTIN_HANDLER (changesyntax)
{
  int i;

  for (i = 1; i < argc; i++)
    {
      char key = *M4ARG (i);
      if ((m4_set_syntax (M4SYNTAX, key,
			  m4_expand_ranges (M4ARG (i)+1, obs)) < 0)
	  && (key != '\0'))
	{
	  M4ERROR ((m4_get_warning_status_opt (context), 0,
		    _("Undefined syntax code %c"), key));
	}
    }
}

/* On-the-fly control of the format of the tracing output.  It takes one
   argument, which is a character string like given to the -d option, or
   none in which case the debug_level is zeroed.  */

/**
 * debugmode([FLAGS])
 **/
M4BUILTIN_HANDLER (debugmode)
{
  int debug_level = m4_get_debug_level_opt (context);
  int new_debug_level;
  int change_flag;

  if (argc == 1)
    m4_set_debug_level_opt (context, 0);
  else
    {
      if (M4ARG (1)[0] == '+' || M4ARG (1)[0] == '-')
	{
	  change_flag = M4ARG (1)[0];
	  new_debug_level = m4_debug_decode (context, M4ARG (1) + 1);
	}
      else
	{
	  change_flag = 0;
	  new_debug_level = m4_debug_decode (context, M4ARG (1));
	}

      if (new_debug_level < 0)
	M4ERROR ((m4_get_warning_status_opt (context), 0,
		  _("Debugmode: bad debug flags: `%s'"), M4ARG (1)));
      else
	{
	  switch (change_flag)
	    {
	    case 0:
	      m4_set_debug_level_opt (context, new_debug_level);
	      break;

	    case '+':
	      m4_set_debug_level_opt (context, debug_level | new_debug_level);
	      break;

	    case '-':
	      m4_set_debug_level_opt (context, debug_level & ~new_debug_level);
	      break;
	    }
	}
    }
}

/* Specify the destination of the debugging output.  With one argument, the
   argument is taken as a file name, with no arguments, revert to stderr.  */

/**
 * debugfile([FILENAME])
 **/
M4BUILTIN_HANDLER (debugfile)
{
  if (argc == 1)
    m4_debug_set_output (context, NULL);
  else if (!m4_debug_set_output (context, M4ARG (1)))
    M4ERROR ((m4_get_warning_status_opt (context), errno,
	      _("Cannot set error file: %s"), M4ARG (1)));
}


/* Compile a REGEXP using the Regex SYNTAX bits return the buffer.
   Report errors on behalf of CALLER.  */

static struct re_pattern_buffer *
m4_regexp_compile (m4 *context, const char *caller,
		   const char *regexp, int syntax)
{
  static struct re_pattern_buffer buf;	/* compiled regular expression */
  static boolean buf_initialized = FALSE;
  const char *msg;		/* error message from re_compile_pattern */

  if (!buf_initialized)
    {
      buf_initialized = TRUE;
      buf.buffer = NULL;
      buf.allocated = 0;
      buf.fastmap = NULL;
      buf.translate = NULL;
    }

  re_set_syntax (syntax);
  msg = re_compile_pattern (regexp, strlen (regexp), &buf);

  if (msg != NULL)
    {
      M4ERROR ((m4_get_warning_status_opt (context), 0,
		_("%s: bad regular expression `%s': %s"),
		caller, regexp, msg));
      return NULL;
    }

  return &buf;
}

/* Regular expression version of index.  Given two arguments, expand to the
   index of the first match of the second argument (a regexp) in the first.
   Expand to -1 if here is no match.  Given a third argument, is changes
   the expansion to this argument.  */

/**
 * regexp(STRING, REGEXP, [REPLACEMENT])
 **/

static void
m4_regexp_do (m4 *context, struct obstack *obs, int argc,
	      m4_symbol_value **argv, int syntax)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */

  struct re_pattern_buffer *buf;/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  int startpos;			/* start position of match */
  int length;			/* length of first argument */

  victim = M4ARG (1);
  regexp = M4ARG (2);

  buf = m4_regexp_compile (context, M4ARG(0), regexp, syntax);
  if (!buf)
    return;

  length = strlen (victim);
  startpos = re_search (buf, victim, length, 0, length, &regs);

  if (startpos  == -2)
    {
      M4ERROR ((m4_get_warning_status_opt (context), 0,
		_("%s: error matching regular expression `%s'"),
		M4ARG (0), regexp));
      return;
    }

  if (argc == 3)
    m4_shipout_int (obs, startpos);
  else if (startpos >= 0)
    substitute (context, obs, victim, M4ARG (3), &regs);

  return;
}


/**
 * regexp(STRING, REGEXP, [REPLACEMENT])
 **/
M4BUILTIN_HANDLER (regexp)
{
  m4_regexp_do (context, obs, argc, argv, RE_SYNTAX_BRE);
}

/**
 * regexp(STRING, REGEXP, [REPLACEMENT])
 **/
M4BUILTIN_HANDLER (eregexp)
{
  m4_regexp_do (context, obs, argc, argv, RE_SYNTAX_ERE);
}



/* Substitute all matches of a regexp occuring in a string.  Each match of
   the second argument (a regexp) in the first argument is changed to the
   third argument, with \& substituted by the matched text, and \N
   substituted by the text matched by the Nth parenthesized sub-expression.  */

/**
 * patsubst(STRING, REGEXP, [REPLACEMENT])
 **/
static void
m4_patsubst_do (m4 *context, struct obstack *obs, int argc,
		m4_symbol_value **argv, int syntax)
{
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */

  struct re_pattern_buffer *buf;/* compiled regular expression */
  struct re_registers regs;	/* for subexpression matches */
  int matchpos;			/* start position of match */
  int offset;			/* current match offset */
  int length;			/* length of first argument */

  regexp = M4ARG (2);
  victim = M4ARG (1);
  length = strlen (victim);

  buf = m4_regexp_compile (context, M4ARG(0), regexp, syntax);
  if (!buf)
    return;

  offset = 0;
  matchpos = 0;
  while (offset < length)
    {
      matchpos = re_search (buf, victim, length,
			    offset, length - offset, &regs);
      if (matchpos < 0)
	{

	  /* Match failed -- either error or there is no match in the
	     rest of the string, in which case the rest of the string is
	     copied verbatim.  */

	  if (matchpos == -2)
	    M4ERROR ((m4_get_warning_status_opt (context), 0,
		      _("%s: error matching regular expression `%s'"),
		      M4ARG (0), regexp));
	  else if (offset < length)
	    obstack_grow (obs, victim + offset, length - offset);
	  break;
	}

      /* Copy the part of the string that was skipped by re_search ().  */

      if (matchpos > offset)
	obstack_grow (obs, victim + offset, matchpos - offset);

      /* Handle the part of the string that was covered by the match.  */

      substitute (context, obs, victim, M4ARG (3), &regs);

      /* Update the offset to the end of the match.  If the regexp
	 matched a null string, advance offset one more, to avoid
	 infinite loops.  */

      offset = regs.end[0];
      if (regs.start[0] == regs.end[0])
	obstack_1grow (obs, victim[offset++]);
    }
  obstack_1grow (obs, '\0');

  return;
}

/**
 * patsubst(STRING, REGEXP, [REPLACEMENT])
 **/
M4BUILTIN_HANDLER (patsubst)
{
  m4_patsubst_do (context, obs, argc, argv, RE_SYNTAX_BRE);
}

/**
 * patsubst(STRING, REGEXP, [REPLACEMENT])
 **/
M4BUILTIN_HANDLER (epatsubst)
{
  m4_patsubst_do (context, obs, argc, argv, RE_SYNTAX_ERE);
}

/* Implementation of "symbols" itself.  It builds up a table of pointers to
   symbols, sorts it and ships out the symbols name.  */

/**
 * symbols([...])
 **/
M4BUILTIN_HANDLER (symbols)
{
  struct m4_dump_symbol_data data;
  struct obstack data_obs;

  obstack_init (&data_obs);
  data.obs = &data_obs;
  m4_dump_symbols (context, &data, argc, argv, FALSE);

  for (; data.size > 0; --data.size, data.base++)
    {
      m4_shipout_string (context, obs, data.base[0], 0, TRUE);
      if (data.size > 1)
	obstack_1grow (obs, ',');
    }
  obstack_free (&data_obs, NULL);
}


/* This contains macro which implements syncoutput() which takes one arg
     1, on, yes - turn on sync lines
     0, off, no - turn off sync lines
     everything else is silently ignored  */

/**
 * syncoutput(SYNC?)
 **/
M4BUILTIN_HANDLER (syncoutput)
{
  if (m4_is_symbol_value_text (argv[1]))
    {
      if (   M4ARG (1)[0] == '0'
	  || M4ARG (1)[0] == 'n'
	  || (M4ARG (1)[0] == 'o' && M4ARG (1)[1] == 'f'))
	m4_set_sync_output_opt (context, FALSE);
      else if (   M4ARG (1)[0] == '1'
	       || M4ARG (1)[0] == 'y'
	       || (M4ARG (1)[0] == 'o' && M4ARG (1)[1] == 'n'))
	m4_set_sync_output_opt (context, TRUE);
    }
}


/**
 * esyscmd(SHELL-COMMAND)
 **/
M4BUILTIN_HANDLER (esyscmd)
{
  FILE *pin;
  int ch;

  m4_debug_flush_files (context);
  pin = popen (M4ARG (1), "r");
  if (pin == NULL)
    {
      M4ERROR ((m4_get_warning_status_opt (context), errno,
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

/* Frontend for printf like formatting.  The function format () lives in
   the file format.c.  */

/**
 * format(FORMAT-STRING, [...])
 **/
M4BUILTIN_HANDLER (format)
{
  format (obs, argc - 1, argv + 1);
}


/**
 * __file__
 **/
M4BUILTIN_HANDLER (__file__)
{
  m4_shipout_string (context, obs, m4_current_file, 0, TRUE);
}


/**
 * __line__
 **/
M4BUILTIN_HANDLER (__line__)
{
  m4_shipout_int (obs, m4_current_line);
}

/* Function to perform substitution by regular expressions.  Used by the
   builtins regexp and patsubst.  The changed text is placed on the
   obstack.  The substitution is REPL, with \& substituted by this part of
   VICTIM matched by the last whole regular expression, taken from REGS[0],
   and \N substituted by the text matched by the Nth parenthesized
   sub-expression, taken from REGS[N].  */
static int substitute_warned = 0;

static void
substitute (m4 *context, struct obstack *obs, const char *victim,
	    const char *repl, struct re_registers *regs)
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
	      M4ERROR ((m4_get_warning_status_opt (context), 0, _("\
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

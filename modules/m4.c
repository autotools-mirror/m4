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

#if HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#ifndef errno
extern int errno;
#endif

#include <m4module.h>

/* Include this header for speed, which gives us direct access to
   the fields of internal structures at the expense of maintaining
   interface/implementation separation.   The builtins in this file
   are the core of m4 and must be optimised for speed.  */
#include "m4private.h"

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	m4_LTX_m4_builtin_table
#define m4_resident_module	m4_LTX_m4_resident_module

/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind */
#define builtin_functions			\
	BUILTIN(changecom,	FALSE,	FALSE )	\
	BUILTIN(changequote,	FALSE,	FALSE )	\
	BUILTIN(decr,		FALSE,	TRUE  )	\
	BUILTIN(define,		TRUE,	TRUE  )	\
	BUILTIN(defn,		FALSE,	TRUE  )	\
	BUILTIN(divert,		FALSE,	FALSE )	\
	BUILTIN(divnum,		FALSE,	FALSE )	\
	BUILTIN(dnl,		FALSE,	FALSE )	\
	BUILTIN(dumpdef,	FALSE,	FALSE )	\
	BUILTIN(errprint,	FALSE,	FALSE )	\
	BUILTIN(eval,		FALSE,	TRUE  )	\
	BUILTIN(ifdef,		FALSE,	TRUE  )	\
	BUILTIN(ifelse,		FALSE,	TRUE  )	\
	BUILTIN(include,	FALSE,	TRUE  )	\
	BUILTIN(incr,		FALSE,	TRUE  )	\
	BUILTIN(index,		FALSE,	TRUE  )	\
	BUILTIN(len,		FALSE,	TRUE  )	\
	BUILTIN(m4exit,		FALSE,	FALSE )	\
	BUILTIN(m4wrap,		FALSE,	FALSE )	\
	BUILTIN(maketemp,	FALSE,	TRUE  )	\
	BUILTIN(popdef,		FALSE,	TRUE  )	\
	BUILTIN(pushdef,	TRUE,	TRUE  )	\
	BUILTIN(shift,		FALSE,	FALSE )	\
	BUILTIN(sinclude,	FALSE,	TRUE  )	\
	BUILTIN(substr,		FALSE,	TRUE  )	\
	BUILTIN(syscmd,		FALSE,	TRUE  )	\
	BUILTIN(sysval,		FALSE,	FALSE )	\
	BUILTIN(traceoff,	FALSE,	FALSE )	\
	BUILTIN(traceon,	FALSE,	FALSE )	\
	BUILTIN(translit,	FALSE,	TRUE  )	\
	BUILTIN(undefine,	FALSE,	TRUE  )	\
	BUILTIN(undivert,	FALSE,	FALSE )

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

/* This module cannot be safely unloaded from memory, incase the unload
   is triggered by m4exit, and the module is removed while m4exit is in
   progress.  */
boolean m4_resident_module = TRUE;



/* The rest of this file is code for builtins and expansion of user
   defined macros.  All the functions for builtins have a prototype as:

	void m4_MACRONAME (struct obstack *obs, int argc, char *argv[]);

   The function are expected to leave their expansion on the obstack OBS,
   as an unfinished object.  ARGV is a table of ARGC pointers to the
   individual arguments to the macro.  Please note that in general
   argv[argc] != NULL.  */

/*-------------------------------------------------------------------------.
| The function macro_install is common for the builtins "define",	   |
| "undefine", "pushdef" and "popdef".  ARGC and ARGV is as for the caller, |
| and MODE argument determines how the macro name is entered into the	   |
| symbol table.								   |
`-------------------------------------------------------------------------*/

static void
macro_install (argc, argv, mode)
     int argc;
     m4_token_data **argv;
     m4_symbol_lookup mode;
{
  const m4_builtin *bp;

  if (m4_bad_argc (argv[0], argc, 2, 3))
    return;

  if (M4_TOKEN_DATA_TYPE (argv[1]) != M4_TOKEN_TEXT)
    return;

  if (argc == 2)
    {
      m4_macro_define (NULL, M4ARG (1), "", mode);
      return;
    }

  switch (M4_TOKEN_DATA_TYPE (argv[2]))
    {
    case M4_TOKEN_TEXT:
      m4_macro_define (NULL, M4ARG (1), M4ARG (2), mode);
      break;

    case M4_TOKEN_FUNC:
      bp = m4_builtin_find_by_func (NULL, M4_TOKEN_DATA_FUNC (argv[2]));
      if (bp)
	m4_builtin_define (NULL, M4ARG (1), bp, mode,
			   M4_TOKEN_DATA_FUNC_TRACED (argv[2]));
      break;

    default:
      M4ERROR ((warning_status, 0,
		_("INTERNAL ERROR: Bad token data type in install_macro ()")));
      abort ();
    }
  return;
}

M4BUILTIN_HANDLER (define)
{
  macro_install (argc, argv, M4_SYMBOL_INSERT);
}

M4BUILTIN_HANDLER (undefine)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  m4_lookup_symbol (M4ARG (1), M4_SYMBOL_DELETE);
}

M4BUILTIN_HANDLER (pushdef)
{
  macro_install (argc, argv,  M4_SYMBOL_PUSHDEF);
}

M4BUILTIN_HANDLER (popdef)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  m4_lookup_symbol (M4ARG (1), M4_SYMBOL_POPDEF);
}


/*---------------------.
| Conditionals of m4.  |
`---------------------*/

M4BUILTIN_HANDLER (ifdef)
{
  m4_symbol *symbol;
  const char *result;

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;
  symbol = m4_lookup_symbol (M4ARG (1), M4_SYMBOL_LOOKUP);

  if (symbol)
    result = M4ARG (2);
  else if (argc == 4)
    result = M4ARG (3);
  else
    result = NULL;

  if (result)
    obstack_grow (obs, result, strlen (result));
}

M4BUILTIN_HANDLER (ifelse)
{
  const char *result;

  if (argc == 2)
    return;

  if (m4_bad_argc (argv[0], argc, 4, -1))
    return;
  else
    /* Diagnose excess arguments if 5, 8, 11, etc., actual arguments.  */
    m4_bad_argc (argv[0], (argc + 2) % 3, -1, 1);

  argv++;
  argc--;

  result = NULL;
  while (result == NULL)

    if (strcmp (M4ARG (0), M4ARG (1)) == 0)
      result = M4ARG (2);

    else
      switch (argc)
	{
	case 3:
	  return;

	case 4:
	case 5:
	  result = M4ARG (3);
	  break;

	default:
	  argc -= 3;
	  argv += 3;
	}

  obstack_grow (obs, result, strlen (result));
}


/*-------------------------------------------------------------------------.
| Implementation of "dumpdef" itself.  It builds up a table of pointers to |
| symbols, sorts it and prints the sorted table.			   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (dumpdef)
{
  struct m4_dump_symbol_data data;
  const m4_builtin *bp;

  data.obs = obs;
  m4_dump_symbols (&data, argc, argv, TRUE);

  for (; data.size > 0; --data.size, data.base++)
    {
      M4_DEBUG_PRINT1 ("%s:\t", SYMBOL_NAME (data.base[0]));

      switch (SYMBOL_TYPE (data.base[0]))
	{
	case M4_TOKEN_TEXT:
	  if (debug_level & M4_DEBUG_TRACE_QUOTE)
	    M4_DEBUG_PRINT3 ("%s%s%s\n",
			  lquote.string, SYMBOL_TEXT (data.base[0]), rquote.string);
	  else
	    M4_DEBUG_PRINT1 ("%s\n", SYMBOL_TEXT (data.base[0]));
	  break;

	case M4_TOKEN_FUNC:
	  bp = m4_builtin_find_by_func (NULL, SYMBOL_FUNC (data.base[0]));
	  if (bp == NULL)
	    {
	      M4ERROR ((warning_status, 0,
			_("Undefined name `%s'"), SYMBOL_NAME (data.base[0])));
	      abort ();
	    }
	  M4_DEBUG_PRINT1 ("<%s>\n", bp->name);
	  break;

	default:
	  M4ERROR ((warning_status, 0, _("\
INTERNAL ERROR: Bad token data type in m4_dumpdef ()")));
	  abort ();
	  break;
	}
    }
}

/*-------------------------------------------------------------------------.
| The macro "defn" returns the quoted definition of the macro named by the |
| first argument.  If the macro is builtin, it will push a special	   |
| macro-definition token on the input stack.				   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (defn)
{
  m4_symbol *symbol;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  symbol = m4_lookup_symbol (M4ARG (1), M4_SYMBOL_LOOKUP);
  if (symbol == NULL)
    return;

  switch (SYMBOL_TYPE (symbol))
    {
    case M4_TOKEN_TEXT:
      m4_shipout_string(obs, SYMBOL_TEXT (symbol), 0, TRUE);
      break;

    case M4_TOKEN_FUNC:
      m4_push_macro (SYMBOL_FUNC (symbol), SYMBOL_TRACED (symbol));
      break;

    case M4_TOKEN_VOID:
      break;

    default:
      M4ERROR ((warning_status, 0,
		_("INTERNAL ERROR: Bad symbol type in m4_defn ()")));
      abort ();
    }
}


/*------------------------------------------------------------------------.
| This section contains macros to handle the builtins "syscmd", "esyscmd" |
| and "sysval".  "esyscmd" is GNU specific.				  |
`------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (syscmd)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  m4_debug_flush_files ();
  m4_sysval = system (M4ARG (1));
}


M4BUILTIN_HANDLER (sysval)
{
  m4_shipout_int (obs, (m4_sysval >> 8) & 0xff);
}


/*----------------------------------------------------------------------.
| This section contains the top level code for the "eval" builtin.  The |
| actual work is done in the function m4_evaluate (), which lives in	|
| eval.c.								|
`----------------------------------------------------------------------*/

M4BUILTIN_HANDLER (eval)
{
  m4_do_eval(obs, argc, argv, m4_evaluate);
}

M4BUILTIN_HANDLER (incr)
{
  int value;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  if (!m4_numeric_arg (argv[0], M4ARG (1), &value))
    return;

  m4_shipout_int (obs, value + 1);
}

M4BUILTIN_HANDLER (decr)
{
  int value;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  if (!m4_numeric_arg (argv[0], M4ARG (1), &value))
    return;

  m4_shipout_int (obs, value - 1);
}


/* This section contains the macros "divert", "undivert" and "divnum" for
   handling diversion.  The utility functions used lives in output.c.  */

/*-----------------------------------------------------------------------.
| Divert further output to the diversion given by ARGV[1].  Out of range |
| means discard further output.						 |
`-----------------------------------------------------------------------*/

M4BUILTIN_HANDLER (divert)
{
  int i = 0;

  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 2 && !m4_numeric_arg (argv[0], M4ARG (1), &i))
    return;

  m4_make_diversion (i);
}

/*-----------------------------------------------------.
| Expand to the current diversion number, -1 if none.  |
`-----------------------------------------------------*/

M4BUILTIN_HANDLER (divnum)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;
  m4_shipout_int (obs, m4_current_diversion);
}

/*-----------------------------------------------------------------------.
| Bring back the diversion given by the argument list.  If none is	 |
| specified, bring back all diversions.  GNU specific is the option of	 |
| undiverting named files, by passing a non-numeric argument to undivert |
| ().									 |
`-----------------------------------------------------------------------*/

M4BUILTIN_HANDLER (undivert)
{
  int i, file;
  FILE *fp;

  if (argc == 1)
    m4_undivert_all ();
  else
    for (i = 1; i < argc; i++)
      {
	if (sscanf (M4ARG (i), "%d", &file) == 1)
	  m4_insert_diversion (file);
	else if (no_gnu_extensions)
	  M4ERROR ((warning_status, 0,
		    _("Non-numeric argument to %s"),
		    M4_TOKEN_DATA_TEXT (argv[0])));
	else
	  {
	    fp = m4_path_search (M4ARG (i), (char **)NULL);
	    if (fp != NULL)
	      {
		m4_insert_file (fp);
		fclose (fp);
	      }
	    else
	      M4ERROR ((warning_status, errno,
			_("Cannot undivert %s"), M4ARG (i)));
	  }
      }
}


/*-------------------------------------------------------------------.
| This section contains various macros, which does not fall into any |
| specific group.  These are "dnl", "shift", "changequote",	     |
| "changecom" and "changesyntax".				     |
`-------------------------------------------------------------------*/

/*------------------------------------------------------------------------.
| Delete all subsequent whitespace from input.  The function skip_line () |
| lives in input.c.							  |
`------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (dnl)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_skip_line ();
}

/*-------------------------------------------------------------------------.
| Shift all argument one to the left, discarding the first argument.  Each |
| output argument is quoted with the current quotes.			   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (shift)
{
  m4_dump_args (obs, argc - 1, argv + 1, ",", TRUE);
}

/*--------------------------------------------------------------------------.
| Change the current quotes.  The function set_quotes () lives in input.c.  |
`--------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (changequote)
{
  if (m4_bad_argc (argv[0], argc, 1, 3))
    return;

  m4_set_quotes ((argc >= 2) ? M4_TOKEN_DATA_TEXT (argv[1]) : NULL,
	     (argc >= 3) ? M4_TOKEN_DATA_TEXT (argv[2]) : NULL);
}

/*--------------------------------------------------------------------.
| Change the current comment delimiters.  The function set_comment () |
| lives in input.c.						      |
`--------------------------------------------------------------------*/

M4BUILTIN_HANDLER (changecom)
{
  if (m4_bad_argc (argv[0], argc, 1, 3))
    return;

  if (argc == 1)
    m4_set_comment ("", "");	/* disable comments */
  else
    m4_set_comment (M4_TOKEN_DATA_TEXT (argv[1]),
		    (argc >= 3) ? M4_TOKEN_DATA_TEXT (argv[2]) : NULL);
}


/* This section contains macros for inclusion of other files -- "include"
   and "sinclude".  This differs from bringing back diversions, in that
   the input is scanned before being copied to the output.  */

/*-------------------------------------------------------------------------.
| Generic include function.  Include the file given by the first argument, |
| if it exists.  Complain about inaccesible files iff SILENT is FALSE.	   |
`-------------------------------------------------------------------------*/

static void
include (int argc, m4_token_data **argv, boolean silent)
{
  FILE *fp;
  char *name = NULL;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  fp = m4_path_search (M4ARG (1), &name);
  if (fp == NULL)
    {
      if (!silent)
	M4ERROR ((warning_status, errno,
		  _("Cannot open %s"), M4ARG (1)));
      return;
    }

  m4_push_file (fp, name);
  xfree (name);
}

/*------------------------------------------------.
| Include a file, complaining in case of errors.  |
`------------------------------------------------*/

M4BUILTIN_HANDLER (include)
{
  include (argc, argv, FALSE);
}

/*----------------------------------.
| Include a file, ignoring errors.  |
`----------------------------------*/

M4BUILTIN_HANDLER (sinclude)
{
  include (argc, argv, TRUE);
}


/* More miscellaneous builtins -- "maketemp", "errprint", "__file__" and
   "__line__".  The last two are GNU specific.  */

/*------------------------------------------------------------------.
| Use the first argument as at template for a temporary file name.  |
`------------------------------------------------------------------*/

M4BUILTIN_HANDLER (maketemp)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  mktemp (M4ARG (1));
  m4_shipout_string (obs, M4ARG (1), 0, FALSE);
}

/*----------------------------------------.
| Print all arguments on standard error.  |
`----------------------------------------*/

M4BUILTIN_HANDLER (errprint)
{
  m4_dump_args (obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  fprintf (stderr, "%s", (char *) obstack_finish (obs));
  fflush (stderr);
}


/* This section contains various macros for exiting, saving input until
   EOF is seen, and tracing macro calls.  That is: "m4exit", "m4wrap",
   "traceon" and "traceoff".  */

/*-------------------------------------------------------------------------.
| Exit immediately, with exitcode specified by the first argument, 0 if no |
| arguments are present.						   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (m4exit)
{
  int exit_code = 0;

  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;
  if (argc == 2  && !m4_numeric_arg (argv[0], M4ARG (1), &exit_code))
    exit_code = 0;

  m4_module_unload_all();

  exit (exit_code);
}

/*-------------------------------------------------------------------------.
| Save the argument text until EOF has been seen, allowing for user	   |
| specified cleanup action.  GNU version saves all arguments, the standard |
| version only the first.						   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (m4wrap)
{
  if (no_gnu_extensions)
    m4_shipout_string (obs, M4ARG (1), 0, FALSE);
  else
    m4_dump_args (obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  m4_push_wrapup (obstack_finish (obs));
}

/* Enable tracing of all specified macros, or all, if none is specified.
   Tracing is disabled by default, when a macro is defined.  This can be
   overridden by the "t" debug flag.  */

/*-----------------------------------------------------------------------.
| Set_trace () is used by "traceon" and "traceoff" to enable and disable |
| tracing of a macro.  It disables tracing if DATA is NULL, otherwise it |
| enable tracing.							 |
`-----------------------------------------------------------------------*/

static void
set_trace (m4_symbol *symbol, const char *data)
{
  SYMBOL_TRACED (symbol) = (boolean) (data != NULL);
}

M4BUILTIN_HANDLER (traceon)
{
  m4_symbol *symbol;
  int i;

  if (argc == 1)
    m4_hack_all_symbols (set_trace, (char *) obs);
  else
    for (i = 1; i < argc; i++)
      {
	symbol = m4_lookup_symbol (M4_TOKEN_DATA_TEXT (argv[i]), M4_SYMBOL_LOOKUP);
	if (symbol != NULL)
	  set_trace (symbol, (char *) obs);
	else
	  M4ERROR ((warning_status, 0,
		    _("Undefined name %s"), M4_TOKEN_DATA_TEXT (argv[i])));
      }
}

/*------------------------------------------------------------------------.
| Disable tracing of all specified macros, or all, if none is specified.  |
`------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (traceoff)
{
  m4_symbol *symbol;
  int i;

  if (argc == 1)
    m4_hack_all_symbols (set_trace, NULL);
  else
    for (i = 1; i < argc; i++)
      {
	symbol = m4_lookup_symbol (M4_TOKEN_DATA_TEXT (argv[i]), M4_SYMBOL_LOOKUP);
	if (symbol != NULL)
	  set_trace (symbol, NULL);
	else
	  M4ERROR ((warning_status, 0,
		    _("Undefined name %s"), M4_TOKEN_DATA_TEXT (argv[i])));
      }
}


/* This section contains text processing macros: "len", "index",
   "substr", "translit", "format", "regexp" and "patsubst".  The last
   three are GNU specific.  */

/*---------------------------------------------.
| Expand to the length of the first argument.  |
`---------------------------------------------*/

M4BUILTIN_HANDLER (len)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;
  m4_shipout_int (obs, strlen (M4ARG (1)));
}

/*-------------------------------------------------------------------------.
| The macro expands to the first index of the second argument in the first |
| argument.								   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (index)
{
  const char *cp, *last;
  int l1, l2, retval;

  if (m4_bad_argc (argv[0], argc, 3, 3))
    return;

  l1 = strlen (M4ARG (1));
  l2 = strlen (M4ARG (2));

  last = M4ARG (1) + l1 - l2;

  for (cp = M4ARG (1); cp <= last; cp++)
    {
      if (strncmp (cp, M4ARG (2), l2) == 0)
	break;
    }
  retval = (cp <= last) ? cp - M4ARG (1) : -1;

  m4_shipout_int (obs, retval);
}

/*-------------------------------------------------------------------------.
| The macro "substr" extracts substrings from the first argument, starting |
| from the index given by the second argument, extending for a length	   |
| given by the third argument.  If the third argument is missing, the	   |
| substring extends to the end of the first argument.			   |
`-------------------------------------------------------------------------*/

M4BUILTIN_HANDLER (substr)
{
  int start, length, avail;

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  length = avail = strlen (M4ARG (1));
  if (!m4_numeric_arg (argv[0], M4ARG (2), &start))
    return;

  if (argc == 4 && !m4_numeric_arg (argv[0], M4ARG (3), &length))
    return;

  if (start < 0 || length <= 0 || start >= avail)
    return;

  if (start + length > avail)
    length = avail - start;
  obstack_grow (obs, M4ARG (1) + start, length);
}


/*----------------------------------------------------------------------.
| The macro "translit" translates all characters in the first argument, |
| which are present in the second argument, into the corresponding      |
| character from the third argument.  If the third argument is shorter  |
| than the second, the extra characters in the second argument, are     |
| deleted from the first (pueh).				        |
`----------------------------------------------------------------------*/

M4BUILTIN_HANDLER (translit)
{
  register const char *data, *tmp;
  const char *from, *to;
  int tolen;

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  from = M4ARG (2);
  if (strchr (from, '-') != NULL)
    {
      from = m4_expand_ranges (from, obs);
      if (from == NULL)
	return;
    }

  if (argc == 4)
    {
      to = M4ARG (3);
      if (strchr (to, '-') != NULL)
	{
	  to = m4_expand_ranges (to, obs);
	  if (to == NULL)
	    return;
	}
    }
  else
    to = "";

  tolen = strlen (to);

  for (data = M4ARG (1); *data; data++)
    {
      tmp = strchr (from, *data);
      if (tmp == NULL)
	{
	  obstack_1grow (obs, *data);
	}
      else
	{
	  if (tmp - from < tolen)
	    obstack_1grow (obs, *(to + (tmp - from)));
	}
    }
}

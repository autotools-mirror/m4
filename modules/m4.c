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

#include <assert.h>

#include <m4module.h>

#ifdef NDEBUG
/* Include this header for speed, which gives us direct access to
   the fields of internal structures at the expense of maintaining
   interface/implementation separation.   The builtins in this file
   are the core of m4 and must be optimised for speed.  */
#  include "m4private.h"
#endif

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	m4_LTX_m4_builtin_table

/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind minargs maxargs */
#define builtin_functions			\
	BUILTIN(changecom,	FALSE,	FALSE,	1,	3  )	\
	BUILTIN(changequote,	FALSE,	FALSE,	1,	3  )	\
	BUILTIN(decr,		FALSE,	TRUE,	2,	2  )	\
	BUILTIN(define,		TRUE,	TRUE,	2,	3  )	\
	BUILTIN(defn,		FALSE,	TRUE,	0,	-1 )	\
	BUILTIN(divert,		FALSE,	FALSE,	1,	2  )	\
	BUILTIN(divnum,		FALSE,	FALSE,	1,	1  )	\
	BUILTIN(dnl,		FALSE,	FALSE,	1,	1  )	\
	BUILTIN(dumpdef,	FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(errprint,	FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(eval,		FALSE,	TRUE,	2,	4  )	\
	BUILTIN(ifdef,		FALSE,	TRUE,	3,	4  )	\
	BUILTIN(ifelse,		FALSE,	TRUE,	-1,	-1 )	\
	BUILTIN(include,	FALSE,	TRUE,	2,	2  )	\
	BUILTIN(incr,		FALSE,	TRUE,	2,	2  )	\
	BUILTIN(index,		FALSE,	TRUE,	3,	3  )	\
	BUILTIN(len,		FALSE,	TRUE,	2,	2  )	\
	BUILTIN(m4exit,		FALSE,	FALSE,	1,	2  )	\
	BUILTIN(m4wrap,		FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(maketemp,	FALSE,	TRUE,	2,	2  )	\
	BUILTIN(popdef,		FALSE,	TRUE,	2,	2  )	\
	BUILTIN(pushdef,	TRUE,	TRUE,	2,	3  )	\
	BUILTIN(shift,		FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(sinclude,	FALSE,	TRUE,	2,	2  )	\
	BUILTIN(substr,		FALSE,	TRUE,	3,	4  )	\
	BUILTIN(syscmd,		FALSE,	TRUE,	2,	2  )	\
	BUILTIN(sysval,		FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(traceoff,	FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(traceon,	FALSE,	FALSE,	0,	-1 )	\
	BUILTIN(translit,	FALSE,	TRUE,	3,	4  )	\
	BUILTIN(undefine,	FALSE,	TRUE,	2,	2  )	\
	BUILTIN(undivert,	FALSE,	FALSE,	0,	-1 )	\


#if defined(SIZEOF_LONG_LONG_INT) && SIZEOF_LONG_LONG_INT > 0
/* Use GNU long long int if available.  */
typedef long long int number;
typedef unsigned long long int unumber;
#else
typedef long int number;
typedef unsigned long int unumber;
#endif


static void	include		(m4 *context, int argc, m4_symbol_value **argv,
				 boolean silent);
static void *	set_trace_CB	(m4_symbol_table *symtab, const char *ignored,
				 m4_symbol *symbol, void *userdata);
static const char *ntoa		(number value, int radix);
static void	numb_obstack	(struct obstack *obs, const number value,
				 const int radix, int min);


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind, min, max) M4BUILTIN(handler)
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



/* This module cannot be safely unloaded from memory, incase the unload
   is triggered by m4exit, and the module is removed while m4exit is in
   progress.  */
M4INIT_HANDLER (m4)
{
  if (handle)
    if (lt_dlmakeresident (handle) != 0)
      {
	M4ERROR ((m4_get_warning_status_opt (context), 0,
		  _("Warning: cannot make module `%s' resident: %s"),
		  m4_get_module_name (handle), lt_dlerror ()));
      }
}



/* The rest of this file is code for builtins and expansion of user
   defined macros.  All the functions for builtins have a prototype as:

	void builtin_MACRONAME (struct obstack *obs, int argc, char *argv[]);

   The function are expected to leave their expansion on the obstack OBS,
   as an unfinished object.  ARGV is a table of ARGC pointers to the
   individual arguments to the macro.  Please note that in general
   argv[argc] != NULL.  */

M4BUILTIN_HANDLER (define)
{
  if (m4_is_symbol_value_text (argv[1]))
    {
      m4_symbol_value *value = m4_symbol_value_create ();

      if (argc == 2)
	m4_set_symbol_value_text (value, xstrdup (""));
      else
	m4_symbol_value_copy (value, argv[2]);

      if (m4_get_posixly_correct_opt (context))
	m4_symbol_delete (M4SYMTAB, M4ARG (1));

      m4_symbol_define (M4SYMTAB, M4ARG (1), value);
    }
}

M4BUILTIN_HANDLER (undefine)
{
  if (!m4_symbol_lookup (M4SYMTAB, M4ARG (1)))
    M4WARN ((m4_get_warning_status_opt (context), 0,
	     _("Warning: %s: undefined name: %s"), M4ARG (0), M4ARG (1)));
  else
    m4_symbol_delete (M4SYMTAB, M4ARG (1));
}

M4BUILTIN_HANDLER (pushdef)
{
  if (m4_is_symbol_value_text (argv[1]))
    {
      m4_symbol_value *value = m4_symbol_value_create ();

      if (argc == 2)
	m4_set_symbol_value_text (value, xstrdup (""));
      else
	m4_symbol_value_copy (value, argv[2]);

      m4_symbol_pushdef (M4SYMTAB, M4ARG (1), value);
    }
}

M4BUILTIN_HANDLER (popdef)
{
  if (!m4_symbol_lookup (M4SYMTAB, M4ARG (1)))
    M4WARN ((m4_get_warning_status_opt (context), 0,
	     _("Warning: %s: undefined name: %s"), M4ARG (0), M4ARG (1)));
  else
    m4_symbol_popdef (M4SYMTAB, M4ARG (1));
}




/* --- CONDITIONALS OF M4 --- */


M4BUILTIN_HANDLER (ifdef)
{
  m4_symbol *symbol;
  const char *result;

  symbol = m4_symbol_lookup (M4SYMTAB, M4ARG (1));

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

  /* The valid ranges of argc for ifelse is discontinuous, we cannot
     rely on the regular mechanisms.  */
  if (argc == 2)
    return;

  if (m4_bad_argc (context, argc, argv, 4, -1))
    return;
  else
    /* Diagnose excess arguments if 5, 8, 11, etc., actual arguments.  */
    m4_bad_argc (context, (argc + 2) % 3, argv, -1, 1);

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


/* Implementation of "dumpdef" itself.  It builds up a table of pointers to
   symbols, sorts it and prints the sorted table.  */
M4BUILTIN_HANDLER (dumpdef)
{
  struct m4_dump_symbol_data data;
  const m4_builtin *bp;

  data.obs = obs;
  m4_dump_symbols (context, &data, argc, argv, TRUE);

  for (; data.size > 0; --data.size, data.base++)
    {
      m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, data.base[0]);

      fprintf (stderr, "%s:\t", data.base[0]);

      if (m4_is_symbol_text (symbol))
	{
	  if (m4_get_debug_level_opt (context) & M4_DEBUG_TRACE_QUOTE)
	    fprintf (stderr, "%s%s%s\n",
		     m4_get_syntax_lquote (M4SYNTAX),
		     m4_get_symbol_text (symbol),
		     m4_get_syntax_rquote (M4SYNTAX));
	  else
	    fprintf (stderr, "%s\n", m4_get_symbol_text (symbol));
	}
      else if (m4_is_symbol_func (symbol))
	{
	  bp = m4_builtin_find_by_func (NULL,
					m4_get_symbol_func (symbol));
	  assert (bp);
	  fprintf (stderr, "<%s>\n", bp->name);
	}
      else
	{
	  assert (!"illegal token in m4_dumpdef");
	}
    }
}

/* The macro "defn" returns the quoted definition of the macro named by
   the first argument.  If the macro is builtin, it will push a special
   macro-definition token on the input stack.  */
M4BUILTIN_HANDLER (defn)
{
  int i;

  for (i = 1; i < argc; i++)
    {
      const char *name = m4_get_symbol_value_text (argv[i]);
      m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, name);

      if (!symbol)
	M4WARN ((m4_get_warning_status_opt (context), 0,
		 _("Warning: %s: undefined name: %s"),
		 m4_get_symbol_value_text (argv[0]), name));
      else if (m4_is_symbol_text (symbol))
	m4_shipout_string (context, obs, m4_get_symbol_text (symbol), 0, TRUE);
      else if (m4_is_symbol_func (symbol))
	m4_push_builtin (m4_get_symbol_value (symbol));
      else
	assert (!"Bad token data type in m4_defn");
    }
}


/* This section contains macros to handle the builtins "syscmd"
   and "sysval".  */
M4BUILTIN_HANDLER (syscmd)
{
  m4_debug_flush_files ();
  m4_sysval = system (M4ARG (1));
}


M4BUILTIN_HANDLER (sysval)
{
  m4_shipout_int (obs, (m4_sysval >> 8) & 0xff);
}


M4BUILTIN_HANDLER (incr)
{
  int value;

  if (!m4_numeric_arg (context, argc, argv, 1, &value))
    return;

  m4_shipout_int (obs, value + 1);
}

M4BUILTIN_HANDLER (decr)
{
  int value;

  if (!m4_numeric_arg (context, argc, argv, 1, &value))
    return;

  m4_shipout_int (obs, value - 1);
}


/* This section contains the macros "divert", "undivert" and "divnum" for
   handling diversion.  The utility functions used lives in output.c.  */

/* Divert further output to the diversion given by ARGV[1].  Out of range
   means discard further output.  */
M4BUILTIN_HANDLER (divert)
{
  int i = 0;

  if (argc == 2 && !m4_numeric_arg (context, argc, argv, 1, &i))
    return;

  m4_make_diversion (i);
}

/* Expand to the current diversion number, -1 if none.  */
M4BUILTIN_HANDLER (divnum)
{
  m4_shipout_int (obs, m4_current_diversion);
}

/* Bring back the diversion given by the argument list.  If none is
   specified, bring back all diversions.  GNU specific is the option
   of undiverting the named file, by passing a non-numeric argument to
   undivert ().  */

M4BUILTIN_HANDLER (undivert)
{
  int i = 0;

  if (argc == 1)
    m4_undivert_all ();
  else
    {
      if (sscanf (M4ARG (1), "%d", &i) == 1)
	m4_insert_diversion (i);
      else if (m4_get_posixly_correct_opt (context))
	m4_numeric_arg (context, argc, argv, 1, &i);
      else
	{
	  FILE *fp = m4_path_search (context, M4ARG (1), (char **) NULL);
	  if (fp != NULL)
	    {
	      m4_insert_file (fp);
	      fclose (fp);
	    }
	  else
	    M4ERROR ((m4_get_warning_status_opt (context), errno,
		      _("Cannot undivert %s"), M4ARG (1)));
	}
    }
}


/* This section contains various macros, which does not fall into
   any specific group.  These are "dnl", "shift", "changequote",
   "changecom" and "changesyntax"  */

/* Delete all subsequent whitespace from input.  The function skip_line ()
   lives in input.c.  */
M4BUILTIN_HANDLER (dnl)
{
  m4_skip_line (context);
}

/* Shift all argument one to the left, discarding the first argument.  Each
   output argument is quoted with the current quotes.  */
M4BUILTIN_HANDLER (shift)
{
  m4_dump_args (context, obs, argc - 1, argv + 1, ",", TRUE);
}

/* Change the current quotes.  The function set_quotes () lives in input.c.  */
M4BUILTIN_HANDLER (changequote)
{
  m4_set_quotes (M4SYNTAX,
		 (argc >= 2) ? M4ARG (1) : NULL,
		 (argc >= 3) ? M4ARG (2) : NULL);
}

/* Change the current comment delimiters.  The function set_comment ()
   lives in input.c.  */
M4BUILTIN_HANDLER (changecom)
{
  if (argc == 1)
    m4_set_comment (M4SYNTAX, "", "");	/* disable comments */
  else
    m4_set_comment (M4SYNTAX, M4ARG (1), (argc >= 3) ? M4ARG (2) : NULL);
}


/* This section contains macros for inclusion of other files -- "include"
   and "sinclude".  This differs from bringing back diversions, in that
   the input is scanned before being copied to the output.  */

/* Generic include function.  Include the file given by the first argument,
   if it exists.  Complain about inaccesible files iff SILENT is FALSE.  */
static void
include (m4 *context, int argc, m4_symbol_value **argv, boolean silent)
{
  FILE *fp;
  char *name = NULL;

  fp = m4_path_search (context, M4ARG (1), &name);
  if (fp == NULL)
    {
      if (!silent)
	M4ERROR ((m4_get_warning_status_opt (context), errno,
		  _("Cannot open %s"), M4ARG (1)));
      return;
    }

  m4_push_file (context, fp, name);
  xfree (name);
}

/* Include a file, complaining in case of errors.  */
M4BUILTIN_HANDLER (include)
{
  include (context, argc, argv, FALSE);
}

/* Include a file, ignoring errors.  */
M4BUILTIN_HANDLER (sinclude)
{
  include (context, argc, argv, TRUE);
}


/* More miscellaneous builtins -- "maketemp", "errprint".  */

/* Use the first argument as at template for a temporary file name.  */
M4BUILTIN_HANDLER (maketemp)
{
  mktemp (M4ARG (1));
  m4_shipout_string (context, obs, M4ARG (1), 0, FALSE);
}

/* Print all arguments on standard error.  */
M4BUILTIN_HANDLER (errprint)
{
  m4_dump_args (context, obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  fputs ((char *) obstack_finish (obs), stderr);
  fflush (stderr);
}


/* This section contains various macros for exiting, saving input until
   EOF is seen, and tracing macro calls.  That is: "m4exit", "m4wrap",
   "traceon" and "traceoff".  */

/* Exit immediately, with exitcode specified by the first argument, 0 if no
   arguments are present.  */
M4BUILTIN_HANDLER (m4exit)
{
  int exit_code = 0;

  if (argc == 2  && !m4_numeric_arg (context, argc, argv, 1, &exit_code))
    exit_code = 0;

  /* Ensure any module exit callbacks are executed.  */
  m4__module_exit (context);

  exit (exit_code);
}

/* Save the argument text until EOF has been seen, allowing for user
   specified cleanup action.  GNU version saves all arguments, the standard
   version only the first.  */
M4BUILTIN_HANDLER (m4wrap)
{
  if (m4_get_no_gnu_extensions_opt (context))
    m4_shipout_string (context, obs, M4ARG (1), 0, FALSE);
  else
    m4_dump_args (context, obs, argc, argv, " ", FALSE);
  obstack_1grow (obs, '\0');
  m4_push_wrapup (obstack_finish (obs));
}

/* Enable tracing of all specified macros, or all, if none is specified.
   Tracing is disabled by default, when a macro is defined.  This can be
   overridden by the "t" debug flag.  */

/* Set_trace () is used by "traceon" and "traceoff" to enable and disable
   tracing of a macro.  It disables tracing if DATA is NULL, otherwise it
   enable tracing.  */
static void *
set_trace_CB (m4_symbol_table *hash, const char *ignored, m4_symbol *symbol,
	   void *userdata)
{
  m4_set_symbol_traced (symbol, (boolean) (userdata != NULL));
  return NULL;
}

M4BUILTIN_HANDLER (traceon)
{
  int i;

  if (argc == 1)
    m4_symtab_apply (M4SYMTAB, set_trace_CB, (void *) obs);
  else
    for (i = 1; i < argc; i++)
      {
	const char *name = M4ARG (i);
	m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, name);
	if (symbol != NULL)
	  set_trace_CB (NULL, NULL, symbol, (char *) obs);
	else
	  M4WARN ((m4_get_warning_status_opt (context), 0,
		   _("Warning: %s: undefined name: %s"), M4ARG (0), name));
      }
}

/* Disable tracing of all specified macros, or all, if none is specified.  */
M4BUILTIN_HANDLER (traceoff)
{
  int i;

  if (argc == 1)
    m4_symtab_apply (M4SYMTAB, set_trace_CB, NULL);
  else
    for (i = 1; i < argc; i++)
      {
	const char *name = M4ARG (i);
	m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, name);
	if (symbol != NULL)
	  set_trace_CB (NULL, NULL, symbol, NULL);
	else
	  M4WARN ((m4_get_warning_status_opt (context), 0,
		   _("Warning: %s: undefined name: %s"), M4ARG (0), name));
      }
}


/* This section contains text processing macros: "len", "index",
   "substr", "translit", "format", "regexp" and "patsubst".  The last
   three are GNU specific.  */

/* Expand to the length of the first argument.  */
M4BUILTIN_HANDLER (len)
{
  m4_shipout_int (obs, strlen (M4ARG (1)));
}

/* The macro expands to the first index of the second argument in the first
   argument.  */
M4BUILTIN_HANDLER (index)
{
  const char *cp, *last;
  int l1, l2, retval;

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

/* The macro "substr" extracts substrings from the first argument, starting
   from the index given by the second argument, extending for a length
   given by the third argument.  If the third argument is missing, the
   substring extends to the end of the first argument.  */
M4BUILTIN_HANDLER (substr)
{
  int start, length, avail;

  length = avail = strlen (M4ARG (1));
  if (!m4_numeric_arg (context, argc, argv, 2, &start))
    return;

  if (argc == 4 && !m4_numeric_arg (context, argc, argv, 3, &length))
    return;

  if (start < 0 || length <= 0 || start >= avail)
    return;

  if (start + length > avail)
    length = avail - start;
  obstack_grow (obs, M4ARG (1) + start, length);
}


/* The macro "translit" translates all characters in the first argument,
   which are present in the second argument, into the corresponding
   character from the third argument.  If the third argument is shorter
   than the second, the extra characters in the second argument, are
   deleted from the first (pueh)  */
M4BUILTIN_HANDLER (translit)
{
  register const char *data, *tmp;
  const char *from, *to;
  int tolen;

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



/* The rest of this  file contains the functions to evaluate integer
 * expressions for the "eval" macro.  `number' should be at least 32 bits.
 */
#define int2numb(i) ((number)(i))
#define numb2int(n) ((n))

#define numb_set(ans,x) ((ans) = x)
#define numb_set_si(ans,si) (*(ans) = int2numb(si))

#define numb_init(x) x=((number)0)
#define numb_fini(x)

#define numb_decr(n) (n) -= 1

#define numb_ZERO ((number)0)
#define numb_ONE  ((number)1)

#define numb_zerop(x)     ((x) == numb_ZERO)
#define numb_positivep(x) ((x) >  numb_ZERO)
#define numb_negativep(x) ((x) <  numb_ZERO)

#define numb_eq(x,y) ((x) = ((x) == (y)))
#define numb_ne(x,y) ((x) = ((x) != (y)))
#define numb_lt(x,y) ((x) = ((x) <  (y)))
#define numb_le(x,y) ((x) = ((x) <= (y)))
#define numb_gt(x,y) ((x) = ((x) >  (y)))
#define numb_ge(x,y) ((x) = ((x) >= (y)))

#define numb_lnot(x)   ((x) = (! (x)))
#define numb_lior(x,y) ((x) = ((x) || (y)))
#define numb_land(x,y) ((x) = ((x) && (y)))

#define numb_not(c,x)   (*(x) = int2numb(~numb2int(*(x))))
#define numb_eor(c,x,y) (*(x) = int2numb(numb2int(*(x)) ^ numb2int(*(y))))
#define numb_ior(c,x,y) (*(x) = int2numb(numb2int(*(x)) | numb2int(*(y))))
#define numb_and(c,x,y) (*(x) = int2numb(numb2int(*(x)) & numb2int(*(y))))

#define numb_plus(x,y)  ((x) = ((x) + (y)))
#define numb_minus(x,y) ((x) = ((x) - (y)))
#define numb_negate(x)  ((x) = (- (x)))

#define numb_times(x,y)  ((x) = ((x) * (y)))
#define numb_ratio(x,y)  ((x) = ((x) / ((y))))
#define numb_divide(x,y) (*(x) = (*(x) / (*(y))))
#define numb_modulo(c,x,y) (*(x) = (*(x) % *(y)))
#define numb_invert(x)   ((x) = 1 / (x))

#define numb_lshift(c,x,y) (*(x) = (*(x) << *(y)))
#define numb_rshift(c,x,y) (*(x) = (*(x) >> *(y)))


/* The function ntoa () converts VALUE to a signed ascii representation in
   radix RADIX.  */
static const char *
ntoa (number value, int radix)
{
  /* Digits for number to ascii conversions.  */
  static char const ntoa_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  boolean negative;
  unumber uvalue;
  static char str[256];
  char *s = &str[sizeof str];

  *--s = '\0';

  if (value < 0)
    {
      negative = TRUE;
      uvalue = (unumber) -value;
    }
  else
    {
      negative = FALSE;
      uvalue = (unumber) value;
    }

  do
    {
      *--s = ntoa_digits[uvalue % radix];
      uvalue /= radix;
    }
  while (uvalue > 0);

  if (negative)
    *--s = '-';
  return s;
}

static void
numb_obstack(struct obstack *obs, const number value,
	     const int radix, int min)
{
  const char *s = ntoa (value, radix);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      min--;
      s++;
    }
  for (min -= strlen (s); --min >= 0;)
    obstack_1grow (obs, '0');

  obstack_grow (obs, s, strlen (s));
}


static void
numb_initialise (void)
{
  ;
}

/* This macro defines the top level code for the "eval" builtin.  The
   actual work is done in the function m4_evaluate (), which lives in
   evalparse.c.  */
#define m4_evaluate	builtin_eval
#include "evalparse.c"

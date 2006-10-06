/* GNU m4 -- A simple macro processor
   Copyright (C) 2000, 2002, 2003, 2004, 2006 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#include <m4module.h>
#include <modules/m4.h>

#ifdef NDEBUG
/* Include this header for speed, which gives us direct access to
   the fields of internal structures at the expense of maintaining
   interface/implementation separation.   The builtins in this file
   are the core of m4 and must be optimised for speed.  */
#  include "m4private.h"
#endif

#include "exitfail.h"

/* Rename exported symbols for dlpreload()ing.  */
#define m4_export_table		m4_LTX_m4_export_table
#define m4_builtin_table	m4_LTX_m4_builtin_table

#define m4_set_sysval		m4_LTX_m4_set_sysval
#define m4_sysval_flush		m4_LTX_m4_sysval_flush
#define m4_dump_symbols		m4_LTX_m4_dump_symbols
#define m4_expand_ranges	m4_LTX_m4_expand_ranges

extern void m4_set_sysval    (int value);
extern void m4_sysval_flush  (m4 *context);
extern void m4_dump_symbols  (m4 *context, m4_dump_symbol_data *data, int argc,
			      m4_symbol_value **argv, bool complain);
extern const char *m4_expand_ranges (const char *s, m4_obstack *obs);

/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions					\
  BUILTIN (changecom,	false,	false,	false,	0,	2  )	\
  BUILTIN (changequote,	false,	false,	false,	0,	2  )	\
  BUILTIN (decr,	false,	true,	true,	1,	1  )	\
  BUILTIN (define,	true,	true,	false,	1,	2  )	\
  BUILTIN (defn,	false,	true,	false,	1,	-1 )	\
  BUILTIN (divert,	false,	false,	false,	0,	1  )	\
  BUILTIN (divnum,	false,	false,	false,	0,	0  )	\
  BUILTIN (dnl,		false,	false,	false,	0,	0  )	\
  BUILTIN (dumpdef,	false,	false,	false,	0,	-1 )	\
  BUILTIN (errprint,	false,	true,	false,	1,	-1 )	\
  BUILTIN (eval,	false,	true,	true,	1,	3  )	\
  BUILTIN (ifdef,	false,	true,	false,	2,	3  )	\
  BUILTIN (ifelse,	false,	true,	false,	1,	-1 )	\
  BUILTIN (include,	false,	true,	false,	1,	1  )	\
  BUILTIN (incr,	false,	true,	true,	1,	1  )	\
  BUILTIN (index,	false,	true,	true,	2,	2  )	\
  BUILTIN (len,		false,	true,	true,	1,	1  )	\
  BUILTIN (m4exit,	false,	false,	false,	0,	1  )	\
  BUILTIN (m4wrap,	false,	true,	false,	1,	-1 )	\
  BUILTIN (maketemp,	false,	true,	false,	1,	1  )	\
  BUILTIN (popdef,	false,	true,	false,	1,	-1 )	\
  BUILTIN (pushdef,	true,	true,	false,	1,	2  )	\
  BUILTIN (shift,	false,	true,	false,	1,	-1 )	\
  BUILTIN (sinclude,	false,	true,	false,	1,	1  )	\
  BUILTIN (substr,	false,	true,	true,	2,	3  )	\
  BUILTIN (syscmd,	false,	true,	true,	1,	1  )	\
  BUILTIN (sysval,	false,	false,	false,	0,	0  )	\
  BUILTIN (traceoff,	false,	false,	false,	0,	-1 )	\
  BUILTIN (traceon,	false,	false,	false,	0,	-1 )	\
  BUILTIN (translit,	false,	true,	true,	2,	3  )	\
  BUILTIN (undefine,	false,	true,	false,	1,	-1 )	\
  BUILTIN (undivert,	false,	false,	false,	0,	-1 )	\


#if defined(SIZEOF_LONG_LONG_INT) && SIZEOF_LONG_LONG_INT > 0
/* Use GNU long long int if available.  */
typedef long long int number;
typedef unsigned long long int unumber;
#else
typedef long int number;
typedef unsigned long int unumber;
#endif

static void	include		(m4 *context, int argc, m4_symbol_value **argv,
				 bool silent);
static int	dumpdef_cmp_CB	(const void *s1, const void *s2);
static void *	dump_symbol_CB  (m4_symbol_table *ignored, const char*name,
				 m4_symbol *symbol, void *userdata);
static const char *ntoa		(number value, int radix);
static void	numb_obstack	(m4_obstack *obs, const number value,
				 const int radix, int min);


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind, side, min, max) M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)	\
  { CONC(builtin_, handler), STR(handler),		\
    ((macros ? M4_BUILTIN_GROKS_MACRO : 0)		\
     | (blind ? M4_BUILTIN_BLIND : 0)			\
     | (side ? M4_BUILTIN_SIDE_EFFECT : 0)),		\
    min, max },
  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};



/* This module cannot be safely unloaded from memory, incase the unload
   is triggered by m4exit, and the module is removed while m4exit is in
   progress.  */
M4INIT_HANDLER (m4)
{
  if (handle && lt_dlmakeresident (handle) != 0)
    {
      m4_error (context, 0, 0, _("cannot make module `%s' resident: %s"),
		m4_get_module_name (handle), lt_dlerror ());
    }
}



/* The rest of this file is code for builtins and expansion of user
   defined macros.  All the functions for builtins have a prototype as:

	void builtin_MACRONAME (m4_obstack *obs, int argc, char *argv[]);

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
  else
    m4_warn (context, 0, _("%s: invalid macro name ignored"), M4ARG (0));
}

M4BUILTIN_HANDLER (undefine)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      const char *name = M4ARG (i);

      if (!m4_symbol_lookup (M4SYMTAB, name))
	m4_warn (context, 0, _("%s: undefined macro `%s'"), M4ARG (0), name);
      else
	m4_symbol_delete (M4SYMTAB, name);
    }
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
  else
    m4_warn (context, 0, _("%s: invalid macro name ignored"), M4ARG (0));
}

M4BUILTIN_HANDLER (popdef)
{
  int i;
  for (i = 1; i < argc; i++)
    {
      const char *name = M4ARG (i);

      if (!m4_symbol_lookup (M4SYMTAB, name))
	m4_warn (context, 0, _("%s: undefined macro `%s'"), M4ARG (0), name);
      else
	m4_symbol_popdef (M4SYMTAB, name);
    }
}




/* --- CONDITIONALS OF M4 --- */


M4BUILTIN_HANDLER (ifdef)
{
  m4_symbol *symbol;
  const char *result;

  symbol = m4_symbol_lookup (M4SYMTAB, M4ARG (1));

  if (symbol)
    result = M4ARG (2);
  else if (argc >= 4)
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

  if (m4_bad_argc (context, argc, argv, 3, -1, false))
    return;
  else if (argc % 3 == 0)
    /* Diagnose excess arguments if 5, 8, 11, etc., actual arguments.  */
    m4_bad_argc (context, argc, argv, 0, argc - 2, false);

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


/* qsort comparison routine, for sorting the table made in m4_dumpdef ().  */
static int
dumpdef_cmp_CB (const void *s1, const void *s2)
{
  return strcmp (*(const char **) s1, *(const char **) s2);
}

/* The function m4_dump_symbols () is for use by "dumpdef".  It builds up a
   table of all defined symbol names.  */
static void *
dump_symbol_CB (m4_symbol_table *ignored, const char *name, m4_symbol *symbol,
		void *userdata)
{
  m4_dump_symbol_data *symbol_data = (m4_dump_symbol_data *) userdata;

  assert (name);
  assert (symbol);
  assert (!m4_is_symbol_value_void (m4_get_symbol_value (symbol)));

  if (symbol_data->size == 0)
    {
      obstack_ptr_grow (symbol_data->obs, name);
      symbol_data->size = (obstack_room (symbol_data->obs)
			   / sizeof (const char *));
    }
  else
    {
      obstack_ptr_grow_fast (symbol_data->obs, name);
      symbol_data->size--;
    }

  return NULL;
}

/* If there are no arguments, build a sorted list of all defined
   symbols, otherwise, only the specified symbols.  */
void
m4_dump_symbols (m4 *context, m4_dump_symbol_data *data, int argc,
		 m4_symbol_value **argv, bool complain)
{
  assert (obstack_object_size (data->obs) == 0);
  data->size = obstack_room (data->obs) / sizeof (const char *);

  if (argc == 1)
    {
      m4_symtab_apply (M4SYMTAB, dump_symbol_CB, data);
    }
  else
    {
      int i;
      m4_symbol *symbol;

      for (i = 1; i < argc; i++)
	{
	  symbol = m4_symbol_lookup (M4SYMTAB, M4ARG (i));
	  if (symbol != NULL)
	    dump_symbol_CB (NULL, M4ARG (i), symbol, data);
	  else if (complain)
	    m4_warn (context, 0, _("%s: undefined macro `%s'"),
		     M4ARG (0), M4ARG (i));
	}
    }

  data->size = obstack_object_size (data->obs) / sizeof (const char *);
  data->base = (const char **) obstack_finish (data->obs);
  qsort (data->base, data->size, sizeof (const char *), dumpdef_cmp_CB);
}


/* Implementation of "dumpdef" itself.  It builds up a table of pointers to
   symbols, sorts it and prints the sorted table.  */
M4BUILTIN_HANDLER (dumpdef)
{
  m4_dump_symbol_data data;
  bool quote = m4_is_debug_bit (context, M4_DEBUG_TRACE_QUOTE);
  const char *lquote = m4_get_syntax_lquote (M4SYNTAX);
  const char *rquote = m4_get_syntax_rquote (M4SYNTAX);
  bool stack = m4_is_debug_bit (context, M4_DEBUG_TRACE_STACK);
  size_t arg_length = m4_get_max_debug_arg_length_opt (context);
  bool module = m4_is_debug_bit (context, M4_DEBUG_TRACE_MODULE);

  data.obs = obs;
  m4_dump_symbols (context, &data, argc, argv, true);

  for (; data.size > 0; --data.size, data.base++)
    {
      m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, data.base[0]);
      assert (symbol);

      obstack_grow (obs, data.base[0], strlen (data.base[0]));
      obstack_1grow (obs, ':');
      obstack_1grow (obs, '\t');
      m4_symbol_print (symbol, obs, quote, lquote, rquote, stack, arg_length,
		       module);
      obstack_1grow (obs, '\n');
    }

  obstack_1grow (obs, '\0');
  m4_sysval_flush (context);
  fputs ((char *) obstack_finish (obs), stderr);
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
	m4_warn (context, 0, _("%s: undefined macro `%s'"), M4ARG (0), name);
      else if (m4_is_symbol_text (symbol))
	m4_shipout_string (context, obs, m4_get_symbol_text (symbol), 0, true);
      else if (m4_is_symbol_func (symbol))
	m4_push_builtin (m4_get_symbol_value (symbol));
      else if (m4_is_symbol_placeholder (symbol))
	m4_warn (context, 0,
		 _("%s: builtin `%s' requested by frozen file not found"),
		 name, m4_get_symbol_placeholder (symbol));
      else
	{
	  assert (!"Bad token data type in m4_defn");
	  abort ();
	}
    }
}


/* This section contains macros to handle the builtins "syscmd"
   and "sysval".  */

/* Exit code from last "syscmd" command.  */
/* FIXME - we should preserve this value across freezing.  See
   http://lists.gnu.org/archive/html/bug-m4/2006-06/msg00059.html
   for ideas on how do to that.  */
static int  m4_sysval = 0;

/* Helper macros for readability.  */
#if UNIX || defined WEXITSTATUS
# define M4_SYSVAL_EXITBITS(status)			\
   (WIFEXITED (status) ? WEXITSTATUS (status) : 0)
# define M4_SYSVAL_TERMSIGBITS(status)			\
   (WIFSIGNALED (status) ? WTERMSIG (status) << 8 : 0)

#else /* ! UNIX && ! defined WEXITSTATUS */
/* Platforms such as mingw do not support the notion of reporting
   which signal terminated a process.  Furthermore if WEXITSTATUS was
   not provided, then the exit value is in the low eight bits.  */
# define M4_SYSVAL_EXITBITS(status) status
# define M4_SYSVAL_TERMSIGBITS(status) 0
#endif /* ! UNIX && ! defined WEXITSTATUS */

/* Fallback definitions if <stdlib.h> or <sys/wait.h> are inadequate.  */
/* FIXME - this may fit better as a gnulib module.  */
#ifndef WEXITSTATUS
# define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#endif
#ifndef WTERMSIG
# define WTERMSIG(status) ((status) & 0x7f)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(status) (WTERMSIG (status) != 0)
#endif
#ifndef WIFEXITED
# define WIFEXITED(status) (WTERMSIG (status) == 0)
#endif

void
m4_set_sysval (int value)
{
  m4_sysval = value;
}

void
m4_sysval_flush (m4 *context)
{
  FILE *debug_file = m4_get_debug_file (context);

  if (debug_file != stdout) fflush (stdout);
  if (debug_file != stderr) fflush (stderr);
  if (debug_file != NULL)   fflush (debug_file);
}

M4BUILTIN_HANDLER (syscmd)
{
   if (m4_get_safer_opt (context))
   {
     m4_error (context, 0, 0, _("%s: disabled by --safer"), M4ARG (0));
     return;
   }

   /* Optimize the empty command.  */
  if (*M4ARG (1) == '\0')
    {
      m4_set_sysval (0);
      return;
    }
  m4_sysval_flush (context);
  m4_sysval = system (M4ARG (1));
  /* FIXME - determine if libtool works for OS/2, in which case the
     FUNC_SYSTEM_BROKEN section on the branch must be ported to work
     around the bug in their EMX libc system().  */
}


M4BUILTIN_HANDLER (sysval)
{
  m4_shipout_int (obs, (m4_sysval == -1 ? 127
			: (M4_SYSVAL_EXITBITS (m4_sysval)
			   | M4_SYSVAL_TERMSIGBITS (m4_sysval))));
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

  m4_make_diversion (context, i);
}

/* Expand to the current diversion number, -1 if none.  */
M4BUILTIN_HANDLER (divnum)
{
  m4_shipout_int (obs, m4_get_current_diversion (context));
}

/* Bring back the diversion given by the argument list.  If none is
   specified, bring back all diversions.  GNU specific is the option
   of undiverting the named file, by passing a non-numeric argument to
   undivert ().  */

M4BUILTIN_HANDLER (undivert)
{
  int i = 0;

  if (argc == 1)
    m4_undivert_all (context);
  else
    {
      if (sscanf (M4ARG (1), "%d", &i) == 1)
	m4_insert_diversion (context, i);
      else if (m4_get_posixly_correct_opt (context))
	m4_numeric_arg (context, argc, argv, 1, &i);
      else
	{
	  FILE *fp = m4_path_search (context, M4ARG (1), (char **) NULL);
	  if (fp != NULL)
	    {
	      m4_insert_file (context, fp);
	      fclose (fp);
	    }
	  else
	    m4_error (context, 0, errno, _("%s: cannot undivert `%s'"),
		      M4ARG (0), M4ARG (1));
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
  m4_skip_line (context, M4ARG (0));
}

/* Shift all argument one to the left, discarding the first argument.  Each
   output argument is quoted with the current quotes.  */
M4BUILTIN_HANDLER (shift)
{
  m4_dump_args (context, obs, argc - 1, argv + 1, ",", true);
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
   if it exists.  Complain about inaccesible files iff SILENT is false.  */
static void
include (m4 *context, int argc, m4_symbol_value **argv, bool silent)
{
  FILE *fp;
  char *name = NULL;

  fp = m4_path_search (context, M4ARG (1), &name);
  if (fp == NULL)
    {
      if (!silent)
	m4_error (context, 0, errno, _("%s: cannot open `%s'"), M4ARG (0),
		  M4ARG (1));
      return;
    }

  m4_push_file (context, fp, name, true);
  free (name);
}

/* Include a file, complaining in case of errors.  */
M4BUILTIN_HANDLER (include)
{
  include (context, argc, argv, false);
}

/* Include a file, ignoring errors.  */
M4BUILTIN_HANDLER (sinclude)
{
  include (context, argc, argv, true);
}


/* More miscellaneous builtins -- "maketemp", "errprint".  */

/* Use the first argument as at template for a temporary file name.  */
M4BUILTIN_HANDLER (maketemp)
{
  int fd;

  if (m4_get_safer_opt (context))
    {
      m4_error (context, 0, 0, _("%s: disabled by --safer"), M4ARG (0));
      return;
    }

  errno = 0;
  if ((fd = mkstemp (M4ARG(1))) < 0)
    {
      m4_error (context, 0, errno, _("%s: cannot create tempfile `%s'"),
		M4ARG (0), M4ARG (1));
      return;
    }
  close (fd);
  m4_shipout_string (context, obs, M4ARG (1), 0, false);
}

/* Print all arguments on standard error.  */
M4BUILTIN_HANDLER (errprint)
{
  assert (obstack_object_size (obs) == 0);
  m4_dump_args (context, obs, argc, argv, " ", false);
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
  int exit_code = EXIT_SUCCESS;

  /* Warn on bad arguments, but still exit.  */
  if (argc >= 2 && !m4_numeric_arg (context, argc, argv, 1, &exit_code))
    exit_code = EXIT_FAILURE;
  if (exit_code < 0 || exit_code > 255)
    {
      m4_warn (context, 0, _("%s: exit status out of range: `%d'"),
	       M4ARG (0), exit_code);
      exit_code = EXIT_FAILURE;
    }

  /* Ensure that atexit handlers see correct nonzero status.  */
  if (exit_code != EXIT_SUCCESS)
    exit_failure = exit_code;

  /* Ensure any module exit callbacks are executed.  */
  m4__module_exit (context);

  /* Change debug stream back to stderr, to force flushing debug
     stream and detect any errors.  */
  m4_debug_set_output (context, NULL);

  /* Check for saved error.  */
  if (exit_code == 0 && m4_get_exit_status (context) != 0)
    exit_code = m4_get_exit_status (context);
  exit (exit_code);
}

/* Save the argument text until EOF has been seen, allowing for user
   specified cleanup action.  GNU version saves all arguments, the standard
   version only the first.  */
M4BUILTIN_HANDLER (m4wrap)
{
  assert (obstack_object_size (obs) == 0);
  if (m4_get_no_gnu_extensions_opt (context))
    m4_shipout_string (context, obs, M4ARG (1), 0, false);
  else
    m4_dump_args (context, obs, argc, argv, " ", false);
  obstack_1grow (obs, '\0');
  m4_push_wrapup (obstack_finish (obs));
}

/* Enable tracing of all specified macros, or all, if none is specified.
   Tracing is disabled by default, when a macro is defined.  This can be
   overridden by the "t" debug flag.  */

M4BUILTIN_HANDLER (traceon)
{
  int i;

  if (argc == 1)
    m4_set_debug_level_opt (context, (m4_get_debug_level_opt (context)
				      | M4_DEBUG_TRACE_ALL));
  else
    for (i = 1; i < argc; i++)
      m4_set_symbol_name_traced (M4SYMTAB, M4ARG (i), true);
}

/* Disable tracing of all specified macros, or all, if none is specified.  */
M4BUILTIN_HANDLER (traceoff)
{
  int i;

  if (argc == 1)
    m4_set_debug_level_opt (context, (m4_get_debug_level_opt (context)
				      & ~M4_DEBUG_TRACE_ALL));
  else
    for (i = 1; i < argc; i++)
      m4_set_symbol_name_traced (M4SYMTAB, M4ARG (i), false);
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


/* Ranges are expanded by the following function, and the expanded strings,
   without any ranges left, are used to translate the characters of the
   first argument.  A single - (dash) can be included in the strings by
   being the first or the last character in the string.  If the first
   character in a range is after the first in the character set, the range
   is made backwards, thus 9-0 is the string 9876543210.  */
const char *
m4_expand_ranges (const char *s, m4_obstack *obs)
{
  char from;
  char to;

  assert (obstack_object_size (obs) == 0);
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

  bool negative;
  unumber uvalue;
  static char str[256];
  char *s = &str[sizeof str];

  *--s = '\0';

  if (value < 0)
    {
      negative = true;
      uvalue = (unumber) -value;
    }
  else
    {
      negative = false;
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
numb_obstack(m4_obstack *obs, const number value,
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

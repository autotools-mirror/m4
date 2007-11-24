/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1998, 1999, 2003,
   2006, 2007 Free Software Foundation, Inc.

   This file is part of GNU M4.

   GNU M4 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU M4 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include "m4private.h"

#include "exitfail.h"
#include "progname.h"
#include "verror.h"
#include "xvasprintf.h"

static const char *skip_space (m4 *, const char *);



/* Give friendly warnings if a builtin macro is passed an
   inappropriate number of arguments.  MIN is the 0-based minimum
   number of acceptable arguments, MAX is the 0-based maximum number
   or UINT_MAX if not applicable, and SIDE_EFFECT is true if the macro
   has side effects even if min is not satisfied.  ARGC is the 1-based
   count of supplied arguments, and CALLER is the name of the macro.
   Return true if the macro is guaranteed to expand to the empty
   string, false otherwise.  */
bool
m4_bad_argc (m4 *context, int argc, const char *caller, unsigned int min,
	     unsigned int max, bool side_effect)
{
  if (argc - 1 < min)
    {
      m4_warn (context, 0, caller, _("too few arguments: %d < %d"),
	       argc - 1, min);
      return !side_effect;
    }

  if (argc - 1 > max)
    {
      m4_warn (context, 0, caller, _("extra arguments ignored: %d > %d"),
	       argc - 1, max);
    }

  return false;
}

static const char *
skip_space (m4 *context, const char *arg)
{
  while (m4_has_syntax (M4SYNTAX, to_uchar (*arg), M4_SYNTAX_SPACE))
    arg++;
  return arg;
}

/* The function m4_numeric_arg () converts ARG to an int pointed to by
   VALUEP. If the conversion fails, print error message for CALLER.
   Return true iff conversion succeeds.  */
/* FIXME: Convert this to use gnulib's xstrtoimax, xstrtoumax.
   Otherwise, we are arbitrarily limiting integer values.  */
bool
m4_numeric_arg (m4 *context, const char *caller, const char *arg, int *valuep)
{
  char *endp;

  if (*arg == '\0')
    {
      *valuep = 0;
      m4_warn (context, 0, caller, _("empty string treated as 0"));
    }
  else
    {
      *valuep = strtol (skip_space (context, arg), &endp, 10);
      if (*skip_space (context, endp) != 0)
	{
	  m4_warn (context, 0, caller, _("non-numeric argument `%s'"), arg);
	  return false;
	}
    }
  return true;
}


/* Print ARGC arguments from the table ARGV to obstack OBS, separated by
   SEP, and quoted by the current quotes, if QUOTED is true.  */
void
m4_dump_args (m4 *context, m4_obstack *obs, int argc,
	      m4_symbol_value **argv, const char *sep, bool quoted)
{
  int i;
  size_t len = strlen (sep);

  for (i = 1; i < argc; i++)
    {
      if (i > 1)
	obstack_grow (obs, sep, len);

      m4_shipout_string (context, obs, M4ARG (i), 0, quoted);
    }
}


/* Parse ARG as a truth value.  If unrecognized, issue a warning on
   behalf of ME and return PREVIOUS; otherwise return the parsed
   value.  */
bool
m4_parse_truth_arg (m4 *context, const char *arg, const char *me,
		    bool previous)
{
  /* 0, no, off, blank... */
  if (!arg || arg[0] == '\0'
      || arg[0] == '0'
      || arg[0] == 'n' || arg[0] == 'N'
      || ((arg[0] == 'o' || arg[0] == 'O')
	  && (arg[1] == 'f' || arg[1] == 'F')))
    return false;
  /* 1, yes, on... */
  if (arg[0] == '1'
      || arg[0] == 'y' || arg[0] == 'Y'
      || ((arg[0] == 'o' || arg[0] == 'O')
	  && (arg[1] == 'n' || arg[1] == 'N')))
    return true;
  m4_warn (context, 0, me, _("unknown directive `%s'"), arg);
  return previous;
}

/* Helper for all error reporting.  Report message based on FORMAT and
   ARGS, on behalf of MACRO, at the optional location FILE and LINE.
   If ERRNUM, decode the errno value as part of the message.  If
   STATUS, exit immediately with that status.  If WARN, prepend
   'Warning: '.  */
static void
m4_verror_at_line (m4 *context, bool warn, int status, int errnum,
		   const char *file, int line, const char *macro,
		   const char *format, va_list args)
{
  char *full = NULL;
  /* Prepend warning and the macro name, as needed.  But if that fails
     for non-memory reasons (unlikely), then still use the original
     format.  */
  if (warn && macro)
    full = xasprintf (_("Warning: %s: %s"), macro, format);
  else if (warn)
    full = xasprintf (_("Warning: %s"), format);
  else if (macro)
    full = xasprintf (_("%s: %s"), macro, format);
  verror_at_line (status, errnum, line ? file : NULL, line,
		  full ? full : format, args);
  free (full);
  if ((!warn || m4_get_fatal_warnings_opt (context))
      && !m4_get_exit_status (context))
    m4_set_exit_status (context, EXIT_FAILURE);
}

/* Issue an error.  The message is printf-style, based on FORMAT and
   any other arguments, and the program name and location (if we are
   currently parsing an input file) are automatically prepended.  If
   ERRNUM is non-zero, include strerror output in the message.  If
   MACRO, prepend the message with the macro where the message
   occurred.  If STATUS is non-zero, or if errors are fatal, call exit
   immediately; otherwise, remember that an error occurred so that m4
   cannot exit with success later on.*/
void
m4_error (m4 *context, int status, int errnum, const char *macro,
	  const char *format, ...)
{
  va_list args;
  int line = m4_get_current_line (context);
  assert (m4_get_current_file (context) || !line);
  va_start (args, format);
  if (status == EXIT_SUCCESS && m4_get_warnings_exit_opt (context))
    status = EXIT_FAILURE;
  m4_verror_at_line (context, false, status, errnum,
		     m4_get_current_file (context), line, macro, format, args);
  va_end (args);
}

/* Issue an error.  The message is printf-style, based on FORMAT and
   any other arguments, and the program name and location (from FILE
   and LINE) are automatically prepended.  If ERRNUM is non-zero,
   include strerror output in the message.  If STATUS is non-zero, or
   if errors are fatal, call exit immediately; otherwise, remember
   that an error occurred so that m4 cannot exit with success later
   on.  If MACRO, prepend the message with the macro where the message
   occurred.  */
void
m4_error_at_line (m4 *context, int status, int errnum, const char *file,
		  int line, const char *macro, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  if (status == EXIT_SUCCESS && m4_get_warnings_exit_opt (context))
    status = EXIT_FAILURE;
  m4_verror_at_line (context, false, status, errnum, file, line, macro,
		     format, args);
  va_end (args);
}

/* Issue a warning, if they are not being suppressed.  The message is
   printf-style, based on FORMAT and any other arguments, and the
   program name, location (if we are currently parsing an input file),
   and "Warning:" are automatically prepended.  If ERRNUM is non-zero,
   include strerror output in the message.  If MACRO, prepend the
   message with the macro where the message occurred.  If warnings are
   fatal, call exit immediately, otherwise exit status is
   unchanged.  */
void
m4_warn (m4 *context, int errnum, const char *macro, const char *format, ...)
{
  if (!m4_get_suppress_warnings_opt (context))
    {
      va_list args;
      int status = EXIT_SUCCESS;
      int line = m4_get_current_line (context);
      assert (m4_get_current_file (context) || !line);
      va_start (args, format);
      if (m4_get_warnings_exit_opt (context))
	status = EXIT_FAILURE;
      m4_verror_at_line (context, true, status, errnum,
			 m4_get_current_file (context), line, macro, format,
			 args);
      va_end (args);
    }
}

/* Issue a warning, if they are not being suppressed.  The message is
   printf-style, based on FORMAT and any other arguments, and the
   program name, location (from FILE and LINE), and "Warning:" are
   automatically prepended.  If ERRNUM is non-zero, include strerror
   output in the message.  If MACRO, prepend the message with the
   macro where the message occurred.  If warnings are fatal, call exit
   immediately, otherwise exit status is unchanged.  */
void
m4_warn_at_line (m4 *context, int errnum, const char *file, int line,
		 const char *macro, const char *format, ...)
{
  if (!m4_get_suppress_warnings_opt (context))
    {
      va_list args;
      int status = EXIT_SUCCESS;
      va_start (args, format);
      if (m4_get_warnings_exit_opt (context))
	status = EXIT_FAILURE;
      m4_verror_at_line (context, true, status, errnum, file, line, macro,
			 format, args);
      va_end (args);
    }
}


/* Wrap the gnulib progname module, to avoid exporting a global
   variable from a library.  Retrieve the program name for use in
   error messages and the __program__ macro.  */
const char *
m4_get_program_name (void)
{
  return program_name;
}

/* Wrap the gnulib progname module, to avoid exporting a global
   variable from a library.  Set the program name for use in error
   messages and the __program__ macro to argv[0].  */
void
m4_set_program_name (const char *name)
{
  set_program_name (name);
}

/* Wrap the gnulib exitfail module, to avoid exporting a global
   variable from a library.  Set the exit status for use in gnulib
   modules and atexit handlers.  */
void
m4_set_exit_failure (int status)
{
  exit_failure = status;
}

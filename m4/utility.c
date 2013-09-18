/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1998-1999, 2003, 2006-2010, 2013 Free
   Software Foundation, Inc.

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
#include "quotearg.h"
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
m4_bad_argc (m4 *context, size_t argc, const m4_call_info *caller, size_t min,
             size_t max, bool side_effect)
{
  if (argc - 1 < min)
    {
      m4_warn (context, 0, caller, _("too few arguments: %zu < %zu"),
               argc - 1, min);
      return !side_effect;
    }

  if (argc - 1 > max)
    {
      m4_warn (context, 0, caller, _("extra arguments ignored: %zu > %zu"),
               argc - 1, max);
    }

  return false;
}

static const char *
skip_space (m4 *context, const char *arg)
{
  while (m4_has_syntax (M4SYNTAX, *arg, M4_SYNTAX_SPACE))
    arg++;
  return arg;
}

/* The function m4_numeric_arg () converts ARG of length LEN to an int
   pointed to by VALUEP. If the conversion fails, print error message
   for CALLER.  Return true iff conversion succeeds.  */
/* FIXME: Convert this to use gnulib's xstrtoimax, xstrtoumax.
   Otherwise, we are arbitrarily limiting integer values.  */
bool
m4_numeric_arg (m4 *context, const m4_call_info *caller, const char *arg,
                size_t len, int *valuep)
{
  char *endp;

  if (!len)
    {
      *valuep = 0;
      m4_warn (context, 0, caller, _("empty string treated as 0"));
    }
  else
    {
      const char *str = skip_space (context, arg);
      *valuep = strtol (str, &endp, 10);
      if (endp - arg != len)
        {
          m4_warn (context, 0, caller, _("non-numeric argument %s"),
                   quotearg_style_mem (locale_quoting_style, arg, len));
          return false;
        }
      if (str != arg)
        m4_warn (context, 0, caller, _("leading whitespace ignored"));
      else if (errno == ERANGE)
        m4_warn (context, 0, caller, _("numeric overflow detected"));
    }
  return true;
}

/* Parse ARG of length LEN as a truth value.  If ARG is NUL, use ""
   instead; otherwise, ARG must have a NUL-terminator (even if it also
   has embedded NUL).  If LEN is SIZE_MAX, use the string length of
   ARG.  If unrecognized, issue a warning on behalf of CALLER and
   return PREVIOUS; otherwise return the parsed value.  */
bool
m4_parse_truth_arg (m4 *context, const m4_call_info *caller, const char *arg,
                    size_t len, bool previous)
{
  /* 0, no, off, blank... */
  if (!arg || len == 0
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
  m4_warn (context, 0, caller, _("unknown directive %s"),
           quotearg_style_mem (locale_quoting_style, arg, len));
  return previous;
}

/* Helper method to look up a symbol table entry given an argument.
   Warn on behalf of ARGV if VALUE is not a text argument, or if
   MUST_EXIST and no macro exists by the name in VALUE.  Return the
   result of the lookup, or NULL.  */
m4_symbol *
m4_symbol_value_lookup (m4 *context, m4_macro_args *argv, size_t i,
                        bool must_exist)
{
  m4_symbol *result = NULL;
  if (m4_is_arg_text (argv, i))
    {
      const char *name = M4ARG (i);
      size_t len = M4ARGLEN (i);
      result = m4_symbol_lookup (M4SYMTAB, name, len);
      if (must_exist && !result
          && m4_is_debug_bit (context, M4_DEBUG_TRACE_DEREF))
        m4_warn (context, 0, argv->info, _("undefined macro %s"),
                 quotearg_style_mem (locale_quoting_style, name, len));
    }
  else
    m4_warn (context, 0, argv->info, _("invalid macro name ignored"));
  return result;
}

/* Return an escaped version of the macro name corresponding to
   CALLER, for use in error messages that do not use the m4_warn
   machinery.  This call occupies slot 0 of the quotearg
   machinery.  */
const char *m4_info_name (const m4_call_info *caller)
{
  return quotearg_style_mem (locale_quoting_style, caller->name,
                             caller->name_len);
}

/* Helper for all error reporting.  Report message based on FORMAT and
   ARGS, on behalf of CALLER (if any), otherwise at the global
   position in CONTEXT.  If ERRNUM, decode the errno value as part of
   the message.  If STATUS, exit immediately with that status.  If
   WARN, prepend 'warning: '.  */
static void
m4_verror_at_line (m4 *context, bool warn, int status, int errnum,
                   const m4_call_info *caller, const char *format,
                   va_list args)
{
  char *full = NULL;
  char *safe_macro = NULL;
  const char *macro = caller ? caller->name : NULL;
  size_t len = caller ? caller->name_len : 0;
  const char *file = caller ? caller->file : m4_get_current_file (context);
  int line = caller ? caller->line : m4_get_current_line (context);

  assert (file || !line);
  /* Sanitize MACRO, since we are turning around and using it in a
     format string.  The allocation is overly conservative, but
     problematic macro names only occur via indir or changesyntax.  */
  if (macro && memchr (macro, '%', len))
    {
      char *p = safe_macro = xcharalloc (2 * len);
      const char *end = macro + len;
      while (macro != end)
        {
          if (*macro == '%')
            {
              *p++ = '%';
              len++;
            }
          *p++ = *macro++;
        }
    }
  if (macro)
    /* Use slot 1, so that the rest of the code can use the simpler
       quotearg interface in slot 0.  */
    macro = quotearg_n_mem (1, safe_macro ? safe_macro : macro, len);
  /* Prepend warning and the macro name, as needed.  But if that fails
     for non-memory reasons (unlikely), then still use the original
     format.  */
  if (warn && macro)
    full = xasprintf (_("warning: %s: %s"), macro, format);
  else if (warn)
    full = xasprintf (_("warning: %s"), format);
  else if (macro)
    full = xasprintf (_("%s: %s"), macro, format);
  verror_at_line (status, errnum, line ? file : NULL, line,
                  full ? full : format, args);
  free (full);
  free (safe_macro);
  if ((!warn || m4_get_fatal_warnings_opt (context))
      && !m4_get_exit_status (context))
    m4_set_exit_status (context, EXIT_FAILURE);
}

/* Issue an error.  The message is printf-style, based on FORMAT and
   any other arguments, and the program name and location (if we are
   currently parsing an input file) are automatically prepended.  If
   ERRNUM is non-zero, include strerror output in the message.  If
   CALLER, prepend the message with the macro where the message
   occurred.  If STATUS is non-zero, or if errors are fatal, call exit
   immediately; otherwise, remember that an error occurred so that m4
   cannot exit with success later on.*/
void
m4_error (m4 *context, int status, int errnum, const m4_call_info *caller,
          const char *format, ...)
{
  va_list args;
  va_start (args, format);
  if (status == EXIT_SUCCESS && m4_get_warnings_exit_opt (context))
    status = EXIT_FAILURE;
  m4_verror_at_line (context, false, status, errnum, caller, format, args);
  va_end (args);
}

/* Issue a warning, if they are not being suppressed.  The message is
   printf-style, based on FORMAT and any other arguments, and the
   program name, location (if we are currently parsing an input file),
   and "warning:" are automatically prepended.  If ERRNUM is non-zero,
   include strerror output in the message.  If CALLER, prepend the
   message with the macro where the message occurred.  If warnings are
   fatal, call exit immediately, otherwise exit status is
   unchanged.  */
void
m4_warn (m4 *context, int errnum, const m4_call_info *caller,
         const char *format, ...)
{
  if (!m4_get_suppress_warnings_opt (context))
    {
      va_list args;
      int status = EXIT_SUCCESS;
      va_start (args, format);
      if (m4_get_warnings_exit_opt (context))
        status = EXIT_FAILURE;
      m4_verror_at_line (context, true, status, errnum, caller, format, args);
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

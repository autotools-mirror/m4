/* GNU m4 -- A simple macro processor
   Copyright (C) 1991-1994, 2006-2010, 2013 Free Software Foundation,
   Inc.

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

#include <sys/stat.h>
#include <stdarg.h>

#include "m4private.h"
#include "close-stream.h"

static void set_debug_file (m4 *, const m4_call_info *, FILE *);



/* Function to decode the debugging flags OPTS of length LEN; or
   SIZE_MAX if OPTS is NUL-terminated.  If OPTS is NULL, use the
   default flags.  Used by main while processing option -d, and by the
   builtin debugmode ().  */
int
m4_debug_decode (m4 *context, const char *opts, size_t len)
{
  int previous = context->debug_level;
  int level;
  char mode = '\0';

  if (!opts)
    opts = "";
  if (len == SIZE_MAX)
    len = strlen (opts);
  if (!len)
    level = M4_DEBUG_TRACE_DEFAULT | previous;
  else
    {
      if (*opts == '-' || *opts == '+')
        {
          len--;
          mode = *opts++;
        }
      for (level = 0; len--; opts++)
        {
          switch (*opts)
            {
            case 'a':
              level |= M4_DEBUG_TRACE_ARGS;
              break;

            case 'e':
              level |= M4_DEBUG_TRACE_EXPANSION;
              break;

            case 'q':
              level |= M4_DEBUG_TRACE_QUOTE;
              break;

            case 't':
              level |= M4_DEBUG_TRACE_ALL;
              break;

            case 'l':
              level |= M4_DEBUG_TRACE_LINE;
              break;

            case 'f':
              level |= M4_DEBUG_TRACE_FILE;
              break;

            case 'p':
              level |= M4_DEBUG_TRACE_PATH;
              break;

            case 'c':
              level |= M4_DEBUG_TRACE_CALL;
              break;

            case 'i':
              level |= M4_DEBUG_TRACE_INPUT;
              break;

            case 'x':
              level |= M4_DEBUG_TRACE_CALLID;
              break;

            case 'm':
              level |= M4_DEBUG_TRACE_MODULE;
              break;

            case 's':
              level |= M4_DEBUG_TRACE_STACK;
              break;

            case 'd':
              level |= M4_DEBUG_TRACE_DEREF;
              break;

            case 'o':
               level |= M4_DEBUG_TRACE_OUTPUT_DUMPDEF;
               break;

            case 'V':
              level |= M4_DEBUG_TRACE_VERBOSE;
              break;

            default:
              return -1;
            }
        }
    }

  switch (mode)
    {
    case '\0':
      /* Replace old level.  */
      break;

    case '-':
      /* Subtract flags.  */
      level = previous & ~level;
      break;

    case '+':
      /* Add flags.  */
      level |= previous;
      break;

    default:
      assert (!"m4_debug_decode");
    }
  context->debug_level = level;
  return level;
}

/* Change the debug output stream to FP.  If the underlying file is the
   same as stdout, use stdout instead so that debug messages appear in the
   correct relative position.  Report errors on behalf of CALLER.  */
static void
set_debug_file (m4 *context, const m4_call_info *caller, FILE *fp)
{
  FILE *debug_file;
  struct stat stdout_stat, debug_stat;

  assert (context);

  debug_file = m4_get_debug_file (context);
  if (debug_file != NULL && debug_file != stderr && debug_file != stdout
      && close_stream (debug_file) != 0)
    m4_error (context, 0, errno, caller, _("error writing to debug stream"));

  debug_file = fp;
  m4_set_debug_file (context, fp);

  if (debug_file != NULL && debug_file != stdout)
    {
      if (fstat (fileno (stdout), &stdout_stat) < 0)
        return;
      if (fstat (fileno (debug_file), &debug_stat) < 0)
        return;

      /* mingw has a bug where fstat on a regular file reports st_ino
         of 0.  On normal system, st_ino should never be 0.  */
      if (stdout_stat.st_ino == debug_stat.st_ino
          && stdout_stat.st_dev == debug_stat.st_dev
          && stdout_stat.st_ino != 0)
        {
          if (debug_file != stderr && close_stream (debug_file) != 0)
            m4_error (context, 0, errno, caller,
                      _("error writing to debug stream"));
          m4_set_debug_file (context, stdout);
        }
    }
}

/* Change the debug output to file NAME.  If NAME is NULL, debug
   output is reverted to stderr, and if empty debug output is
   discarded.  Return true iff the output stream was changed.  Report
   errors on behalf of CALLER.  */
bool
m4_debug_set_output (m4 *context, const m4_call_info *caller, const char *name)
{
  FILE *fp;

  assert (context);

  if (name == NULL)
    set_debug_file (context, caller, stderr);
  else if (*name == '\0')
    set_debug_file (context, caller, NULL);
  else
    {
      fp = fopen (name, "a");
      if (fp == NULL)
        return false;

      if (set_cloexec_flag (fileno (fp), true) != 0)
        m4_warn (context, errno, caller,
                 _("cannot protect debug file across forks"));
      set_debug_file (context, caller, fp);
    }
  return true;
}

/* Print the header of a one-line debug message, starting with "m4debug:".  */
void
m4_debug_message_prefix (m4 *context)
{
  FILE *debug_file;

  assert (context);

  debug_file = m4_get_debug_file (context);
  fputs ("m4debug:", debug_file);
  if (m4_get_current_line (context))
    {
      if (m4_is_debug_bit (context, M4_DEBUG_TRACE_FILE))
        xfprintf (debug_file, "%s:", m4_get_current_file (context));
      if (m4_is_debug_bit (context, M4_DEBUG_TRACE_LINE))
        xfprintf (debug_file, "%d:", m4_get_current_line (context));
    }
  putc (' ', debug_file);
}

/* If the current debug mode includes MODE, and there is a current
   debug file, then output a debug message described by FORMAT.  A
   message header is supplied, as well as a trailing newline.  */
void
m4_debug_message (m4 *context, int mode, const char *format, ...)
{
  /* Check that mode has exactly one bit set.  */
  assert (mode && (mode & (mode - 1)) == 0);
  assert (format);

  if (m4_get_debug_file (context) != NULL
      && m4_is_debug_bit (context, mode))
    {
      va_list args;

      m4_debug_message_prefix (context);
      va_start (args, format);
      xvfprintf (m4_get_debug_file (context), format, args);
      va_end (args);
      putc ('\n', m4_get_debug_file (context));
    }
}

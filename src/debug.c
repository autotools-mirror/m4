/* GNU m4 -- A simple macro processor

   Copyright (C) 1991-1994, 2004, 2006-2014, 2016-2017, 2020-2025 Free
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "m4.h"

#include <stdarg.h>
#include <sys/stat.h>

/* File for debugging output.  */
FILE *debug = NULL;

/* Obstack for trace messages.  */
static struct obstack trace;

static void debug_set_file (const call_info *, FILE *);

/* Initialize the debugging module.  */
void
debug_init (void)
{
  debug_set_file (NULL, stderr);
  obstack_init (&trace);
}

/* Dump the current set of debug flags into OBS, in a way that can be
   be passed back to debugmode to restore that state.  */
void
debug_dump (struct obstack *obs)
{
  int i;

  obstack_grow (obs, curr_quote.str1, curr_quote.len1);

#define DEBUG_DUMP(bit, ch)               \
  if (!(debug_level & bit) == (i != '+')) \
    obstack_1grow (obs, ch)

  /* Both ASCII and EBCDIC sort '+' before '-'.  */
  for (i = '+'; i <= '-'; i += '-' - '+') {
    obstack_1grow (obs, i);
    DEBUG_DUMP (DEBUG_TRACE_ARGS, 'a');
    DEBUG_DUMP (DEBUG_TRACE_CALL, 'c');
    DEBUG_DUMP (DEBUG_TRACE_DEREF, 'd');
    DEBUG_DUMP (DEBUG_TRACE_EXPANSION, 'e');
    DEBUG_DUMP (DEBUG_TRACE_FILE, 'f');
    DEBUG_DUMP (DEBUG_TRACE_INPUT, 'i');
    DEBUG_DUMP (DEBUG_TRACE_LINE, 'l');
    DEBUG_DUMP (DEBUG_TRACE_OUTPUT_DUMPDEF, 'o');
    DEBUG_DUMP (DEBUG_TRACE_PATH, 'p');
    DEBUG_DUMP (DEBUG_TRACE_QUOTE, 'q');
    DEBUG_DUMP (DEBUG_TRACE_ALL, 't');
    DEBUG_DUMP (DEBUG_TRACE_CALLID, 'x');
  }
#undef DEBUG_DUMP

  obstack_grow (obs, curr_quote.str2, curr_quote.len2);
}

/* Merge LEVEL into ACCUM according to MODE, then set LEVEL to 0.  */
static void
debug_set (char mode, int *accum, int *level)
{
  switch (mode)
    {
    case '\0':
      /* Replace old level.  */
      *accum = *level;
      break;

    case '-':
      /* Subtract flags.  */
      *accum &= ~*level;
      break;

    case '+':
      /* Add flags.  */
      *accum |= *level;
      break;

    default:
      assert (!"debug_set");
      abort ();
    }
  *level = 0;
}

/* Function to decode the debugging flags OPTS of length LEN.  If LEN
   is SIZE_MAX, use strlen (OPTS) instead.  Used by main while
   processing option -d, and by the builtin debugmode.  Return -1 if
   the parse failed, otherwise change the debug level.  */
int
debug_decode (const char *opts, size_t len)
{
  int accum = debug_level;
  int level;
  char mode = '\0';

  if (!opts)
    opts = "";
  if (len == SIZE_MAX)
    len = strlen (opts);
  if (*opts == '-' || *opts == '+')
    {
      len--;
      mode = *opts++;
    }
  if (!len)
    level = DEBUG_TRACE_DEFAULT;
  else
    {
      for (level = 0; len--; opts++)
        {
          switch (*opts)
            {
            case 'a':
              level |= DEBUG_TRACE_ARGS;
              break;

            case 'e':
              level |= DEBUG_TRACE_EXPANSION;
              break;

            case 'q':
              level |= DEBUG_TRACE_QUOTE;
              break;

            case 't':
              level |= DEBUG_TRACE_ALL;
              break;

            case 'l':
              level |= DEBUG_TRACE_LINE;
              break;

            case 'f':
              level |= DEBUG_TRACE_FILE;
              break;

            case 'p':
              level |= DEBUG_TRACE_PATH;
              break;

            case 'c':
              level |= DEBUG_TRACE_CALL;
              break;

            case 'i':
              level |= DEBUG_TRACE_INPUT;
              break;

            case 'x':
              level |= DEBUG_TRACE_CALLID;
              break;

            case 'd':
              level |= DEBUG_TRACE_DEREF;
              break;

            case 'o':
              level |= DEBUG_TRACE_OUTPUT_DUMPDEF;
              break;

            case 'V':
              level |= DEBUG_TRACE_VERBOSE;
              break;

            case '-':
            case '+':
              debug_set (mode, &accum, &level);
              mode = *opts;
              break;

            default:
              return -1;
            }
        }
    }
  debug_set (mode, &accum, &level);
  debug_level = accum;
  return 0;
}

/* Change the debug output stream to FP.  If the underlying file is
   the same as stdout, use stdout instead so that debug messages
   appear in the correct relative position.  Report any failure on
   behalf of CALLER.  */
static void
debug_set_file (const call_info *caller, FILE *fp)
{
  struct stat stdout_stat, debug_stat;

  if (debug != NULL && debug != stderr && debug != stdout
      && close_stream (debug) != 0)
    m4_error (0, errno, caller, _("error writing to debug stream"));
  debug = fp;

  if (debug != NULL && debug != stdout)
    {
      if (fstat (STDOUT_FILENO, &stdout_stat) < 0)
        return;
      if (fstat (fileno (debug), &debug_stat) < 0)
        return;

      /* mingw has a bug where fstat on a regular file reports st_ino
         of 0.  On normal system, st_ino should never be 0.  */
      if (stdout_stat.st_ino == debug_stat.st_ino
          && stdout_stat.st_dev == debug_stat.st_dev
          && stdout_stat.st_ino != 0)
        {
          if (debug != stderr && close_stream (debug) != 0)
            m4_error (0, errno, caller, _("error writing to debug stream"));
          debug = stdout;
        }
    }
}

/* Serialize files.  Used before executing a system command.  */
void
debug_flush_files (void)
{
  fflush (stdout);
  fflush (stderr);
  if (debug != NULL && debug != stdout && debug != stderr)
    fflush (debug);
  /* POSIX requires that if m4 doesn't consume all input, but stdin is
     opened on a seekable file, that the file pointer be left at the
     next character on exit (but places no restrictions on the file
     pointer location on a non-seekable file).  It also requires that
     fflush() followed by fseeko() on an input file set the underlying
     file pointer, and gnulib guarantees these semantics.  However,
     fflush() on a non-seekable file can lose buffered data, which we
     might otherwise want to process after syscmd.  Hence, we must
     check whether stdin is seekable.  We must also be tolerant of
     operating with stdin closed, so we don't report any failures in
     this attempt.  The stdio-safer module and friends are essential,
     so that if stdin was closed, this lseek is not on some other file
     that we have since opened.  */
  if (lseek (STDIN_FILENO, 0, SEEK_CUR) >= 0 && fflush (stdin) == 0)
    {
      fseeko (stdin, 0, SEEK_CUR);
    }
}

/* Change the debug output to file NAME.  If NAME is NULL, debug
   output is reverted to stderr, and if empty, debug output is
   discarded.  Return true iff the output stream was changed.  Report
   errors on behalf of CALLER.  */
bool
debug_set_output (const call_info *caller, const char *name)
{
  FILE *fp;

  if (name == NULL)
    debug_set_file (caller, stderr);
  else if (*name == '\0')
    debug_set_file (caller, NULL);
  else
    {
      fp = fopen (name, "ae");
      if (fp == NULL)
        return false;
      debug_set_file (caller, fp);
    }
  return true;
}

/* Print the header of a one-line debug message, starting by
   "m4debug:".  */
void
debug_message (const char *format, ...)
{
  va_list args;
  if (debug)
    {
      xfprintf (debug, "m4debug:");
      if (current_line)
        {
          if (debug_level & DEBUG_TRACE_FILE)
            xfprintf (debug, "%s:", current_file);
          if (debug_level & DEBUG_TRACE_LINE)
            xfprintf (debug, "%d:", current_line);
        }
      putc (' ', debug);
      va_start (args, format);
      xvfprintf (debug, format, args);
      va_end (args);
      putc ('\n', debug);
    }
}

/* The rest of this file contains the functions for macro tracing output.
   All tracing output for a macro call is collected on an obstack TRACE,
   and printed whenever the line is complete.  This prevents tracing
   output from interfering with other debug messages generated by the
   various builtins.  */

/* Format the standard header attached to all tracing output lines,
   using the context in INFO as appropriate.  Return the offset into
   the trace obstack where this particular trace begins.  */
static unsigned int
trace_header (const call_info *info)
{
  int trace_level = info->debug_level;
  unsigned int result = obstack_object_size (&trace);

  obstack_grow (&trace, "m4trace:", 8);
  if (trace_level & DEBUG_TRACE_FILE)
    obstack_printf (&trace, "%s:", info->file);
  if (trace_level & DEBUG_TRACE_LINE)
    obstack_printf (&trace, "%d:", info->line);
  obstack_printf (&trace, " -%d- ", expansion_level);
  if (trace_level & DEBUG_TRACE_CALLID)
    obstack_printf (&trace, "id %d: ", info->call_id);
  return result;
}

/* Print current tracing line starting at offset START, as returned
   from an earlier trace_header(), then clear the obstack to END.  */
static void
trace_flush (unsigned int start, unsigned int end)
{
  char *base = (char *) obstack_base (&trace);
  size_t len = obstack_object_size (&trace);

  if (debug)
    {
      /* TODO - quote nonprintable characters if debug is tty?  */
      xfwrite (&base[start], 1, len - start, debug);
      fputc ('\n', debug);
    }
  obstack_blank_fast (&trace, end - len);
}

/* Do pre-argument-collection tracing for the macro call described in
   INFO.  Used from expand_macro ().  */
void
trace_pre (call_info *info)
{
  assert (info->trace);
  info->start = trace_header (info);
  obstack_grow (&trace, info->name, info->name_len);
  info->rest = obstack_object_size (&trace);
  if (info->debug_level & DEBUG_TRACE_CALL)
    {
      obstack_grow (&trace, " ...", 4);
      trace_flush (info->start, info->rest);
    }
}

/* Format the parts of a trace line that are known via ARGV before the
   macro is actually expanded.  Used from call_macro ().  Return the
   start of the current trace, in case other traces are printed before
   this trace completes trace_post.  */
void
trace_args (macro_arguments *argv)
{
  const call_info *info = arg_info (argv);
  int trace_level = info->debug_level;

  if (!info->trace)
    return;
  obstack_blank_fast (&trace, info->rest - obstack_object_size (&trace));
  if (1 < arg_argc (argv) && (trace_level & DEBUG_TRACE_ARGS))
    {
      size_t len = max_debug_argument_length;
      obstack_1grow (&trace, '(');
      arg_print (&trace, argv, 1,
                 (trace_level & DEBUG_TRACE_QUOTE) ? &curr_quote : NULL,
                 false, NULL, ", ", &len, true);
      obstack_1grow (&trace, ')');
    }
}

/* If requested by the trace state in INFO, format the final part of a
   trace line.  Then print all collected information from START,
   returned from a prior trace_pre().  Used from call_macro ().  */
void
trace_post (const call_info *info)
{
  if (!info->trace)
    return;
  if (info->debug_level & DEBUG_TRACE_EXPANSION)
    {
      obstack_grow (&trace, " -> ", 4);
      if (info->debug_level & DEBUG_TRACE_QUOTE)
        obstack_grow (&trace, curr_quote.str1, curr_quote.len1);
      input_print (&trace);
      if (info->debug_level & DEBUG_TRACE_QUOTE)
        obstack_grow (&trace, curr_quote.str2, curr_quote.len2);
    }
  trace_flush (info->start, info->start);
}

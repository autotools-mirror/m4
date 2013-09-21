/* GNU m4 -- A simple macro processor
   Copyright (C) 2000, 2002-2004, 2006-2010, 2013 Free Software
   Foundation, Inc.

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

/* Build using only the exported interfaces, unless NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

#include "execute.h"
#include "memchr2.h"
#include "memcmp2.h"
#include "quotearg.h"
#include "stdlib--.h"
#include "tempname.h"
#include "unistd--.h"

#include <modules/m4.h>

extern void m4_set_sysval    (int);
extern void m4_sysval_flush  (m4 *, bool);
extern void m4_dump_symbols  (m4 *, m4_dump_symbol_data *, size_t,
                              m4_macro_args *, bool);
extern const char *m4_expand_ranges (const char *, size_t *, m4_obstack *);
extern void m4_make_temp     (m4 *, m4_obstack *, const m4_call_info *,
                              const char *, size_t, bool);

/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

           function     macros  blind   side    minargs maxargs */
#define builtin_functions                                       \
  BUILTIN (changecom,   false,  false,  false,  0,      2  )    \
  BUILTIN (changequote, false,  false,  false,  0,      2  )    \
  BUILTIN (decr,        false,  true,   true,   1,      1  )    \
  BUILTIN (define,      true,   true,   false,  1,      2  )    \
  BUILTIN (defn,        true,   true,   false,  1,      -1 )    \
  BUILTIN (divert,      false,  false,  false,  0,      2  )    \
  BUILTIN (divnum,      false,  false,  false,  0,      0  )    \
  BUILTIN (dnl,         false,  false,  false,  0,      0  )    \
  BUILTIN (dumpdef,     true,   false,  false,  0,      -1 )    \
  BUILTIN (errprint,    false,  true,   false,  1,      -1 )    \
  BUILTIN (eval,        false,  true,   true,   1,      3  )    \
  BUILTIN (ifdef,       true,   true,   false,  2,      3  )    \
  BUILTIN (ifelse,      true,   true,   false,  1,      -1 )    \
  BUILTIN (include,     false,  true,   false,  1,      1  )    \
  BUILTIN (incr,        false,  true,   true,   1,      1  )    \
  BUILTIN (index,       false,  true,   true,   2,      3  )    \
  BUILTIN (len,         false,  true,   true,   1,      1  )    \
  BUILTIN (m4exit,      false,  false,  false,  0,      1  )    \
  BUILTIN (m4wrap,      true,   true,   false,  1,      -1 )    \
  BUILTIN (maketemp,    false,  true,   false,  1,      1  )    \
  BUILTIN (mkstemp,     false,  true,   false,  1,      1  )    \
  BUILTIN (popdef,      true,   true,   false,  1,      -1 )    \
  BUILTIN (pushdef,     true,   true,   false,  1,      2  )    \
  BUILTIN (shift,       true,   true,   false,  1,      -1 )    \
  BUILTIN (sinclude,    false,  true,   false,  1,      1  )    \
  BUILTIN (substr,      false,  true,   true,   2,      4  )    \
  BUILTIN (syscmd,      false,  true,   true,   1,      1  )    \
  BUILTIN (sysval,      false,  false,  false,  0,      0  )    \
  BUILTIN (traceoff,    true,   false,  false,  0,      -1 )    \
  BUILTIN (traceon,     true,   false,  false,  0,      -1 )    \
  BUILTIN (translit,    false,  true,   true,   2,      3  )    \
  BUILTIN (undefine,    true,   true,   false,  1,      -1 )    \
  BUILTIN (undivert,    false,  false,  false,  0,      -1 )    \


typedef intmax_t number;
typedef uintmax_t unumber;

static void	include		(m4 *context, m4_obstack *obs, size_t argc,
				 m4_macro_args *argv, bool silent);
static int      dumpdef_cmp_CB  (const void *s1, const void *s2);
static void *   dump_symbol_CB  (m4_symbol_table *, const char *, size_t,
                                 m4_symbol *symbol, void *userdata);
static const char *ntoa         (number value, int radix);
static void     numb_obstack    (m4_obstack *obs, number value,
                                 int radix, int min);


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind, side, min, max) M4BUILTIN (handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
static const m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)                 \
  M4BUILTIN_ENTRY (handler, #handler, macros, blind, side, min, max)

  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};


void
include_m4 (m4 *context, m4_module *module, m4_obstack *obs)
{
  m4_install_builtins (context, module, m4_builtin_table);
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
  const m4_call_info *me = m4_arg_info (argv);

  if (m4_is_arg_text (argv, 1))
    {
      m4_symbol_value *value = m4_symbol_value_create ();

      if (m4_symbol_value_copy (context, value, m4_arg_symbol (argv, 2)))
        m4_warn (context, 0, me, _("cannot concatenate builtins"));
      m4_symbol_define (M4SYMTAB, M4ARG (1), M4ARGLEN (1), value);
    }
  else
    m4_warn (context, 0, me, _("invalid macro name ignored"));
}

M4BUILTIN_HANDLER (undefine)
{
  size_t i;
  for (i = 1; i < argc; i++)
    if (m4_symbol_value_lookup (context, argv, i, true))
      m4_symbol_delete (M4SYMTAB, M4ARG (i), M4ARGLEN (i));
}

M4BUILTIN_HANDLER (pushdef)
{
  const m4_call_info *me = m4_arg_info (argv);

  if (m4_is_arg_text (argv, 1))
    {
      m4_symbol_value *value = m4_symbol_value_create ();

      if (m4_symbol_value_copy (context, value, m4_arg_symbol (argv, 2)))
        m4_warn (context, 0, me, _("cannot concatenate builtins"));
      m4_symbol_pushdef (M4SYMTAB, M4ARG (1), M4ARGLEN (1), value);
    }
  else
    m4_warn (context, 0, me, _("invalid macro name ignored"));
}

M4BUILTIN_HANDLER (popdef)
{
  size_t i;
  for (i = 1; i < argc; i++)
    if (m4_symbol_value_lookup (context, argv, i, true))
      m4_symbol_popdef (M4SYMTAB, M4ARG (i), M4ARGLEN (i));
}



/* --- CONDITIONALS OF M4 --- */


M4BUILTIN_HANDLER (ifdef)
{
  m4_push_arg (context, obs, argv,
               m4_symbol_value_lookup (context, argv, 1, false) ? 2 : 3);
}

M4BUILTIN_HANDLER (ifelse)
{
  const m4_call_info *me = m4_arg_info (argv);
  size_t i;

  /* The valid ranges of argc for ifelse is discontinuous, we cannot
     rely on the regular mechanisms.  */
  if (argc == 2 || m4_bad_argc (context, argc, me, 3, -1, false))
    return;
  else if (argc % 3 == 0)
    /* Diagnose excess arguments if 5, 8, 11, etc., actual arguments.  */
    m4_bad_argc (context, argc, me, 0, argc - 2, false);

  i = 1;
  argc--;

  while (true)
    {
      if (m4_arg_equal (context, argv, i, i + 1))
        {
          m4_push_arg (context, obs, argv, i + 2);
          return;
        }
      switch (argc)
        {
        case 3:
          return;

        case 4:
        case 5:
          m4_push_arg (context, obs, argv, i + 3);
          return;

        default:
          argc -= 3;
          i += 3;
        }
    }
}


/* qsort comparison routine, for sorting the table made in m4_dumpdef ().  */
static int
dumpdef_cmp_CB (const void *s1, const void *s2)
{
  const m4_string *a = (const m4_string *) s1;
  const m4_string *b = (const m4_string *) s2;
  return memcmp2 (a->str, a->len, b->str, b->len);
}

/* The function m4_dump_symbols () is for use by "dumpdef".  It builds up a
   table of all defined symbol names.  */
static void *
dump_symbol_CB (m4_symbol_table *ignored M4_GNUC_UNUSED, const char *name,
                size_t len, m4_symbol *symbol, void *userdata)
{
  m4_dump_symbol_data *symbol_data = (m4_dump_symbol_data *) userdata;
  m4_string *key;

  assert (name);
  assert (symbol);
  assert (!m4_is_symbol_value_void (m4_get_symbol_value (symbol)));

  if (symbol_data->size == 0)
    {
      char *base;
      size_t offset = obstack_object_size (symbol_data->obs);
      obstack_blank (symbol_data->obs, sizeof *symbol_data->base);
      symbol_data->size = (obstack_room (symbol_data->obs)
                           / sizeof *symbol_data->base);
      base = (char *) obstack_base (symbol_data->obs) + offset;
      symbol_data->base = (m4_string *) base;
    }
  else
    {
      obstack_blank_fast (symbol_data->obs, sizeof *symbol_data->base);
      symbol_data->size--;
    }

  /* Safe to cast away const, since m4_dump_symbols adds it back.  */
  key = (m4_string *) symbol_data->base++;
  key->str = (char *) name;
  key->len = len;
  return NULL;
}

/* If there are no arguments, build a sorted list of all defined
   symbols, otherwise, only the specified symbols.  */
void
m4_dump_symbols (m4 *context, m4_dump_symbol_data *data, size_t argc,
                 m4_macro_args *argv, bool complain)
{
  assert (obstack_object_size (data->obs) == 0);
  data->size = obstack_room (data->obs) / sizeof *data->base;
  data->base = (m4_string *) obstack_base (data->obs);

  if (argc == 1)
    m4_symtab_apply (M4SYMTAB, false, dump_symbol_CB, data);
  else
    {
      size_t i;
      m4_symbol *symbol;

      for (i = 1; i < argc; i++)
        {
          symbol = m4_symbol_value_lookup (context, argv, i, complain);
          if (symbol)
            dump_symbol_CB (NULL, M4ARG (i), M4ARGLEN (i), symbol, data);
        }
    }

  data->size = obstack_object_size (data->obs) / sizeof *data->base;
  data->base = (m4_string *) obstack_finish (data->obs);
  /* Safe to cast away const, since we don't modify entries.  */
  qsort ((m4_string *) data->base, data->size, sizeof *data->base,
         dumpdef_cmp_CB);
}


/* Implementation of "dumpdef" itself.  It builds up a table of pointers to
   symbols, sorts it and prints the sorted table.  */
M4BUILTIN_HANDLER (dumpdef)
{
  m4_dump_symbol_data data;
  const m4_string_pair *quotes = NULL;
  bool stack = m4_is_debug_bit (context, M4_DEBUG_TRACE_STACK);
  size_t arg_length = m4_get_max_debug_arg_length_opt (context);
  bool module = m4_is_debug_bit (context, M4_DEBUG_TRACE_MODULE);
  FILE *output = (m4_is_debug_bit (context, M4_DEBUG_TRACE_OUTPUT_DUMPDEF)
                  ? stderr : m4_get_debug_file (context));

  if (!output)
    return;
  if (m4_is_debug_bit (context, M4_DEBUG_TRACE_QUOTE))
    quotes = m4_get_syntax_quotes (M4SYNTAX);
  data.obs = m4_arg_scratch (context);
  m4_dump_symbols (context, &data, argc, argv, true);
  m4_sysval_flush (context, false);

  for (; data.size > 0; --data.size, data.base++)
    {
      m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, data.base->str,
                                            data.base->len);
      char *value;
      size_t len;
      assert (symbol);

      /* TODO - add debugmode(b) option to control quoting style.  */
      obstack_grow (obs, data.base->str, data.base->len);
      obstack_1grow (obs, ':');
      obstack_1grow (obs, '\t');
      m4_symbol_print (context, symbol, obs, quotes, stack, arg_length,
                       module);
      obstack_1grow (obs, '\n');
      len = obstack_object_size (obs);
      value = (char *) obstack_finish (obs);
      fwrite (value, 1, len, output);
      obstack_free (obs, value);
    }
}

/* The macro "defn" returns the quoted definition of the macro named by
   the first argument.  If the macro is builtin, it will push a special
   macro-definition token on the input stack.  */
M4BUILTIN_HANDLER (defn)
{
  const m4_call_info *me = m4_arg_info (argv);
  size_t i;

  for (i = 1; i < argc; i++)
    {
      m4_symbol *symbol = m4_symbol_value_lookup (context, argv, i, true);

      if (!symbol)
        ;
      else if (m4_is_symbol_text (symbol))
        m4_shipout_string (context, obs, m4_get_symbol_text (symbol),
                           m4_get_symbol_len (symbol), true);
      else if (m4_is_symbol_func (symbol))
        m4_push_builtin (context, obs, m4_get_symbol_value (symbol));
      else if (m4_is_symbol_placeholder (symbol))
        m4_warn (context, 0, me,
                 _("%s: builtin %s requested by frozen file not found"),
                 quotearg_n_mem (2, M4ARG (i), M4ARGLEN (i)),
                 quotearg_style (locale_quoting_style,
                                 m4_get_symbol_placeholder (symbol)));
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

void
m4_set_sysval (int value)
{
  m4_sysval = value;
}

/* Flush a given output STREAM.  If REPORT, also print an error
   message and clear the stream error bit.  */
static void
sysval_flush_helper (m4 *context, FILE *stream, bool report)
{
  if (fflush (stream) == EOF && report)
    {
      m4_error (context, 0, errno, NULL, _("write error"));
      clearerr (stream);
    }
}

/* Flush all user output streams, prior to doing something that can
   could lose unflushed data or interleave debug and normal output
   incorrectly.  If REPORT, then print an error message on failure and
   clear the stream error bit; otherwise a subsequent ferror can track
   that an error occurred.  */
void
m4_sysval_flush (m4 *context, bool report)
{
  FILE *debug_file = m4_get_debug_file (context);

  if (debug_file != stdout)
    sysval_flush_helper (context, stdout, report);
  if (debug_file != stderr)
    /* If we have problems with stderr, we can't really report that
       problem to stderr.  The closeout module will ensure the exit
       status reflects the problem, though.  */
    fflush (stderr);
  if (debug_file != NULL)
    sysval_flush_helper (context, debug_file, report);
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
  if (lseek (STDIN_FILENO, 0, SEEK_CUR) >= 0
      && fflush (stdin) == 0)
    {
      fseeko (stdin, 0, SEEK_CUR);
    }
}

M4BUILTIN_HANDLER (syscmd)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *cmd = M4ARG (1);
  size_t len = M4ARGLEN (1);
  int status;
  int sig_status;
  const char *prog_args[4] = { "sh", "-c" };

  if (m4_get_safer_opt (context))
    {
      m4_error (context, 0, 0, m4_arg_info (argv), _("disabled by --safer"));
      return;
    }
  if (strlen (cmd) != len)
    m4_warn (context, 0, me, _("argument %s truncated"),
             quotearg_style_mem (locale_quoting_style, cmd, len));

  /* Optimize the empty command.  */
  if (!*cmd)
    {
      m4_set_sysval (0);
      return;
    }
  m4_sysval_flush (context, false);
#if W32_NATIVE
  if (strstr (M4_SYSCMD_SHELL, "cmd"))
    {
      prog_args[0] = "cmd";
      prog_args[1] = "/c";
    }
#endif
  prog_args[2] = cmd;
  errno = 0;
  status = execute (m4_info_name (me), M4_SYSCMD_SHELL, (char **) prog_args,
                    false, false, false, false, true, false, &sig_status);
  if (sig_status)
    {
      assert (status == 127);
      m4_sysval = sig_status << 8;
    }
  else
    {
      if (status == 127 && errno)
        m4_warn (context, errno, me, _("cannot run command %s"),
                 quotearg_style (locale_quoting_style, cmd));
      m4_sysval = status;
    }
}


M4BUILTIN_HANDLER (sysval)
{
  m4_shipout_int (obs, m4_sysval);
}


M4BUILTIN_HANDLER (incr)
{
  int value;

  if (!m4_numeric_arg (context, m4_arg_info (argv), M4ARG (1), M4ARGLEN (1),
                       &value))
    return;

  m4_shipout_int (obs, value + 1);
}

M4BUILTIN_HANDLER (decr)
{
  int value;

  if (!m4_numeric_arg (context, m4_arg_info (argv), M4ARG (1), M4ARGLEN (1),
                       &value))
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

  if (argc >= 2 && !m4_numeric_arg (context, m4_arg_info (argv), M4ARG (1),
                                    M4ARGLEN (1), &i))
    return;
  m4_make_diversion (context, i);
  m4_divert_text (context, NULL, M4ARG (2), M4ARGLEN (2),
                  m4_get_current_line (context));
}

/* Expand to the current diversion number.  */
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
  size_t i = 0;
  const m4_call_info *me = m4_arg_info (argv);

  if (argc == 1)
    m4_undivert_all (context);
  else
    for (i = 1; i < argc; i++)
      {
        const char *str = M4ARG (i);
        size_t len = M4ARGLEN (i);
        char *endp;
        int diversion = strtol (str, &endp, 10);
        if (endp - str == len && !isspace ((unsigned char) *str))
          m4_insert_diversion (context, diversion);
        else if (m4_get_posixly_correct_opt (context))
          m4_warn (context, 0, me, _("non-numeric argument %s"),
                   quotearg_style_mem (locale_quoting_style, str, len));
        else if (strlen (str) != len)
          m4_warn (context, 0, me, _("invalid file name %s"),
                   quotearg_style_mem (locale_quoting_style, str, len));
        else
          {
	    char *filepath = m4_path_search (context, str, NULL);
	    FILE *fp = m4_fopen (context, filepath, "r");

	    free (filepath);
            if (fp != NULL)
              {
                m4_insert_file (context, fp);
                if (fclose (fp) == EOF)
                  m4_error (context, 0, errno, me, _("error undiverting %s"),
                            quotearg_style (locale_quoting_style, str));
              }
            else
              m4_error (context, 0, errno, me, _("cannot undivert %s"),
                        quotearg_style (locale_quoting_style, str));
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
  m4_skip_line (context, m4_arg_info (argv));
}

/* Shift all arguments one to the left, discarding the first argument.
   Each output argument is quoted with the current quotes.  */
M4BUILTIN_HANDLER (shift)
{
  m4_push_args (context, obs, argv, true, true);
}

/* Change the current quotes.  The function set_quotes () lives in
   syntax.c.  */
M4BUILTIN_HANDLER (changequote)
{
  m4_set_quotes (M4SYNTAX,
                 (argc >= 2) ? M4ARG (1) : NULL, M4ARGLEN (1),
                 (argc >= 3) ? M4ARG (2) : NULL, M4ARGLEN (2));
}

/* Change the current comment delimiters.  The function set_comment ()
   lives in syntax.c.  */
M4BUILTIN_HANDLER (changecom)
{
  m4_set_comment (M4SYNTAX,
                  (argc >= 2) ? M4ARG (1) : NULL, M4ARGLEN (1),
                  (argc >= 3) ? M4ARG (2) : NULL, M4ARGLEN (2));
}


/* This section contains macros for inclusion of other files -- "include"
   and "sinclude".  This differs from bringing back diversions, in that
   the input is scanned before being copied to the output.  */

static void
include (m4 *context, m4_obstack *obs, size_t argc, m4_macro_args *argv, bool silent)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *arg = M4ARG (1);
  size_t len = M4ARGLEN (1);

  if (strlen (arg) != len)
    m4_warn (context, 0, me, _("argument %s truncated"),
             quotearg_style_mem (locale_quoting_style, arg, len));
  m4_load_filename (context, me, arg, obs, silent);
}

/* Include a file, complaining in case of errors.  */
M4BUILTIN_HANDLER (include)
{
  include (context, obs, argc, argv, false);
}

/* Include a file, ignoring errors.  */
M4BUILTIN_HANDLER (sinclude)
{
  include (context, obs, argc, argv, true);
}


/* More miscellaneous builtins -- "maketemp", "errprint".  */

/* Add trailing `X' to PATTERN of length LEN as necessary, then
   securely create the temporary file system object.  If DIR, create a
   directory instead of a file.  Report errors on behalf of CALLER.  If
   successful, output the quoted resulting name on OBS.  */
void
m4_make_temp (m4 *context, m4_obstack *obs, const m4_call_info *caller,
              const char *pattern, size_t len, bool dir)
{
  int fd;
  int i;
  char *name;
  const m4_string_pair *quotes = m4_get_syntax_quotes (M4SYNTAX);

  if (m4_get_safer_opt (context))
    {
      m4_error (context, 0, 0, caller, _("disabled by --safer"));
      return;
    }

  /* Guarantee that there are six trailing 'X' characters, even if the
     user forgot to supply them.  Output must be quoted if
     successful.  */
  assert (obstack_object_size (obs) == 0);
  obstack_grow (obs, quotes->str1, quotes->len1);
  if (strlen (pattern) < len)
    {
      m4_warn (context, 0, caller, _("argument %s truncated"),
               quotearg_style_mem (locale_quoting_style, pattern, len));
      len = strlen (pattern);
    }
  obstack_grow (obs, pattern, len);
  for (i = 0; len > 0 && i < 6; i++)
    if (pattern[--len] != 'X')
      break;
  obstack_grow0 (obs, "XXXXXX", 6 - i);
  name = (char *) obstack_base (obs) + quotes->len1;

  /* Make the temporary object.  */
  errno = 0;
  fd = gen_tempname (name, 0, 0, dir ? GT_DIR : GT_FILE);
  if (fd < 0)
    {
      /* This use of _() will need to change if xgettext ever changes
         its undocumented behavior of parsing both string options.  */

      m4_warn (context, errno, caller,
               _(dir ? "cannot create directory from template %s"
                 : "cannot create file from template %s"),
               quotearg_style (locale_quoting_style, pattern));
      obstack_free (obs, obstack_finish (obs));
    }
  else
    {
      if (!dir)
        close (fd);
      /* Remove NUL, then finish quote.  */
      obstack_blank (obs, -1);
      obstack_grow (obs, quotes->str2, quotes->len2);
    }
}

/* Use the first argument as at template for a temporary file name.  */
M4BUILTIN_HANDLER (maketemp)
{
  const m4_call_info *me = m4_arg_info (argv);
  m4_warn (context, 0, me, _("recommend using mkstemp instead"));
  if (m4_get_posixly_correct_opt (context))
    {
      /* POSIX states "any trailing 'X' characters [are] replaced with
         the current process ID as a string", without referencing the
         file system.  Horribly insecure, but we have to do it.

         For reference, Solaris m4 does:
           maketemp() -> `'
           maketemp(X) -> `X'
           maketemp(XX) -> `Xn', where n is last digit of pid
           maketemp(XXXXXXXX) -> `X00nnnnn', where nnnnn is 16-bit pid
      */
      const char *str = M4ARG (1);
      size_t len = M4ARGLEN (1);
      size_t i;
      m4_obstack *scratch = m4_arg_scratch (context);
      size_t pid_len = obstack_printf (scratch, "%lu",
                                       (unsigned long) getpid ());
      char *pid = (char *) obstack_copy0 (scratch, "", 0);

      for (i = len; i > 1; i--)
        if (str[i - 1] != 'X')
          break;
      obstack_grow (obs, str, i);
      if (len - i < pid_len)
        obstack_grow (obs, pid + pid_len - (len - i), len - i);
      else
        obstack_printf (obs, "%.*d%s", len - i - pid_len, 0, pid);
    }
  else
    m4_make_temp (context, obs, me, M4ARG (1), M4ARGLEN (1), false);
}

/* Use the first argument as a template for a temporary file name.  */
M4BUILTIN_HANDLER (mkstemp)
{
  m4_make_temp (context, obs, m4_arg_info (argv), M4ARG (1), M4ARGLEN (1),
                false);
}

/* Print all arguments on standard error.  */
M4BUILTIN_HANDLER (errprint)
{
  size_t i;

  m4_sysval_flush (context, false);
  /* The close_stdin module makes it safe to skip checking the return
     values here.  */
  fwrite (M4ARG (1), 1, M4ARGLEN (1), stderr);
  for (i = 2; i < m4_arg_argc (argv); i++)
    {
      fputc (' ', stderr);
      fwrite (M4ARG (i), 1, M4ARGLEN (i), stderr);
    }
  fflush (stderr);
}


/* This section contains various macros for exiting, saving input until
   EOF is seen, and tracing macro calls.  That is: "m4exit", "m4wrap",
   "traceon" and "traceoff".  */

/* Exit immediately, with exitcode specified by the first argument, 0 if no
   arguments are present.  */
M4BUILTIN_HANDLER (m4exit)
{
  const m4_call_info *me = m4_arg_info (argv);
  int exit_code = EXIT_SUCCESS;

  /* Warn on bad arguments, but still exit.  */
  if (argc >= 2 && !m4_numeric_arg (context, me, M4ARG (1), M4ARGLEN (1),
                                    &exit_code))
    exit_code = EXIT_FAILURE;
  if (exit_code < 0 || exit_code > 255)
    {
      m4_warn (context, 0, me, _("exit status out of range: `%d'"), exit_code);
      exit_code = EXIT_FAILURE;
    }

  /* Ensure that atexit handlers see correct nonzero status.  */
  if (exit_code != EXIT_SUCCESS)
    m4_set_exit_failure (exit_code);

  /* Change debug stream back to stderr, to force flushing debug
     stream and detect any errors.  */
  m4_debug_set_output (context, me, NULL);
  m4_sysval_flush (context, true);

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
  m4_wrap_args (context, argv);
}

/* Enable tracing of all specified macros, or all, if none is specified.
   Tracing is disabled by default, when a macro is defined.  This can be
   overridden by the "t" debug flag.  */

M4BUILTIN_HANDLER (traceon)
{
  const m4_call_info *me = m4_arg_info (argv);
  size_t i;

  if (argc == 1)
    m4_set_debug_level_opt (context, (m4_get_debug_level_opt (context)
                                      | M4_DEBUG_TRACE_ALL));
  else
    for (i = 1; i < argc; i++)
      if (m4_is_arg_text (argv, i))
        m4_set_symbol_name_traced (M4SYMTAB, M4ARG (i), M4ARGLEN (i), true);
      else
        m4_warn (context, 0, me, _("invalid macro name ignored"));
}

/* Disable tracing of all specified macros, or all, if none is specified.  */
M4BUILTIN_HANDLER (traceoff)
{
  const m4_call_info *me = m4_arg_info (argv);
  size_t i;

  if (argc == 1)
    m4_set_debug_level_opt (context, (m4_get_debug_level_opt (context)
                                      & ~M4_DEBUG_TRACE_ALL));
  else
    for (i = 1; i < argc; i++)
      if (m4_is_arg_text (argv, i))
        m4_set_symbol_name_traced (M4SYMTAB, M4ARG (i), M4ARGLEN (i), false);
      else
        m4_warn (context, 0, me, _("invalid macro name ignored"));
}


/* This section contains text processing macros: "len", "index",
   "substr", "translit", "format", "regexp" and "patsubst".  The last
   three are GNU specific.  */

/* Expand to the length of the first argument.  */
M4BUILTIN_HANDLER (len)
{
  m4_shipout_int (obs, M4ARGLEN (1));
}

/* The macro expands to the first index of the second argument in the
   first argument.  As an extension, start the search at the index
   indicated by the third argument.  */
M4BUILTIN_HANDLER (index)
{
  const char *haystack = M4ARG (1);
  size_t haystack_len = M4ARGLEN (1);
  const char *needle = M4ARG (2);
  const char *result = NULL;
  int offset = 0;
  int retval = -1;

  if (!m4_arg_empty (argv, 3) && !m4_numeric_arg (context, m4_arg_info (argv),
                                                  M4ARG (3), M4ARGLEN (3),
                                                  &offset))
    return;
  if (offset < 0)
    {
      offset += haystack_len;
      if (offset < 0)
        offset = 0;
    }
  else if (haystack_len < offset)
    {
      m4_shipout_int (obs, -1);
      return;
    }

  /* Rely on the optimizations guaranteed by gnulib's memmem
     module.  */
  result = (char *) memmem (haystack + offset, haystack_len - offset, needle,
                            M4ARGLEN (2));
  if (result)
    retval = result - haystack;

  m4_shipout_int (obs, retval);
}

/* The macro "substr" extracts substrings from the first argument,
   starting from the index given by the second argument, extending for
   a length given by the third argument.  If the third argument is
   missing or empty, the substring extends to the end of the first
   argument.  As an extension, negative arguments are treated as
   indices relative to the string length.  Also, if a fourth argument
   is supplied, the original string is output with the selected
   substring replaced by the argument.  */
M4BUILTIN_HANDLER (substr)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *str = M4ARG (1);
  int start = 0;
  int end;
  int length;

  if (argc <= 2)
    {
      m4_push_arg (context, obs, argv, 1);
      return;
    }

  length = M4ARGLEN (1);
  if (!m4_arg_empty (argv, 2)
      && !m4_numeric_arg (context, me, M4ARG (2), M4ARGLEN (2), &start))
    return;
  if (start < 0)
    start += length;

  if (m4_arg_empty (argv, 3))
    end = length;
  else
    {
      if (!m4_numeric_arg (context, me, M4ARG (3), M4ARGLEN (3), &end))
        return;
      if (end < 0)
        end += length;
      else
        end += start;
    }

  if (5 <= argc)
    {
      /* Replacement text provided.  */
      if (end < start)
        end = start;
      if (end < 0 || length < start)
        {
          m4_warn (context, 0, me, _("substring out of range"));
          return;
        }
      if (start < 0)
        start = 0;
      if (length < end)
        end = length;
      obstack_grow (obs, str, start);
      m4_push_arg (context, obs, argv, 4);
      obstack_grow (obs, str + end, length - end);
      return;
    }

  if (start < 0)
    start = 0;
  if (length < end)
    end = length;
  if (end <= start)
    return;

  obstack_grow (obs, str + start, end - start);
}


/* Any ranges in string S of length *LEN are expanded, using OBS for
   scratch space, and the expansion returned.  *LEN is set to the
   expanded length.  A single - (dash) can be included in the strings
   by being the first or the last character in the string.  If the
   first character in a range is after the first in the character set,
   the range is made backwards, thus 9-0 is the string 9876543210.  */
const char *
m4_expand_ranges (const char *s, size_t *len, m4_obstack *obs)
{
  unsigned char from;
  unsigned char to;
  const char *end = s + *len;

  assert (obstack_object_size (obs) == 0);
  assert (s != end);
  from = *s++;
  obstack_1grow (obs, from);

  for ( ; s != end; from = *s++)
    {
      if (*s == '-')
        {
          if (++s == end)
            {
              /* trailing dash */
              obstack_1grow (obs, '-');
              break;
            }
          to = *s;
          if (from <= to)
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
  *len = obstack_object_size (obs);
  return (char *) obstack_finish (obs);
}

/* The macro "translit" translates all characters in the first
   argument, which are present in the second argument, into the
   corresponding character from the third argument.  If the third
   argument is shorter than the second, the extra characters in the
   second argument are deleted from the first.  */
M4BUILTIN_HANDLER (translit)
{
  const char *data;
  const char *from;
  const char *to;
  size_t from_len;
  size_t to_len;
  char map[UCHAR_MAX + 1];
  char found[UCHAR_MAX + 1];
  unsigned char ch;

  enum { ASIS, REPLACE, DELETE };

  if (m4_arg_empty (argv, 1) || m4_arg_empty (argv, 2))
    {
      m4_push_arg (context, obs, argv, 1);
      return;
    }

  from = M4ARG (2);
  from_len = M4ARGLEN (2);

  to = M4ARG (3);
  to_len = M4ARGLEN (3);
  if (memchr (to, '-', to_len) != NULL)
    to = m4_expand_ranges (to, &to_len, m4_arg_scratch (context));

  /* If there are only one or two bytes to replace, it is faster to
     use memchr2.  Using expand_ranges does nothing unless there are
     at least three bytes.  */
  if (from_len <= 2)
    {
      const char *p;
      size_t len = M4ARGLEN (1);
      int second = from[from_len / 2];
      data = M4ARG (1);
      while ((p = (char *) memchr2 (data, from[0], second, len)))
        {
          obstack_grow (obs, data, p - data);
          len -= p - data + 1;
          data = p + 1;
          if (*p == from[0] && to_len)
            obstack_1grow (obs, to[0]);
          else if (*p == second && 1 < to_len)
            obstack_1grow (obs, to[1]);
        }
      obstack_grow (obs, data, len);
      return;
    }

  if (memchr (from, '-', from_len) != NULL)
    from = m4_expand_ranges (from, &from_len, m4_arg_scratch (context));

  /* Calling memchr(from) for each character in data is quadratic,
     since both strings can be arbitrarily long.  Instead, create a
     from-to mapping in one pass of from, then use that map in one
     pass of data, for linear behavior.  Traditional behavior is that
     only the first instance of a character in from is consulted,
     hence the found map.  */
  memset (map, 0, sizeof map);
  memset (found, 0, sizeof found);
  while (from_len--)
    {
      ch = *from++;
      if (found[ch] == ASIS)
        {
          if (to_len)
            {
              found[ch] = REPLACE;
              map[ch] = *to;
            }
          else
            found[ch] = DELETE;
        }
      if (to_len)
        {
          to++;
          to_len--;
        }
    }

  data = M4ARG (1);
  from_len = M4ARGLEN (1);
  while (from_len--)
    {
      ch = *data++;
      switch (found[ch])
        {
        case ASIS:
          obstack_1grow (obs, ch);
          break;
        case REPLACE:
          obstack_1grow (obs, map[ch]);
          break;
        case DELETE:
          break;
        default:
          assert (!"translit");
          abort ();
        }
    }
}



/* The rest of this file contains the functions to evaluate integer
 * expressions for the "eval" macro.  `number' should be at least 32 bits.
 */
#define numb_set(ans, x) ((ans) = (x))
#define numb_set_si(ans, si) (*(ans) = (number) (si))

#define numb_ZERO ((number) 0)
#define numb_ONE  ((number) 1)

#define numb_init(x) ((x) = numb_ZERO)
#define numb_fini(x)

#define numb_incr(n) ((n) += numb_ONE)
#define numb_decr(n) ((n) -= numb_ONE)

#define numb_zerop(x)     ((x) == numb_ZERO)
#define numb_positivep(x) ((x) >  numb_ZERO)
#define numb_negativep(x) ((x) <  numb_ZERO)

#define numb_eq(x, y) ((x) = ((x) == (y)))
#define numb_ne(x, y) ((x) = ((x) != (y)))
#define numb_lt(x, y) ((x) = ((x) <  (y)))
#define numb_le(x, y) ((x) = ((x) <= (y)))
#define numb_gt(x, y) ((x) = ((x) >  (y)))
#define numb_ge(x, y) ((x) = ((x) >= (y)))

#define numb_lnot(x)    ((x) = (!(x)))
#define numb_lior(x, y) ((x) = ((x) || (y)))
#define numb_land(x, y) ((x) = ((x) && (y)))

#define numb_not(c, x)    (*(x) = ~ *(x))
#define numb_eor(c, x, y) (*(x) = *(x) ^ *(y))
#define numb_ior(c, x, y) (*(x) = *(x) | *(y))
#define numb_and(c, x, y) (*(x) = *(x) & *(y))

#define numb_plus(x, y)  ((x) = ((x) + (y)))
#define numb_minus(x, y) ((x) = ((x) - (y)))
#define numb_negate(x)   ((x) = (- (x)))

#define numb_times(x, y)     ((x) = ((x) * (y)))
/* Be careful of x86 SIGFPE.  */
#define numb_ratio(x, y)                                                \
  (((y) == -1) ? (numb_negate (x)) : ((x) /= (y)))
#define numb_divide(x, y)                                               \
  ((*(y) == -1) ? (numb_negate (*(y))) : (*(x) /= *(y)))
#define numb_modulo(c, x, y)                                            \
  ((*(y) == -1) ? (*(x) = numb_ZERO) : (*(x) %= *(y)))
/* numb_invert is only used in the context of x**-y, which integral math
   does not support.  */
#define numb_invert(x)       return NEGATIVE_EXPONENT

/* Minimize undefined C behavior (shifting by a negative number,
   shifting by the width or greater, left shift overflow, or right
   shift of a negative number).  Implement Java wrap-around semantics,
   with implicit masking of shift amount.  This code assumes that the
   implementation-defined overflow when casting unsigned to signed is
   a silent twos-complement wrap-around.  */
#define shift_mask (sizeof (number) * CHAR_BIT - 1)
#define numb_lshift(c, x, y)                                    \
  (*(x) = (number) ((unumber) *(x) << (*(y) & shift_mask)))
#define numb_rshift(c, x, y)                                    \
  (*(x) = (number) (*(x) < 0                                    \
                    ? ~(~(unumber) *(x) >> (*(y) & shift_mask)) \
                    : (unumber) *(x) >> (*(y) & shift_mask)))
#define numb_urshift(c, x, y)                                   \
  (*(x) = (number) ((unumber) *(x) >> (*(y) & shift_mask)))


/* The function ntoa () converts VALUE to a signed ASCII representation in
   radix RADIX.  Radix must be between 2 and 36, inclusive.  */
static const char *
ntoa (number value, int radix)
{
  /* Digits for number to ASCII conversions.  */
  static char const ntoa_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  bool negative;
  unumber uvalue;
  /* Sized for radix 2, plus sign and trailing NUL.  */
  static char str[sizeof value * CHAR_BIT + 2];
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
numb_obstack (m4_obstack *obs, number value, int radix, int min)
{
  const char *s;
  size_t len;
  unumber uvalue;

  if (radix == 1)
    {
      if (value < 0)
        {
          obstack_1grow (obs, '-');
          uvalue = -value;
        }
      else
        uvalue = value;
      if (uvalue < min)
        {
          obstack_blank (obs, min - uvalue);
          memset ((char *) obstack_next_free (obs) - (min - uvalue), '0',
                  min - uvalue);
        }
      obstack_blank (obs, uvalue);
      memset ((char *) obstack_next_free (obs) - uvalue, '1', uvalue);
      return;
    }

  s = ntoa (value, radix);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      s++;
    }
  len = strlen (s);
  if (len < min)
    {
      min -= len;
      obstack_blank (obs, min);
      memset ((char *) obstack_next_free (obs) - min, '0', min);
    }
  obstack_grow (obs, s, len);
}


static void
numb_initialise (void)
{
  ;
}

/* This macro defines the top level code for the "eval" builtin.  The
   actual work is done in the function m4_evaluate (), which lives in
   evalparse.c.  */
#define m4_evaluate     builtin_eval
#include "evalparse.c"

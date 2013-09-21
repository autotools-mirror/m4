/* GNU m4 -- A simple macro processor
   Copyright (C) 2000, 2004-2010, 2013 Free Software Foundation, Inc.

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

/* Build using only the exported interfaces, unles NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

#include "modules/m4.h"
#include "quotearg.h"
#include "spawn-pipe.h"
#include "wait-process.h"

/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

           function     macros  blind   side    minargs maxargs */
#define builtin_functions                                       \
  BUILTIN (__file__,    false,  false,  false,  0,      0 )     \
  BUILTIN (__line__,    false,  false,  false,  0,      0  )    \
  BUILTIN (__program__, false,  false,  false,  0,      0  )    \
  BUILTIN (builtin,     true,   true,   false,  1,      -1 )    \
  BUILTIN (changeresyntax,false,true,   false,  1,      1  )    \
  BUILTIN (changesyntax,false,  true,   false,  1,      -1 )    \
  BUILTIN (debugfile,   false,  false,  false,  0,      1  )    \
  BUILTIN (debuglen,    false,  true,   false,  1,      1  )    \
  BUILTIN (debugmode,   false,  false,  false,  0,      1  )    \
  BUILTIN (esyscmd,     false,  true,   true,   1,      1  )    \
  BUILTIN (format,      false,  true,   false,  1,      -1 )    \
  BUILTIN (indir,       true,   true,   false,  1,      -1 )    \
  BUILTIN (mkdtemp,     false,  true,   false,  1,      1  )    \
  BUILTIN (patsubst,    false,  true,   true,   2,      4  )    \
  BUILTIN (regexp,      false,  true,   true,   2,      4  )    \
  BUILTIN (renamesyms,  false,  true,   false,  2,      3  )    \
  BUILTIN (m4modules,   false,  false,  false,  0,      0  )    \
  BUILTIN (m4symbols,   true,   false,  false,  0,      -1 )    \
  BUILTIN (syncoutput,  false,  true,   false,  1,      1  )    \


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros, blind, side, min, max)  M4BUILTIN (handler)
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


/* A table for mapping m4 symbol names to simple expansion text. */
static const m4_macro m4_macro_table[] =
{
  /* name               text    min     max */
#if UNIX
  { "__unix__",         "",     0,      0 },
#endif
#if W32_NATIVE
  { "__windows__",      "",     0,      0 },
#endif
#if OS2
  { "__os2__",          "",     0,      0 },
#endif
  { "__gnu__",          "",     0,      0 },
  { "__m4_version__",   VERSION,0,      0 },

  { NULL,               NULL,   0,      0 },
};


void
include_gnu (m4 *context, m4_module *module, m4_obstack *obs)
{
  m4_install_builtins (context, module, m4_builtin_table);
  m4_install_macros   (context, module, m4_macro_table);
}



/* Regular expressions.  Reuse re_registers among multiple
   re_pattern_buffer allocations to reduce malloc usage.  */

/* Maybe this is worth making runtime tunable.  Too small, and nothing
   gets cached because the working set of active regex is larger than
   the cache, and we are always swapping out entries.  Too large, and
   the time spent searching the cache for a match overtakes the time
   saved by caching.  For now, this size proved reasonable for the
   typical working set of Autoconf 2.62.  */
#define REGEX_CACHE_SIZE 16

/* Structure for using a compiled regex, as well as making it easier
   to cache frequently used expressions.  */
typedef struct {
  unsigned count;                       /* usage counter */
  int resyntax;                         /* flavor of regex */
  size_t len;                           /* length of string */
  char *str;                            /* copy of compiled string */
  struct re_pattern_buffer *pat;        /* compiled regex, allocated */
  struct re_registers regs;             /* match registers, reused */
} m4_pattern_buffer;

/* Storage for the cache of regular expressions.  */
static m4_pattern_buffer regex_cache[REGEX_CACHE_SIZE];

/* Compile a REGEXP of length LEN using the RESYNTAX flavor, and
   return the buffer.  On error, report the problem on behalf of
   CALLER, and return NULL.  */

static m4_pattern_buffer *
regexp_compile (m4 *context, const m4_call_info *caller, const char *regexp,
                size_t len, int resyntax)
{
  /* regex_cache is guaranteed to start life 0-initialized, which
     works in the algorithm below.

     FIXME - this method is not reentrant, since re_compile_pattern
     mallocs memory, depends on the global variable re_syntax_options
     for its syntax (but at least the compiled regex remembers its
     syntax even if the global variable changes later), and since we
     use a static variable.  To be reentrant, we would need a mutex in
     this method, and move the storage for regex_cache into context.  */

  const char *msg;              /* error message from re_compile_pattern */
  int i;                        /* iterator */
  m4_pattern_buffer *victim;    /* cache slot to replace */
  unsigned victim_count;        /* track which victim to replace */
  struct re_pattern_buffer *pat;/* newly compiled regex */

  /* First, check if REGEXP is already cached with the given RESYNTAX.
     If so, increase its use count and return it.  */
  for (i = 0; i < REGEX_CACHE_SIZE; i++)
    if (len == regex_cache[i].len && resyntax == regex_cache[i].resyntax
        && regex_cache[i].str && memcmp (regexp, regex_cache[i].str, len) == 0)
      {
        regex_cache[i].count++;
        return &regex_cache[i];
      }

  /* Next, check if REGEXP can be compiled.  */
  pat = (struct re_pattern_buffer *) xzalloc (sizeof *pat);
  re_set_syntax (resyntax);
  msg = re_compile_pattern (regexp, len, pat);

  if (msg != NULL)
    {
      m4_warn (context, 0, caller, _("bad regular expression %s: %s"),
               quotearg_style_mem (locale_quoting_style, regexp, len), msg);
      regfree (pat);
      free (pat);
      return NULL;
    }
  /* Use a fastmap for speed; it is freed by regfree.  */
  pat->fastmap = xcharalloc (UCHAR_MAX + 1);

  /* Now, find a victim slot.  Decrease the count of all entries, then
     prime the count of the victim slot at REGEX_CACHE_SIZE.  This
     way, frequently used entries and newly created entries are least
     likely to be victims next time we have a cache miss.  */
  victim = regex_cache;
  victim_count = victim->count;
  if (victim_count)
    victim->count--;
  for (i = 1; i < REGEX_CACHE_SIZE; i++)
    {
      if (regex_cache[i].count < victim_count)
        {
          victim_count = regex_cache[i].count;
          victim = &regex_cache[i];
        }
      if (regex_cache[i].count)
        regex_cache[i].count--;
    }
  victim->count = REGEX_CACHE_SIZE;
  victim->resyntax = resyntax;
  victim->len = len;
  if (victim->str)
    {
      free (victim->str);
      regfree (victim->pat);
      free (victim->pat);
    }
  victim->str = xstrdup (regexp);
  victim->pat = pat;
  re_set_registers (pat, &victim->regs, victim->regs.num_regs,
                    victim->regs.start, victim->regs.end);
  return victim;
}


/* Wrap up GNU Regex re_search call to work with an m4_pattern_buffer.
   If NO_SUB, then storing matches in buf->regs is not necessary.  */

static regoff_t
regexp_search (m4_pattern_buffer *buf, const char *string, const int size,
               const int start, const int range, bool no_sub)
{
  return re_search (buf->pat, string, size, start, range,
                    no_sub ? NULL : &buf->regs);
}


/* Function to perform substitution by regular expressions.  Used by
   the builtins regexp, patsubst and renamesyms.  The changed text is
   placed on the obstack OBS.  The substitution is REPL of length
   REPL_LEN, with \& substituted by this part of VICTIM matched by the
   last whole regular expression, and \N substituted by the text
   matched by the Nth parenthesized sub-expression in BUF.  Any
   warnings are issued on behalf of CALLER.  BUF may be NULL for the
   empty regex.  */

static void
substitute (m4 *context, m4_obstack *obs, const m4_call_info *caller,
            const char *victim, const char *repl, size_t repl_len,
            m4_pattern_buffer *buf)
{
  int ch;
  while (1)
    {
      const char *backslash = (char *) memchr (repl, '\\', repl_len);
      if (!backslash)
        {
          obstack_grow (obs, repl, repl_len);
          return;
        }
      obstack_grow (obs, repl, backslash - repl);
      repl_len -= backslash - repl + 1;
      if (!repl_len)
        {
          m4_warn (context, 0, caller,
                   _("trailing \\ ignored in replacement"));
          return;
        }
      repl = backslash + 1;
      ch = *repl++;
      repl_len--;
      switch (ch)
        {
        case '&':
          if (buf)
            obstack_grow (obs, victim + buf->regs.start[0],
                          buf->regs.end[0] - buf->regs.start[0]);
          break;

        case '1': case '2': case '3': case '4': case '5': case '6':
        case '7': case '8': case '9':
          ch -= '0';
          if (!buf || buf->pat->re_nsub < ch)
            m4_warn (context, 0, caller, _("sub-expression %d not present"),
                     ch);
          else if (buf->regs.end[ch] > 0)
            obstack_grow (obs, victim + buf->regs.start[ch],
                          buf->regs.end[ch] - buf->regs.start[ch]);
          break;

        default:
          obstack_1grow (obs, ch);
          break;
        }
    }
}


/* For each match against REGEXP of length REGEXP_LEN (precompiled in
   BUF as returned by regexp_compile) in VICTIM of length LEN,
   substitute REPLACE of length REPL_LEN.  Non-matching characters are
   copied verbatim, and the result copied to the obstack.  Errors are
   reported on behalf of CALLER.  Return true if a substitution was
   made.  If OPTIMIZE is set, don't worry about copying the input if
   no changes are made.  */

static bool
regexp_substitute (m4 *context, m4_obstack *obs, const m4_call_info *caller,
                   const char *victim, size_t len, const char *regexp,
                   size_t regexp_len, m4_pattern_buffer *buf,
                   const char *replace, size_t repl_len, bool optimize)
{
  regoff_t matchpos = 0;        /* start position of match */
  size_t offset = 0;            /* current match offset */
  bool subst = !optimize;       /* if a substitution has been made */

  while (offset <= len)
    {
      matchpos = regexp_search (buf, victim, len, offset, len - offset,
                                false);

      if (matchpos < 0)
        {

          /* Match failed -- either error or there is no match in the
             rest of the string, in which case the rest of the string is
             copied verbatim.  */

          if (matchpos == -2)
            m4_error (context, 0, 0, caller,
                      _("problem matching regular expression %s"),
                      quotearg_style_mem (locale_quoting_style, regexp,
                                          regexp_len));
          else if (offset < len && subst)
            obstack_grow (obs, victim + offset, len - offset);
          break;
        }

      /* Copy the part of the string that was skipped by re_search ().  */

      if (matchpos > offset)
        obstack_grow (obs, victim + offset, matchpos - offset);

      /* Handle the part of the string that was covered by the match.  */

      substitute (context, obs, caller, victim, replace, repl_len, buf);
      subst = true;

      /* Update the offset to the end of the match.  If the regexp
         matched a null string, advance offset one more, to avoid
         infinite loops.  */

      offset = buf->regs.end[0];
      if (buf->regs.start[0] == buf->regs.end[0])
        {
          if (offset < len)
            obstack_1grow (obs, victim[offset]);
          offset++;
        }
    }

  return subst;
}



/**
 * __file__
 **/
M4BUILTIN_HANDLER (__file__)
{
  m4_shipout_string (context, obs, m4_get_current_file (context), SIZE_MAX,
                     true);
}


/**
 * __line__
 **/
M4BUILTIN_HANDLER (__line__)
{
  m4_shipout_int (obs, m4_get_current_line (context));
}


/**
 * __program__
 **/
M4BUILTIN_HANDLER (__program__)
{
  m4_shipout_string (context, obs, m4_get_program_name (), SIZE_MAX, true);
}


/* The builtin "builtin" allows calls to builtin macros, even if their
   definition has been overridden or shadowed.  It is thus possible to
   redefine builtins, and still access their original definition.  A
   special form allows one to retrieve the special token that defn
   would normally return, even if that builtin is not currently
   defined and hence can't be passed to defn.  */

/**
 * builtin(MACRO, [...])
 * builtin(defn(`builtin'), MACRO)
 **/
M4BUILTIN_HANDLER (builtin)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *name;
  size_t len;
  m4_symbol_value *value = NULL;

  if (!m4_is_arg_text (argv, 1))
    {
      assert (m4_is_arg_func (argv, 1));
      if (m4_arg_func (argv, 1) == builtin_builtin)
        {
          if (m4_bad_argc (context, argc, me, 2, 2, false))
            return;
          if (!m4_is_arg_text (argv, 2))
            {
              m4_warn (context, 0, me, _("invalid macro name ignored"));
              return;
            }
          name = M4ARG (2);
          len = M4ARGLEN (2);
          if (len == strlen (name))
            value = m4_builtin_find_by_name (context, NULL, name);
          if (value)
            {
              m4_push_builtin (context, obs, value);
              free (value);
            }
          else if (m4_is_debug_bit (context, M4_DEBUG_TRACE_DEREF))
            m4_warn (context, 0, me, _("undefined builtin %s"),
                     quotearg_style_mem (locale_quoting_style, name, len));
        }
      else
        m4_warn (context, 0, me, _("invalid macro name ignored"));
    }
  else
    {
      name = M4ARG (1);
      len = M4ARGLEN (1);
      if (len == strlen (name))
        value = m4_builtin_find_by_name (context, NULL, name);
      if (value == NULL)
        {
          if (m4_is_debug_bit (context, M4_DEBUG_TRACE_DEREF))
            m4_warn (context, 0, me, _("undefined builtin %s"),
                     quotearg_style_mem (locale_quoting_style, name, len));
        }
      else
        {
          const m4_builtin *bp = m4_get_symbol_value_builtin (value);
          m4_macro_args *new_argv;
          bool flatten = (bp->flags & M4_BUILTIN_FLATTEN_ARGS) != 0;
          new_argv = m4_make_argv_ref (context, argv, name, len, flatten,
                                       false);
          if (!m4_bad_argc (context, argc - 1, m4_arg_info (new_argv),
                            bp->min_args, bp->max_args,
                            (bp->flags & M4_BUILTIN_SIDE_EFFECT) != 0))
            bp->func (context, obs, argc - 1, new_argv);
          free (value);
        }
    }
}


/* Change the current regexp syntax to SPEC of length LEN, or report
   failure on behalf of CALLER.  Currently this affects the builtins:
   `patsubst', `regexp' and `renamesyms'.  */

static int
m4_resyntax_encode_safe (m4 *context, const m4_call_info *caller,
                         const char *spec, size_t len)
{
  int resyntax;

  if (strlen (spec) < len)
    resyntax = -1;
  else
    resyntax = m4_regexp_syntax_encode (spec);

  if (resyntax < 0)
    m4_warn (context, 0, caller, _("bad syntax-spec: %s"),
             quotearg_style_mem (locale_quoting_style, spec, len));

  return resyntax;
}


/**
 * changeresyntax(RESYNTAX-SPEC)
 **/
M4BUILTIN_HANDLER (changeresyntax)
{
  int resyntax = m4_resyntax_encode_safe (context, m4_arg_info (argv),
                                          M4ARG (1), M4ARGLEN (1));

  if (resyntax >= 0)
    m4_set_regexp_syntax_opt (context, resyntax);
}


/* Change the current input syntax.  The function m4_set_syntax ()
   lives in syntax.c.  Any changes to comment delimiters and quotes
   made here will be overridden by a call to `changecom' or
   `changequote'.  */

/**
 * changesyntax(SYNTAX-SPEC, ...)
 **/
M4BUILTIN_HANDLER (changesyntax)
{
  const m4_call_info *me = m4_arg_info (argv);
  M4_MODULE_IMPORT (m4, m4_expand_ranges);

  if (m4_expand_ranges)
    {
      size_t i;
      for (i = 1; i < argc; i++)
        {
          size_t len = M4ARGLEN (i);
          const char *spec;
          char key;
          char action;

          if (!len)
            {
              m4_reset_syntax (M4SYNTAX);
              continue;
            }
          spec = M4ARG (i);
          key = *spec++;
          len--;
          action = len ? *spec : '\0';
          switch (action)
            {
            case '-':
            case '+':
            case '=':
              spec++;
              len--;
              break;
            case '\0':
              if (!len)
                break;
              /* fall through */
            default:
              action = '=';
              break;
            }
          if (len)
            spec = m4_expand_ranges (spec, &len, m4_arg_scratch (context));
          if (m4_set_syntax (M4SYNTAX, key, action, spec, len) < 0)
            m4_warn (context, 0, me, _("undefined syntax code: %s"),
                     quotearg_style_mem (locale_quoting_style, &key, 1));
        }
    }
  else
    assert (!"Unable to import from m4 module");
}


/* Specify the destination of the debugging output.  With one argument, the
   argument is taken as a file name, with no arguments, revert to stderr.  */

/**
 * debugfile([FILENAME])
 **/
M4BUILTIN_HANDLER (debugfile)
{
  const m4_call_info *me = m4_arg_info (argv);

  if (argc == 1)
    m4_debug_set_output (context, me, NULL);
  else if (m4_get_safer_opt (context) && !m4_arg_empty (argv, 1))
    m4_error (context, 0, 0, me, _("disabled by --safer"));
  else
    {
      const char *str = M4ARG (1);
      size_t len = M4ARGLEN (1);
      if (strlen (str) < len)
        m4_warn (context, 0, me, _("argument %s truncated"),
                 quotearg_style_mem (locale_quoting_style, str, len));
      if (!m4_debug_set_output (context, me, str))
        m4_warn (context, errno, me, _("cannot set debug file %s"),
              quotearg_style (locale_quoting_style, str));
    }
}


/* On-the-fly control of debug length.  It takes one integer
   argument.  */

/**
 * debuglen(LEN)
 **/
M4BUILTIN_HANDLER (debuglen)
{
  int i;
  size_t s;
  if (!m4_numeric_arg (context, m4_arg_info (argv), M4ARG (1), M4ARGLEN (1),
                       &i))
    return;
  /* FIXME - make m4_numeric_arg more powerful - we want to accept
     suffixes, and limit the result to size_t.  */
  s = i <= 0 ? SIZE_MAX : i;
  m4_set_max_debug_arg_length_opt (context, s);
}

/* On-the-fly control of the format of the tracing output.  It takes one
   argument, which is a character string like given to the -d option, or
   none in which case the debug_level is zeroed.  */

/**
 * debugmode([FLAGS])
 **/
M4BUILTIN_HANDLER (debugmode)
{
  const char* mode = M4ARG (1);
  size_t len = M4ARGLEN (1);
  if (argc == 1)
    m4_set_debug_level_opt (context, 0);
  else if (m4_debug_decode (context, mode, len) < 0)
    m4_warn (context, 0, m4_arg_info (argv),
             _("bad debug flags: %s"),
             quotearg_style_mem (locale_quoting_style, mode, len));
}


/* Same as the sysymd builtin from m4.c module, but expand to the
   output of SHELL-COMMAND. */

/**
 * esyscmd(SHELL-COMMAND)
 **/

M4BUILTIN_HANDLER (esyscmd)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *cmd = M4ARG (1);
  size_t len = M4ARGLEN (1);
  M4_MODULE_IMPORT (m4, m4_set_sysval);
  M4_MODULE_IMPORT (m4, m4_sysval_flush);

  if (m4_set_sysval && m4_sysval_flush)
    {
      pid_t child;
      int fd;
      FILE *pin;
      int status;
      int sig_status;
      const char *prog_args[4] = { "sh", "-c" };
      const char *caller;

      if (m4_get_safer_opt (context))
        {
          m4_error (context, 0, 0, me, _("disabled by --safer"));
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
      caller = m4_info_name (me);
      errno = 0;
      child = create_pipe_in (caller, M4_SYSCMD_SHELL, (char **) prog_args,
                              NULL, false, true, false, &fd);
      if (child == -1)
        {
          m4_error (context, 0, errno, me, _("cannot run command %s"),
                    quotearg_style (locale_quoting_style, cmd));
          m4_set_sysval (127);
          return;
        }
      pin = fdopen (fd, "r");
      if (!pin)
        {
          m4_error (context, 0, errno, me, _("cannot run command %s"),
                    quotearg_style (locale_quoting_style, cmd));
          m4_set_sysval (127);
          close (fd);
          return;
        }
      while (1)
        {
          size_t avail = obstack_room (obs);
          if (!avail)
            {
              int ch = getc (pin);
              if (ch == EOF)
                break;
              obstack_1grow (obs, ch);
            }
          else
            {
              size_t len = fread (obstack_next_free (obs), 1, avail, pin);
              if (len <= 0)
                break;
              obstack_blank_fast (obs, len);
            }
        }
      if (ferror (pin) || fclose (pin))
        m4_error (context, EXIT_FAILURE, errno, me,
                  _("cannot read pipe to command %s"),
                  quotearg_style (locale_quoting_style, cmd));
      errno = 0;
      status = wait_subprocess (child, caller, false, true, true, false,
                                &sig_status);
      if (sig_status)
        {
          assert (status == 127);
          m4_set_sysval (sig_status << 8);
        }
      else
        {
          if (status == 127 && errno)
            m4_error (context, 0, errno, me, _("cannot run command %s"),
                      quotearg_style (locale_quoting_style, cmd));
          m4_set_sysval (status);
        }
    }
  else
    assert (!"Unable to import from m4 module");
}


/* Frontend for printf like formatting.  The function format () lives in
   the file format.c.  */

#include "format.c"

/**
 * format(FORMAT-STRING, [...])
 **/
M4BUILTIN_HANDLER (format)
{
  format (context, obs, argc, argv);
}


/* The builtin "indir" allows indirect calls to macros, even if their name
   is not a proper macro name.  It is thus possible to define macros with
   ill-formed names for internal use in larger macro packages.  */

/**
 * indir(MACRO, [...])
 **/
M4BUILTIN_HANDLER (indir)
{
  const m4_call_info *me = m4_arg_info (argv);
  if (!m4_is_arg_text (argv, 1))
    m4_warn (context, 0, me, _("invalid macro name ignored"));
  else
    {
      const char *name = M4ARG (1);
      size_t len = M4ARGLEN (1);
      m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, name, len);

      if (symbol == NULL)
        {
          if (m4_is_debug_bit (context, M4_DEBUG_TRACE_DEREF))
            m4_warn (context, 0, me, _("undefined macro %s"),
                     quotearg_style_mem (locale_quoting_style, name, len));
        }
      else
        {
          m4_macro_args *new_argv;
          m4_symbol_value *value = m4_get_symbol_value (symbol);
          new_argv = m4_make_argv_ref (context, argv, name, len,
                                       m4_symbol_flatten_args (symbol),
                                       m4_get_symbol_traced (symbol));
          m4_trace_prepare (context, m4_arg_info (new_argv), value);
          m4_macro_call (context, value, obs, new_argv);
        }
    }
}


/* The builtin "mkdtemp" allows creation of temporary directories.  */

/**
 * mkdtemp(TEMPLATE)
 **/
M4BUILTIN_HANDLER (mkdtemp)
{
  M4_MODULE_IMPORT (m4, m4_make_temp);

  if (m4_make_temp)
    m4_make_temp (context, obs, m4_arg_info (argv), M4ARG (1), M4ARGLEN (1),
                  true);
  else
    assert (!"Unable to import from m4 module");
}


/* Substitute all matches of a regexp occurring in a string.  Each
   match of the second argument (a regexp) in the first argument is
   changed to the optional third argument, with \& substituted by the
   matched text, and \N substituted by the text matched by the Nth
   parenthesized sub-expression.  The optional fourth argument changes
   the regex flavor.  */

/**
 * patsubst(VICTIM, REGEXP, [REPLACEMENT], [RESYNTAX])
 **/
M4BUILTIN_HANDLER (patsubst)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *pattern;          /* regular expression */
  const char *replace;          /* replacement */
  m4_pattern_buffer *buf;       /* compiled regular expression */
  int resyntax;

  resyntax = m4_get_regexp_syntax_opt (context);
  if (argc >= 5)                /* additional args ignored */
    {
      resyntax = m4_resyntax_encode_safe (context, me, M4ARG (4),
                                          M4ARGLEN (4));
      if (resyntax < 0)
        return;
    }

  /* The empty regex matches everywhere, but if there is no
     replacement, we need not waste time with it.  */
  if (m4_arg_empty (argv, 2) && m4_arg_empty (argv, 3))
    {
      m4_push_arg (context, obs, argv, 1);
      return;
    }

  pattern = M4ARG (2);
  replace = M4ARG (3);

  buf = regexp_compile (context, me, pattern, M4ARGLEN (2), resyntax);
  if (!buf)
    return;

  regexp_substitute (context, obs, me, M4ARG (1), M4ARGLEN (1), pattern,
                     M4ARGLEN (2), buf, replace, M4ARGLEN (3), false);
}


/* Regular expression version of index.  Given two arguments, expand
   to the index of the first match of the second argument (a regexp)
   in the first.  Expand to -1 if there is no match.  Given a third
   argument, a match is substituted according to this argument.  The
   optional fourth argument changes the regex flavor.  */

/**
 * regexp(VICTIM, REGEXP, RESYNTAX)
 * regexp(VICTIM, REGEXP, [REPLACEMENT], [RESYNTAX])
 **/
M4BUILTIN_HANDLER (regexp)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *victim;           /* string to search */
  const char *pattern;          /* regular expression */
  const char *replace;          /* optional replacement string */
  m4_pattern_buffer *buf;       /* compiled regular expression */
  regoff_t startpos;            /* start position of match */
  size_t len;                   /* length of first argument */
  int resyntax;

  pattern = M4ARG (2);
  replace = M4ARG (3);
  resyntax = m4_get_regexp_syntax_opt (context);

  if (argc == 4)
    {
      resyntax = m4_regexp_syntax_encode (replace);

      /* The first case is the most difficult, because the empty string
         is a valid RESYNTAX, yet we want `regexp(aab, a*, )' to return
         an empty string as per M4 1.4.x.  */

      if (m4_arg_empty (argv, 3) || (resyntax < 0))
        /* regexp(VICTIM, REGEXP, REPLACEMENT) */
        resyntax = m4_get_regexp_syntax_opt (context);
      else
        /* regexp(VICTIM, REGEXP, RESYNTAX) */
        replace = NULL;
    }
  else if (argc >= 5)
    {
      /* regexp(VICTIM, REGEXP, REPLACEMENT, RESYNTAX) */
      resyntax = m4_resyntax_encode_safe (context, me, M4ARG (4),
                                          M4ARGLEN (4));
      if (resyntax < 0)
        return;
    }
  else
    /* regexp(VICTIM, REGEXP)  */
    replace = NULL;

  if (m4_arg_empty (argv, 2))
    {
      /* The empty regex matches everything.  */
      if (replace)
        substitute (context, obs, me, M4ARG (1), replace, M4ARGLEN (3), NULL);
      else
        m4_shipout_int (obs, 0);
      return;
    }

  buf = regexp_compile (context, me, pattern, M4ARGLEN (2), resyntax);
  if (!buf)
    return;

  victim = M4ARG (1);
  len = M4ARGLEN (1);
  startpos = regexp_search (buf, victim, len, 0, len, replace == NULL);

  if (startpos == -2)
    {
      m4_error (context, 0, 0, me, _("problem matching regular expression %s"),
                quotearg_style_mem (locale_quoting_style, pattern,
                                    M4ARGLEN (2)));
      return;
    }

  if (replace == NULL)
    m4_shipout_int (obs, startpos);
  else if (startpos >= 0)
    substitute (context, obs, me, victim, replace, M4ARGLEN (3), buf);
}


/* Rename all current symbols that match REGEXP according to the
   REPLACEMENT specification.  */

/**
 * renamesyms(REGEXP, REPLACEMENT, [RESYNTAX])
 **/
M4BUILTIN_HANDLER (renamesyms)
{
  M4_MODULE_IMPORT (m4, m4_dump_symbols);

  if (m4_dump_symbols)
    {
      const m4_call_info *me = m4_arg_info (argv);
      const char *regexp;       /* regular expression string */
      size_t regexp_len;
      const char *replace;      /* replacement expression string */
      size_t replace_len;

      m4_pattern_buffer *buf;   /* compiled regular expression */

      m4_dump_symbol_data       data;

      int resyntax;

      regexp  = M4ARG (1);
      regexp_len = M4ARGLEN (1);
      replace = M4ARG (2);
      replace_len = M4ARGLEN (2);

      resyntax = m4_get_regexp_syntax_opt (context);
      if (argc >= 4)
        {
          resyntax = m4_resyntax_encode_safe (context, me, M4ARG (3),
                                              M4ARGLEN (3));
          if (resyntax < 0)
            return;
        }

      buf = regexp_compile (context, me, regexp, regexp_len, resyntax);
      if (!buf)
        return;

      data.obs = m4_arg_scratch (context);
      m4_dump_symbols (context, &data, 1, argv, false);

      for (; data.size > 0; --data.size, data.base++)
        {
          const m4_string *key = &data.base[0];

          if (regexp_substitute (context, data.obs, me, key->str, key->len,
                                 regexp, regexp_len, buf, replace, replace_len,
                                 true))
            {
              size_t newlen = obstack_object_size (data.obs);
              m4_symbol_rename (M4SYMTAB, key->str, key->len,
                                (char *) obstack_finish (data.obs), newlen);
            }
        }
    }
  else
    assert (!"Unable to import from m4 module");
}


/**
 * m4modules()
 **/
M4BUILTIN_HANDLER (m4modules)
{
  /* The expansion of this builtin is a comma separated list of
     loaded modules.  */
  m4_module *module = m4_module_next (context, NULL);

  if (module)
    do
      {
        m4_shipout_string (context, obs, m4_get_module_name (module), SIZE_MAX,
                           true);

        if ((module = m4_module_next (context, module)))
          obstack_1grow (obs, ',');
      }
    while (module);
}


/* Implementation of "m4symbols".  It builds up a table of pointers to
   symbols, sorts it and ships out the symbol names.  */

/**
 * m4symbols([...])
 **/
M4BUILTIN_HANDLER (m4symbols)
{
  M4_MODULE_IMPORT (m4, m4_dump_symbols);

  if (m4_dump_symbols)
    {
      m4_dump_symbol_data data;

      data.obs = m4_arg_scratch (context);
      m4_dump_symbols (context, &data, argc, argv, false);

      for (; data.size > 0; --data.size, data.base++)
        {
          m4_shipout_string (context, obs, data.base->str, data.base->len,
                             true);
          if (data.size > 1)
            obstack_1grow (obs, ',');
        }
    }
  else
    assert (!"Unable to import from m4 module");
}


/* This contains macro which implements syncoutput() which takes one arg
     1, on, yes - turn on sync lines
     0, off, no, blank - turn off sync lines
     everything else is silently ignored  */

/**
 * syncoutput(TRUTH)
 **/
M4BUILTIN_HANDLER (syncoutput)
{
  bool value = m4_get_syncoutput_opt (context);
  value = m4_parse_truth_arg (context, m4_arg_info (argv), M4ARG (1),
                              M4ARGLEN (1), value);
  m4_set_syncoutput_opt (context, value);
}

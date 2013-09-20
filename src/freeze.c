/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 2004-2010, 2013 Free Software Foundation,
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

/* This module handles frozen files.  */

#include <config.h>

#include "m4.h"

#include "binary-io.h"
#include "close-stream.h"
#include "quotearg.h"
#include "verify.h"
#include "xmemdup0.h"

static  void  produce_mem_dump          (FILE *, const char *, size_t);
static  void  produce_resyntax_dump     (m4 *, FILE *);
static  void  produce_syntax_dump       (FILE *, m4_syntax_table *, char);
static  void  produce_module_dump       (m4 *, FILE *, m4_module *);
static  void  produce_symbol_dump       (m4 *, FILE *, m4_symbol_table *);
static  void *dump_symbol_CB            (m4_symbol_table *, const char *,
                                         size_t, m4_symbol *, void *);
static  void  issue_expect_message      (m4 *, int);
static  int   decode_char               (m4 *, FILE *, bool *);


/* Dump an ASCII-encoded representation of LEN bytes at MEM to FILE.
   MEM may contain embedded NUL characters.  */
static void
produce_mem_dump (FILE *file, const char *mem, size_t len)
{
  char *quoted = quotearg_style_mem (escape_quoting_style, mem, len);
  /* Any errors will be detected by ferror later.  */
  fwrite (quoted, strlen (quoted), 1, file);
}


/* Produce the 'R14\nPOSIX_EXTENDED\n' frozen file dump of the current
   default regular expression syntax.  Note that it would be a little
   faster to use the encoded syntax in this format as used by re_compile(),
   but the representation of RE_SYNTAX_POSIX_EXTENDED may change in
   future (or alternative) implementations of re_compile, so we use an
   unencoded representation here.  */

static void
produce_resyntax_dump (m4 *context, FILE *file)
{
  int code = m4_get_regexp_syntax_opt (context);

  /* Don't dump default syntax code (`0' for GNU_EMACS).  */
  if (code)
    {
      const char *resyntax = m4_regexp_syntax_decode (code);

      if (!resyntax)
        m4_error (context, EXIT_FAILURE, 0, NULL,
                  _("invalid regexp syntax code `%d'"), code);

      /* No need to use produce_mem_dump, since we know all resyntax
         names are already ASCII-encoded.  */
      xfprintf (file, "R%zu\n%s\n", strlen (resyntax), resyntax);
    }
}

static void
produce_syntax_dump (FILE *file, m4_syntax_table *syntax, char ch)
{
  char buf[UCHAR_MAX + 1];
  int code = m4_syntax_code (ch);
  int count = 0;
  int i;

  for (i = 0; i < UCHAR_MAX + 1; ++i)
    if (m4_has_syntax (syntax, i, code) && code != syntax->orig[i])
      buf[count++] = i;

  /* If code falls in M4_SYNTAX_MASKS, then we must treat it
     specially, since it will not be found in syntax->orig.  */
  if (count == 1
      && ((code == M4_SYNTAX_RQUOTE && *buf == *DEF_RQUOTE)
          || (code == M4_SYNTAX_ECOMM && *buf == *DEF_ECOMM)))
    return;

  if (count || (code & M4_SYNTAX_MASKS))
    {
      xfprintf (file, "S%c%d\n", ch, count);
      produce_mem_dump (file, buf, count);
      fputc ('\n', file);
    }
}

/* Store the debug mode in textual format.  */
static void
produce_debugmode_state (FILE *file, int flags)
{
  /* This code tracks the number of bits in M4_DEBUG_TRACE_VERBOSE.  */
  char str[15];
  int offset = 0;
  verify ((1 << (sizeof str - 1)) - 1 == M4_DEBUG_TRACE_VERBOSE);
  if (flags & M4_DEBUG_TRACE_ARGS)
    str[offset++] = 'a';
  if (flags & M4_DEBUG_TRACE_EXPANSION)
    str[offset++] = 'e';
  if (flags & M4_DEBUG_TRACE_QUOTE)
    str[offset++] = 'q';
  if (flags & M4_DEBUG_TRACE_ALL)
    str[offset++] = 't';
  if (flags & M4_DEBUG_TRACE_LINE)
    str[offset++] = 'l';
  if (flags & M4_DEBUG_TRACE_FILE)
    str[offset++] = 'f';
  if (flags & M4_DEBUG_TRACE_PATH)
    str[offset++] = 'p';
  if (flags & M4_DEBUG_TRACE_CALL)
    str[offset++] = 'c';
  if (flags & M4_DEBUG_TRACE_INPUT)
    str[offset++] = 'i';
  if (flags & M4_DEBUG_TRACE_CALLID)
    str[offset++] = 'x';
  if (flags & M4_DEBUG_TRACE_MODULE)
    str[offset++] = 'm';
  if (flags & M4_DEBUG_TRACE_STACK)
    str[offset++] = 's';
  if (flags & M4_DEBUG_TRACE_DEREF)
    str[offset++] = 'd';
  if (flags & M4_DEBUG_TRACE_OUTPUT_DUMPDEF)
    str[offset++] = 'o';
  str[offset] = '\0';
  if (offset)
    xfprintf (file, "d%d\n%s\n", offset, str);
}

/* The modules must be dumped in the order in which they will be
   reloaded from the frozen file.  We store handles in a push
   down stack, so we need to dump them in the reverse order to that.  */
static void
produce_module_dump (m4 *context, FILE *file, m4_module *module)
{
  const char *name = m4_get_module_name (module);
  size_t len = strlen (name);

  module = m4_module_next (context, module);
  if (module)
    produce_module_dump (context, file, module);

  xfprintf (file, "M%zu\n", len);
  produce_mem_dump (file, name, len);
  fputc ('\n', file);
}

/* Process all entries in one bucket, from the last to the first.
   This order ensures that, at reload time, pushdef's will be
   executed with the oldest definitions first.  */
static void
produce_symbol_dump (m4 *context, FILE *file, m4_symbol_table *symtab)
{
  if (m4_symtab_apply (symtab, true, dump_symbol_CB, file))
    assert (false);
}

/* Given a stack of symbol values starting with VALUE, destructively
   reverse the stack and return the pointer to what was previously the
   last value in the stack.  VALUE may be NULL.  The symbol table that
   owns the value stack should not be modified or consulted until this
   is called again to undo the effect.  */
static m4_symbol_value *
reverse_symbol_value_stack (m4_symbol_value *value)
{
  m4_symbol_value *result = NULL;
  m4_symbol_value *next;
  while (value)
    {
      next = VALUE_NEXT (value);
      VALUE_NEXT (value) = result;
      result = value;
      value = next;
    }
  return result;
}

/* Dump the stack of values for SYMBOL, with name SYMBOL_NAME and
   length LEN, located in SYMTAB.  USERDATA is interpreted as the
   FILE* to dump to.  */
static void *
dump_symbol_CB (m4_symbol_table *symtab, const char *symbol_name, size_t len,
                m4_symbol *symbol, void *userdata)
{
  FILE *file = (FILE *) userdata;
  m4_symbol_value *value;
  m4_symbol_value *last;

  last = value = reverse_symbol_value_stack (m4_get_symbol_value (symbol));
  while (value)
    {
      m4_module *module = VALUE_MODULE (value);
      const char *module_name = module ? m4_get_module_name (module) : NULL;
      size_t module_len = module_name ? strlen (module_name) : 0;

      if (m4_is_symbol_value_text (value))
        {
          const char *text = m4_get_symbol_value_text (value);
          size_t text_len = m4_get_symbol_value_len (value);
          xfprintf (file, "T%zu,%zu", len, text_len);
          if (module)
            xfprintf (file, ",%zu", module_len);
          fputc ('\n', file);

          produce_mem_dump (file, symbol_name, len);
          fputc ('\n', file);
          produce_mem_dump (file, text, text_len);
          fputc ('\n', file);
          if (module)
            {
              produce_mem_dump (file, module_name, module_len);
              fputc ('\n', file);
            }
        }
      else if (m4_is_symbol_value_func (value))
        {
          const m4_builtin *bp = m4_get_symbol_value_builtin (value);
          size_t bp_len;
          if (bp == NULL)
            assert (!"INTERNAL ERROR: builtin not found in builtin table!");
          bp_len = strlen (bp->name);

          xfprintf (file, "F%zu,%zu", len, bp_len);
          if (module)
            xfprintf (file, ",%zu", module_len);
          fputc ('\n', file);

          produce_mem_dump (file, symbol_name, len);
          fputc ('\n', file);
          produce_mem_dump (file, bp->name, bp_len);
          fputc ('\n', file);
          if (module)
            {
              produce_mem_dump (file, module_name, module_len);
              fputc ('\n', file);
            }
        }
      else if (m4_is_symbol_value_placeholder (value))
        ; /* Nothing to do for a builtin we couldn't reload earlier.  */
      else
        assert (!"dump_symbol_CB");
      value = VALUE_NEXT (value);
    }
  reverse_symbol_value_stack (last);
  if (m4_get_symbol_traced (symbol))
    xfprintf (file, "t%zu\n%s\n", len, symbol_name);
  return NULL;
}

/* Produce a frozen state to the given file NAME. */
void
produce_frozen_state (m4 *context, const char *name)
{
  FILE *file = fopen (name, O_BINARY ? "wb" : "w");
  const char *str;
  const m4_string_pair *pair;

  if (!file)
    {
      m4_error (context, 0, errno, NULL, _("cannot open %s"),
                quotearg_style (locale_quoting_style, name));
      return;
    }

  /* Write a recognizable header.  */

  xfprintf (file, "# This is a frozen state file generated by GNU %s %s\n",
            PACKAGE, VERSION);
  fputs ("V2\n", file);

  /* Dump quote delimiters.  */
  pair = m4_get_syntax_quotes (M4SYNTAX);
  if (STRNEQ (pair->str1, DEF_LQUOTE) || STRNEQ (pair->str2, DEF_RQUOTE))
    {
      xfprintf (file, "Q%zu,%zu\n", pair->len1, pair->len2);
      produce_mem_dump (file, pair->str1, pair->len1);
      fputc ('\n', file);
      produce_mem_dump (file, pair->str2, pair->len2);
      fputc ('\n', file);
    }

  /* Dump comment delimiters.  */
  pair = m4_get_syntax_comments (M4SYNTAX);
  if (STRNEQ (pair->str1, DEF_BCOMM) || STRNEQ (pair->str2, DEF_ECOMM))
    {
      xfprintf (file, "C%zu,%zu\n", pair->len1, pair->len2);
      produce_mem_dump (file, pair->str1, pair->len1);
      fputc ('\n', file);
      produce_mem_dump (file, pair->str2, pair->len2);
      fputc ('\n', file);
    }

  /* Dump regular expression syntax.  */
  produce_resyntax_dump (context, file);

  /* Dump syntax table.  */
  str = "I@WLBOD${}SA(),RE";
  while (*str)
    produce_syntax_dump (file, M4SYNTAX, *str++);

  /* Dump debugmode state.  */
  produce_debugmode_state (file, m4_get_debug_level_opt (context));

  /* Dump all loaded modules.  */
  produce_module_dump (context, file, m4_module_next (context, NULL));

  /* Dump all symbols.  */
  produce_symbol_dump (context, file, M4SYMTAB);

  /* Let diversions be issued from output.c module, its cleaner to have this
     piece of code there.  */
  m4_freeze_diversions (context, file);

  /* All done.  */

  fputs ("# End of frozen state file\n", file);
  if (close_stream (file) != 0)
    m4_error (context, EXIT_FAILURE, errno, NULL,
              _("unable to create frozen state"));
}

/* Issue a message saying that some character is an EXPECTED character. */
static void
issue_expect_message (m4 *context, int expected)
{
  if (expected == '\n')
    m4_error (context, EXIT_FAILURE, 0, NULL,
              _("expecting line feed in frozen file"));
  else
    m4_error (context, EXIT_FAILURE, 0, NULL,
              _("expecting character `%c' in frozen file"), expected);
}


/* Reload frozen state.  */

/* Read the next character from the IN stream.  Various escape
   sequences are converted, and returned.  EOF is returned if the end
   of file is reached whilst reading the character, or on an
   unrecognized escape sequence.  */

static int
decode_char (m4 *context, FILE *in, bool *advance_line)
{
  int ch = getc (in);
  int next;
  int value = 0;

  if (*advance_line)
    {
      m4_set_current_line (context, m4_get_current_line (context) + 1);
      *advance_line = false;
    }

  while (ch == '\\')
    {
      ch = getc (in);
      switch (ch)
        {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';

        case '\n':
          ch = getc (in);
          m4_set_current_line (context, m4_get_current_line (context) + 1);
          continue;

        case 'x': case 'X':
          next = getc (in);
          if (next >= '0' && next <= '9')
            ch = (next - '0') * 16;
          else if (next >= 'a' && next <= 'f')
            ch = (next - 'a' + 10) * 16;
          else if (next >= 'A' && next <= 'F')
            ch = (next - 'A' + 10) * 16;
          else
            return EOF;
          next = getc (in);
          if (next >= '0' && next <= '9')
            ch += next - '0';
          else if (next >= 'a' && next <= 'f')
            ch += next - 'a' + 10;
          else if (next >= 'A' && next <= 'F')
            ch += next - 'A' + 10;
          else
            return EOF;
          return ch;
        case '0': case '1': case '2': case '3':
          value = ch - '0';
          ch = getc (in);
          /* fall through */
        case '4': case '5': case '6': case '7':
          if (ch >= '0' && ch <= '7')
            {
              value = value * 8 + ch - '0';
              ch = getc (in);
            }
          else
            {
              ungetc (ch, in);
              return value;
            }
          if (ch >= '0' && ch <= '7')
            value = value * 8 + ch - '0';
          else
            ungetc (ch, in);
          return value;

        default:
          return EOF;
        }
    }

  if (ch == '\n')
    *advance_line = true;
  return ch;
}


/*  Reload state from the given file NAME.  We are seeking speed,
    here.  */

void
reload_frozen_state (m4 *context, const char *name)
{
  FILE *file = NULL;
  char *filepath;
  int version;
  int character;
  int operation;
  char syntax;
  char *string[3];
  size_t allocated[3];
  int number[3] = {0};
  bool advance_line = true;

#define GET_CHARACTER                                                   \
  do                                                                    \
    {                                                                   \
      if (advance_line)                                                 \
        {                                                               \
          m4_set_current_line (context,                                 \
                               m4_get_current_line (context) + 1);      \
          advance_line = false;                                         \
        }                                                               \
      character = getc (file);                                          \
      if (character == '\n')                                            \
        advance_line = true;                                            \
    }                                                                   \
  while (0)

#define GET_NUMBER(Number, AllowNeg)                            \
  do                                                            \
    {                                                           \
      unsigned int n = 0;                                       \
      while (isdigit (character) && n <= INT_MAX / 10)          \
        {                                                       \
          n = 10 * n + character - '0';                         \
          GET_CHARACTER;                                        \
        }                                                       \
      if (((AllowNeg) ? INT_MIN: INT_MAX) < n                   \
          || isdigit (character))                               \
        m4_error (context, EXIT_FAILURE, 0, NULL,               \
                  _("integer overflow in frozen file"));        \
      (Number) = n;                                             \
    }                                                           \
  while (0)

#define GET_STRING(File, Buf, BufSize, StrLen, UseChar)         \
  do                                                            \
    {                                                           \
      size_t len = (StrLen);                                    \
      char *p;                                                  \
      int ch;                                                   \
      if (UseChar)                                              \
        {                                                       \
          ungetc (character, File);                             \
          if (advance_line)                                     \
            {                                                   \
              assert (character == '\n');                       \
              advance_line = false;                             \
            }                                                   \
        }                                                       \
      CHECK_ALLOCATION ((Buf), (BufSize), len);                 \
      p = (Buf);                                                \
      while (len-- > 0)                                         \
        {                                                       \
          ch = (version > 1                                     \
                ? decode_char (context, File, &advance_line)    \
                : getc (File));                                 \
          if (ch == EOF)                                        \
            m4_error (context, EXIT_FAILURE, 0, NULL,           \
                      _("premature end of frozen file"));       \
          *p++ = ch;                                            \
        }                                                       \
      *p = '\0';                                                \
      GET_CHARACTER;                                            \
      while (version > 1 && character == '\\')                  \
        {                                                       \
          GET_CHARACTER;                                        \
          VALIDATE ('\n');                                      \
          GET_CHARACTER;                                        \
        }                                                       \
    }                                                           \
  while (0)

#define VALIDATE(Expected)                                      \
  do                                                            \
    {                                                           \
      if (character != (Expected))                              \
        issue_expect_message (context, (Expected));             \
    }                                                           \
  while (0)

#define CHECK_ALLOCATION(Where, Allocated, Needed)              \
  do                                                            \
    {                                                           \
      if ((Needed) + 1 > (Allocated))                           \
        {                                                       \
          free (Where);                                         \
          (Allocated) = (Needed) + 1;                           \
          (Where) = xcharalloc (Allocated);                     \
        }                                                       \
    }                                                           \
  while (0)

  /* Skip comments (`#' at beginning of line) and blank lines, setting
     character to the next directive or to EOF.  */

#define GET_DIRECTIVE                                           \
  do                                                            \
    {                                                           \
      GET_CHARACTER;                                            \
      if (character == '#')                                     \
        {                                                       \
          while (character != EOF && character != '\n')         \
            GET_CHARACTER;                                      \
          VALIDATE ('\n');                                      \
        }                                                       \
    }                                                           \
  while (character == '\n')

  filepath = m4_path_search (context, name, NULL);
  file = m4_fopen (context, filepath, "r");
  if (file == NULL)
    m4_error (context, EXIT_FAILURE, errno, NULL, _("cannot open %s"),
              quotearg_style (locale_quoting_style, name));
  m4_set_current_file (context, name);

  allocated[0] = 100;
  string[0] = xcharalloc (allocated[0]);
  allocated[1] = 100;
  string[1] = xcharalloc (allocated[1]);
  allocated[2] = 100;
  string[2] = xcharalloc (allocated[2]);

  /* Validate format version.  Accept both `1' (m4 1.3 and 1.4.x) and
     `2' (m4 2.0).  */
  GET_DIRECTIVE;
  VALIDATE ('V');
  GET_CHARACTER;
  GET_NUMBER (version, false);
  switch (version)
    {
    case 2:
      break;
    case 1:
      m4__module_open (context, "m4", NULL);
      if (m4_get_posixly_correct_opt (context))
        m4__module_open (context, "traditional", NULL);
      else
        m4__module_open (context, "gnu", NULL);
      /* Disable { and } categories, since ${11} was not supported in
         1.4.x.  */
      m4_set_syntax (M4SYNTAX, 'O', '+', "{}", 2);
      break;
    default:
      if (version > 2)
        m4_error (context, EXIT_MISMATCH, 0, NULL,
                  _("frozen file version %d greater than max supported of 2"),
                  version);
      else
        m4_error (context, EXIT_FAILURE, 0, NULL,
                  _("ill-formed frozen file, version directive expected"));
    }
  VALIDATE ('\n');

  GET_DIRECTIVE;
  while (character != EOF)
    {
      switch (character)
        {
        default:
          m4_error (context, EXIT_FAILURE, 0, NULL,
                    _("ill-formed frozen file, unknown directive %c"),
                    character);

        case 'd':
          /* Set debugmode flags.  */
          if (version < 2)
            {
              /* 'd' operator is not supported in format version 1. */
              m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'd');
            }

          GET_CHARACTER;
          GET_NUMBER (number[0], false);
          VALIDATE ('\n');
          GET_STRING (file, string[0], allocated[0], number[0], false);
          VALIDATE ('\n');

          if (m4_debug_decode (context, string[0], number[0]) < 0)
            m4_error (context, EXIT_FAILURE, 0, NULL,
                      _("unknown debug mode %s"),
                      quotearg_style_mem (locale_quoting_style, string[0],
                                          number[0]));
          break;

        case 'F':
          GET_CHARACTER;

          /* Get string lengths. */

          GET_NUMBER (number[0], false);
          VALIDATE (',');
          GET_CHARACTER;
          GET_NUMBER (number[1], false);

          if (character == ',')
            {
              if (version > 1)
                {
                  /* 'F' operator accepts an optional third argument for
                     format versions 2 or later.  */
                  GET_CHARACTER;
                  GET_NUMBER (number[2], false);
                }
              else
                /* 3 argument 'F' operations are invalid for format
                   version 1.  */
                m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'F');
            }
          else
            {
              number[2] = 0;
            }

          VALIDATE ('\n');


          /* Get string contents.  */

          GET_STRING (file, string[0], allocated[0], number[0], false);
          if (version > 1)
            {
              VALIDATE ('\n');
              GET_CHARACTER;
            }
          GET_STRING (file, string[1], allocated[1], number[1], true);
          if (version > 1 && number[2])
            {
              VALIDATE ('\n');
              GET_CHARACTER;
            }
          GET_STRING (file, string[2], allocated[2], number[2], true);
          VALIDATE ('\n');

          /* Enter a macro having a builtin function as a definition.  */
          {
            m4_module *module = NULL;
            m4_symbol_value *token;

            // Builtins cannot contain a NUL byte.
            if (strlen (string[1]) < number[1])
              m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, invalid builtin %s encountered"),
                        quotearg_style_mem (locale_quoting_style, string[1],
                                            number[1]));
            if (number[2] > 0)
              {
                if (strlen (string[2]) < number[2])
                  m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, invalid module %s encountered"),
                            quotearg_style_mem (locale_quoting_style,
                                                string[2], number[2]));
                module = m4__module_find (context, string[2]);
              }
            token = m4_builtin_find_by_name (context, module, string[1]);

            if (token == NULL)
              {
                token = (m4_symbol_value *) xzalloc (sizeof *token);
                m4_set_symbol_value_placeholder (token, xstrdup (string[1]));
                VALUE_MODULE (token) = module;
                VALUE_MIN_ARGS (token) = 0;
                VALUE_MAX_ARGS (token) = -1;
              }
            m4_symbol_pushdef (M4SYMTAB, string[0], number[0], token);
          }
          break;

        case 'M':

          /* Load a module, but *without* perturbing the symbol table.
             Note that any expansion from loading the module which would
             have been seen when loading it originally is discarded
             when loading it from a frozen file. */

          if (version < 2)
            {
              /* 'M' operator is not supported in format version 1. */
              m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'M');
            }

          GET_CHARACTER;
          GET_NUMBER (number[0], false);
          VALIDATE ('\n');
          GET_STRING (file, string[0], allocated[0], number[0], false);
          VALIDATE ('\n');

          if (strlen (string[0]) < number[0])
            m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, invalid module %s encountered"),
                      quotearg_style_mem (locale_quoting_style,
                                          string[0], number[0]));
          m4__module_open (context, string[0], NULL);

          break;

        case 'R':

          if (version < 2)
            {
              /* 'R' operator is not supported in format version 1. */
              m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'R');
            }

          GET_CHARACTER;
          GET_NUMBER (number[0], false);
          VALIDATE ('\n');
          GET_STRING (file, string[0], allocated[0], number[0], false);
          VALIDATE ('\n');

          m4_set_regexp_syntax_opt (context,
                                    m4_regexp_syntax_encode (string[0]));
          if (m4_get_regexp_syntax_opt (context) < 0
              || strlen (string[0]) < number[0])
            {
              m4_error (context, EXIT_FAILURE, 0, NULL,
                        _("bad syntax-spec %s"),
                        quotearg_style_mem (locale_quoting_style, string[0],
                                            number[0]));
            }

          break;

        case 'S':

          if (version < 2)
            {
              /* 'S' operator is not supported in format version 1. */
              m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'S');
            }

          GET_CHARACTER;
          syntax = character;
          GET_CHARACTER;
          GET_NUMBER (number[0], false);
          VALIDATE ('\n');
          GET_STRING (file, string[0], allocated[0], number[0], false);

          /* Syntax under M4_SYNTAX_MASKS is handled specially; all
             other characters are additive.  */
          if ((m4_set_syntax (M4SYNTAX, syntax,
                              (m4_syntax_code (syntax) & M4_SYNTAX_MASKS
                               ? '=' : '+'), string[0], number[0]) < 0)
              && (syntax != '\0'))
            {
              m4_error (context, 0, 0, NULL,
                        _("undefined syntax code %c"), syntax);
            }
          break;

        case 't':
          /* Trace a macro name.  */
          if (version < 2)
            {
              /* 't' operator is not supported in format version 1. */
              m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 't');
            }

          GET_CHARACTER;
          GET_NUMBER (number[0], false);
          VALIDATE ('\n');
          GET_STRING (file, string[0], allocated[0], number[0], false);
          VALIDATE ('\n');

          m4_set_symbol_name_traced (M4SYMTAB, string[0], number[0], true);

          break;

        case 'C':
        case 'D':
        case 'Q':
          operation = character;
          GET_CHARACTER;

          /* Get string lengths. */

          if (operation == 'D' && character == '-')
            {
              /* Accept a negative diversion number.  */
              GET_CHARACTER;
              GET_NUMBER (number[0], true);
              number[0] = -number[0];
            }
          else
            GET_NUMBER (number[0], false);
          VALIDATE (',');
          GET_CHARACTER;
          GET_NUMBER (number[1], false);
          VALIDATE ('\n');

          /* Get string contents.  */
          if (operation != 'D')
            {
              GET_STRING (file, string[0], allocated[0], number[0], false);
              if (version > 1)
                {
                  VALIDATE ('\n');
                  GET_CHARACTER;
                }
            }
          else
            GET_CHARACTER;
          GET_STRING (file, string[1], allocated[1], number[1], true);
          VALIDATE ('\n');

          /* Act according to operation letter.  */

          switch (operation)
            {
            case 'C':

              /* Change comment strings.  */

              m4_set_comment (M4SYNTAX, string[0], number[0], string[1],
                              number[1]);
              break;

            case 'D':

              /* Select a diversion and add a string to it.  */

              m4_make_diversion (context, number[0]);
              if (number[1] > 0)
                m4_output_text (context, string[1], number[1]);
              break;

            case 'Q':

              /* Change quote strings.  */

              m4_set_quotes (M4SYNTAX, string[0], number[0], string[1],
                             number[1]);
              break;

            default:

              /* Cannot happen.  */

              break;
            }
          break;

        case 'T':
          GET_CHARACTER;

          /* Get string lengths. */

          GET_NUMBER (number[0], false);
          VALIDATE (',');
          GET_CHARACTER;
          GET_NUMBER (number[1], false);

          if (character == ',')
            {
              if (version > 1)
                {
                  /* 'T' operator accepts an optional third argument for
                     format versions 2 or later.  */
                  GET_CHARACTER;
                  GET_NUMBER (number[2], false);
                }
              else
                {
                  /* 3 argument 'T' operations are invalid for format
                     version 1.  */
                  m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, version 2 directive `%c' encountered"), 'T');
                }
            }
          else
            number[2] = 0;

          VALIDATE ('\n');

          /* Get string contents.  */
          GET_STRING (file, string[0], allocated[0], number[0], false);
          if (version > 1)
            {
              VALIDATE ('\n');
              GET_CHARACTER;
            }
          GET_STRING (file, string[1], allocated[1], number[1], true);
          if (version > 1 && number[2])
            {
              VALIDATE ('\n');
              GET_CHARACTER;
            }
          GET_STRING (file, string[2], allocated[2], number[2], true);
          VALIDATE ('\n');

          /* Enter a macro having an expansion text as a definition.  */
          {
            m4_symbol_value *token;
            m4_module *module = NULL;

            token = (m4_symbol_value *) xzalloc (sizeof *token);
            if (number[2] > 0)
              {
                if (strlen (string[2]) < number[2])
                  m4_error (context, EXIT_FAILURE, 0, NULL, _("\
ill-formed frozen file, invalid module %s encountered"),
                            quotearg_style_mem (locale_quoting_style,
                                                string[2], number[2]));
                module = m4__module_find (context, string[2]);
              }

            m4_set_symbol_value_text (token, xmemdup0 (string[1], number[1]),
                                      number[1], 0);
            VALUE_MODULE (token) = module;
            VALUE_MAX_ARGS (token) = -1;

            m4_symbol_pushdef (M4SYMTAB, string[0], number[0], token);
          }
          break;

        }
      GET_DIRECTIVE;
    }

  free (string[0]);
  free (string[1]);
  free (string[2]);
  if (close_stream (file) != 0)
    m4_error (context, EXIT_FAILURE, errno, NULL,
              _("unable to read frozen state"));
  m4_set_current_file (context, NULL);
  m4_set_current_line (context, 0);

#undef GET_STRING
#undef GET_CHARACTER
#undef GET_NUMBER
#undef VALIDATE
#undef CHECK_ALLOCATION
#undef GET_DIRECTIVE
}

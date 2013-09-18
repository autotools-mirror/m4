/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 2006-2010, 2013 Free Software Foundation,
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

/* Handling of different input sources, and lexical analysis.  */

#include <config.h>

#include "m4private.h"

#include "freadptr.h"
#include "freadseek.h"
#include "memchr2.h"

/* Define this to see runtime debug info.  Implied by DEBUG.  */
/*#define DEBUG_INPUT */

/* Maximum number of bytes where it is more efficient to inline the
   reference as a string than it is to track reference bookkeeping for
   those bytes.  */
#define INPUT_INLINE_THRESHOLD 16

/*
   Unread input can be either files that should be read (from the
   command line or by include/sinclude), strings which should be
   rescanned (normal macro expansion text), or quoted builtin
   definitions (as returned by the builtin "defn").  Unread input is
   organized in a stack, implemented with an obstack.  Each input
   source is described by a "struct m4_input_block".  The obstack is
   "input_stack".  The top of the input stack is "isp".

   Each input_block has an associated struct input_funcs, which is a
   vtable that defines polymorphic functions for peeking, reading,
   unget, cleanup, and printing in trace output.  Getting a single
   character at a time is inefficient, so there are also functions for
   accessing the readahead buffer and consuming bulk input.  All input
   is done through the function pointers of the input_funcs on the
   given input_block, and all characters are unsigned, to distinguish
   between stdio EOF and between special sentinel characters.  When a
   input_block is exhausted, its reader returns CHAR_RETRY which
   causes the input_block to be popped from the input_stack.

   The macro "m4wrap" places the text to be saved on another input
   stack, on the obstack "wrapup_stack", whose top is "wsp".  When EOF
   is seen on normal input (eg, when "current_input" is empty), input
   is switched over to "wrapup_stack", and the original
   "current_input" is freed.  A new stack is allocated for
   "wrapup_stack", which will accept any text produced by calls to
   "m4wrap" from within the wrapped text.  This process of shuffling
   "wrapup_stack" to "current_input" can continue indefinitely, even
   generating infinite loops (e.g. "define(`f',`m4wrap(`f')')f"),
   without memory leaks.  Adding wrapped data is done through
   m4__push_wrapup_init/m4__push_wrapup_finish().

   Pushing new input on the input stack is done by m4_push_file(), the
   conceptual m4_push_string(), and m4_push_builtin() (for builtin
   definitions).  As an optimization, since most macro expansions
   result in strings, m4_push_string() is split in two parts,
   push_string_init(), which returns a pointer to the obstack for
   growing the output, and push_string_finish(), which returns a
   pointer to the finished input_block.  Thus, instead of creating a
   new input block for every character pushed, macro expansion need
   only add text to the top of the obstack.  However, it is not safe
   to alter the input stack while a string is being constructed.  This
   means the input engine is one of two states: consuming input, or
   collecting a macro's expansion.  The input_block *next is used to
   manage the coordination between the different push routines.

   Normally, input sources behave in LIFO order, resembling a stack.
   But thanks to the defn and m4wrap macros, when collecting the
   expansion of a macro, it is possible that we must intermix multiple
   input blocks in FIFO order.  Therefore, when collecting an
   expansion, a meta-input block is formed which will visit its
   children in FIFO order, without losing data when the obstack is
   cleared in LIFO order.

   The current file and line number are stored in the context, for use
   by the error handling functions in utility.c.  When collecting a
   macro's expansion, these variables can be temporarily inconsistent
   in order to provide better error message locations, but they must
   be restored before further parsing takes place.  Each input block
   maintains its own notion of the current file and line, so swapping
   between input blocks must update the context accordingly.  */

typedef struct m4_input_block m4_input_block;

static  int             file_peek       (m4_input_block *, m4 *, bool);
static  int             file_read       (m4_input_block *, m4 *, bool, bool,
                                         bool);
static  void            file_unget      (m4_input_block *, int);
static  bool            file_clean      (m4_input_block *, m4 *, bool);
static  void            file_print      (m4_input_block *, m4 *, m4_obstack *,
                                         int);
static  const char *    file_buffer     (m4_input_block *, m4 *, size_t *,
                                         bool);
static  void            file_consume    (m4_input_block *, m4 *, size_t);
static  int             string_peek     (m4_input_block *, m4 *, bool);
static  int             string_read     (m4_input_block *, m4 *, bool, bool,
                                         bool);
static  void            string_unget    (m4_input_block *, int);
static  void            string_print    (m4_input_block *, m4 *, m4_obstack *,
                                         int);
static  const char *    string_buffer   (m4_input_block *, m4 *, size_t *,
                                         bool);
static  void            string_consume  (m4_input_block *, m4 *, size_t);
static  int             composite_peek  (m4_input_block *, m4 *, bool);
static  int             composite_read  (m4_input_block *, m4 *, bool, bool,
                                         bool);
static  void            composite_unget (m4_input_block *, int);
static  bool            composite_clean (m4_input_block *, m4 *, bool);
static  void            composite_print (m4_input_block *, m4 *, m4_obstack *,
                                         int);
static  const char *    composite_buffer (m4_input_block *, m4 *, size_t *,
                                          bool);
static  void            composite_consume (m4_input_block *, m4 *, size_t);
static  int             eof_peek        (m4_input_block *, m4 *, bool);
static  int             eof_read        (m4_input_block *, m4 *, bool, bool,
                                         bool);
static  void            eof_unget       (m4_input_block *, int);
static  const char *    eof_buffer      (m4_input_block *, m4 *, size_t *,
                                         bool);

static  void    init_builtin_token      (m4 *, m4_obstack *,
                                         m4_symbol_value *);
static  void    append_quote_token      (m4 *, m4_obstack *,
                                         m4_symbol_value *);
static  bool    match_input             (m4 *, const char *, size_t, bool);
static  int     next_char               (m4 *, bool, bool, bool);
static  int     peek_char               (m4 *, bool);
static  bool    pop_input               (m4 *, bool);
static  void    unget_input             (int);
static  const char * next_buffer        (m4 *, size_t *, bool);
static  void    consume_buffer          (m4 *, size_t);
static  bool    consume_syntax          (m4 *, m4_obstack *, unsigned int);

#ifdef DEBUG_INPUT
# include "quotearg.h"

static int m4_print_token (m4 *, const char *, m4__token_type,
                           m4_symbol_value *);
#endif

/* Vtable of callbacks for each input method.  */
struct input_funcs
{
  /* Peek at input, return an unsigned char, CHAR_BUILTIN if it is a
     builtin, or CHAR_RETRY if none available.  If ALLOW_ARGV, then
     CHAR_ARGV may be returned.  */
   int  (*peek_func)    (m4_input_block *, m4 *, bool);

  /* Read input, return an unsigned char, CHAR_BUILTIN if it is a
     builtin, or CHAR_RETRY if none available.  If ALLOW_QUOTE, then
     CHAR_QUOTE may be returned.  If ALLOW_ARGV, then CHAR_ARGV may be
     returned.  If ALLOW_UNGET, then ensure that the next unget_func
     will work with the returned character.  */
  int   (*read_func)    (m4_input_block *, m4 *, bool allow_quote,
                         bool allow_argv, bool allow_unget);

  /* Unread a single unsigned character or CHAR_BUILTIN, must be the
     same character previously read by read_func.  */
  void  (*unget_func)   (m4_input_block *, int);

  /* Optional function to perform cleanup at end of input.  If
     CLEANUP, it is safe to perform non-recoverable cleanup actions.
     Return true only if no cleanup remains to be done.  */
  bool  (*clean_func)   (m4_input_block *, m4 *, bool cleanup);

  /* Add a representation of the input block to the obstack, for use
     in trace expansion output.  */
  void  (*print_func)   (m4_input_block *, m4 *, m4_obstack *, int);

  /* Return a pointer to the current readahead buffer, and set LEN to
     the length of the result.  If ALLOW_QUOTE, do not return a buffer
     for a quoted string.  If there is data, but the result of
     next_char() would not fit in a char (for example, CHAR_EOF or
     CHAR_QUOTE) or there is no readahead data available, return NULL,
     and the caller must use next_char().  If there is no more data,
     return buffer_retry.  The buffer is only valid until the next
     consume_buffer() or next_char().  */
  const char *(*buffer_func) (m4_input_block *, m4 *, size_t *, bool);

  /* Optional function to consume data from a readahead buffer
     previously obtained through buffer_func.  */
  void (*consume_func) (m4_input_block *, m4 *, size_t);
};

/* A block of input to be scanned.  */
struct m4_input_block
{
  m4_input_block *prev;         /* Previous input_block on the input stack.  */
  struct input_funcs *funcs;    /* Virtual functions of this input_block.  */
  const char *file;             /* File where this input is from.  */
  int line;                     /* Line where this input is from.  */

  union
    {
      struct
        {
          char *str;            /* String value.  */
          size_t len;           /* Remaining length.  */
        }
      u_s;      /* See string_funcs.  */
      struct
        {
          FILE *fp;                     /* Input file handle.  */
          bool_bitfield end : 1;        /* True iff peek returned EOF.  */
          bool_bitfield close : 1;      /* True to close file on pop.  */
          bool_bitfield line_start : 1; /* Saved start_of_input_line state.  */
        }
      u_f;      /* See file_funcs.  */
      struct
        {
          m4__symbol_chain *chain;      /* Current link in chain.  */
          m4__symbol_chain *end;        /* Last link in chain.  */
        }
      u_c;      /* See composite_funcs.  */
    }
  u;
};


/* Obstack for storing individual tokens.  */
static m4_obstack token_stack;

/* Obstack for storing input file names.  */
static m4_obstack file_names;

/* Wrapup input stack.  */
static m4_obstack *wrapup_stack;

/* Current stack, from input or wrapup.  */
static m4_obstack *current_input;

/* Bottom of token_stack, for obstack_free.  */
static void *token_bottom;

/* Pointer to top of current_input, never NULL.  */
static m4_input_block *isp;

/* Pointer to top of wrapup_stack, never NULL.  */
static m4_input_block *wsp;

/* Auxiliary for handling split m4_push_string (), NULL when not
   pushing text for rescanning.  */
static m4_input_block *next;

/* Flag for next_char () to increment current_line.  */
static bool start_of_input_line;

/* Flag for next_char () to recognize change in input block.  */
static bool input_change;

/* Vtable for handling input from files.  */
static struct input_funcs file_funcs = {
  file_peek, file_read, file_unget, file_clean, file_print, file_buffer,
  file_consume
};

/* Vtable for handling input from strings.  */
static struct input_funcs string_funcs = {
  string_peek, string_read, string_unget, NULL, string_print, string_buffer,
  string_consume
};

/* Vtable for handling input from composite chains.  */
static struct input_funcs composite_funcs = {
  composite_peek, composite_read, composite_unget, composite_clean,
  composite_print, composite_buffer, composite_consume
};

/* Vtable for recognizing end of input.  */
static struct input_funcs eof_funcs = {
  eof_peek, eof_read, eof_unget, NULL, NULL, eof_buffer, NULL
};

/* Marker at end of an input stack.  */
static m4_input_block input_eof = { NULL, &eof_funcs, "", 0 };

/* Marker for buffer_func when current block has no more data.  */
static const char buffer_retry[1];


/* Input files, from command line or [s]include.  */
static int
file_peek (m4_input_block *me, m4 *context M4_GNUC_UNUSED,
           bool allow_argv M4_GNUC_UNUSED)
{
  int ch;

  ch = me->u.u_f.end ? EOF : getc (me->u.u_f.fp);
  if (ch == EOF)
    {
      me->u.u_f.end = true;
      return CHAR_RETRY;
    }

  ungetc (ch, me->u.u_f.fp);
  return ch;
}

static int
file_read (m4_input_block *me, m4 *context, bool allow_quote M4_GNUC_UNUSED,
           bool allow_argv M4_GNUC_UNUSED, bool allow_unget M4_GNUC_UNUSED)
{
  int ch;

  if (start_of_input_line)
    {
      start_of_input_line = false;
      m4_set_current_line (context, ++me->line);
    }

  /* If stdin is a terminal, calling getc after peek_char already
     called it would make the user have to hit ^D twice to quit.  */
  ch = me->u.u_f.end ? EOF : getc (me->u.u_f.fp);
  if (ch == EOF)
    {
      me->u.u_f.end = true;
      return CHAR_RETRY;
    }

  if (ch == '\n')
    start_of_input_line = true;
  return ch;
}

static void
file_unget (m4_input_block *me, int ch)
{
  assert (ch < CHAR_EOF);
  if (ungetc (ch, me->u.u_f.fp) < 0)
    {
      assert (!"INTERNAL ERROR: failed ungetc!");
      abort (); /* ungetc should not be called without a previous read.  */
    }
  me->u.u_f.end = false;
  if (ch == '\n')
    start_of_input_line = false;
}

static bool
file_clean (m4_input_block *me, m4 *context, bool cleanup)
{
  if (!cleanup)
    return false;
  if (me->prev != &input_eof)
    m4_debug_message (context, M4_DEBUG_TRACE_INPUT,
                      _("input reverted to %s, line %d"),
                      me->prev->file, me->prev->line);
  else
    m4_debug_message (context, M4_DEBUG_TRACE_INPUT, _("input exhausted"));

  if (ferror (me->u.u_f.fp))
    {
      m4_error (context, 0, 0, NULL, _("error reading %s"),
                quotearg_style (locale_quoting_style, me->file));
      if (me->u.u_f.close)
        fclose (me->u.u_f.fp);
    }
  else if (me->u.u_f.close && fclose (me->u.u_f.fp) == EOF)
    m4_error (context, 0, errno, NULL, _("error reading %s"),
              quotearg_style (locale_quoting_style, me->file));
  start_of_input_line = me->u.u_f.line_start;
  m4_set_output_line (context, -1);
  return true;
}

static void
file_print (m4_input_block *me, m4 *context M4_GNUC_UNUSED, m4_obstack *obs,
            int debug_level M4_GNUC_UNUSED)
{
  const char *text = me->file;
  assert (obstack_object_size (current_input) == 0);
  obstack_grow (obs, "<file: ", strlen ("<file: "));
  obstack_grow (obs, text, strlen (text));
  obstack_1grow (obs, '>');
}

static const char *
file_buffer (m4_input_block *me, m4 *context M4_GNUC_UNUSED, size_t *len,
             bool allow_quote M4_GNUC_UNUSED)
{
  if (start_of_input_line)
    {
      start_of_input_line = false;
      m4_set_current_line (context, ++me->line);
    }
  if (me->u.u_f.end)
    return buffer_retry;
  return freadptr (isp->u.u_f.fp, len);
}

static void
file_consume (m4_input_block *me, m4 *context, size_t len)
{
  const char *buf;
  const char *p;
  size_t buf_len;
  assert (!start_of_input_line);
  buf = freadptr (me->u.u_f.fp, &buf_len);
  assert (buf && len <= buf_len);
  buf_len = 0;
  while ((p = (char *) memchr (buf + buf_len, '\n', len - buf_len)))
    {
      if (p == buf + len - 1)
        start_of_input_line = true;
      else
        m4_set_current_line (context, ++me->line);
      buf_len = p - buf + 1;
    }
  if (freadseek (isp->u.u_f.fp, len) != 0)
    assert (false);
}

/* m4_push_file () pushes an input file FP with name TITLE on the
  input stack, saving the current file name and line number.  If next
  is non-NULL, this push invalidates a call to m4_push_string_init (),
  whose storage is consequently released.  If CLOSE, then close FP at
  end of file.

  file_read () manages line numbers for error messages, so they do not
  get wrong due to lookahead.  The token consisting of a newline
  alone is taken as belonging to the line it ends, and the current
  line number is not incremented until the next character is read.  */
void
m4_push_file (m4 *context, FILE *fp, const char *title, bool close_file)
{
  m4_input_block *i;

  if (next != NULL)
    {
      obstack_free (current_input, next);
      next = NULL;
    }

  m4_debug_message (context, M4_DEBUG_TRACE_INPUT, _("input read from %s"),
                    quotearg_style (locale_quoting_style, title));

  i = (m4_input_block *) obstack_alloc (current_input, sizeof *i);
  i->funcs = &file_funcs;
  /* Save title on a separate obstack, so that wrapped text can refer
     to it even after the file is popped.  */
  i->file = obstack_copy0 (&file_names, title, strlen (title));
  i->line = 1;

  i->u.u_f.fp = fp;
  i->u.u_f.end = false;
  i->u.u_f.close = close_file;
  i->u.u_f.line_start = start_of_input_line;

  m4_set_output_line (context, -1);

  i->prev = isp;
  isp = i;
  input_change = true;
}


/* Handle string expansion text.  */
static int
string_peek (m4_input_block *me, m4 *context M4_GNUC_UNUSED,
             bool allow_argv M4_GNUC_UNUSED)
{
  return me->u.u_s.len ? to_uchar (*me->u.u_s.str) : CHAR_RETRY;
}

static int
string_read (m4_input_block *me, m4 *context M4_GNUC_UNUSED,
             bool allow_quote M4_GNUC_UNUSED, bool allow_argv M4_GNUC_UNUSED,
             bool allow_unget M4_GNUC_UNUSED)
{
  if (!me->u.u_s.len)
    return CHAR_RETRY;
  me->u.u_s.len--;
  return to_uchar (*me->u.u_s.str++);
}

static void
string_unget (m4_input_block *me, int ch)
{
  assert (ch < CHAR_EOF && to_uchar (me->u.u_s.str[-1]) == ch);
  me->u.u_s.str--;
  me->u.u_s.len++;
}

static void
string_print (m4_input_block *me, m4 *context, m4_obstack *obs,
              int debug_level)
{
  bool quote = (debug_level & M4_DEBUG_TRACE_QUOTE) != 0;
  size_t arg_length = m4_get_max_debug_arg_length_opt (context);

  assert (!me->u.u_s.len);
  m4_shipout_string_trunc (obs, (char *) obstack_base (current_input),
                           obstack_object_size (current_input),
                           quote ? m4_get_syntax_quotes (M4SYNTAX) : NULL,
                           &arg_length);
}

static const char *
string_buffer (m4_input_block *me, m4 *context M4_GNUC_UNUSED, size_t *len,
               bool allow_quote M4_GNUC_UNUSED)
{
  if (!me->u.u_s.len)
    return buffer_retry;
  *len = me->u.u_s.len;
  return me->u.u_s.str;
}

static void
string_consume (m4_input_block *me, m4 *context M4_GNUC_UNUSED, size_t len)
{
  assert (len <= me->u.u_s.len);
  me->u.u_s.len -= len;
  me->u.u_s.str += len;
}

/* First half of m4_push_string ().  The pointer next points to the
   new input_block.  FILE and LINE describe the location where the
   macro starts that is generating the expansion (even if the location
   has advanced in the meantime).  Return the obstack that will
   collect the expansion text.  */
m4_obstack *
m4_push_string_init (m4 *context, const char *file, int line)
{
  /* Free any memory occupied by completely parsed input.  */
  assert (!next);
  while (pop_input (context, false));

  /* Reserve the next location on the obstack.  */
  next = (m4_input_block *) obstack_alloc (current_input, sizeof *next);
  next->funcs = &string_funcs;
  next->file = file;
  next->line = line;
  next->u.u_s.len = 0;

  return current_input;
}

/* This function allows gathering input from multiple locations,
   rather than copying everything consecutively onto the input stack.
   Must be called between push_string_init and push_string_finish.

   Convert the current input block into a chain if it is not one
   already, and add the contents of VALUE as a new link in the chain.
   LEVEL describes the current expansion level, or SIZE_MAX if VALUE
   is composite, its contents reside entirely on the current_input
   stack, and VALUE lives in temporary storage.  If VALUE is a simple
   string, then it belongs to the current macro expansion.  If VALUE
   is composite, then each text link has a level of SIZE_MAX if it
   belongs to the current macro expansion, otherwise it is a
   back-reference where level tracks which stack it came from.  The
   resulting input block chain contains links with a level of SIZE_MAX
   if the text belongs to the input stack, otherwise the level where
   the back-reference comes from.

   Return true only if a reference was created to the contents of
   VALUE, in which case, LEVEL is less than SIZE_MAX and the lifetime
   of VALUE and its contents must last as long as the input engine can
   parse references from it.  INUSE determines whether composite
   symbols should favor creating back-references or copying text.  */
bool
m4__push_symbol (m4 *context, m4_symbol_value *value, size_t level, bool inuse)
{
  m4__symbol_chain *src_chain = NULL;
  m4__symbol_chain *chain;

  assert (next);

  /* Speed consideration - for short enough symbols, the speed and
     memory overhead of parsing another INPUT_CHAIN link outweighs the
     time to inline the symbol text.  But don't copy text if it
     already lives on the obstack.  */
  if (m4_is_symbol_value_text (value))
    {
      assert (level < SIZE_MAX);
      if (m4_get_symbol_value_len (value) <= INPUT_INLINE_THRESHOLD)
        {
          obstack_grow (current_input, m4_get_symbol_value_text (value),
                        m4_get_symbol_value_len (value));
          return false;
        }
    }
  else if (m4_is_symbol_value_func (value))
    {
      if (next->funcs == &string_funcs)
        {
          next->funcs = &composite_funcs;
          next->u.u_c.chain = next->u.u_c.end = NULL;
        }
      m4__append_builtin (current_input, value->u.builtin, &next->u.u_c.chain,
                          &next->u.u_c.end);
      return false;
    }
  else
    {
      /* For composite values, if argv is already in use, creating
         additional references for long text segments is more
         efficient in time.  But if argv is not yet in use, and we
         have a composite value, then the value must already contain a
         back-reference, and memory usage is more efficient if we can
         avoid using the current expand_macro, even if it means larger
         copies.  */
      assert (value->type == M4_SYMBOL_COMP);
      src_chain = value->u.u_c.chain;
      while (level < SIZE_MAX && src_chain && src_chain->type == M4__CHAIN_STR
             && (src_chain->u.u_s.len <= INPUT_INLINE_THRESHOLD
                 || (!inuse && src_chain->u.u_s.level == SIZE_MAX)))
        {
          obstack_grow (current_input, src_chain->u.u_s.str,
                        src_chain->u.u_s.len);
          src_chain = src_chain->next;
        }
      if (!src_chain)
        return false;
    }

  if (next->funcs == &string_funcs)
    {
      next->funcs = &composite_funcs;
      next->u.u_c.chain = next->u.u_c.end = NULL;
    }
  m4__make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);
  if (m4_is_symbol_value_text (value))
    {
      chain = (m4__symbol_chain *) obstack_alloc (current_input,
                                                  sizeof *chain);
      if (next->u.u_c.end)
        next->u.u_c.end->next = chain;
      else
        next->u.u_c.chain = chain;
      next->u.u_c.end = chain;
      chain->next = NULL;
      chain->type = M4__CHAIN_STR;
      chain->quote_age = m4_get_symbol_value_quote_age (value);
      chain->u.u_s.str = m4_get_symbol_value_text (value);
      chain->u.u_s.len = m4_get_symbol_value_len (value);
      chain->u.u_s.level = level;
      m4__adjust_refcount (context, level, true);
      inuse = true;
    }
  while (src_chain)
    {
      if (src_chain->type == M4__CHAIN_FUNC)
        {
          m4__append_builtin (current_input, src_chain->u.builtin,
                              &next->u.u_c.chain, &next->u.u_c.end);
          src_chain = src_chain->next;
          continue;
        }
      if (level == SIZE_MAX)
        {
          /* Nothing to copy, since link already lives on obstack.  */
          assert (src_chain->type != M4__CHAIN_STR
                  || src_chain->u.u_s.level == SIZE_MAX);
          chain = src_chain;
        }
      else
        {
          /* Allow inlining the final link with subsequent text.  */
          if (!src_chain->next && src_chain->type == M4__CHAIN_STR
              && (src_chain->u.u_s.len <= INPUT_INLINE_THRESHOLD
                  || (!inuse && src_chain->u.u_s.level == SIZE_MAX)))
            {
              obstack_grow (current_input, src_chain->u.u_s.str,
                            src_chain->u.u_s.len);
              break;
            }
          /* We must clone each link in the chain, since next_char
             destructively modifies the chain it is parsing.  */
          chain = (m4__symbol_chain *) obstack_copy (current_input, src_chain,
                                                     sizeof *chain);
          chain->next = NULL;
          if (chain->type == M4__CHAIN_STR && chain->u.u_s.level == SIZE_MAX)
            {
              if (chain->u.u_s.len <= INPUT_INLINE_THRESHOLD || !inuse)
                chain->u.u_s.str = (char *) obstack_copy (current_input,
                                                          chain->u.u_s.str,
                                                          chain->u.u_s.len);
              else
                {
                  chain->u.u_s.level = level;
                  inuse = true;
                }
            }
        }
      if (next->u.u_c.end)
        next->u.u_c.end->next = chain;
      else
        next->u.u_c.chain = chain;
      next->u.u_c.end = chain;
      if (chain->type == M4__CHAIN_ARGV)
        {
          assert (!chain->u.u_a.comma && !chain->u.u_a.skip_last);
          inuse |= m4__arg_adjust_refcount (context, chain->u.u_a.argv, true);
        }
      else if (chain->type == M4__CHAIN_STR && chain->u.u_s.level < SIZE_MAX)
        m4__adjust_refcount (context, chain->u.u_s.level, true);
      src_chain = src_chain->next;
    }
  return inuse;
}

/* Last half of m4_push_string ().  If next is now NULL, a call to
   m4_push_file () has pushed a different input block to the top of
   the stack.  Otherwise, all unfinished text on the obstack returned
   from push_string_init is collected into the input stack.  If the
   new object is empty, we do not push it.  */
void
m4_push_string_finish (void)
{
  size_t len = obstack_object_size (current_input);

  if (next == NULL)
    {
      assert (!len);
      return;
    }

  if (len || next->funcs == &composite_funcs)
    {
      if (next->funcs == &string_funcs)
        {
          next->u.u_s.str = (char *) obstack_finish (current_input);
          next->u.u_s.len = len;
        }
      else
        m4__make_text_link (current_input, &next->u.u_c.chain,
                            &next->u.u_c.end);
      next->prev = isp;
      isp = next;
      input_change = true;
    }
  else
    obstack_free (current_input, next);
  next = NULL;
}


/* A composite block contains multiple sub-blocks which are processed
   in FIFO order, even though the obstack allocates memory in LIFO
   order.  */
static int
composite_peek (m4_input_block *me, m4 *context, bool allow_argv)
{
  m4__symbol_chain *chain = me->u.u_c.chain;
  size_t argc;

  while (chain)
    {
      switch (chain->type)
        {
        case M4__CHAIN_STR:
          if (chain->u.u_s.len)
            return to_uchar (chain->u.u_s.str[0]);
          break;
        case M4__CHAIN_FUNC:
          if (chain->u.builtin)
            return CHAR_BUILTIN;
          break;
        case M4__CHAIN_ARGV:
          argc = m4_arg_argc (chain->u.u_a.argv);
          if (chain->u.u_a.index == argc)
            break;
          if (chain->u.u_a.comma)
            return ','; /* FIXME - support M4_SYNTAX_COMMA.  */
          /* Only return a reference in the quoting is correct and the
             reference has more than one argument left.  */
          if (allow_argv && chain->quote_age == m4__quote_age (M4SYNTAX)
              && chain->u.u_a.quotes && chain->u.u_a.index + 1 < argc)
            return CHAR_ARGV;
          /* Rather than directly parse argv here, we push another
             input block containing the next unparsed argument from
             argv.  */
          m4_push_string_init (context, me->file, me->line);
          m4__push_arg_quote (context, current_input, chain->u.u_a.argv,
                              chain->u.u_a.index,
                              m4__quote_cache (M4SYNTAX, NULL,
                                               chain->quote_age,
                                               chain->u.u_a.quotes));
          chain->u.u_a.index++;
          chain->u.u_a.comma = true;
          m4_push_string_finish ();
          return peek_char (context, allow_argv);
        case M4__CHAIN_LOC:
          break;
        default:
          assert (!"composite_peek");
          abort ();
        }
      chain = chain->next;
    }
  return CHAR_RETRY;
}

static int
composite_read (m4_input_block *me, m4 *context, bool allow_quote,
                bool allow_argv, bool allow_unget)
{
  m4__symbol_chain *chain = me->u.u_c.chain;
  size_t argc;
  while (chain)
    {
      if (allow_quote && chain->quote_age == m4__quote_age (M4SYNTAX))
        return CHAR_QUOTE;
      switch (chain->type)
        {
        case M4__CHAIN_STR:
          if (chain->u.u_s.len)
            {
              /* Partial consumption invalidates quote age.  */
              chain->quote_age = 0;
              chain->u.u_s.len--;
              return to_uchar (*chain->u.u_s.str++);
            }
          if (chain->u.u_s.level < SIZE_MAX)
            m4__adjust_refcount (context, chain->u.u_s.level, false);
          break;
        case M4__CHAIN_FUNC:
          if (chain->u.builtin)
            return CHAR_BUILTIN;
          break;
        case M4__CHAIN_ARGV:
          argc = m4_arg_argc (chain->u.u_a.argv);
          if (chain->u.u_a.index == argc)
            {
              m4__arg_adjust_refcount (context, chain->u.u_a.argv, false);
              break;
            }
          if (chain->u.u_a.comma)
            {
              chain->u.u_a.comma = false;
              return ','; /* FIXME - support M4_SYNTAX_COMMA.  */
            }
          /* Only return a reference in the quoting is correct and the
             reference has more than one argument left.  */
          if (allow_argv && chain->quote_age == m4__quote_age (M4SYNTAX)
              && chain->u.u_a.quotes && chain->u.u_a.index + 1 < argc)
            return CHAR_ARGV;
          /* Rather than directly parse argv here, we push another
             input block containing the next unparsed argument from
             argv.  */
          m4_push_string_init (context, me->file, me->line);
          m4__push_arg_quote (context, current_input, chain->u.u_a.argv,
                              chain->u.u_a.index,
                              m4__quote_cache (M4SYNTAX, NULL,
                                               chain->quote_age,
                                               chain->u.u_a.quotes));
          chain->u.u_a.index++;
          chain->u.u_a.comma = true;
          m4_push_string_finish ();
          return next_char (context, allow_quote, allow_argv, allow_unget);
        case M4__CHAIN_LOC:
          me->file = chain->u.u_l.file;
          me->line = chain->u.u_l.line;
          input_change = true;
          me->u.u_c.chain = chain->next;
          return next_char (context, allow_quote, allow_argv, allow_unget);
        default:
          assert (!"composite_read");
          abort ();
        }
      me->u.u_c.chain = chain = chain->next;
    }
  return CHAR_RETRY;
}

static void
composite_unget (m4_input_block *me, int ch)
{
  m4__symbol_chain *chain = me->u.u_c.chain;
  switch (chain->type)
    {
    case M4__CHAIN_STR:
      assert (ch < CHAR_EOF && to_uchar (chain->u.u_s.str[-1]) == ch);
      chain->u.u_s.str--;
      chain->u.u_s.len++;
      break;
    case M4__CHAIN_FUNC:
      assert (ch == CHAR_BUILTIN && chain->u.builtin);
      break;
    case M4__CHAIN_ARGV:
      /* FIXME - support M4_SYNTAX_COMMA.  */
      assert (ch == ',' && !chain->u.u_a.comma);
      chain->u.u_a.comma = true;
      break;
    default:
      assert (!"composite_unget");
      abort ();
    }
}

static bool
composite_clean (m4_input_block *me, m4 *context, bool cleanup)
{
  m4__symbol_chain *chain = me->u.u_c.chain;
  assert (!chain || !cleanup);
  while (chain)
    {
      switch (chain->type)
        {
        case M4__CHAIN_STR:
          if (chain->u.u_s.len)
            {
              assert (!cleanup);
              return false;
            }
          if (chain->u.u_s.level < SIZE_MAX)
            m4__adjust_refcount (context, chain->u.u_s.level, false);
          break;
        case M4__CHAIN_FUNC:
          if (chain->u.builtin)
            return false;
          break;
        case M4__CHAIN_ARGV:
          if (chain->u.u_a.index < m4_arg_argc (chain->u.u_a.argv))
            {
              assert (!cleanup);
              return false;
            }
          m4__arg_adjust_refcount (context, chain->u.u_a.argv, false);
          break;
        case M4__CHAIN_LOC:
          return false;
        default:
          assert (!"composite_clean");
          abort ();
        }
      me->u.u_c.chain = chain = chain->next;
    }
  return true;
}

static void
composite_print (m4_input_block *me, m4 *context, m4_obstack *obs,
                 int debug_level)
{
  bool quote = (debug_level & M4_DEBUG_TRACE_QUOTE) != 0;
  size_t maxlen = m4_get_max_debug_arg_length_opt (context);
  m4__symbol_chain *chain = me->u.u_c.chain;
  const m4_string_pair *quotes = m4_get_syntax_quotes (M4SYNTAX);
  bool module = (debug_level & M4_DEBUG_TRACE_MODULE) != 0;
  bool done = false;
  size_t len = obstack_object_size (current_input);

  if (quote)
    m4_shipout_string (context, obs, quotes->str1, quotes->len1, false);
  while (chain && !done)
    {
      switch (chain->type)
        {
        case M4__CHAIN_STR:
          if (m4_shipout_string_trunc (obs, chain->u.u_s.str,
                                       chain->u.u_s.len, NULL, &maxlen))
            done = true;
          break;
        case M4__CHAIN_FUNC:
          m4__builtin_print (obs, chain->u.builtin, false, NULL, NULL, module);
          break;
        case M4__CHAIN_ARGV:
          assert (!chain->u.u_a.comma);
          if (m4__arg_print (context, obs, chain->u.u_a.argv,
                             chain->u.u_a.index,
                             m4__quote_cache (M4SYNTAX, NULL, chain->quote_age,
                                              chain->u.u_a.quotes),
                             chain->u.u_a.flatten, NULL, NULL, &maxlen, false,
                             module))
            done = true;
          break;
        default:
          assert (!"composite_print");
          abort ();
        }
      chain = chain->next;
    }
  if (len)
    m4_shipout_string_trunc (obs, (char *) obstack_base (current_input), len,
                             NULL, &maxlen);
  if (quote)
    m4_shipout_string (context, obs, quotes->str2, quotes->len2, false);
}

static const char *
composite_buffer (m4_input_block *me, m4 *context, size_t *len,
                  bool allow_quote)
{
  m4__symbol_chain *chain = me->u.u_c.chain;
  while (chain)
    {
      if (allow_quote && chain->quote_age == m4__quote_age (M4SYNTAX))
        return NULL; /* CHAR_QUOTE doesn't fit in buffer.  */
      switch (chain->type)
        {
        case M4__CHAIN_STR:
          if (chain->u.u_s.len)
            {
              *len = chain->u.u_s.len;
              return chain->u.u_s.str;
            }
          if (chain->u.u_s.level < SIZE_MAX)
            m4__adjust_refcount (context, chain->u.u_s.level, false);
          break;
        case M4__CHAIN_FUNC:
          if (chain->u.builtin)
            return NULL; /* CHAR_BUILTIN doesn't fit in buffer.  */
          break;
        case M4__CHAIN_ARGV:
          if (chain->u.u_a.index == m4_arg_argc (chain->u.u_a.argv))
            {
              m4__arg_adjust_refcount (context, chain->u.u_a.argv, false);
              break;
            }
          return NULL; /* No buffer to provide.  */
        case M4__CHAIN_LOC:
          me->file = chain->u.u_l.file;
          me->line = chain->u.u_l.line;
          input_change = true;
          me->u.u_c.chain = chain->next;
          return next_buffer (context, len, allow_quote);
        default:
          assert (!"composite_buffer");
          abort ();
        }
      me->u.u_c.chain = chain = chain->next;
    }
  return buffer_retry;
}

static void
composite_consume (m4_input_block *me, m4 *context M4_GNUC_UNUSED, size_t len)
{
  m4__symbol_chain *chain = me->u.u_c.chain;
  assert (chain && chain->type == M4__CHAIN_STR && len <= chain->u.u_s.len);
  /* Partial consumption invalidates quote age.  */
  chain->quote_age = 0;
  chain->u.u_s.len -= len;
  chain->u.u_s.str += len;
}

/* Given an obstack OBS, capture any unfinished text as a link in the
   chain that starts at *START and ends at *END.  START may be NULL if
   *END is non-NULL.  */
void
m4__make_text_link (m4_obstack *obs, m4__symbol_chain **start,
                    m4__symbol_chain **end)
{
  m4__symbol_chain *chain;
  size_t len = obstack_object_size (obs);

  assert (end && (start || *end));
  if (len)
    {
      char *str = (char *) obstack_finish (obs);
      chain = (m4__symbol_chain *) obstack_alloc (obs, sizeof *chain);
      if (*end)
        (*end)->next = chain;
      else
        *start = chain;
      *end = chain;
      chain->next = NULL;
      chain->type = M4__CHAIN_STR;
      chain->quote_age = 0;
      chain->u.u_s.str = str;
      chain->u.u_s.len = len;
      chain->u.u_s.level = SIZE_MAX;
    }
}

/* Given an obstack OBS, capture any unfinished text as a link, then
   append the builtin FUNC as the next link in the chain that starts
   at *START and ends at *END.  START may be NULL if *END is
   non-NULL.  */
void
m4__append_builtin (m4_obstack *obs, const m4__builtin *func,
                    m4__symbol_chain **start, m4__symbol_chain **end)
{
  m4__symbol_chain *chain;

  assert (func);
  m4__make_text_link (obs, start, end);
  chain = (m4__symbol_chain *) obstack_alloc (obs, sizeof *chain);
  if (*end)
    (*end)->next = chain;
  else
    *start = chain;
  *end = chain;
  chain->next = NULL;
  chain->type = M4__CHAIN_FUNC;
  chain->quote_age = 0;
  chain->u.builtin = func;
}

/* Push TOKEN, which contains a builtin's definition, onto the obstack
   OBS, which is either input stack or the wrapup stack.  */
void
m4_push_builtin (m4 *context, m4_obstack *obs, m4_symbol_value *token)
{
  m4_input_block *i = (obs == current_input ? next : wsp);
  assert (i);
  if (i->funcs == &string_funcs)
    {
      i->funcs = &composite_funcs;
      i->u.u_c.chain = i->u.u_c.end = NULL;
    }
  else
    assert (i->funcs == &composite_funcs);
  m4__append_builtin (obs, token->u.builtin, &i->u.u_c.chain, &i->u.u_c.end);
}


/* End of input optimization.  By providing these dummy callback
   functions, we guarantee that the input stack is never NULL, and
   thus make fewer execution branches.  */
static int
eof_peek (m4_input_block *me, m4 *context M4_GNUC_UNUSED,
           bool allow_argv M4_GNUC_UNUSED)
{
  assert (me == &input_eof);
  return CHAR_EOF;
}

static int
eof_read (m4_input_block *me, m4 *context M4_GNUC_UNUSED,
          bool allow_quote M4_GNUC_UNUSED, bool allow_argv M4_GNUC_UNUSED,
          bool allow_unget M4_GNUC_UNUSED)
{
  assert (me == &input_eof);
  return CHAR_EOF;
}

static void
eof_unget (m4_input_block *me M4_GNUC_UNUSED, int ch)
{
  assert (ch == CHAR_EOF);
}

static const char *
eof_buffer (m4_input_block *me M4_GNUC_UNUSED, m4 *context M4_GNUC_UNUSED,
            size_t *len M4_GNUC_UNUSED, bool allow_unget M4_GNUC_UNUSED)
{
  return NULL;
}


/* When tracing, print a summary of the contents of the input block
   created by push_string_init/push_string_finish to OBS.  Use
   DEBUG_LEVEL to determine whether to add quotes or module
   designations.  */
void
m4_input_print (m4 *context, m4_obstack *obs, int debug_level)
{
  m4_input_block *block = next ? next : isp;
  assert (context && obs && (debug_level & M4_DEBUG_TRACE_EXPANSION));
  assert (block->funcs->print_func);
  block->funcs->print_func (block, context, obs, debug_level);
}

/* Return an obstack ready for direct expansion of wrapup text, and
   set *END to the location that should be updated if any builtin
   tokens are wrapped.  Store the location of CALLER with the wrapped
   text.  This should be followed by m4__push_wrapup_finish ().  */
m4_obstack *
m4__push_wrapup_init (m4 *context, const m4_call_info *caller,
                      m4__symbol_chain ***end)
{
  m4_input_block *i;
  m4__symbol_chain *chain;

  assert (obstack_object_size (wrapup_stack) == 0);
  if (wsp != &input_eof)
    {
      i = wsp;
      assert (i->funcs == &composite_funcs && i->u.u_c.end
              && i->u.u_c.end->type != M4__CHAIN_LOC);
    }
  else
    {
      i = (m4_input_block *) obstack_alloc (wrapup_stack, sizeof *i);
      i->prev = wsp;
      i->funcs = &composite_funcs;
      i->file = caller->file;
      i->line = caller->line;
      i->u.u_c.chain = i->u.u_c.end = NULL;
      wsp = i;
    }
  chain = (m4__symbol_chain *) obstack_alloc (wrapup_stack, sizeof *chain);
  if (i->u.u_c.end)
    i->u.u_c.end->next = chain;
  else
    i->u.u_c.chain = chain;
  i->u.u_c.end = chain;
  chain->next = NULL;
  chain->type = M4__CHAIN_LOC;
  chain->quote_age = 0;
  chain->u.u_l.file = caller->file;
  chain->u.u_l.line = caller->line;
  *end = &i->u.u_c.end;
  return wrapup_stack;
}

/* After pushing wrapup text, this completes the bookkeeping.  */
void
m4__push_wrapup_finish (void)
{
  m4__make_text_link (wrapup_stack, &wsp->u.u_c.chain, &wsp->u.u_c.end);
  assert (wsp->u.u_c.end->type != M4__CHAIN_LOC);
}


/* The function pop_input () pops one level of input sources.  If
   CLEANUP, the current_file and current_line are restored as needed.
   The return value is false if cleanup is still required, or if the
   current input source is not at the end.  */
static bool
pop_input (m4 *context, bool cleanup)
{
  m4_input_block *tmp = isp->prev;

  assert (isp);
  if (isp->funcs->clean_func
      ? !isp->funcs->clean_func (isp, context, cleanup)
      : (isp->funcs->peek_func (isp, context, true) != CHAR_RETRY))
    return false;

  obstack_free (current_input, isp);
  m4__quote_uncache (M4SYNTAX);
  next = NULL; /* might be set in m4_push_string_init () */

  isp = tmp;
  input_change = true;
  return true;
}

/* To switch input over to the wrapup stack, main calls pop_wrapup.
   Since wrapup text can install new wrapup text, pop_wrapup ()
   returns true if there is more wrapped text to parse.  */
bool
m4_pop_wrapup (m4 *context)
{
  static size_t level = 0;

  next = NULL;
  obstack_free (current_input, NULL);
  free (current_input);

  if (wsp == &input_eof)
    {
      obstack_free (wrapup_stack, NULL);
      m4_set_current_file (context, NULL);
      m4_set_current_line (context, 0);
      m4_debug_message (context, M4_DEBUG_TRACE_INPUT,
                       _("input from m4wrap exhausted"));
      current_input = NULL;
      DELETE (wrapup_stack);
      return false;
    }

  m4_debug_message (context, M4_DEBUG_TRACE_INPUT,
                    _("input from m4wrap recursion level %zu"), ++level);

  current_input = wrapup_stack;
  wrapup_stack = (m4_obstack *) xmalloc (sizeof *wrapup_stack);
  obstack_init (wrapup_stack);

  isp = wsp;
  wsp = &input_eof;
  input_change = true;

  return true;
}

/* Populate TOKEN with the builtin token at the top of the input
   stack, then consume the input.  If OBS, TOKEN will be converted to
   a composite token using storage from OBS as necessary; otherwise,
   if TOKEN is NULL, the builtin token is discarded.  */
static void
init_builtin_token (m4 *context, m4_obstack *obs, m4_symbol_value *token)
{
  m4__symbol_chain *chain;
  assert (isp->funcs == &composite_funcs);
  chain = isp->u.u_c.chain;
  assert (!chain->quote_age && chain->type == M4__CHAIN_FUNC
          && chain->u.builtin);
  if (obs)
    {
      assert (token);
      if (token->type == M4_SYMBOL_VOID)
        {
          token->type = M4_SYMBOL_COMP;
          token->u.u_c.chain = token->u.u_c.end = NULL;
          token->u.u_c.wrapper = false;
          token->u.u_c.has_func = false;
        }
      assert (token->type == M4_SYMBOL_COMP);
      m4__append_builtin (obs, chain->u.builtin, &token->u.u_c.chain,
                          &token->u.u_c.end);
    }
  else if (token)
    {
      assert (token->type == M4_SYMBOL_VOID);
      m4__set_symbol_value_builtin (token, chain->u.builtin);
    }
  chain->u.builtin = NULL;
}

/* When a QUOTE token is seen, convert VALUE to a composite (if it is
   not one already), consisting of any unfinished text on OBS, as well
   as the quoted token from the top of the input stack.  Use OBS for
   any additional allocations needed to store the token chain.  */
static void
append_quote_token (m4 *context, m4_obstack *obs, m4_symbol_value *value)
{
  m4__symbol_chain *src_chain = isp->u.u_c.chain;
  m4__symbol_chain *chain;
  assert (isp->funcs == &composite_funcs && obs && m4__quote_age (M4SYNTAX));
  isp->u.u_c.chain = src_chain->next;

  /* Speed consideration - for short enough symbols, the speed and
     memory overhead of parsing another INPUT_CHAIN link outweighs the
     time to inline the symbol text.  */
  if (src_chain->type == M4__CHAIN_STR
      && src_chain->u.u_s.len <= INPUT_INLINE_THRESHOLD)
    {
      assert (src_chain->u.u_s.level <= SIZE_MAX);
      obstack_grow (obs, src_chain->u.u_s.str, src_chain->u.u_s.len);
      m4__adjust_refcount (context, src_chain->u.u_s.level, false);
      return;
    }

  if (value->type == M4_SYMBOL_VOID)
    {
      value->type = M4_SYMBOL_COMP;
      value->u.u_c.chain = value->u.u_c.end = NULL;
      value->u.u_c.wrapper = value->u.u_c.has_func = false;
    }
  assert (value->type == M4_SYMBOL_COMP);
  m4__make_text_link (obs, &value->u.u_c.chain, &value->u.u_c.end);
  chain = (m4__symbol_chain *) obstack_copy (obs, src_chain, sizeof *chain);
  if (value->u.u_c.end)
    value->u.u_c.end->next = chain;
  else
    value->u.u_c.chain = chain;
  value->u.u_c.end = chain;
  if (chain->type == M4__CHAIN_ARGV && chain->u.u_a.has_func)
    value->u.u_c.has_func = true;
  chain->next = NULL;
}

/* When an ARGV token is seen, convert VALUE to point to it via a
   composite chain.  Use OBS for any additional allocations
   needed.  */
static void
init_argv_symbol (m4 *context, m4_obstack *obs, m4_symbol_value *value)
{
  m4__symbol_chain *src_chain;
  m4__symbol_chain *chain;
  int ch;
  const m4_string_pair *comments = m4_get_syntax_comments (M4SYNTAX);

  assert (value->type == M4_SYMBOL_VOID && isp->funcs == &composite_funcs
          && isp->u.u_c.chain->type == M4__CHAIN_ARGV
          && obs && obstack_object_size (obs) == 0);

  src_chain = isp->u.u_c.chain;
  isp->u.u_c.chain = src_chain->next;
  value->type = M4_SYMBOL_COMP;
  /* Clone the link, since the input will be discarded soon.  */
  chain = (m4__symbol_chain *) obstack_copy (obs, src_chain, sizeof *chain);
  value->u.u_c.chain = value->u.u_c.end = chain;
  value->u.u_c.wrapper = true;
  value->u.u_c.has_func = chain->u.u_a.has_func;
  chain->next = NULL;

  /* If the next character is not ',' or ')', then unlink the last
     argument from argv and schedule it for reparsing.  This way,
     expand_argument never has to deal with concatenation of argv with
     arbitrary text.  Note that the implementation of safe_quotes
     ensures peek_input won't return CHAR_ARGV if the user is perverse
     enough to mix comment delimiters with argument separators:

       define(n,`$#')define(echo,$*)changecom(`,,',`)')n(echo(a,`,b`)'',c))
       => 2 (not 3)

     Therefore, we do not have to worry about calling MATCH, and thus
     do not have to worry about pop_input being called and
     invalidating the argv reference.

     When the $@ ref is used unchanged, we completely bypass the
     decrement of the argv refcount in next_char, since the ref is
     still live via the current collect_arguments.  However, when the
     last element of the $@ ref is reparsed, we must increase the argv
     refcount here, to compensate for the fact that it will be
     decreased once the final element is parsed.  */
  assert (!comments->len1
          || (!m4_has_syntax (M4SYNTAX, *comments->str1,
                              M4_SYNTAX_COMMA | M4_SYNTAX_CLOSE)
              && *comments->str1 != *src_chain->u.u_a.quotes->str1));
  ch = peek_char (context, true);
  if (!m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_COMMA | M4_SYNTAX_CLOSE))
    {
      isp->u.u_c.chain = src_chain;
      src_chain->u.u_a.index = m4_arg_argc (chain->u.u_a.argv) - 1;
      src_chain->u.u_a.comma = true;
      chain->u.u_a.skip_last = true;
      m4__arg_adjust_refcount (context, chain->u.u_a.argv, true);
    }
}


/* Low level input is done a character at a time.  The function
   next_char () is used to read and advance the input to the next
   character.  If ALLOW_QUOTE, and the current input matches the
   current quote age, return CHAR_QUOTE and leave consumption of data
   for append_quote_token; otherwise, if ALLOW_ARGV, and the current
   input matches an argv reference with the correct quoting, return
   CHAR_ARGV and leave consumption of data for init_argv_symbol.  If
   ALLOW_UNGET, then pop input to avoid returning CHAR_RETRY, and
   ensure that unget_input can safely be called next.  */
static int
next_char (m4 *context, bool allow_quote, bool allow_argv, bool allow_unget)
{
  int ch;

  while (1)
    {
      if (input_change)
        {
          m4_set_current_file (context, isp->file);
          m4_set_current_line (context, isp->line);
          input_change = false;
        }

      assert (isp->funcs->read_func);
      while (((ch = isp->funcs->read_func (isp, context, allow_quote,
                                           allow_argv, allow_unget))
              != CHAR_RETRY)
             || allow_unget)
        {
          /* if (!IS_IGNORE (ch)) */
          return ch;
        }

      /* End of input source --- pop one level.  */
      pop_input (context, true);
    }
}

/* The function peek_char () is used to look at the next character in
   the input stream.  At any given time, it reads from the input_block
   on the top of the current input stack.  If ALLOW_ARGV, then return
   CHAR_ARGV if an entire $@ reference is available for use.  */
static int
peek_char (m4 *context, bool allow_argv)
{
  int ch;
  m4_input_block *block = isp;

  while (1)
    {
      assert (block->funcs->peek_func);
      ch = block->funcs->peek_func (block, context, allow_argv);
      if (ch != CHAR_RETRY)
        {
/*        if (IS_IGNORE (ch)) */
/*          return next_char (context, false, true, false); */
          return ch;
        }

      block = block->prev;
    }
}

/* The function unget_input () puts back a character on the input
   stack, using an existing input_block if possible.  This is not safe
   to call except immediately after next_char(context, aq, aa, true).  */
static void
unget_input (int ch)
{
  assert (isp->funcs->unget_func != NULL);
  isp->funcs->unget_func (isp, ch);
}

/* Return a pointer to the available bytes of the current input block,
   and set *LEN to the length of the result.  If ALLOW_QUOTE, do not
   return a buffer for a quoted string.  If the result does not fit in
   a char (for example, CHAR_EOF or CHAR_QUOTE), or if there is no
   readahead data available, return NULL, and the caller must fall
   back to next_char().  The buffer is only valid until the next
   consume_buffer() or next_char().  */
static const char *
next_buffer (m4 *context, size_t *len, bool allow_quote)
{
  const char *buf;
  while (1)
    {
      assert (isp);
      if (input_change)
        {
          m4_set_current_file (context, isp->file);
          m4_set_current_line (context, isp->line);
          input_change = false;
        }

      assert (isp->funcs->buffer_func);
      buf = isp->funcs->buffer_func (isp, context, len, allow_quote);
      if (buf != buffer_retry)
        return buf;
      /* End of input source --- pop one level.  */
      pop_input (context, true);
    }
}

/* Consume LEN bytes from the current input block, as though by LEN
   calls to next_char().  LEN must be less than or equal to the
   previous length returned by a successful call to next_buffer().  */
static void
consume_buffer (m4 *context, size_t len)
{
  assert (isp && !input_change);
  if (len)
    {
      assert (isp->funcs->consume_func);
      isp->funcs->consume_func (isp, context, len);
    }
}

/* skip_line () simply discards all immediately following characters,
   up to the first newline.  It is only used from m4_dnl ().  Report
   errors on behalf of CALLER.  */
void
m4_skip_line (m4 *context, const m4_call_info *caller)
{
  int ch;

  while (1)
    {
      size_t len;
      const char *buffer = next_buffer (context, &len, false);
      if (buffer)
        {
          const char *p = (char *) memchr (buffer, '\n', len);
          if (p)
            {
              consume_buffer (context, p - buffer + 1);
              ch = '\n';
              break;
            }
          consume_buffer (context, len);
        }
      else
        {
          ch = next_char (context, false, false, false);
          if (ch == CHAR_EOF || ch == '\n')
            break;
        }
    }
  if (ch == CHAR_EOF)
    m4_warn (context, 0, caller, _("end of file treated as newline"));
}


/* If the string S of length LEN matches the next characters of the
   input stream, return true.  If CONSUME, the first byte has already
   been matched.  If a match is found and CONSUME is true, the input
   is discarded; otherwise any characters read are pushed back again.
   The function is used only when multicharacter quotes or comment
   delimiters are used.

   All strings herein should be unsigned.  Otherwise sign-extension
   of individual chars might break quotes with 8-bit chars in it.

   FIXME - when matching multiquotes that cross file boundaries, we do
   not properly restore the current input file and line when we
   restore unconsumed characters.  */
static bool
match_input (m4 *context, const char *s, size_t len, bool consume)
{
  int n; /* number of characters matched */
  int ch; /* input character */
  const char *t;
  m4_obstack *st;
  bool result = false;
  size_t buf_len;

  if (consume)
    {
      s++;
      len--;
    }
  /* Try a buffer match first.  */
  assert (len);
  t = next_buffer (context, &buf_len, false);
  if (t && len <= buf_len && memcmp (s, t, len) == 0)
    {
      if (consume)
        consume_buffer (context, len);
      return true;
    }
  /* Fall back on byte matching.  */
  ch = peek_char (context, false);
  if (ch != to_uchar (*s))
    return false;

  if (len == 1)
    {
      if (consume)
        next_char (context, false, false, false);
      return true; /* short match */
    }

  next_char (context, false, false, false);
  for (n = 1, t = s++; peek_char (context, false) == to_uchar (*s++); )
    {
      next_char (context, false, false, false);
      n++;
      if (--len == 1) /* long match */
        {
          if (consume)
            return true;
          result = true;
          break;
        }
    }

  /* Failed or shouldn't consume, push back input.  */
  st = m4_push_string_init (context, m4_get_current_file (context),
                            m4_get_current_line (context));
  obstack_grow (st, t, n);
  m4_push_string_finish ();
  return result;
}

/* Check whether the current input matches a delimiter, which either
   belongs to syntax category CAT or matches the string S of length
   LEN.  The first character is handled inline for speed, and S[LEN]
   must be safe to dereference (it is faster to do character
   comparison prior to length checks).  This improves efficiency for
   the common case of single character quotes and comment delimiters,
   while being safe for disabled delimiters as well as longer
   delimiters.  If CONSUME, then CH is the result of next_char, and a
   successful match will discard the matched string.  Otherwise, CH is
   the result of peek_char, and the input stream is effectively
   unchanged.  */
#define MATCH(C, ch, cat, s, len, consume)                              \
  (m4_has_syntax (m4_get_syntax_table (C), ch, cat)                     \
   || (to_uchar ((s)[0]) == (ch)                                        \
       && ((len) >> 1 ? match_input (C, s, len, consume) : (len))))

/* While the current input character has the given SYNTAX, append it
   to OBS.  Take care not to pop input source unless the next source
   would continue the chain.  Return true if the chain ended with
   CHAR_EOF.  */
static bool
consume_syntax (m4 *context, m4_obstack *obs, unsigned int syntax)
{
  int ch;
  bool allow = m4__safe_quotes (M4SYNTAX);
  assert (syntax);
  while (1)
    {
      /* Start with a buffer search.  */
      size_t len;
      const char *buffer = next_buffer (context, &len, allow);
      if (buffer)
        {
          const char *p = buffer;
          while (len && m4_has_syntax (M4SYNTAX, *p, syntax))
            {
              len--;
              p++;
            }
          obstack_grow (obs, buffer, p - buffer);
          consume_buffer (context, p - buffer);
          if (len)
            return false;
        }
      /* Fall back to byte-wise search.  It is safe to call next_char
         without first checking peek_char, except at input source
         boundaries, which we detect by CHAR_RETRY.  */
      ch = next_char (context, allow, allow, true);
      if (ch < CHAR_EOF && m4_has_syntax (M4SYNTAX, ch, syntax))
        {
          obstack_1grow (obs, ch);
          continue;
        }
      if (ch == CHAR_RETRY || ch == CHAR_QUOTE || ch == CHAR_ARGV)
        {
          ch = peek_char (context, false);
          /* We exploit the fact that CHAR_EOF, CHAR_BUILTIN,
             CHAR_QUOTE, and CHAR_ARGV do not satisfy any syntax
             categories.  */
          if (m4_has_syntax (M4SYNTAX, ch, syntax))
            {
              assert (ch < CHAR_EOF);
              obstack_1grow (obs, ch);
              next_char (context, false, false, false);
              continue;
            }
          return ch == CHAR_EOF;
        }
      unget_input (ch);
      return false;
    }
}


/* Initialize input stacks.  */
void
m4_input_init (m4 *context)
{
  obstack_init (&file_names);
  m4_set_current_file (context, NULL);
  m4_set_current_line (context, 0);

  current_input = (m4_obstack *) xmalloc (sizeof *current_input);
  obstack_init (current_input);
  wrapup_stack = (m4_obstack *) xmalloc (sizeof *wrapup_stack);
  obstack_init (wrapup_stack);

  /* Allocate an object in the current chunk, so that obstack_free
     will always work even if the first token parsed spills to a new
     chunk.  */
  obstack_init (&token_stack);
  token_bottom = obstack_finish (&token_stack);

  isp = &input_eof;
  wsp = &input_eof;
  next = NULL;

  start_of_input_line = false;
}

/* Free memory used by the input engine.  */
void
m4_input_exit (void)
{
  assert (!current_input && isp == &input_eof);
  assert (!wrapup_stack && wsp == &input_eof);
  obstack_free (&file_names, NULL);
  obstack_free (&token_stack, NULL);
}


/* Parse and return a single token from the input stream, constructed
   into TOKEN.  See m4__token_type for the valid return types, along
   with a description of what TOKEN will contain.  If LINE is not
   NULL, set *LINE to the line number where the token starts.  If OBS,
   expand safe tokens (strings and comments) directly into OBS rather
   than in a temporary staging area.  If ALLOW_ARGV, OBS must be
   non-NULL, and an entire series of arguments can be returned if a $@
   reference is encountered.  Report errors (unterminated comments or
   strings) on behalf of CALLER, if non-NULL.

   If OBS is NULL or the token expansion is unknown, the token text is
   collected on the obstack token_stack, which never contains more
   than one token text at a time.  The storage pointed to by the
   fields in TOKEN is therefore subject to change the next time
   m4__next_token () is called.  */
m4__token_type
m4__next_token (m4 *context, m4_symbol_value *token, int *line,
                m4_obstack *obs, bool allow_argv, const m4_call_info *caller)
{
  int ch;
  int quote_level;
  m4__token_type type;
  const char *file = NULL;
  size_t len;
  /* The obstack where token data is stored.  Generally token_stack,
     for tokens where argument collection might not use the literal
     token.  But for comments and strings, we can output directly into
     the argument collection obstack OBS, if provided.  */
  m4_obstack *obs_safe = &token_stack;

  assert (next == NULL);
  memset (token, '\0', sizeof *token);
  do {
    obstack_free (&token_stack, token_bottom);

    /* Must consume an input character.  */
    ch = next_char (context, false, allow_argv && m4__quote_age (M4SYNTAX),
                    false);
    if (line)
      {
        *line = m4_get_current_line (context);
        file = m4_get_current_file (context);
      }
    if (ch == CHAR_EOF) /* EOF */
      {
#ifdef DEBUG_INPUT
        xfprintf (stderr, "next_token -> EOF\n");
#endif
        return M4_TOKEN_EOF;
      }

    if (ch == CHAR_BUILTIN) /* BUILTIN TOKEN */
      {
        init_builtin_token (context, obs, token);
#ifdef DEBUG_INPUT
        m4_print_token (context, "next_token", M4_TOKEN_MACDEF, token);
#endif
        return M4_TOKEN_MACDEF;
      }
    if (ch == CHAR_ARGV)
      {
        init_argv_symbol (context, obs, token);
#ifdef DEBUG_INPUT
        m4_print_token (context, "next_token", M4_TOKEN_ARGV, token);
#endif
        return M4_TOKEN_ARGV;
      }

    if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_ESCAPE))
      { /* ESCAPED WORD */
        obstack_1grow (&token_stack, ch);
        if ((ch = next_char (context, false, false, false)) < CHAR_EOF)
          {
            obstack_1grow (&token_stack, ch);
            if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_ALPHA))
              consume_syntax (context, &token_stack,
                              M4_SYNTAX_ALPHA | M4_SYNTAX_NUM);
            type = M4_TOKEN_WORD;
          }
        else
          type = M4_TOKEN_SIMPLE; /* escape before eof */
      }
    else if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_ALPHA))
      {
        type = (m4_is_syntax_macro_escaped (M4SYNTAX)
                ? M4_TOKEN_STRING : M4_TOKEN_WORD);
        if (type == M4_TOKEN_STRING && obs)
          obs_safe = obs;
        obstack_1grow (obs_safe, ch);
        consume_syntax (context, obs_safe, M4_SYNTAX_ALPHA | M4_SYNTAX_NUM);
      }
    else if (MATCH (context, ch, M4_SYNTAX_LQUOTE,
                    context->syntax->quote.str1,
                    context->syntax->quote.len1, true))
      { /* QUOTED STRING */
        if (obs)
          obs_safe = obs;
        quote_level = 1;
        type = M4_TOKEN_STRING;
        while (1)
          {
            /* Start with buffer search for either potential delimiter.  */
            size_t len;
            const char *buffer = next_buffer (context, &len,
                                              obs && m4__quote_age (M4SYNTAX));
            if (buffer)
              {
                const char *p = buffer;
                if (m4_is_syntax_single_quotes (M4SYNTAX))
                  do
                    {
                      p = (char *) memchr2 (p, *context->syntax->quote.str1,
                                            *context->syntax->quote.str2,
                                            buffer + len - p);
                    }
                  while (p && m4__quote_age (M4SYNTAX)
                         && (*p++ == *context->syntax->quote.str2
                             ? --quote_level : ++quote_level));
                else
                  {
                    size_t remaining = len;
                    assert (context->syntax->quote.len1 == 1
                            && context->syntax->quote.len2 == 1);
                    while (remaining && !m4_has_syntax (M4SYNTAX, *p,
                                                        (M4_SYNTAX_LQUOTE
                                                         | M4_SYNTAX_RQUOTE)))
                      {
                        p++;
                        remaining--;
                      }
                    if (!remaining)
                      p = NULL;
                  }
                if (p)
                  {
                    if (m4__quote_age (M4SYNTAX))
                      {
                        assert (!quote_level
                                && context->syntax->quote.len1 == 1
                                && context->syntax->quote.len2 == 1);
                        obstack_grow (obs_safe, buffer, p - buffer - 1);
                        consume_buffer (context, p - buffer);
                        break;
                      }
                    obstack_grow (obs_safe, buffer, p - buffer);
                    ch = to_uchar (*p);
                    consume_buffer (context, p - buffer + 1);
                  }
                else
                  {
                    obstack_grow (obs_safe, buffer, len);
                    consume_buffer (context, len);
                    continue;
                  }
              }
            /* Fall back to byte-wise search.  */
            else
              ch = next_char (context, obs && m4__quote_age (M4SYNTAX), false,
                              false);
            if (ch == CHAR_EOF)
              {
                if (!caller)
                  {
                    assert (line);
                    m4_set_current_file (context, file);
                    m4_set_current_line (context, *line);
                  }
                m4_error (context, EXIT_FAILURE, 0, caller,
                          _("end of file in string"));
              }
            if (ch == CHAR_BUILTIN)
              init_builtin_token (context, obs, obs ? token : NULL);
            else if (ch == CHAR_QUOTE)
              append_quote_token (context, obs, token);
            else if (MATCH (context, ch, M4_SYNTAX_RQUOTE,
                            context->syntax->quote.str2,
                            context->syntax->quote.len2, true))
              {
                if (--quote_level == 0)
                  break;
                if (1 < context->syntax->quote.len2)
                  obstack_grow (obs_safe, context->syntax->quote.str2,
                                context->syntax->quote.len2);
                else
                  obstack_1grow (obs_safe, ch);
              }
            else if (MATCH (context, ch, M4_SYNTAX_LQUOTE,
                            context->syntax->quote.str1,
                            context->syntax->quote.len1, true))
              {
                quote_level++;
                if (1 < context->syntax->quote.len1)
                  obstack_grow (obs_safe, context->syntax->quote.str1,
                                context->syntax->quote.len1);
                else
                  obstack_1grow (obs_safe, ch);
              }
            else
              obstack_1grow (obs_safe, ch);
          }
      }
    else if (MATCH (context, ch, M4_SYNTAX_BCOMM,
                    context->syntax->comm.str1,
                    context->syntax->comm.len1, true))
      { /* COMMENT */
        if (obs && !m4_get_discard_comments_opt (context))
          obs_safe = obs;
        if (1 < context->syntax->comm.len1)
          obstack_grow (obs_safe, context->syntax->comm.str1,
                        context->syntax->comm.len1);
        else
          obstack_1grow (obs_safe, ch);
        while (1)
          {
            /* Start with buffer search for potential end delimiter.  */
            size_t len;
            const char *buffer = next_buffer (context, &len, false);
            if (buffer)
              {
                const char *p;
                if (m4_is_syntax_single_comments (M4SYNTAX))
                  p = (char *) memchr (buffer, *context->syntax->comm.str2,
                                       len);
                else
                  {
                    size_t remaining = len;
                    assert (context->syntax->comm.len2 == 1);
                    p = buffer;
                    while (remaining
                           && !m4_has_syntax (M4SYNTAX, *p, M4_SYNTAX_ECOMM))
                      {
                        p++;
                        remaining--;
                      }
                    if (!remaining)
                      p = NULL;
                  }
                if (p)
                  {
                    obstack_grow (obs_safe, buffer, p - buffer);
                    ch = to_uchar (*p);
                    consume_buffer (context, p - buffer + 1);
                  }
                else
                  {
                    obstack_grow (obs_safe, buffer, len);
                    consume_buffer (context, len);
                    continue;
                  }
              }
            /* Fall back to byte-wise search.  */
            else
              ch = next_char (context, false, false, false);
            if (ch == CHAR_EOF)
              {
                if (!caller)
                  {
                    assert (line);
                    m4_set_current_file (context, file);
                    m4_set_current_line (context, *line);
                  }
                m4_error (context, EXIT_FAILURE, 0, caller,
                          _("end of file in comment"));
              }
            if (ch == CHAR_BUILTIN)
              {
                init_builtin_token (context, NULL, NULL);
                continue;
              }
            if (MATCH (context, ch, M4_SYNTAX_ECOMM,
                       context->syntax->comm.str2,
                       context->syntax->comm.len2, true))
              {
                if (1 < context->syntax->comm.len2)
                  obstack_grow (obs_safe, context->syntax->comm.str2,
                                context->syntax->comm.len2);
                else
                  obstack_1grow (obs_safe, ch);
                break;
              }
            assert (ch < CHAR_EOF);
            obstack_1grow (obs_safe, ch);
          }
        type = (m4_get_discard_comments_opt (context)
                ? M4_TOKEN_NONE : M4_TOKEN_COMMENT);
      }
    else if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_ACTIVE))
      { /* ACTIVE CHARACTER */
        obstack_1grow (&token_stack, ch);
        type = M4_TOKEN_WORD;
      }
    else if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_OPEN))
      { /* OPEN PARENTHESIS */
        obstack_1grow (&token_stack, ch);
        type = M4_TOKEN_OPEN;
      }
    else if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_COMMA))
      { /* COMMA */
        obstack_1grow (&token_stack, ch);
        type = M4_TOKEN_COMMA;
      }
    else if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_CLOSE))
      { /* CLOSE PARENTHESIS */
        obstack_1grow (&token_stack, ch);
        type = M4_TOKEN_CLOSE;
      }
    else
      { /* EVERYTHING ELSE */
        assert (ch < CHAR_EOF);
        obstack_1grow (&token_stack, ch);
        if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_OTHER | M4_SYNTAX_NUM))
          {
            if (obs)
              {
                obs_safe = obs;
                obstack_1grow (obs, ch);
              }
            if (m4__safe_quotes (M4SYNTAX))
              consume_syntax (context, obs_safe,
                              M4_SYNTAX_OTHER | M4_SYNTAX_NUM);
            type = M4_TOKEN_STRING;
          }
        else if (m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_SPACE))
          {
            /* Coalescing newlines when interactive or when synclines
               are enabled is wrong.  */
            if (!m4_get_interactive_opt (context)
                && !m4_get_syncoutput_opt (context)
                && m4__safe_quotes (M4SYNTAX))
              consume_syntax (context, &token_stack, M4_SYNTAX_SPACE);
            type = M4_TOKEN_SPACE;
          }
        else
          type = M4_TOKEN_SIMPLE;
      }
  } while (type == M4_TOKEN_NONE);

  if (token->type == M4_SYMBOL_VOID)
    {
      if (obs_safe != obs)
        {
          len = obstack_object_size (&token_stack);
          obstack_1grow (&token_stack, '\0');

          m4_set_symbol_value_text (token, obstack_finish (&token_stack), len,
                                    m4__quote_age (M4SYNTAX));
        }
      else
        assert (type == M4_TOKEN_STRING || type == M4_TOKEN_COMMENT);
    }
  else
    assert (token->type == M4_SYMBOL_COMP
            && (type == M4_TOKEN_STRING || type == M4_TOKEN_COMMENT));
  VALUE_MAX_ARGS (token) = -1;

#ifdef DEBUG_INPUT
  if (token->type == M4_SYMBOL_VOID)
    {
      len = obstack_object_size (&token_stack);
      obstack_1grow (&token_stack, '\0');

      m4_set_symbol_value_text (token, obstack_finish (&token_stack), len,
                                m4__quote_age (M4SYNTAX));
    }

  m4_print_token (context, "next_token", type, token);
#endif

  return type;
}

/* Peek at the next token in the input stream to see if it is an open
   parenthesis.  It is possible that what is peeked at may change as a
   result of changequote (or friends).  This honors multi-character
   comments and quotes, just as next_token does.  */
bool
m4__next_token_is_open (m4 *context)
{
  int ch = peek_char (context, false);

  if (ch == CHAR_EOF || ch == CHAR_BUILTIN
      || m4_has_syntax (M4SYNTAX, ch, (M4_SYNTAX_BCOMM | M4_SYNTAX_ESCAPE
                                       | M4_SYNTAX_ALPHA | M4_SYNTAX_LQUOTE
                                       | M4_SYNTAX_ACTIVE))
      || (MATCH (context, ch, M4_SYNTAX_BCOMM, context->syntax->comm.str1,
                 context->syntax->comm.len1, false))
      || (MATCH (context, ch, M4_SYNTAX_LQUOTE, context->syntax->quote.str1,
                 context->syntax->quote.len1, false)))
    return false;
  return m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_OPEN);
}


#ifdef DEBUG_INPUT

int
m4_print_token (m4 *context, const char *s, m4__token_type type,
                m4_symbol_value *token)
{
  m4_obstack obs;
  size_t len;

  if (!s)
    s = "m4input";
  xfprintf (stderr, "%s: ", s);
  switch (type)
    { /* TOKSW */
    case M4_TOKEN_EOF:
      fputs ("eof", stderr);
      token = NULL;
      break;
    case M4_TOKEN_NONE:
      fputs ("none", stderr);
      token = NULL;
      break;
    case M4_TOKEN_STRING:
      fputs ("string\t", stderr);
      break;
    case M4_TOKEN_COMMENT:
      fputs ("comment\t", stderr);
      break;
    case M4_TOKEN_SPACE:
      fputs ("space\t", stderr);
      break;
    case M4_TOKEN_WORD:
      fputs ("word\t", stderr);
      break;
    case M4_TOKEN_OPEN:
      fputs ("open\t", stderr);
      break;
    case M4_TOKEN_COMMA:
      fputs ("comma\t", stderr);
      break;
    case M4_TOKEN_CLOSE:
      fputs ("close\t", stderr);
      break;
    case M4_TOKEN_SIMPLE:
      fputs ("simple\t", stderr);
      break;
    case M4_TOKEN_MACDEF:
      fputs ("builtin\t", stderr);
      break;
    case M4_TOKEN_ARGV:
      fputs ("argv\t", stderr);
      break;
    default:
      abort ();
    }
  if (token)
    {
      obstack_init (&obs);
      m4__symbol_value_print (context, token, &obs, NULL, false, NULL, NULL,
                              true);
      len = obstack_object_size (&obs);
      xfprintf (stderr, "%s\n", quotearg_style_mem (c_maybe_quoting_style,
                                                    obstack_finish (&obs),
                                                    len));
      obstack_free (&obs, NULL);
    }
  else
    fputc ('\n', stderr);
  return 0;
}
#endif /* DEBUG_INPUT */

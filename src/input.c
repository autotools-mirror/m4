/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2004, 2005, 2006,
   2007, 2008, 2009 Free Software Foundation, Inc.

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

#include "m4.h"

#include "freadptr.h"
#include "freadseek.h"
#include "memchr2.h"

/* Unread input can be either files to be read (command line,
   "include", "sinclude"), strings which should be rescanned (macro
   expansion text), or quoted macro definitions (as returned by the
   builtin "defn").  Unread input is organized in a stack, implemented
   with an obstack.  Each input source is described by a "struct
   input_block".  The obstack is "current_input".  The top of the
   input stack is "isp".

   The macro "m4wrap" places the text to be saved on another input
   stack, on the obstack "wrapup_stack", whose top is "wsp".  When EOF
   is seen on normal input (eg, when "current_input" is empty), input is
   switched over to "wrapup_stack", and the original "current_input" is
   freed.  A new stack is allocated for "wrapup_stack", which will
   accept any text produced by calls to "m4wrap" from within the
   wrapped text.  This process of shuffling "wrapup_stack" to
   "current_input" can continue indefinitely, even generating infinite
   loops (e.g. "define(`f',`m4wrap(`f')')f"), without memory leaks.

   Pushing new input on the input stack is done by push_file (),
   push_string (), push_wrapup_init/push_wrapup_finish () (for wrapup
   text), and push_macro () (for macro definitions).  Because macro
   expansion needs direct access to the current input obstack (for
   optimization), push_string () is split in two functions,
   push_string_init (), which returns a pointer to the current input
   stack, and push_string_finish (), which returns a pointer to the
   final text.  The input_block *next is used to manage the
   coordination between the different push routines.

   The current file and line number are stored in two global
   variables, for use by the error handling functions in m4.c.  Macro
   expansion wants to report the line where a macro name was detected,
   rather than where it finished collecting arguments.  This also
   applies to text resulting from macro expansions.  So each input
   block maintains its own notion of the current file and line, and
   swapping between input blocks updates the global variables
   accordingly.  */

#ifdef ENABLE_CHANGEWORD
# include "regex.h"
#endif /* ENABLE_CHANGEWORD */

/* Number of bytes where it is more efficient to inline the reference
   as a string than it is to track reference bookkeeping for those
   bytes.  */
#define INPUT_INLINE_THRESHOLD 16

/* Type of an input block.  */
enum input_type
{
  INPUT_STRING,	/* String resulting from macro expansion.  */
  INPUT_FILE,	/* File from command line or include.  */
  INPUT_FILE_Q,	/* Quotation around a file, via qindir.  */
  INPUT_CHAIN,	/* FIFO chain of separate strings, builtins, and $@ refs.  */
  INPUT_EOF	/* Placeholder at bottom of input stack.  */
};

typedef enum input_type input_type;

typedef struct input_block input_block;

/* A block of input to be scanned.  */
struct input_block
{
  input_block *prev;		/* Previous input_block on the input stack.  */
  input_type type;		/* See enum values.  */
  const char *file;		/* File where this input is from.  */
  int line;			/* Line where this input is from.  */
  union
    {
      struct
	{
	  char *str;		/* Remaining string value.  */
	  size_t len;		/* Remaining length.  */
	}
	u_s;	/* INPUT_STRING */
      struct
	{
	  FILE *fp;		     /* Input file handle.  */
	  bool_bitfield end : 1;     /* True if peek has seen EOF.  */
	  bool_bitfield close : 1;   /* True to close file on pop.  */
	  bool_bitfield advance : 1; /* Track previous start_of_input_line.  */
	}
	u_f;	/* INPUT_FILE */
      struct
	{
	  string_pair *quotes;	/* Quotes to use when wrapping.  */
	  size_t count;		/* Count of quotes to wrap.  */
	}
	u_q;	/* INPUT_FILE_Q */
      struct
	{
	  token_chain *chain;	/* Current link in chain.  */
	  token_chain *end;	/* Last link in chain.  */
	}
	u_c;	/* INPUT_CHAIN */
    }
  u;
};


/* Current input file name.  */
const char *current_file;

/* Current input line number.  */
int current_line;

/* Obstack for storing individual tokens.  */
static struct obstack token_stack;

/* Obstack for storing file names.  */
static struct obstack file_names;

/* Wrapup input stack.  */
static struct obstack *wrapup_stack;

/* Current stack, from input or wrapup.  */
static struct obstack *current_input;

/* Bottom of token_stack, for obstack_free.  */
static void *token_bottom;

/* Pointer to top of current_input, never NULL.  */
static input_block *isp;

/* Pointer to top of wrapup_stack, never NULL.  */
static input_block *wsp;

/* Auxiliary for handling split push_string (), NULL if not pushing
   text for rescanning.  */
static input_block *next;

/* Marker at the end of the input stack.  */
static input_block input_eof = { NULL, INPUT_EOF, "", 0 };

/* Flag for next_char () to increment current_line.  */
static bool start_of_input_line;

/* Flag for next_char () to recognize change in input block.  */
static bool input_change;

#define CHAR_EOF	256	/* Character return on EOF.  */
#define CHAR_MACRO	257	/* Character return for MACRO token.  */
#define CHAR_QUOTE	258	/* Character return for quoted string.  */
#define CHAR_ARGV	259	/* Character return for $@ reference.  */

/* Quote chars.  */
string_pair curr_quote;

/* Comment chars.  */
string_pair curr_comm;

#ifdef ENABLE_CHANGEWORD

# define DEFAULT_WORD_REGEXP "[_a-zA-Z][_a-zA-Z0-9]*"

/* Current regular expression for detecting words.  */
static struct re_pattern_buffer word_regexp;

/* True if changeword is not active.  */
static bool default_word_regexp;

/* Reused memory for detecting matches in word detection.  */
static struct re_registers regs;

#else /* !ENABLE_CHANGEWORD */
# define default_word_regexp true
#endif /* !ENABLE_CHANGEWORD */

/* Track the current quote age, determined by all significant
   changequote, changecom, and changeword calls, since any one of
   these can alter the rescan of a prior parameter in a quoted
   context.  */
static unsigned int current_quote_age;

/* Cache a quote pair.  See quote_cache.  */
static string_pair *cached_quote;

static bool pop_input (bool);
static void set_quote_age (void);

#ifdef DEBUG_INPUT
static const char *token_type_string (token_type);
#endif /* DEBUG_INPUT */


/*-------------------------------------------------------------------.
| Given an obstack OBS, capture any unfinished text as a link in the |
| chain that starts at *START and ends at *END.  START may be NULL   |
| if *END is non-NULL.                                               |
`-------------------------------------------------------------------*/
void
make_text_link (struct obstack *obs, token_chain **start, token_chain **end)
{
  token_chain *chain;
  size_t len = obstack_object_size (obs);

  assert (end && (start || *end));
  if (len)
    {
      char *str = (char *) obstack_finish (obs);
      chain = (token_chain *) obstack_alloc (obs, sizeof *chain);
      if (*end)
	(*end)->next = chain;
      else
	*start = chain;
      *end = chain;
      chain->next = NULL;
      chain->type = CHAIN_STR;
      chain->quote_age = 0;
      chain->u.u_s.str = str;
      chain->u.u_s.len = len;
      chain->u.u_s.level = -1;
    }
}

/*-------------------------------------------------------------------.
| push_file () pushes an input file on the input stack, saving the   |
| current file name and line number.  If next is non-NULL, this push |
| invalidates a call to push_string_init (), whose storage is	     |
| consequently released.  If CLOSE_WHEN_DONE, then close FP after    |
| EOF is detected.  TITLE is used as the location for text parsed    |
| from the file (not necessarily the file name).		     |
`-------------------------------------------------------------------*/

void
push_file (FILE *fp, const char *title, bool close_when_done)
{
  input_block *i;

  if (next != NULL)
    {
      assert (obstack_object_size (current_input) == 0
	      && next->type == INPUT_STRING);
      i = next;
    }
  else
    {
      i = (input_block *) obstack_alloc (current_input, sizeof *i);
      i->prev = isp;
      isp = i;
    }

  if (debug_level & DEBUG_TRACE_INPUT)
    debug_message ("input read from %s", title);

  i->type = INPUT_FILE;
  i->file = (char *) obstack_copy0 (&file_names, title, strlen (title));
  i->line = 1;
  input_change = true;

  i->u.u_f.fp = fp;
  i->u.u_f.end = false;
  i->u.u_f.close = close_when_done;
  i->u.u_f.advance = start_of_input_line;
  output_current_line = -1;
}

/*------------------------------------------------------------------.
| Given an obstack OBS, capture any unfinished text as a link, then |
| append the builtin FUNC as the next link in the chain that starts |
| at *START and ends at *END.  START may be NULL if *END is         |
| non-NULL.                                                         |
`------------------------------------------------------------------*/
void
append_macro (struct obstack *obs, builtin_func *func, token_chain **start,
	      token_chain **end)
{
  token_chain *chain;

  assert (func);
  make_text_link (obs, start, end);
  chain = (token_chain *) obstack_alloc (obs, sizeof *chain);
  if (*end)
    (*end)->next = chain;
  else
    *start = chain;
  *end = chain;
  chain->next = NULL;
  chain->type = CHAIN_FUNC;
  chain->quote_age = 0;
  chain->u.func = func;
}

/*------------------------------------------------------------------.
| push_macro () pushes the builtin FUNC onto the obstack OBS, which |
| is either the input or wrapup stack.                              |
`------------------------------------------------------------------*/

void
push_macro (struct obstack *obs, builtin_func *func)
{
  input_block *block = (obs == current_input ? next : wsp);
  assert (block);
  if (block->type == INPUT_STRING)
    {
      block->type = INPUT_CHAIN;
      block->u.u_c.chain = block->u.u_c.end = NULL;
    }
  else
    assert (block->type == INPUT_CHAIN);
  append_macro (obs, func, &block->u.u_c.chain, &block->u.u_c.end);
}

/*--------------------------------------------------------------.
| First half of push_string ().  The return value points to the |
| obstack where expansion text should be placed.                |
`--------------------------------------------------------------*/

struct obstack *
push_string_init (const char *file, int line)
{
  /* Free any memory occupied by completely parsed strings.  */
  assert (next == NULL);
  while (pop_input (false));

  /* Reserve the next location on the obstack.  */
  next = (input_block *) obstack_alloc (current_input, sizeof *next);
  next->prev = isp;
  next->type = INPUT_STRING;
  next->file = file;
  next->line = line;
  next->u.u_s.len = 0;

  return current_input;
}

/*-------------------------------------------------------------.
| Push the text macro definition in SYM onto the input stack.  |
`-------------------------------------------------------------*/
void
push_defn (symbol *sym)
{
  size_t len = SYMBOL_TEXT_LEN (sym);

  assert (next && SYMBOL_TYPE (sym) == TOKEN_TEXT);

  /* Speed consideration - for short enough tokens, the speed and
     memory overhead of parsing another INPUT_CHAIN link outweighs the
     time to inline the token text.  */
  if (len <= INPUT_INLINE_THRESHOLD)
    {
      obstack_grow (current_input, SYMBOL_TEXT (sym), len);
      return;
    }

  if (next->type == INPUT_STRING)
    {
      next->type = INPUT_CHAIN;
      next->u.u_c.chain = next->u.u_c.end = NULL;
    }
  make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);

  /* TODO - optimize this to increment the symbol's reference counter,
     then decrement it again upon rescan, rather than copying.  */
  obstack_grow (current_input, SYMBOL_TEXT (sym), len);
  make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);
  next->u.u_c.end->quote_age = SYMBOL_TEXT_QUOTE_AGE (sym);
}

/*--------------------------------------------------------------------.
| This function allows gathering input from multiple locations,	      |
| rather than copying everything consecutively onto the input stack.  |
| Must be called between push_string_init and push_string_finish.     |
|                                                                     |
| Convert the current input block into a chain if it is not one	      |
| already, and add the contents of TOKEN as a new link in the chain.  |
| LEVEL describes the current expansion level, or -1 if TOKEN is      |
| composite, its contents reside entirely on the current_input	      |
| stack, and TOKEN lives in temporary storage.  If TOKEN is a simple  |
| string, then it belongs to the current macro expansion.  If TOKEN   |
| is composite, then each text link has a level of -1 if it belongs   |
| to the current macro expansion, otherwise it is a back-reference    |
| where level tracks which stack it came from.  The resulting input   |
| block chain contains links with a level of -1 if the text belongs   |
| to the input stack, otherwise the level where the back-reference    |
| comes from.							      |
|                                                                     |
| Return true only if a reference was created to the contents of      |
| TOKEN, in which case, LEVEL was non-negative and the lifetime of    |
| TOKEN and its contents must last as long as the input engine can    |
| parse references to it.  INUSE determines whether composite tokens  |
| should favor creating back-references or copying text.	      |
`--------------------------------------------------------------------*/
bool
push_token (token_data *token, int level, bool inuse)
{
  token_chain *src_chain = NULL;
  token_chain *chain;

  assert (next);

  /* Speed consideration - for short enough tokens, the speed and
     memory overhead of parsing another INPUT_CHAIN link outweighs the
     time to inline the token text.  But don't re-copy text if it
     already lives on the obstack.  */
  if (TOKEN_DATA_TYPE (token) == TOKEN_TEXT)
    {
      assert (level >= 0);
      if (TOKEN_DATA_LEN (token) <= INPUT_INLINE_THRESHOLD)
	{
	  obstack_grow (current_input, TOKEN_DATA_TEXT (token),
			TOKEN_DATA_LEN (token));
	  return false;
	}
    }
  else if (TOKEN_DATA_TYPE (token) == TOKEN_FUNC)
    {
      if (next->type == INPUT_STRING)
	{
	  next->type = INPUT_CHAIN;
	  next->u.u_c.chain = next->u.u_c.end = NULL;
	}
      append_macro (current_input, TOKEN_DATA_FUNC (token), &next->u.u_c.chain,
		    &next->u.u_c.end);
      return false;
    }
  else
    {
      /* For composite tokens, if argv is already in use, creating
	 additional references for long text segments is more
	 efficient in time.  But if argv is not yet in use, and we
	 have a composite token, then the token must already contain a
	 back-reference, and memory usage is more efficient if we can
	 avoid using the current expand_macro, even if it means larger
	 copies.  */
      assert (TOKEN_DATA_TYPE (token) == TOKEN_COMP);
      src_chain = token->u.u_c.chain;
      while (level >= 0 && src_chain && src_chain->type == CHAIN_STR
	     && (src_chain->u.u_s.len <= INPUT_INLINE_THRESHOLD
		 || (!inuse && src_chain->u.u_s.level == -1)))
	{
	  obstack_grow (current_input, src_chain->u.u_s.str,
			src_chain->u.u_s.len);
	  src_chain = src_chain->next;
	}
      if (!src_chain)
	return false;
    }

  if (next->type == INPUT_STRING)
    {
      next->type = INPUT_CHAIN;
      next->u.u_c.chain = next->u.u_c.end = NULL;
    }
  make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);
  if (TOKEN_DATA_TYPE (token) == TOKEN_TEXT)
    {
      chain = (token_chain *) obstack_alloc (current_input, sizeof *chain);
      if (next->u.u_c.end)
	next->u.u_c.end->next = chain;
      else
	next->u.u_c.chain = chain;
      next->u.u_c.end = chain;
      chain->next = NULL;
      chain->type = CHAIN_STR;
      chain->quote_age = TOKEN_DATA_QUOTE_AGE (token);
      chain->u.u_s.str = TOKEN_DATA_TEXT (token);
      chain->u.u_s.len = TOKEN_DATA_LEN (token);
      chain->u.u_s.level = level;
      adjust_refcount (level, true);
      inuse = true;
    }
  while (src_chain)
    {
      if (src_chain->type == CHAIN_FUNC)
	{
	  append_macro (current_input, src_chain->u.func, &next->u.u_c.chain,
			&next->u.u_c.end);
	  src_chain = src_chain->next;
	  continue;
	}
      if (level == -1)
	{
	  /* Nothing to copy, since link already lives on obstack.  */
	  assert (src_chain->type != CHAIN_STR
		  || src_chain->u.u_s.level == -1);
	  chain = src_chain;
	}
      else
	{
	  /* Allow inlining the final link with subsequent text.  */
	  if (!src_chain->next && src_chain->type == CHAIN_STR
	      && (src_chain->u.u_s.len <= INPUT_INLINE_THRESHOLD
		  || (!inuse && src_chain->u.u_s.level == -1)))
	    {
	      obstack_grow (current_input, src_chain->u.u_s.str,
			    src_chain->u.u_s.len);
	      break;
	    }
	  /* We must clone each link in the chain, since next_char
	     destructively modifies the chain it is parsing.  */
	  chain = (token_chain *) obstack_copy (current_input, src_chain,
						sizeof *chain);
	  chain->next = NULL;
	  if (chain->type == CHAIN_STR && chain->u.u_s.level == -1)
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
      if (chain->type == CHAIN_ARGV)
	{
	  assert (!chain->u.u_a.comma && !chain->u.u_a.skip_last);
	  inuse |= arg_adjust_refcount (chain->u.u_a.argv, true);
	}
      else if (chain->type == CHAIN_STR && chain->u.u_s.level >= 0)
	adjust_refcount (chain->u.u_s.level, true);
      src_chain = src_chain->next;
    }
  return inuse;
}


/*----------------------------------------------------------------.
| Wrap the current pending expansion in another level of quotes.  |
`----------------------------------------------------------------*/
void
push_quote_wrapper (void)
{
  token_chain *chain;
  size_t len = obstack_object_size (current_input);

  assert (next);
  if (next->type == INPUT_FILE)
    {
      input_block *block;
      string_pair *quotes;
      assert (!len);
      block = (input_block *) obstack_alloc (current_input, sizeof *block);
      quotes = (string_pair *) obstack_alloc (current_input, sizeof *quotes);
      quotes->str1 = (char *) obstack_copy (current_input, curr_quote.str1,
					    curr_quote.len1);
      quotes->len1 = curr_quote.len1;
      obstack_grow (current_input, curr_quote.str2, curr_quote.len2);
      block->type = INPUT_FILE_Q;
      block->file = current_file;
      block->line = current_line;
      block->prev = next;
      block->u.u_q.quotes = quotes;
      block->u.u_q.count = 1;
      next = block;
    }
  else if (next->type == INPUT_FILE_Q)
    {
      assert (len == curr_quote.len2 * next->u.u_q.count);
      next->u.u_q.count++;
      obstack_grow (current_input, curr_quote.str2, curr_quote.len2);
    }
  else if (!len && next->type == INPUT_STRING)
    {
      obstack_grow (current_input, curr_quote.str1, curr_quote.len1);
      obstack_grow (current_input, curr_quote.str2, curr_quote.len2);
    }
  else
    {
      obstack_grow (current_input, curr_quote.str2, curr_quote.len2);
      if (next->type == INPUT_STRING)
	{
	  next->type = INPUT_CHAIN;
	  next->u.u_c.chain = next->u.u_c.end = NULL;
	}
      assert (next->type == INPUT_CHAIN);
      make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);
      assert (obstack_object_size (current_input) == 0 && next->u.u_c.chain);
      chain = (token_chain *) obstack_alloc (current_input, sizeof *chain);
      chain->next = next->u.u_c.chain;
      next->u.u_c.chain = chain;
      chain->type = CHAIN_STR;
      chain->quote_age = 0;
      chain->u.u_s.str = (char *) obstack_copy (current_input, curr_quote.str1,
						curr_quote.len1);
      chain->u.u_s.len = curr_quote.len1;
      chain->u.u_s.level = -1;
    }
}

/*-------------------------------------------------------------------.
| Last half of push_string ().  All remaining unfinished text on the |
| obstack returned from push_string_init is collected into the input |
| stack.                                                             |
`-------------------------------------------------------------------*/

void
push_string_finish (void)
{
  size_t len = obstack_object_size (current_input);

  assert (next);
  if (next->type == INPUT_FILE)
    {
      assert (!len);
      isp = next;
    }
  else if (next->type == INPUT_FILE_Q)
    {
      assert (len == next->u.u_q.count * curr_quote.len2);
      next->u.u_q.quotes->str2 = (char *) obstack_finish (current_input);
      next->u.u_q.quotes->len2 = len;
      isp = next;
    }
  else if (len || next->type == INPUT_CHAIN)
    {
      if (next->type == INPUT_STRING)
	{
	  next->u.u_s.str = (char *) obstack_finish (current_input);
	  next->u.u_s.len = len;
	}
      else
	make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);
      isp = next;
      input_change = true;
    }
  else
    obstack_free (current_input, next);
  next = NULL;
}

/*--------------------------------------------------------------.
| The function push_wrapup_init () returns an obstack ready for |
| direct expansion of wrapup text, and should be followed by    |
| push_wrapup_finish ().                                        |
`--------------------------------------------------------------*/

struct obstack *
push_wrapup_init (const call_info *caller, token_chain ***end)
{
  input_block *i;
  token_chain *chain;

  assert (obstack_object_size (wrapup_stack) == 0);
  if (wsp != &input_eof)
    {
      i = wsp;
      assert (i->type == INPUT_CHAIN && i->u.u_c.end
	      && i->u.u_c.end->type != CHAIN_LOC);
    }
  else
    {
      i = (input_block *) obstack_alloc (wrapup_stack, sizeof *i);
      i->prev = wsp;
      i->file = caller->file;
      i->line = caller->line;
      i->type = INPUT_CHAIN;
      i->u.u_c.chain = i->u.u_c.end = NULL;
      wsp = i;
    }
  chain = (token_chain *) obstack_alloc (wrapup_stack, sizeof *chain);
  if (i->u.u_c.end)
    i->u.u_c.end->next = chain;
  else
    i->u.u_c.chain = chain;
  i->u.u_c.end = chain;
  chain->next = NULL;
  chain->type = CHAIN_LOC;
  chain->quote_age = 0;
  chain->u.u_l.file = caller->file;
  chain->u.u_l.line = caller->line;
  *end = &i->u.u_c.end;
  return wrapup_stack;
}

/*---------------------------------------------------------------.
| After pushing wrapup text, push_wrapup_finish () completes the |
| bookkeeping.                                                   |
`---------------------------------------------------------------*/
void
push_wrapup_finish (void)
{
  make_text_link (wrapup_stack, &wsp->u.u_c.chain, &wsp->u.u_c.end);
}


/*-------------------------------------------------------------------.
| The function pop_input () pops one level of input sources.  If     |
| CLEANUP, and the popped input_block is a file, current_file and    |
| current_line are reset to the saved values before the memory for   |
| the input_block is released.  The return value is false if cleanup |
| is still required, or if the current input source is not           |
| exhausted.                                                         |
`-------------------------------------------------------------------*/

static bool
pop_input (bool cleanup)
{
  input_block *tmp = isp->prev;
  token_chain *chain;

  switch (isp->type)
    {
    case INPUT_STRING:
      assert (!cleanup || !isp->u.u_s.len);
      if (isp->u.u_s.len)
	return false;
      break;

    case INPUT_CHAIN:
      chain = isp->u.u_c.chain;
      assert (!chain || !cleanup);
      while (chain)
	{
	  switch (chain->type)
	    {
	    case CHAIN_STR:
	      if (chain->u.u_s.len)
		return false;
	      if (chain->u.u_s.level >= 0)
		adjust_refcount (chain->u.u_s.level, false);
	      break;
	    case CHAIN_FUNC:
	       if (chain->u.func)
		 return false;
	       break;
	    case CHAIN_ARGV:
	      if (chain->u.u_a.index < arg_argc (chain->u.u_a.argv))
		return false;
	      arg_adjust_refcount (chain->u.u_a.argv, false);
	      break;
	    case CHAIN_LOC:
	      return false;
	    default:
	      assert (!"pop_input");
	      abort ();
	    }
	  isp->u.u_c.chain = chain = chain->next;
	}
      break;

    case INPUT_FILE_Q:
      return false;
    case INPUT_FILE:
      if (!cleanup)
	return false;
      if (debug_level & DEBUG_TRACE_INPUT)
	{
	  if (tmp != &input_eof)
	    debug_message ("input reverted to %s, line %d",
			   tmp->file, tmp->line);
	  else
	    debug_message ("input exhausted");
	}

      if (ferror (isp->u.u_f.fp))
	{
	  m4_error (0, 0, NULL, _("read error"));
	  if (isp->u.u_f.close)
	    fclose (isp->u.u_f.fp);
	}
      else if (isp->u.u_f.close && fclose (isp->u.u_f.fp) == EOF)
	m4_error (0, errno, NULL, _("error reading file"));
      start_of_input_line = isp->u.u_f.advance;
      output_current_line = -1;
      break;

    case INPUT_EOF:
      return false;

    default:
      assert (!"pop_input");
      abort ();
    }
  if (next)
    {
      /* Only possible after dnl.  */
      assert (obstack_object_size (current_input) == 0
	      && next->type == INPUT_STRING);
      next = NULL;
    }
  obstack_free (current_input, isp);
  cached_quote = NULL;

  isp = tmp;
  input_change = true;
  return true;
}

/*------------------------------------------------------------------------.
| To switch input over to the wrapup stack, main () calls pop_wrapup ().  |
| Since wrapup text can install new wrapup text, pop_wrapup () returns	  |
| false when there is no wrapup text on the stack, and true otherwise.	  |
`------------------------------------------------------------------------*/

bool
pop_wrapup (void)
{
  assert (!next);
  obstack_free (current_input, NULL);
  free (current_input);

  if (wsp == &input_eof)
    {
      /* End of the program.  Free all memory even though we are about
	 to exit, since it makes leak detection easier.  */
      obstack_free (&token_stack, NULL);
      obstack_free (&file_names, NULL);
      obstack_free (wrapup_stack, NULL);
      free (wrapup_stack);
#ifdef ENABLE_CHANGEWORD
      regfree (&word_regexp);
#endif /* ENABLE_CHANGEWORD */
      return false;
    }

  current_input = wrapup_stack;
  wrapup_stack = (struct obstack *) xmalloc (sizeof *wrapup_stack);
  obstack_init (wrapup_stack);

  isp = wsp;
  wsp = &input_eof;
  input_change = true;

  return true;
}

/*--------------------------------------------------------------.
| Dump a representation of INPUT to the obstack OBS, for use in |
| tracing.                                                      |
`--------------------------------------------------------------*/
void
input_print (struct obstack *obs)
{
  size_t len = obstack_object_size (current_input);
  size_t maxlen = max_debug_argument_length;
  token_chain *chain;
  input_block *block = next ? next : isp;
  size_t count;

  switch (block->type)
    {
    case INPUT_STRING:
      assert (next && !block->u.u_s.len);
      break;
    case INPUT_FILE_Q:
      assert (next && len);
      count = block->u.u_q.count;
      while (count--)
	if (shipout_string_trunc (obs, block->u.u_q.quotes->str1,
				  block->u.u_q.quotes->len1, &maxlen))
	  return;
      obstack_grow (obs, "<file: ", strlen ("<file: "));
      obstack_grow (obs, block->prev->file, strlen (block->prev->file));
      obstack_1grow (obs, '>');
      break;
    case INPUT_FILE:
      assert (!len);
      obstack_grow (obs, "<file: ", strlen ("<file: "));
      obstack_grow (obs, block->file, strlen (block->file));
      obstack_1grow (obs, '>');
      break;
    case INPUT_CHAIN:
      chain = block->u.u_c.chain;
      while (chain)
	{
	  switch (chain->type)
	    {
	    case CHAIN_STR:
	      if (shipout_string_trunc (obs, chain->u.u_s.str,
					chain->u.u_s.len, &maxlen))
		return;
	      break;
	    case CHAIN_FUNC:
	      func_print (obs, find_builtin_by_addr (chain->u.func), false,
			  NULL, NULL);
	      break;
	    case CHAIN_ARGV:
	      assert (!chain->u.u_a.comma);
	      if (arg_print (obs, chain->u.u_a.argv, chain->u.u_a.index,
			     quote_cache (NULL, chain->quote_age,
					  chain->u.u_a.quotes),
			     chain->u.u_a.flatten, NULL, NULL, &maxlen, false))
		return;
	      break;
	    default:
	      assert (!"input_print");
	      abort ();
	    }
	  chain = chain->next;
	}
      break;
    default:
      assert (!"input_print");
      abort ();
    }
  if (len)
    shipout_string_trunc (obs, (char *) obstack_base (current_input), len,
			  &maxlen);
}


/*-------------------------------------------------------------------.
| Return a pointer to the available bytes of the current input       |
| block, and set *LEN to the length of the result.  If ALLOW_QUOTE,  |
| do not return a buffer for a quoted string.  If the result of      |
| next_char() would not fit in an unsigned char (for example,        |
| CHAR_EOF or CHAR_QUOTE), or if the input block does not have an    |
| available buffer at the moment (for example, when hitting a buffer |
| block boundary of a file), return NULL, and the caller must fall   |
| back on using next_char().  The buffer is only valid until the     |
| next consume_buffer() or next_char().  When searching for a        |
| particular byte, it is more efficient to search a buffer at a time |
| than it is to repeatedly call next_char.                           |
`-------------------------------------------------------------------*/

static const char *
next_buffer (size_t *len, bool allow_quote)
{
  token_chain *chain;
  input_block *block = isp;

  while (1)
    {
      assert (block);
      if (input_change)
	{
	  current_file = block->file;
	  current_line = block->line;
	  input_change = false;
	}

      switch (block->type)
	{
	case INPUT_STRING:
	  if (block->u.u_s.len)
	    {
	      *len = block->u.u_s.len;
	      return block->u.u_s.str;
	    }
	  break;

	case INPUT_FILE_Q:
	  if (block->u.u_q.count)
	    {
	      push_string_init (block->file, block->line);
	      while (block->u.u_q.count)
		{
		  obstack_grow (current_input, block->u.u_q.quotes->str1,
				block->u.u_q.quotes->len1);
		  block->u.u_q.count--;
		}
	      push_string_finish ();
	      return next_buffer (len, allow_quote);
	    }
	  if (block->prev->u.u_f.end)
	    {
	      string_pair *pair = block->u.u_q.quotes;
	      block->type = INPUT_STRING;
	      block->u.u_s.str = pair->str2;
	      block->u.u_s.len = pair->len2;
	      return next_buffer (len, allow_quote);
	    }
	  block = block->prev;
	  /* fall through */
	case INPUT_FILE:
	  if (start_of_input_line)
	    {
	      start_of_input_line = false;
	      current_line = ++block->line;
	    }
	  if (block->u.u_f.end)
	    break;
	  return freadptr (block->u.u_f.fp, len);

	case INPUT_CHAIN:
	  chain = block->u.u_c.chain;
	  while (chain)
	    {
	      if (allow_quote && chain->quote_age == current_quote_age)
		return NULL; /* CHAR_QUOTE doesn't fit in buffer.  */
	      switch (chain->type)
		{
		case CHAIN_STR:
		  if (chain->u.u_s.len)
		    {
		      *len = chain->u.u_s.len;
		      return chain->u.u_s.str;
		    }
		  if (chain->u.u_s.level >= 0)
		    adjust_refcount (chain->u.u_s.level, false);
		  break;
		case CHAIN_FUNC:
		  if (chain->u.func)
		    return NULL; /* CHAR_MACRO doesn't fit in buffer.  */
		  break;
		case CHAIN_ARGV:
		  if (chain->u.u_a.index == arg_argc (chain->u.u_a.argv))
		    {
		      arg_adjust_refcount (chain->u.u_a.argv, false);
		      break;
		    }
		  return NULL; /* No buffer to provide.  */
		case CHAIN_LOC:
		  block->file = chain->u.u_l.file;
		  block->line = chain->u.u_l.line;
		  input_change = true;
		  block->u.u_c.chain = chain->next;
		  return next_buffer (len, allow_quote);
		default:
		  assert (!"next_buffer");
		  abort ();
		}
	      block->u.u_c.chain = chain = chain->next;
	    }
	  break;

	case INPUT_EOF:
	  return NULL; /* CHAR_EOF doesn't fit in buffer.  */

	default:
	  assert (!"next_buffer");
	  abort ();
	}

      /* End of input source --- pop one level.  */
      pop_input (true);
      block = isp;
    }
}

/*-----------------------------------------------------------------.
| Consume LEN bytes from the current input block, as though by LEN |
| calls to next_char().  LEN must be less than or equal to the     |
| previous length returned by a successful call to next_buffer().  |
`-----------------------------------------------------------------*/

static void
consume_buffer (size_t len)
{
  token_chain *chain;
  const char *buf;
  const char *p;
  size_t buf_len;
  input_block *block = isp;

  assert (block && !input_change && len);
  switch (block->type)
    {
    case INPUT_STRING:
      assert (len <= block->u.u_s.len);
      block->u.u_s.len -= len;
      block->u.u_s.str += len;
      break;

    case INPUT_FILE_Q:
      assert (!block->u.u_q.count);
      block = block->prev;
      /* fall through */
    case INPUT_FILE:
      assert (!start_of_input_line && !block->u.u_f.end);
      buf = freadptr (block->u.u_f.fp, &buf_len);
      assert (buf && len <= buf_len);
      buf_len = 0;
      while ((p = memchr (buf + buf_len, '\n', len - buf_len)))
	{
	  if (p == buf + len - 1)
	    start_of_input_line = true;
	  else
	    current_line = ++block->line;
	  buf_len = p - buf + 1;
	}
      if (freadseek (block->u.u_f.fp, len) != 0)
	assert (false);
      break;

    case INPUT_CHAIN:
      chain = block->u.u_c.chain;
      assert (chain && chain->type == CHAIN_STR && len <= chain->u.u_s.len);
      /* Partial consumption invalidates quote age.  */
      chain->quote_age = 0;
      chain->u.u_s.len -= len;
      chain->u.u_s.str += len;
      break;

    default:
      assert (!"consume_buffer");
      abort ();
    }
}

/*------------------------------------------------------------------.
| Low level input is done a character at a time.  The function      |
| peek_input () is used to look at the next character in the input  |
| stream.  At any given time, it reads from the input_block on the  |
| top of the current input stack.  The return value is an unsigned  |
| char, CHAR_EOF if there is no more input, CHAR_MACRO if a builtin |
| token occurs next, or CHAR_ARGV if ALLOW_ARGV and the input is    |
| visiting an argv reference with the correct quoting.              |
`------------------------------------------------------------------*/

static int
peek_input (bool allow_argv)
{
  int ch;
  input_block *block = isp;
  token_chain *chain;

  while (1)
    {
      assert (block);
      switch (block->type)
	{
	case INPUT_STRING:
	  if (!block->u.u_s.len)
	    break;
	  return to_uchar (block->u.u_s.str[0]);

	case INPUT_FILE_Q:
	  if (block->u.u_q.count)
	    {
	      push_string_init (block->file, block->line);
	      while (block->u.u_q.count)
		{
		  obstack_grow (current_input, block->u.u_q.quotes->str1,
				block->u.u_q.quotes->len1);
		  block->u.u_q.count--;
		}
	      push_string_finish ();
	      return peek_input (allow_argv);
	    }
	  if (block->prev->u.u_f.end)
	    {
	      string_pair *pair = block->u.u_q.quotes;
	      block->type = INPUT_STRING;
	      block->u.u_s.str = pair->str2;
	      block->u.u_s.len = pair->len2;
	      return peek_input (allow_argv);
	    }
	  block = block->prev;
	  /* fall through */
	case INPUT_FILE:
	  ch = getc (block->u.u_f.fp);
	  if (ch != EOF)
	    {
	      ungetc (ch, block->u.u_f.fp);
	      return ch;
	    }
	  block->u.u_f.end = true;
	  break;

	case INPUT_CHAIN:
	  chain = block->u.u_c.chain;
	  while (chain)
	    {
	      unsigned int argc;
	      switch (chain->type)
		{
		case CHAIN_STR:
		  if (chain->u.u_s.len)
		    return to_uchar (*chain->u.u_s.str);
		  break;
		case CHAIN_FUNC:
		  if (chain->u.func)
		    return CHAR_MACRO;
		  break;
		case CHAIN_ARGV:
		  argc = arg_argc (chain->u.u_a.argv);
		  if (chain->u.u_a.index == argc)
		    break;
		  if (chain->u.u_a.comma)
		    return ',';
		  /* Only return a reference if the quoting is correct
		     and the reference has more than one argument
		     left.  */
		  if (allow_argv && chain->quote_age == current_quote_age
		      && chain->u.u_a.quotes && chain->u.u_a.index + 1 < argc)
		    return CHAR_ARGV;
		  /* Rather than directly parse argv here, we push
		     another input block containing the next unparsed
		     argument from argv.  */
		  push_string_init (block->file, block->line);
		  push_arg_quote (current_input, chain->u.u_a.argv,
				  chain->u.u_a.index,
				  quote_cache (NULL, chain->quote_age,
					       chain->u.u_a.quotes));
		  chain->u.u_a.index++;
		  chain->u.u_a.comma = true;
		  push_string_finish ();
		  return peek_input (allow_argv);
		case CHAIN_LOC:
		  break;
		default:
		  assert (!"peek_input");
		  abort ();
		}
	      chain = chain->next;
	    }
	  break;

	case INPUT_EOF:
	  return CHAR_EOF;

	default:
	  assert (!"peek_input");
	  abort ();
	}
      block = block->prev;
    }
}

/*-------------------------------------------------------------------.
| The function next_char () is used to read and advance the input to |
| the next character.  It also manages line numbers for error        |
| messages, so they do not get wrong due to lookahead.  The token    |
| consisting of a newline alone is taken as belonging to the line it |
| ends, and the current line number is not incremented until the     |
| next character is read.  99.9% of all calls will read from a       |
| string, so factor that out into a macro for speed.  If             |
| ALLOW_QUOTE, and the current input matches the current quote age,  |
| return CHAR_QUOTE and leave consumption of data for                |
| append_quote_token; otherwise, if ALLOW_ARGV and the current input |
| matches an argv reference with the correct quoting, return         |
| CHAR_ARGV and leave consuption of data for init_argv_token.        |
`-------------------------------------------------------------------*/

#define next_char(AQ, AA)						\
  (isp->type == INPUT_STRING && isp->u.u_s.len && !input_change		\
   ? (isp->u.u_s.len--, to_uchar (*isp->u.u_s.str++))			\
   : next_char_1 (AQ, AA))

static int
next_char_1 (bool allow_quote, bool allow_argv)
{
  int ch;
  token_chain *chain;
  input_block *block = isp;

  while (1)
    {
      assert (block);
      if (input_change)
	{
	  current_file = block->file;
	  current_line = block->line;
	  input_change = false;
	}

      switch (block->type)
	{
	case INPUT_STRING:
	  if (!block->u.u_s.len)
	    break;
	  block->u.u_s.len--;
	  return to_uchar (*block->u.u_s.str++);

	case INPUT_FILE_Q:
	  if (block->u.u_q.count)
	    {
	      push_string_init (block->file, block->line);
	      while (block->u.u_q.count)
		{
		  obstack_grow (current_input, block->u.u_q.quotes->str1,
				block->u.u_q.quotes->len1);
		  block->u.u_q.count--;
		}
	      push_string_finish ();
	      return next_char_1 (allow_quote, allow_argv);
	    }
	  if (block->prev->u.u_f.end)
	    {
	      string_pair *pair = block->u.u_q.quotes;
	      block->type = INPUT_STRING;
	      block->u.u_s.str = pair->str2;
	      block->u.u_s.len = pair->len2;
	      return next_char_1 (allow_quote, allow_argv);
	    }
	  block = block->prev;
	  /* fall through */
	case INPUT_FILE:
	  if (start_of_input_line)
	    {
	      start_of_input_line = false;
	      current_line = ++block->line;
	    }

	  /* If stdin is a terminal, calling getc after peek_input
	     already called it would make the user have to hit ^D
	     twice to quit.  */
	  ch = block->u.u_f.end ? EOF : getc (block->u.u_f.fp);
	  if (ch != EOF)
	    {
	      if (ch == '\n')
		start_of_input_line = true;
	      return ch;
	    }
	  block->u.u_f.end = true;
	  break;

	case INPUT_CHAIN:
	  chain = block->u.u_c.chain;
	  while (chain)
	    {
	      unsigned int argc;
	      if (allow_quote && chain->quote_age == current_quote_age)
		return CHAR_QUOTE;
	      switch (chain->type)
		{
		case CHAIN_STR:
		  if (chain->u.u_s.len)
		    {
		      /* Partial consumption invalidates quote age.  */
		      chain->quote_age = 0;
		      chain->u.u_s.len--;
		      return to_uchar (*chain->u.u_s.str++);
		    }
		  if (chain->u.u_s.level >= 0)
		    adjust_refcount (chain->u.u_s.level, false);
		  break;
		case CHAIN_FUNC:
		  if (chain->u.func)
		    return CHAR_MACRO;
		  break;
		case CHAIN_ARGV:
		  argc = arg_argc (chain->u.u_a.argv);
		  if (chain->u.u_a.index == argc)
		    {
		      arg_adjust_refcount (chain->u.u_a.argv, false);
		      break;
		    }
		  if (chain->u.u_a.comma)
		    {
		      chain->u.u_a.comma = false;
		      return ',';
		    }
		  /* Only return a reference if the quoting is correct
		     and the reference has more than one argument
		     left.  */
		  if (allow_argv && chain->quote_age == current_quote_age
		      && chain->u.u_a.quotes && chain->u.u_a.index + 1 < argc)
		    return CHAR_ARGV;
		  /* Rather than directly parse argv here, we push
		     another input block containing the next unparsed
		     argument from argv.  */
		  push_string_init (block->file, block->line);
		  push_arg_quote (current_input, chain->u.u_a.argv,
				  chain->u.u_a.index,
				  quote_cache (NULL, chain->quote_age,
					       chain->u.u_a.quotes));
		  chain->u.u_a.index++;
		  chain->u.u_a.comma = true;
		  push_string_finish ();
		  return next_char_1 (allow_quote, allow_argv);
		case CHAIN_LOC:
		  block->file = chain->u.u_l.file;
		  block->line = chain->u.u_l.line;
		  input_change = true;
		  block->u.u_c.chain = chain->next;
		  return next_char_1 (allow_quote, allow_argv);
		default:
		  assert (!"next_char_1");
		  abort ();
		}
	      block->u.u_c.chain = chain = chain->next;
	    }
	  break;

	case INPUT_EOF:
	  return CHAR_EOF;

	default:
	  assert (!"next_char_1");
	  abort ();
	}

      /* End of input source --- pop one level.  */
      pop_input (true);
      block = isp;
    }
}

/*-------------------------------------------------------------------.
| skip_line () simply discards all immediately following characters, |
| up to the first newline.  It is only used from m4_dnl ().  Report  |
| warnings on behalf of NAME.                                        |
`-------------------------------------------------------------------*/

void
skip_line (const call_info *name)
{
  int ch;

  while (1)
    {
      size_t len;
      const char *buffer = next_buffer (&len, false);
      if (buffer)
	{
	  const char *p = (char *) memchr (buffer, '\n', len);
	  if (p)
	    {
	      consume_buffer (p - buffer + 1);
	      ch = '\n';
	      break;
	    }
	  consume_buffer (len);
	}
      else
	{
	  ch = next_char (false, false);
	  if (ch == CHAR_EOF || ch == '\n')
	    break;
	}
    }
  if (ch == CHAR_EOF)
    m4_warn (0, name, _("end of file treated as newline"));
  if (!next)
    /* Possible if dnl popped input looking for newline.  */
    push_string_init (current_file, current_line);
}

/*------------------------------------------------------------------.
| When next_token() sees a builtin token with peek_input, this	    |
| retrieves the value of the function pointer, stores it in TD, and |
| consumes the input so the caller does not need to do next_char.   |
| If OBS, TD will be converted to a composite token using storage   |
| from OBS as necessary; otherwise, if TD is NULL, the builtin is   |
| discarded.                                                        |
`------------------------------------------------------------------*/

static void
init_macro_token (struct obstack *obs, token_data *td)
{
  token_chain *chain;

  assert (isp->type == INPUT_CHAIN);
  chain = isp->u.u_c.chain;
  assert (!chain->quote_age && chain->type == CHAIN_FUNC && chain->u.func);
  if (obs)
    {
      assert (td);
      if (TOKEN_DATA_TYPE (td) == TOKEN_VOID)
	{
	  TOKEN_DATA_TYPE (td) = TOKEN_COMP;
	  td->u.u_c.chain = td->u.u_c.end = NULL;
	  td->u.u_c.wrapper = false;
	  td->u.u_c.has_func = true;
	}
      assert (TOKEN_DATA_TYPE (td) == TOKEN_COMP);
      append_macro (obs, chain->u.func, &td->u.u_c.chain, &td->u.u_c.end);
    }
  else if (td)
    {
      assert (TOKEN_DATA_TYPE (td) == TOKEN_VOID);
      TOKEN_DATA_TYPE (td) = TOKEN_FUNC;
      TOKEN_DATA_FUNC (td) = chain->u.func;
    }
  chain->u.func = NULL;
}

/*-------------------------------------------------------------------.
| When a QUOTE token is seen, convert TD to a composite (if it is    |
| not one already), consisting of any unfinished text on OBS, as     |
| well as the quoted token from the top of the input stack.  Use OBS |
| for any additional allocations needed to store the token chain.    |
`-------------------------------------------------------------------*/
static void
append_quote_token (struct obstack *obs, token_data *td)
{
  token_chain *src_chain = isp->u.u_c.chain;
  token_chain *chain;

  assert (isp->type == INPUT_CHAIN && obs && current_quote_age);
  isp->u.u_c.chain = src_chain->next;

  /* Speed consideration - for short enough tokens, the speed and
     memory overhead of parsing another INPUT_CHAIN link outweighs the
     time to inline the token text.  Also, if the quoted string does
     not live in a back-reference, it must be copied.  */
  if (src_chain->type == CHAIN_STR
      && (src_chain->u.u_s.len <= INPUT_INLINE_THRESHOLD
	  || src_chain->u.u_s.level < 0))
    {
      obstack_grow (obs, src_chain->u.u_s.str, src_chain->u.u_s.len);
      if (src_chain->u.u_s.level >= 0)
	adjust_refcount (src_chain->u.u_s.level, false);
      return;
    }

  if (TOKEN_DATA_TYPE (td) == TOKEN_VOID)
    {
      TOKEN_DATA_TYPE (td) = TOKEN_COMP;
      td->u.u_c.chain = td->u.u_c.end = NULL;
      td->u.u_c.wrapper = td->u.u_c.has_func = false;
    }
  assert (TOKEN_DATA_TYPE (td) == TOKEN_COMP);
  make_text_link (obs, &td->u.u_c.chain, &td->u.u_c.end);
  chain = (token_chain *) obstack_copy (obs, src_chain, sizeof *chain);
  if (td->u.u_c.end)
    td->u.u_c.end->next = chain;
  else
    td->u.u_c.chain = chain;
  td->u.u_c.end = chain;
  if (chain->type == CHAIN_ARGV && chain->u.u_a.has_func)
    td->u.u_c.has_func = true;
  chain->next = NULL;
}


/*-------------------------------------------------------------------.
| When an ARGV token is seen, convert TD to point to it via a	     |
| composite token.  Use OBS for any additional allocations needed to |
| store the token chain.					     |
`-------------------------------------------------------------------*/
static void
init_argv_token (struct obstack *obs, token_data *td)
{
  token_chain *src_chain;
  token_chain *chain;
  int ch;

  assert (TOKEN_DATA_TYPE (td) == TOKEN_VOID
	  && isp->type == INPUT_CHAIN && isp->u.u_c.chain->type == CHAIN_ARGV
	  && obs && obstack_object_size (obs) == 0);

  src_chain = isp->u.u_c.chain;
  isp->u.u_c.chain = src_chain->next;
  TOKEN_DATA_TYPE (td) = TOKEN_COMP;
  /* Clone the link, since the input will be discarded soon.  */
  chain = (token_chain *) obstack_copy (obs, src_chain, sizeof *chain);
  td->u.u_c.chain = td->u.u_c.end = chain;
  td->u.u_c.wrapper = true;
  td->u.u_c.has_func = chain->u.u_a.has_func;
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
     decrement of the argv refcount in next_char_1, since the ref is
     still live via the current collect_arguments.  However, when the
     last element of the $@ ref is reparsed, we must increase the argv
     refcount here, to compensate for the fact that it will be
     decreased once the final element is parsed.  */
  assert (!curr_comm.len1 || (*curr_comm.str1 != ',' && *curr_comm.str1 != ')'
			      && *curr_comm.str1 != *curr_quote.str1));
  ch = peek_input (true);
  if (ch != ',' && ch != ')')
    {
      isp->u.u_c.chain = src_chain;
      src_chain->u.u_a.index = arg_argc (chain->u.u_a.argv) - 1;
      src_chain->u.u_a.comma = true;
      chain->u.u_a.skip_last = true;
      arg_adjust_refcount (chain->u.u_a.argv, true);
    }
}


/*------------------------------------------------------------------.
| If the string S of length SLEN matches the next characters of the |
| input stream, return true.  If CONSUME, the first character has   |
| already been matched.  If a match is found and CONSUME is true,   |
| the input is discarded; otherwise any characters read are pushed  |
| back again.  The function is used only when multicharacter quotes |
| or comment delimiters are used.                                   |
`------------------------------------------------------------------*/

static bool
match_input (const char *s, size_t slen, bool consume)
{
  int n;			/* number of characters matched */
  int ch;			/* input character */
  const char *t;
  bool result = false;
  size_t len;

  if (consume)
    {
      s++;
      slen--;
    }
  /* Try a buffer match first.  */
  assert (slen);
  t = next_buffer (&len, false);
  if (t && slen <= len && memcmp (s, t, slen) == 0)
    {
      if (consume)
	consume_buffer (slen);
      return true;
    }

  /* Fall back on byte matching.  */
  ch = peek_input (false);
  if (ch != to_uchar (*s))
    return false;

  if (slen == 1)
    {
      if (consume)
	next_char (false, false);
      return true;			/* short match */
    }

  next_char (false, false);
  for (n = 1, t = s++; (ch = peek_input (false)) == to_uchar (*s++); )
    {
      next_char (false, false);
      n++;
      if (--slen == 1)		/* long match */
	{
	  if (consume)
	    return true;
	  result = true;
	  break;
	}
    }

  /* Failed or shouldn't consume, push back input.  */
  push_string_init (current_file, current_line);
  obstack_grow (current_input, t, n);
  push_string_finish ();
  return result;
}

/*--------------------------------------------------------------------.
| The macro MATCH() is used to match a string S of length SLEN        |
| against the input.  The first character is handled inline for       |
| speed, and S[SLEN] must be safe to dereference (it is faster to do  |
| character comparison prior to length checks).  This improves        |
| efficiency for the common case of single character quotes and       |
| comment delimiters, while being safe for disabled delimiters as     |
| well as longer delimiters.  If CONSUME, then CH is the result of    |
| next_char, and a successful match will discard the matched string.  |
| Otherwise, CH is the result of peek_input, and the input stream is  |
| effectively unchanged.                                              |
`--------------------------------------------------------------------*/

#define MATCH(ch, s, slen, consume)					\
  (to_uchar ((s)[0]) == (ch)						\
   && ((slen) >> 1 ? match_input (s, slen, consume) : (slen)))


/*----------------------------------------------------------.
| Inititialize input stacks, and quote/comment characters.  |
`----------------------------------------------------------*/

void
input_init (void)
{
  current_file = "";
  current_line = 0;

  current_input = (struct obstack *) xmalloc (sizeof *current_input);
  obstack_init (current_input);
  wrapup_stack = (struct obstack *) xmalloc (sizeof *wrapup_stack);
  obstack_init (wrapup_stack);

  obstack_init (&file_names);

  /* Allocate an object in the current chunk, so that obstack_free
     will always work even if the first token parsed spills to a new
     chunk.  */
  obstack_init (&token_stack);
  token_bottom = obstack_finish (&token_stack);

  isp = &input_eof;
  wsp = &input_eof;
  next = NULL;

  start_of_input_line = false;

  curr_quote.str1 = xmemdup0 (DEF_LQUOTE, 1);
  curr_quote.len1 = 1;
  curr_quote.str2 = xmemdup0 (DEF_RQUOTE, 1);
  curr_quote.len2 = 1;
  curr_comm.str1 = xmemdup0 (DEF_BCOMM, 1);
  curr_comm.len1 = 1;
  curr_comm.str2 = xmemdup0 (DEF_ECOMM, 1);
  curr_comm.len2 = 1;

#ifdef ENABLE_CHANGEWORD
  set_word_regexp (NULL, user_word_regexp, SIZE_MAX);
#endif /* ENABLE_CHANGEWORD */

  set_quote_age ();
}


/*-----------------------------------------------------------------.
| Set the quote delimiters to LQ and RQ, with respective lengths   |
| LQ_LEN and RQ_LEN.  Used by m4_changequote ().  Pass NULL if the |
| argument was not present, to distinguish from an explicit empty  |
| string.                                                          |
`-----------------------------------------------------------------*/

void
set_quotes (const char *lq, size_t lq_len, const char *rq, size_t rq_len)
{
  /* POSIX states that with 0 arguments, the default quotes are used.
     POSIX XCU ERN 112 states that behavior is implementation-defined
     if there was only one argument, or if there is an empty string in
     either position when there are two arguments.  We allow an empty
     left quote to disable quoting, but a non-empty left quote will
     always create a non-empty right quote.  See the texinfo for what
     some other implementations do.  */
  if (!lq)
    {
      lq = DEF_LQUOTE;
      lq_len = 1;
      rq = DEF_RQUOTE;
      rq_len = 1;
    }
  else if (!rq || (lq_len && !rq_len))
    {
      rq = DEF_RQUOTE;
      rq_len = 1;
    }

  if (curr_quote.len1 == lq_len && curr_quote.len2 == rq_len
      && memcmp (curr_quote.str1, lq, lq_len) == 0
      && memcmp (curr_quote.str2, rq, rq_len) == 0)
    return;

  free (curr_quote.str1);
  free (curr_quote.str2);
  /* The use of xmemdup0 is essential for MATCH() to work.  */
  curr_quote.str1 = xmemdup0 (lq, lq_len);
  curr_quote.len1 = lq_len;
  curr_quote.str2 = xmemdup0 (rq, rq_len);
  curr_quote.len2 = rq_len;
  set_quote_age ();
}

/*-----------------------------------------------------------------.
| Set the comment delimiters to BC and EC, with respective lengths |
| BC_LEN and EC_LEN.  Used by m4_changecom ().  Pass NULL if the   |
| argument was not present, to distinguish from an explicit empty  |
| string.                                                          |
`-----------------------------------------------------------------*/

void
set_comment (const char *bc, size_t bc_len, const char *ec, size_t ec_len)
{
  /* POSIX requires no arguments to disable comments.  It requires
     empty arguments to be used as-is, but this is counter to
     traditional behavior, because a non-null begin and null end makes
     it impossible to end a comment.  An aardvark has been filed:
     http://www.opengroup.org/austin/mailarchives/ag-review/msg02168.html
     This implementation assumes the aardvark will be approved.  See
     the texinfo for what some other implementations do.  */
  if (!bc)
    {
      bc = ec = "";
      bc_len = ec_len = 0;
    }
  else if (!ec || (bc_len && !ec_len))
    {
      ec = DEF_ECOMM;
      ec_len = 1;
    }

  if (curr_comm.len1 == bc_len && curr_comm.len2 == ec_len
      && memcmp (curr_comm.str1, bc, bc_len) == 0
      && memcmp (curr_comm.str2, ec, ec_len) == 0)
    return;

  free (curr_comm.str1);
  free (curr_comm.str2);
  /* The use of xmemdup0 is essential for MATCH() to work.  */
  curr_comm.str1 = xmemdup0 (bc, bc_len);
  curr_comm.len1 = bc_len;
  curr_comm.str2 = xmemdup0 (ec, ec_len);
  curr_comm.len2 = ec_len;
  set_quote_age ();
}

#ifdef ENABLE_CHANGEWORD

/*-----------------------------------------------------------------.
| Set the regular expression for recognizing words to REGEXP of    |
| length LEN, and report errors on behalf of CALLER.  If REGEXP is |
| NULL, revert back to the default parsing rules.  If LEN is       |
| SIZE_MAX, use strlen(REGEXP) instead.                            |
`-----------------------------------------------------------------*/

void
set_word_regexp (const call_info *caller, const char *regexp, size_t len)
{
  const char *msg;
  struct re_pattern_buffer new_word_regexp;

  if (len == SIZE_MAX)
    len = strlen (regexp);
  if (len == 0
      || (len == strlen (DEFAULT_WORD_REGEXP)
	  && !memcmp (regexp, DEFAULT_WORD_REGEXP, len)))
    {
      default_word_regexp = true;
      set_quote_age ();
      return;
    }

  /* Dry run to see whether the new expression is compilable.  */
  init_pattern_buffer (&new_word_regexp, NULL);
  msg = re_compile_pattern (regexp, len, &new_word_regexp);
  regfree (&new_word_regexp);

  if (msg != NULL)
    {
      m4_warn (0, caller, _("bad regular expression %s: %s"),
	       quotearg_style_mem (locale_quoting_style, regexp, len), msg);
      return;
    }

  /* If compilation worked, retry using the word_regexp struct.  We
     can't rely on struct assigns working, so redo the compilation.
     The fastmap can be reused between compilations, and will be freed
     by the final regfree.  */
  if (!word_regexp.fastmap)
    word_regexp.fastmap = xcharalloc (UCHAR_MAX + 1);
  msg = re_compile_pattern (regexp, len, &word_regexp);
  assert (!msg);
  re_set_registers (&word_regexp, &regs, regs.num_regs, regs.start, regs.end);
  if (re_compile_fastmap (&word_regexp))
    assert (false);

  default_word_regexp = false;
  set_quote_age ();
}

#endif /* ENABLE_CHANGEWORD */

/* Call this when changing anything that might impact the quote age,
   so that quote_age and safe_quotes will reflect the change.  */
static void
set_quote_age (void)
{
  /* Multi-character quotes are inherently unsafe, since concatenation
     of individual characters can result in a quote delimiter,
     consider:

     define(echo,``$1'')define(a,A)changequote(<[,]>)echo(<[]]><[>a]>)
     => A]> (not ]>a)

   Also, unquoted close delimiters are unsafe, consider:

     define(echo,``$1'')define(a,A)echo(`a''`a')
     => aA' (not a'a)

   Comment delimiters that overlap with quote delimiters or active
   characters also present a problem, consider:

     define(echo,$*)echo(a,a,a`'define(a,A)changecom(`,',`,'))
     => A,a,A (not A,A,A)

   And let's not even think about the impact of changeword, since it
   will disappear for M4 2.0.

   So rather than check every token for an unquoted delimiter, we
   merely encode current_quote_age to 0 when things are unsafe, and
   non-zero when safe (namely, to the 16-bit value composed of the
   single-character start and end quote delimiters).  There may be
   other situations which are safe even when this algorithm sets the
   quote_age to zero, but at least a quote_age of zero always produces
   correct results (although it may take more time in doing so).  */

  /* Hueristic of characters that might impact rescan if they appear
     in a quote delimiter.  Using a single NUL as one of the two quote
     delimiters is safe, but strchr matches it, so we must special
     case the strchr below.  If we were willing to guarantee a
     trailing NUL, we could use strpbrk(quote, unsafe) rather than
     strchr(unsafe, *quote) and avoid the special case; on the other
     hand, many strpbrk implementations are not as efficient as
     strchr, and we save memory by avoiding the trailing NUL.  */
#define Letters "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
  static const char unsafe[] = Letters "_0123456789(,) \t\n\r\f\v";
#undef Letters

  if (curr_quote.len1 == 1 && curr_quote.len2 == 1
      && (!*curr_quote.str1 || strchr (unsafe, *curr_quote.str1) == NULL)
      && (!*curr_quote.str2 || strchr (unsafe, *curr_quote.str2) == NULL)
      && default_word_regexp && *curr_quote.str1 != *curr_quote.str2
      && (!curr_comm.len1
	  || (*curr_comm.str1 != '(' && *curr_comm.str1 != ','
	      && *curr_comm.str1 != ')'
	      && *curr_comm.str1 != *curr_quote.str1)))
    current_quote_age = (((*curr_quote.str1 & 0xff) << 8)
			 | (*curr_quote.str2 & 0xff));
  else
    current_quote_age = 0;
  cached_quote = NULL;
}

/* Return the current quote age.  Each non-trivial changequote alters
   this value; the idea is that if quoting hasn't changed, then we can
   skip parsing a single argument, quoted or unquoted, within the
   context of a quoted string, as well as skip parsing a series of
   quoted arguments within the context of argument collection.  */
unsigned int
quote_age (void)
{
  /* This accessor is a function, so that the implementation can
     change if needed.  See set_quote_age for the current
     implementation.  */
  return current_quote_age;
}

/* Return true if the current quote delimiters guarantee that
   reparsing the current token in the context of a quoted string will
   be safe.  This could always return false and behavior would still
   be correct, just slower.  */
bool
safe_quotes (void)
{
  return current_quote_age != 0;
}

/* Interface for caching frequently used quote pairs, using AGE for
   optimization.  If QUOTES is NULL, don't use quoting.  If OBS is
   non-NULL, AGE should be the current quote age, and QUOTES should be
   &curr_quote; the return value will be a cached quote pair, where
   the pointer is valid at least as long as OBS is not reset, but
   whose contents are only guaranteed until the next changequote or
   quote_cache.  Otherwise, OBS is NULL, AGE should be the same as
   before, and QUOTES should be a previously returned cache value;
   used to refresh the contents of the result.  */
const string_pair *
quote_cache (struct obstack *obs, unsigned int age, const string_pair *quotes)
{
  static char lquote[2];
  static char rquote[2];
  static string_pair simple = {lquote, 1, rquote, 1};

  /* Implementation - if AGE is non-zero, then the implementation of
     set_quote_age guarantees that we can recreate the return value on
     the fly; so we use static storage, and the contents must be used
     immediately.  If AGE is zero, then we must copy QUOTES onto OBS
     (since changequote will invalidate the original), but we might as
     well cache that copy (in case the current expansion contains more
     than one instance of $@).  */
  if (!quotes)
    return NULL;
  if (age)
    {
      *lquote = (age >> 8) & 0xff;
      *rquote = age & 0xff;
      return &simple;
    }
  if (!obs)
    return quotes;
  assert (next && quotes == &curr_quote);
  if (!cached_quote)
    {
      assert (obs == current_input && obstack_object_size (obs) == 0);
      cached_quote = (string_pair *) obstack_copy (obs, quotes,
						   sizeof *quotes);
      cached_quote->str1 = (char *) obstack_copy0 (obs, quotes->str1,
						   quotes->len1);
      cached_quote->str2 = (char *) obstack_copy0 (obs, quotes->str2,
						   quotes->len2);
    }
  return cached_quote;
}


/*--------------------------------------------------------------------.
| Parse a single token from the input stream, set TD to its	      |
| contents, and return its type.  A token is TOKEN_EOF if the	      |
| input_stack is empty; TOKEN_STRING for a quoted string or comment;  |
| TOKEN_WORD for something that is a potential macro name; and	      |
| TOKEN_SIMPLE for any single character that is not a part of any of  |
| the previous types.  If LINE is not NULL, set *LINE to the line     |
| where the token starts.  If OBS is not NULL, expand TOKEN_STRING    |
| directly into OBS rather than in token_stack temporary storage      |
| area, and TD could be a TOKEN_COMP instead of the usual	      |
| TOKEN_TEXT.  If ALLOW_ARGV, OBS must be non-NULL, and an entire     |
| series of arguments can be returned as TOKEN_ARGV when a $@	      |
| reference is encountered.  Report errors (unterminated comments or  |
| strings) on behalf of CALLER, if non-NULL.			      |
|								      |
| Next_token () returns the token type, and passes back a pointer to  |
| the token data through TD.  Non-string token text is collected on   |
| the obstack token_stack, which never contains more than one token   |
| text at a time.  The storage pointed to by the fields in TD is      |
| therefore subject to change the next time next_token () is called.  |
`--------------------------------------------------------------------*/

token_type
next_token (token_data *td, int *line, struct obstack *obs, bool allow_argv,
	    const call_info *caller)
{
  int ch;
  int quote_level;
  token_type type;
#ifdef ENABLE_CHANGEWORD
  char *orig_text = NULL;
#endif /* ENABLE_CHANGEWORD */
  const char *file = NULL;
  /* The obstack where token data is stored.  Generally token_stack,
     for tokens where argument collection might not use the literal
     token.  But for comments and strings, we can output directly into
     the argument collection obstack obs, if one was provided.  */
  struct obstack *obs_td = &token_stack;
  obstack_free (&token_stack, token_bottom);

  TOKEN_DATA_TYPE (td) = TOKEN_VOID;
  ch = next_char (false, allow_argv && current_quote_age);
  if (line)
    {
      *line = current_line;
      file = current_file;
    }
  if (ch == CHAR_EOF)
    {
#ifdef DEBUG_INPUT
      xfprintf (stderr, "next_token -> EOF\n");
#endif /* DEBUG_INPUT */
      return TOKEN_EOF;
    }
  if (ch == CHAR_MACRO)
    {
      init_macro_token (obs, td);
#ifdef DEBUG_INPUT
      xfprintf (stderr, "next_token -> MACDEF (%s)\n",
		find_builtin_by_addr (TOKEN_DATA_FUNC (td))->name);
#endif /* DEBUG_INPUT */
      return TOKEN_MACDEF;
    }
  if (ch == CHAR_ARGV)
    {
      init_argv_token (obs, td);
#ifdef DEBUG_INPUT
      xfprintf (stderr, "next_token -> ARGV (%d args)\n",
		(arg_argc (td->u.u_c.chain->u.u_a.argv)
		 - td->u.u_c.chain->u.u_a.index
		 - (td->u.u_c.chain->u.u_a.skip_last ? 1 : 0)));
#endif
      return TOKEN_ARGV;
    }

  if (MATCH (ch, curr_comm.str1, curr_comm.len1, true))
    {
      if (obs)
	obs_td = obs;
      obstack_grow (obs_td, curr_comm.str1, curr_comm.len1);
      while (1)
	{
	  /* Start with buffer search for potential end delimiter.  */
	  size_t len;
	  const char *buffer = next_buffer (&len, false);
	  if (buffer)
	    {
	      const char *p = (char *) memchr (buffer, *curr_comm.str2, len);
	      if (p)
		{
		  obstack_grow (obs_td, buffer, p - buffer);
		  ch = to_uchar (*p);
		  consume_buffer (p - buffer + 1);
		}
	      else
		{
		  obstack_grow (obs_td, buffer, len);
		  consume_buffer (len);
		  continue;
		}
	    }

	  /* Fall back to byte-wise search.  */
	  else
	    ch = next_char (false, false);
	  if (ch == CHAR_EOF)
	    {
	      /* Current_file changed to "" if we see CHAR_EOF, use
		 the previous value we stored earlier.  */
	      if (!caller)
		{
		  assert (line);
		  current_line = *line;
		  current_file = file;
		}
	      m4_error (EXIT_FAILURE, 0, caller, _("end of file in comment"));
	    }
	  if (ch == CHAR_MACRO)
	    {
	      init_macro_token (obs, obs ? td : NULL);
	      continue;
	    }
	  if (MATCH (ch, curr_comm.str2, curr_comm.len2, true))
	    {
	      obstack_grow (obs_td, curr_comm.str2, curr_comm.len2);
	      break;
	    }
	  assert (ch < CHAR_EOF);
	  obstack_1grow (obs_td, ch);
	}
      type = TOKEN_COMMENT;
    }
  else if (default_word_regexp && (isalpha (ch) || ch == '_'))
    {
      obstack_1grow (&token_stack, ch);
      while (1)
	{
	  size_t len;
	  const char *buffer = next_buffer (&len, false);
	  if (buffer)
	    {
	      const char *p = buffer;
	      while (len && (isalnum (to_uchar (*p)) || *p == '_'))
		{
		  p++;
		  len--;
		}
	      if (p != buffer)
		{
		  obstack_grow (&token_stack, buffer, p - buffer);
		  consume_buffer (p - buffer);
		}
	      if (len)
		break;
	    }
	  else
	    {
	      ch = peek_input (false);
	      if (ch < CHAR_EOF && (isalnum (ch) || ch == '_'))
		{
		  obstack_1grow (&token_stack, ch);
		  next_char (false, false);
		}
	      else
		break;
	    }
	}
      type = TOKEN_WORD;
    }

#ifdef ENABLE_CHANGEWORD

  else if (!default_word_regexp && word_regexp.fastmap[ch])
    {
      obstack_1grow (&token_stack, ch);
      while (1)
	{
	  ch = peek_input (false);
	  if (ch >= CHAR_EOF)
	    break;
	  obstack_1grow (&token_stack, ch);
	  if (re_match (&word_regexp, (char *) obstack_base (&token_stack),
			obstack_object_size (&token_stack), 0, &regs)
	      != obstack_object_size (&token_stack))
	    {
	      obstack_blank (&token_stack, -1);
	      break;
	    }
	  next_char (false, false);
	}

      TOKEN_DATA_ORIG_LEN (td) = obstack_object_size (&token_stack);
      obstack_1grow (&token_stack, '\0');
      orig_text = (char *) obstack_finish (&token_stack);
      TOKEN_DATA_ORIG_TEXT (td) = orig_text;

      if (regs.start[1] != -1)
	obstack_grow (&token_stack, orig_text + regs.start[1],
		      regs.end[1] - regs.start[1]);
      else
	obstack_grow (&token_stack, orig_text, regs.end[0]);

      type = TOKEN_WORD;
    }

#endif /* ENABLE_CHANGEWORD */

  else if (!MATCH (ch, curr_quote.str1, curr_quote.len1, true))
    {
      assert (ch < CHAR_EOF);
      switch (ch)
	{
	case '(':
	  type = TOKEN_OPEN;
	  break;
	case ',':
	  type = TOKEN_COMMA;
	  break;
	case ')':
	  type = TOKEN_CLOSE;
	  break;
	default:
	  type = TOKEN_SIMPLE;
	  break;
	}
      obstack_1grow (&token_stack, ch);
    }
  else
    {
      if (obs)
	obs_td = obs;
      quote_level = 1;
      type = TOKEN_STRING;
      while (1)
	{
	  /* Start with buffer search for either potential delimiter.  */
	  size_t len;
	  const char *buffer = next_buffer (&len, obs && current_quote_age);
	  if (buffer)
	    {
	      const char *p = buffer;
	      do
		{
		  p = (char *) memchr2 (p, *curr_quote.str1, *curr_quote.str2,
					buffer + len - p);
		}
	      while (p && current_quote_age
		     && (*p++ == *curr_quote.str2
			 ? --quote_level : ++quote_level));
	      if (p)
		{
		  if (current_quote_age)
		    {
		      assert (!quote_level);
		      obstack_grow (obs_td, buffer, p - buffer - 1);
		      consume_buffer (p - buffer);
		      break;
		    }
		  obstack_grow (obs_td, buffer, p - buffer);
		  ch = to_uchar (*p);
		  consume_buffer (p - buffer + 1);
		}
	      else
		{
		  obstack_grow (obs_td, buffer, len);
		  consume_buffer (len);
		  continue;
		}
	    }

	  /* Fall back to byte-wise search.  */
	  else
	    ch = next_char (obs && current_quote_age, false);
	  if (ch == CHAR_EOF)
	    {
	      /* Current_file changed to "" if we see CHAR_EOF, use
		 the previous value we stored earlier.  */
	      if (!caller)
		{
		  assert (line);
		  current_line = *line;
		  current_file = file;
		}
	      m4_error (EXIT_FAILURE, 0, caller, _("end of file in string"));
	    }
	  if (ch == CHAR_MACRO)
	    init_macro_token (obs, obs ? td : NULL);
	  else if (ch == CHAR_QUOTE)
	    append_quote_token (obs, td);
	  else if (MATCH (ch, curr_quote.str2, curr_quote.len2, true))
	    {
	      if (--quote_level == 0)
		break;
	      obstack_grow (obs_td, curr_quote.str2, curr_quote.len2);
	    }
	  else if (MATCH (ch, curr_quote.str1, curr_quote.len1, true))
	    {
	      quote_level++;
	      obstack_grow (obs_td, curr_quote.str1, curr_quote.len1);
	    }
	  else
	    {
	      assert (ch < CHAR_EOF);
	      obstack_1grow (obs_td, ch);
	    }
	}
    }

  if (TOKEN_DATA_TYPE (td) == TOKEN_VOID)
    {
      TOKEN_DATA_TYPE (td) = TOKEN_TEXT;
      TOKEN_DATA_LEN (td) = obstack_object_size (obs_td);
      if (obs_td != obs)
	{
	  obstack_1grow (obs_td, '\0');
	  TOKEN_DATA_TEXT (td) = (char *) obstack_finish (obs_td);
	}
      else
	TOKEN_DATA_TEXT (td) = NULL;
      TOKEN_DATA_QUOTE_AGE (td) = current_quote_age;
#ifdef ENABLE_CHANGEWORD
      if (!orig_text)
	{
	  TOKEN_DATA_ORIG_TEXT (td) = TOKEN_DATA_TEXT (td);
	  TOKEN_DATA_ORIG_LEN (td) = TOKEN_DATA_LEN (td);
	}
#endif /* ENABLE_CHANGEWORD */
#ifdef DEBUG_INPUT
      xfprintf (stderr, "next_token -> %s (%s), len %zu\n",
		token_type_string (type), TOKEN_DATA_TEXT (td),
		TOKEN_DATA_LEN (td));
#endif /* DEBUG_INPUT */
    }
  else
    {
      assert (TOKEN_DATA_TYPE (td) == TOKEN_COMP
	      && (type == TOKEN_STRING || type == TOKEN_COMMENT));
#ifdef DEBUG_INPUT
      {
	token_chain *chain;
	size_t len = 0;
	int links = 0;
	chain = td->u.u_c.chain;
	xfprintf (stderr, "next_token -> %s <chain> (",
		  token_type_string (type));
	while (chain)
	  {
	    switch (chain->type)
	      {
	      case CHAIN_STR:
		xfprintf (stderr, "%s", chain->u.u_s.str);
		len += chain->u.u_s.len;
		break;
	      case CHAIN_FUNC:
		xfprintf (stderr, "<func>");
		break;
	      case CHAIN_ARGV:
		xfprintf (stderr, "{$@}");
		break;
	      default:
		assert (!"next_token");
		abort ();
	      }
	    links++;
	    chain = chain->next;
	  }
	xfprintf (stderr, "), %d links, len %zu\n",
		  links, len);
      }
#endif /* DEBUG_INPUT */
    }
  return type;
}

/*-----------------------------------------------.
| Peek at the next token from the input stream.  |
`-----------------------------------------------*/

token_type
peek_token (void)
{
  token_type result;
  int ch = peek_input (false);

  if (ch == CHAR_EOF)
    {
      result = TOKEN_EOF;
    }
  else if (ch == CHAR_MACRO)
    {
      result = TOKEN_MACDEF;
    }
  else if (MATCH (ch, curr_comm.str1, curr_comm.len1, false))
    {
      result = TOKEN_COMMENT;
    }
  else if ((default_word_regexp && (isalpha (ch) || ch == '_'))
#ifdef ENABLE_CHANGEWORD
      || (!default_word_regexp && word_regexp.fastmap[ch])
#endif /* ENABLE_CHANGEWORD */
      )
    {
      result = TOKEN_WORD;
    }
  else if (MATCH (ch, curr_quote.str1, curr_quote.len1, false))
    {
      result = TOKEN_STRING;
    }
  else
    switch (ch)
      {
      case '(':
	result = TOKEN_OPEN;
	break;
      case ',':
	result = TOKEN_COMMA;
	break;
      case ')':
	result = TOKEN_CLOSE;
	break;
      default:
	result = TOKEN_SIMPLE;
      }

#ifdef DEBUG_INPUT
  xfprintf (stderr, "peek_token -> %s\n", token_type_string (result));
#endif /* DEBUG_INPUT */
  return result;
}


#ifdef DEBUG_INPUT

static const char *
token_type_string (token_type t)
{
 switch (t)
    {				/* TOKSW */
    case TOKEN_EOF:
      return "EOF";
    case TOKEN_STRING:
      return "STRING";
    case TOKEN_COMMENT:
      return "COMMENT";
    case TOKEN_WORD:
      return "WORD";
    case TOKEN_OPEN:
      return "OPEN";
    case TOKEN_COMMA:
      return "COMMA";
    case TOKEN_CLOSE:
      return "CLOSE";
    case TOKEN_SIMPLE:
      return "SIMPLE";
    case TOKEN_MACDEF:
      return "MACDEF";
    default:
      abort ();
    }
 }

static void
print_token (const char *s, token_type t, token_data *td)
{
  xfprintf (stderr, "%s: ", s);
  switch (t)
    {				/* TOKSW */
    case TOKEN_OPEN:
    case TOKEN_COMMA:
    case TOKEN_CLOSE:
    case TOKEN_SIMPLE:
      xfprintf (stderr, "char:");
      break;

    case TOKEN_WORD:
      xfprintf (stderr, "word:");
      break;

    case TOKEN_STRING:
      xfprintf (stderr, "string:");
      break;

    case TOKEN_COMMENT:
      xfprintf (stderr, "comment:");
      break;

    case TOKEN_MACDEF:
      xfprintf (stderr, "macro: %p\n", TOKEN_DATA_FUNC (td));
      break;

    case TOKEN_ARGV:
      xfprintf (stderr, "argv:");
      break;

    case TOKEN_EOF:
      xfprintf (stderr, "eof\n");
      break;
    }
  xfprintf (stderr, "\t\"%s\"\n", TOKEN_DATA_TEXT (td));
}

static void M4_GNUC_UNUSED
lex_debug (void)
{
  token_type t;
  token_data td;
  int line;

  while ((t = next_token (&td, &line, NULL, false, NULL)) != TOKEN_EOF)
    print_token ("lex", t, &td);
}
#endif /* DEBUG_INPUT */

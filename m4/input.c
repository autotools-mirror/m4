/* GNU m4 -- A simple macro processor
   Copyright 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307  USA
*/

/* Handling of different input sources, and lexical analysis.  */

#include <ctype.h>

#include "m4private.h"

#define DEBUG_INPUT
#undef DEBUG_INPUT

/*
   Unread input can be either files, that should be read (eg. included
   files), strings, which should be rescanned (eg. macro expansion
   text), single characters or quoted builtin definitions (as returned by
   the builtin "defn").  Unread input are organised in a stack,
   implemented with an obstack.  Each input source is described by a
   "struct input_block".  The obstack is "input_stack".  The top of the
   input stack is "isp".

   Each input_block has an associated struct input_funcs, that defines
   functions for peeking, reading, unget and cleanup.  All input is done
   through the functions pointers of the input_funcs of the top most
   input_block.  When a input_block is exausted, its reader returns
   CHAR_RETRY which causes the input_block to be popped from the
   input_stack.

   The macro "m4wrap" places the text to be saved on another input stack,
   on the obstack "wrapup_stack", whose top is "wsp".  When EOF is seen
   on normal input (eg, when "input_stack" is empty), input is switched
   over to "wrapup_stack".  To make this easier, all references to the
   current input stack, whether it be "input_stack" or "wrapup_stack",
   are done through a pointer "current_input", which points to either
   "input_stack" or "wrapup_stack".

   Pushing new input on the input stack is done by m4_push_file (),
   m4_push_string (), m4_push_single () or m4_push_wrapup () (for wrapup
   text), and m4_push_builtin () (for builtin definitions).  Because
   macro expansion needs direct access to the current input obstack (for
   optimisation), m4_push_string () are split in two functions,
   push_string_init (), which returns a pointer to the current input
   stack, and push_string_finish (), which return a pointer to the final
   text.  The input_block *next is used to manage the coordination
   between the different push routines.

   The current file and line number are stored in two global variables,
   for use by the error handling functions in m4.c.  Whenever a file
   input_block is pushed, the current file name and line number is saved
   in the input_block, and the two variables are reset to match the new
   input file.  */

static	int   file_peek			(void);
static	int   file_read			(void);
static	void  file_unget		(int ch);
static	void  file_clean		(void);
static	void  init_builtin_token	(m4_token *td);
static	int   builtin_peek		(void);
static	int   builtin_read		(void);
static	int   match_input		(const unsigned char *s);
static	int   next_char			(void);
static	void  pop_input			(void);
static	int   single_peek		(void);
static	int   single_read		(void);
static	int   string_peek		(void);
static	int   string_read		(void);
static	void  string_unget		(int ch);
static	void  unget_input		(int ch);

struct input_funcs
{
  int (*peek_func) (void);	/* function to peek input */
  int (*read_func) (void);	/* function to read input */
  void (*unget_func) (int);	/* function to unread input */
  void (*clean_func) (void);	/* function to clean up */
};

struct input_block
{
  struct input_block *prev;	/* previous input_block on the input stack */
  struct input_funcs *funcs;	/* functions on this input_block */

  union
    {
      struct
	{
	  unsigned int ch;	/* single char value */
	}
      u_c;
      struct
	{
	  unsigned char *start; /* string value */
	  unsigned char *current; /* current value */
	}
      u_s;
      struct
	{
	  FILE *file;		/* input file handle */
	  const char *name;	/* name of PREVIOUS input file */
	  int lineno;		/* current line number for do */
	  /* Yet another attack of "The curse of global variables" (sigh) */
	  int out_lineno;	/* current output line number do */
	  boolean advance_line;	/* start_of_input_line from next_char () */
	}
      u_f;
      struct
	{
	  m4_builtin_func *func;  /* pointer to builtins function. */
	  lt_dlhandle handle;	  /* originating module. */
	  int flags;		  /* flags associated with the builtin. */
	  m4_hash *arg_signature; /* argument signature for builtin.  */
	  int min_args, max_args; /* argv maxima and minima for the builtin. */
	  boolean traced;	  /* TRUE iff builtin is traced. */
	  boolean read;		  /* TRUE iff block has been read. */
	}
      u_b;
    }
  u;
};

typedef struct input_block input_block;


/* Current input file name.  */
const char *m4_current_file;

/* Current input line number.  */
int m4_current_line;

/* Obstack for storing individual tokens.  */
static struct obstack token_stack;

/* Normal input stack.  */
static struct obstack input_stack;

/* Wrapup input stack.  */
static struct obstack wrapup_stack;

/* Input or wrapup.  */
static struct obstack *current_input;

/* Bottom of token_stack, for obstack_free.  */
static char *token_bottom;

/* Pointer to top of current_input.  */
static input_block *isp;

/* Pointer to top of wrapup_stack.  */
static input_block *wsp;

/* Aux. for handling split m4_push_string ().  */
static input_block *next;

/* Flag for next_char () to increment m4_current_line.  */
static boolean start_of_input_line;

#define CHAR_EOF	256	/* character return on EOF */
#define CHAR_BUILTIN	257	/* character return for BUILTIN token */
#define CHAR_RETRY	258	/* character return for end of input block */



/* m4_push_file () pushes an input file on the input stack, saving the
  current file name and line number.  If next is non-NULL, this push
  invalidates a call to m4_push_string_init (), whose storage are
  consequentely released.

  file_read () manages line numbers for error messages, so they do not
  get wrong, due to lookahead.  The token consisting of a newline
  alone is taken as belonging to the line it ends, and the current
  line number is not incremented until the next character is read.  */
static int
file_peek (void)
{
  int ch;

  ch = getc (isp->u.u_f.file);
  if (ch == EOF)
    return CHAR_RETRY;

  ungetc (ch, isp->u.u_f.file);
  return ch;
}

static int
file_read (void)
{
  int ch;

  if (start_of_input_line)
    {
      start_of_input_line = FALSE;
      m4_current_line++;
    }

  ch = getc (isp->u.u_f.file);
  if (ch == EOF)
    return CHAR_RETRY;

  if (ch == '\n')
    start_of_input_line = TRUE;
  return ch;
}

static void
file_unget (ch)
     int ch;
{
  ungetc (ch, isp->u.u_f.file);
  if (ch == '\n')
    start_of_input_line = FALSE;
}

static void
file_clean (void)
{
  if (debug_level & M4_DEBUG_TRACE_INPUT)
    M4_DEBUG_MESSAGE2 (_("Input reverted to %s, line %d"),
		    isp->u.u_f.name, isp->u.u_f.lineno);

  fclose (isp->u.u_f.file);
  m4_current_file = isp->u.u_f.name;
  m4_current_line = isp->u.u_f.lineno;
  m4_output_current_line = isp->u.u_f.out_lineno;
  start_of_input_line = isp->u.u_f.advance_line;
  if (isp->prev != NULL)
    m4_output_current_line = -1;
}

static struct input_funcs file_funcs = {
  file_peek, file_read, file_unget, file_clean
};

void
m4_push_file (FILE *fp, const char *title)
{
  input_block *i;

  if (next != NULL)
    {
      obstack_free (current_input, next);
      next = NULL;
    }

  if (debug_level & M4_DEBUG_TRACE_INPUT)
    M4_DEBUG_MESSAGE1 (_("Input read from %s"), title);

  i = (input_block *) obstack_alloc (current_input,
				     sizeof (struct input_block));
  i->funcs = &file_funcs;

  i->u.u_f.file = fp;
  i->u.u_f.name = m4_current_file;
  i->u.u_f.lineno = m4_current_line;
  i->u.u_f.out_lineno = m4_output_current_line;
  i->u.u_f.advance_line = start_of_input_line;

  m4_current_file = obstack_copy0 (current_input, title, strlen (title));
  m4_current_line = 1;
  m4_output_current_line = -1;

  i->prev = isp;
  isp = i;
}

/* m4_push_builtin () pushes a builtins definition on the input stack.  If
   next is non-NULL, this push invalidates a call to m4_push_string_init (),
   whose storage are consequentely released.  */
static int
builtin_peek (void)
{
  if (isp->u.u_b.read == TRUE)
    return CHAR_RETRY;

  return CHAR_BUILTIN;
}

static int
builtin_read (void)
{
  if (isp->u.u_b.read == TRUE)
    return CHAR_RETRY;

  isp->u.u_b.read = TRUE;
  return CHAR_BUILTIN;
}

static struct input_funcs builtin_funcs = {
  builtin_peek, builtin_read, NULL, NULL
};

void
m4_push_builtin (m4_token *td)
{
  input_block *i;

  /* Make sure we were passed a builtin function type token.  */
  assert (TOKEN_TYPE (td) == M4_TOKEN_FUNC);

  if (next != NULL)
    {
      obstack_free (current_input, next);
      next = NULL;
    }

  i = (input_block *) obstack_alloc (current_input,
				     sizeof (struct input_block));
  i->funcs = &builtin_funcs;

  i->u.u_b.func		= TOKEN_FUNC (td);
  i->u.u_b.handle	= TOKEN_HANDLE (td);
  i->u.u_b.arg_signature= TOKEN_ARG_SIGNATURE (td);
  i->u.u_b.min_args	= TOKEN_MIN_ARGS (td);
  i->u.u_b.max_args	= TOKEN_MAX_ARGS (td);
  i->u.u_b.flags	= TOKEN_FLAGS (td);
  i->u.u_b.read		= FALSE;

  i->prev = isp;
  isp = i;
}

/* Push a single character on to the input stack.  */
static int
single_peek (void)
{
  return isp->u.u_c.ch;
}

static int
single_read (void)
{
  int ch = isp->u.u_c.ch;

  if (ch != CHAR_RETRY)
    isp->u.u_c.ch = CHAR_RETRY;

  return ch;
}

static struct input_funcs single_funcs = {
  single_peek, single_read, NULL, NULL
};

void
m4_push_single (int ch)
{
  input_block *i;

  if (next != NULL)
    {
      obstack_free (current_input, next);
      next = NULL;
    }

  i = (input_block *) obstack_alloc (current_input,
				     sizeof (struct input_block));

  i->funcs = &single_funcs;

  i->u.u_c.ch = ch;

  i->prev = isp;
  isp = i;
}

/* First half of m4_push_string ().  The pointer next points to the new
   input_block.  */
static int
string_peek (void)
{
  int ch = *isp->u.u_s.current;

  return (ch == '\0') ? CHAR_RETRY : ch;
}

static int
string_read (void)
{
  int ch = *isp->u.u_s.current++;

  return (ch == '\0') ? CHAR_RETRY : ch;

}

static void
string_unget (int ch)
{
  if (isp->u.u_s.current > isp->u.u_s.start)
    *--isp->u.u_s.current = ch;
  else
    m4_push_single(ch);
}

static struct input_funcs string_funcs = {
  string_peek, string_read, string_unget, NULL
};

struct obstack *
m4_push_string_init (void)
{
  if (next != NULL)
    {
      M4ERROR ((warning_status, 0,
		"INTERNAL ERROR: Recursive m4_push_string!"));
      abort ();
    }

  next = (input_block *) obstack_alloc (current_input,
				        sizeof (struct input_block));
  next->funcs = &string_funcs;

  return current_input;
}

/* Last half of m4_push_string ().  If next is now NULL, a call to
   m4_push_file () has invalidated the previous call to
   m4_push_string_init (), so we just give up.  If the new object is
   void, we do not push it.  The function m4_push_string_finish ()
   returns a pointer to the finished object.  This pointer is only for
   temporary use, since reading the next token might release the memory
   used for the object.  */

const char *
m4_push_string_finish (void)
{
  const char *ret = NULL;

  if (next == NULL)
    return NULL;

  if (obstack_object_size (current_input) > 0)
    {
      obstack_1grow (current_input, '\0');
      next->u.u_s.start = obstack_finish (current_input);
      next->u.u_s.current = next->u.u_s.start;
      next->prev = isp;
      isp = next;
      ret = isp->u.u_s.start;	/* for immediate use only */
    }
  else
    obstack_free (current_input, next); /* people might leave garbage on it. */
  next = NULL;
  return ret;
}

/* The function m4_push_wrapup () pushes a string on the wrapup stack.
   When the normal input stack gets empty, the wrapup stack will become
   the input stack, and m4_push_string () and m4_push_file () will
   operate on wrapup_stack.  M4_push_wrapup should be done as
   m4_push_string (), but this will suffice, as long as arguments to
   m4_m4wrap () are moderate in size.  */
void
m4_push_wrapup (const char *s)
{
  input_block *i = (input_block *) obstack_alloc (&wrapup_stack,
						  sizeof (struct input_block));
  i->prev = wsp;

  i->funcs = &string_funcs;

  i->u.u_s.start = obstack_copy0 (&wrapup_stack, s, strlen (s));
  i->u.u_s.current = i->u.u_s.start;

  wsp = i;
}


/* The function pop_input () pops one level of input sources.  If the
   popped input_block is a file, m4_current_file and m4_current_line are
   reset to the saved values before the memory for the input_block are
   released.  */
static void
pop_input (void)
{
  input_block *tmp = isp->prev;

  if (isp->funcs->clean_func != NULL)
    (*isp->funcs->clean_func)();

  obstack_free (current_input, isp);
  next = NULL;			/* might be set in m4_push_string_init () */

  isp = tmp;
}

/* To switch input over to the wrapup stack, main () calls pop_wrapup
   Since wrapup text can install new wrapup text, pop_wrapup () returns
   FALSE when there is no wrapup text on the stack, and TRUE otherwise.  */
boolean
m4_pop_wrapup (void)
{
  if (wsp == NULL)
    return FALSE;

  current_input = &wrapup_stack;
  isp = wsp;
  wsp = NULL;

  return TRUE;
}

/* When a BUILTIN token is seen, next_token () uses init_builtin_token
   to retrieve the value of the function pointer.  */
static void
init_builtin_token (m4_token *td)
{
  if (isp->funcs->read_func != builtin_read)
    {
      M4ERROR ((warning_status, 0,
		"INTERNAL ERROR: Bad call to init_builtin_token ()"));
      abort ();
    }

  TOKEN_TYPE (td)		= M4_TOKEN_FUNC;
  TOKEN_FUNC (td)		= isp->u.u_b.func;
  TOKEN_HANDLE (td)		= isp->u.u_b.handle;
  TOKEN_FLAGS (td)		= isp->u.u_b.flags;
  TOKEN_ARG_SIGNATURE(td)	= isp->u.u_b.arg_signature;
  TOKEN_MIN_ARGS (td)		= isp->u.u_b.min_args;
  TOKEN_MAX_ARGS (td)		= isp->u.u_b.max_args;
}


/* Low level input is done a character at a time.  The function
   next_char () is used to read and advance the input to the next
   character.  */
static int
next_char (void)
{
  int ch;
  int (*f) (void);

  while (1)
    {
      if (isp == NULL)
	return CHAR_EOF;

      f = isp->funcs->read_func;
      if (f != NULL)
	{
	  while ((ch = (*f)()) != CHAR_RETRY)
	    {
	      /* if (!IS_IGNORE(ch)) */
		return ch;
	    }
	}
      else
	{
	  M4ERROR ((warning_status, 0,
		    "INTERNAL ERROR: Input stack botch in next_char ()"));
	  abort ();
	}

      /* End of input source --- pop one level.  */
      pop_input ();
    }
}

/* The function m4_peek_input () is used to look at the next character in
   the input stream.  At any given time, it reads from the input_block
   on the top of the current input stack.  */
int
m4_peek_input (void)
{
  int ch;
  int (*f) (void);

  while (1)
    {
      if (isp == NULL)
	return CHAR_EOF;

      f = isp->funcs->peek_func;
      if (f != NULL)
	{
	  if ((ch = (*f)()) != CHAR_RETRY)
	    {
	      return /* (IS_IGNORE(ch)) ? next_char () : */ ch;
	    }
	}
      else
	{
	  M4ERROR ((warning_status, 0,
		    "INTERNAL ERROR: Input stack botch in m4_peek_input ()"));
	  abort ();
	}

      /* End of input source --- pop one level.  */
      pop_input ();
    }
}

/* The function unget_input () puts back a character on the input
   stack, using an existing input_block if possible.  */
static void
unget_input (int ch)
{
  if (isp != NULL && isp->funcs->unget_func != NULL)
    (*isp->funcs->unget_func)(ch);
  else
    m4_push_single(ch);
}

/* skip_line () simply discards all immediately following characters, upto
   the first newline.  It is only used from m4_dnl ().  */
void
m4_skip_line (void)
{
  int ch;

  while ((ch = next_char ()) != CHAR_EOF && ch != '\n')
    ;
}



/* This function is for matching a string against a prefix of the
   input stream.  If the string matches the input, the input is
   discarded, otherwise the characters read are pushed back again.
   The function is used only when multicharacter quotes or comment
   delimiters are used.

   All strings herein should be unsigned.  Otherwise sign-extension
   of individual chars might break quotes with 8-bit chars in it.  */
static int
match_input (const unsigned char *s)
{
  int n;			/* number of characters matched */
  int ch;			/* input character */
  const unsigned char *t;
  struct obstack *st;

  ch = m4_peek_input ();
  if (ch != *s)
    return 0;			/* fail */
  (void) next_char ();

  if (s[1] == '\0')
    return 1;			/* short match */

  for (n = 1, t = s++; (ch = m4_peek_input ()) == *s++; n++)
    {
      (void) next_char ();
      if (*s == '\0')		/* long match */
	return 1;
    }

  /* Failed, push back input.  */
  st = m4_push_string_init ();
  obstack_grow (st, t, n);
  m4_push_string_finish ();
  return 0;
}

/* The macro MATCH() is used to match a string against the input.  The
  first character is handled inline, for speed.  Hopefully, this will not
  hurt efficiency too much when single character quotes and comment
  delimiters are used.  */
#define MATCH(ch, s) \
  ((s)[0] == (ch) \
   && (ch) != '\0' \
   && ((s)[1] == '\0' \
       || (match_input ((s) + 1) ? (ch) = m4_peek_input (), 1 : 0)))



/* Inititialise input stacks, and quote/comment characters.  */
void
m4_input_init (void)
{
  m4_current_file = _("NONE");
  m4_current_line = 0;

  obstack_init (&token_stack);
  obstack_init (&input_stack);
  obstack_init (&wrapup_stack);

  current_input = &input_stack;

  obstack_1grow (&token_stack, '\0');
  token_bottom = obstack_finish (&token_stack);

  isp = NULL;
  wsp = NULL;
  next = NULL;

  start_of_input_line = FALSE;

  lquote.string = xstrdup (DEF_LQUOTE);
  lquote.length = strlen (lquote.string);
  rquote.string = xstrdup (DEF_RQUOTE);
  rquote.length = strlen (rquote.string);
  m4__single_quotes = TRUE;

  bcomm.string = xstrdup (DEF_BCOMM);
  bcomm.length = strlen (bcomm.string);
  ecomm.string = xstrdup (DEF_ECOMM);
  ecomm.length = strlen (ecomm.string);
  m4__single_comments = TRUE;

  m4__use_macro_escape = FALSE;
}

 void
m4_input_exit (void)
{
  XFREE (lquote.string);
  XFREE (rquote.string);
  XFREE (bcomm.string);
  XFREE (ecomm.string);
  obstack_free (&wrapup_stack, NULL);
  obstack_free (&input_stack, NULL);
  obstack_free (&token_stack, NULL);
}



/* Parse and return a single token from the input stream.  A token can
   either be M4_TOKEN_EOF, if the input_stack is empty; it can be
   M4_TOKEN_STRING for a quoted string; M4_TOKEN_WORD for something that
   is a potential macro name; and M4_TOKEN_SIMPLE for any single character
   that is not a part of any of the previous types.

   M4_next_token () returns the token type, and passes back a pointer to the
   token data through TD.  The token text is collected on the obstack
   token_stack, which never contains more than one token text at a time.
   The storage pointed to by the fields in TD is therefore subject to
   change the next time next_token () is called.	 */
m4_token_t
m4_next_token (m4_token *td)
{
  int ch;
  int quote_level;
  m4_token_t type;

  do {
    obstack_free (&token_stack, token_bottom);
    obstack_1grow (&token_stack, '\0');
    token_bottom = obstack_finish (&token_stack);

    ch = m4_peek_input ();
    if (ch == CHAR_EOF)			/* EOF */
      {
#ifdef DEBUG_INPUT
	fprintf (stderr, "next_token -> EOF\n");
#endif
	return M4_TOKEN_EOF;
      }

    if (ch == CHAR_BUILTIN)		/* BUILTIN TOKEN */
      {
	init_builtin_token (td);
	(void) next_char ();
#ifdef DEBUG_INPUT
	print_token ("next_token", M4_TOKEN_MACDEF, td);
#endif
	return M4_TOKEN_MACDEF;
      }

    (void) next_char ();
    if (M4_IS_BCOMM(ch))			/* COMMENT, SHORT DELIM */
      {
	obstack_1grow (&token_stack, ch);
	while ((ch = next_char ()) != CHAR_EOF && !M4_IS_ECOMM(ch))
	  obstack_1grow (&token_stack, ch);
	if (ch != CHAR_EOF)
	  obstack_1grow (&token_stack, ch);
	type = discard_comments ? M4_TOKEN_NONE : M4_TOKEN_STRING;
      }
					/* COMMENT, LONGER DELIM */
    else if (!m4__single_comments && MATCH (ch, bcomm.string))
      {
	obstack_grow (&token_stack, bcomm.string, bcomm.length);
	while ((ch = next_char ()) != CHAR_EOF && !MATCH (ch, ecomm.string))
	  obstack_1grow (&token_stack, ch);
	if (ch != CHAR_EOF)
	  obstack_grow (&token_stack, ecomm.string, ecomm.length);
	type = discard_comments ? M4_TOKEN_NONE : M4_TOKEN_STRING;
      }
    else if (M4_IS_ESCAPE(ch))		/* ESCAPED WORD */
      {
	obstack_1grow (&token_stack, ch);
	if ((ch = next_char ()) != CHAR_EOF)
	  {
	    if (M4_IS_ALPHA(ch))
	      {
		obstack_1grow (&token_stack, ch);
		while ((ch = next_char ()) != CHAR_EOF && (M4_IS_ALNUM(ch)))
		  {
		    obstack_1grow (&token_stack, ch);
		  }

		if (ch != CHAR_EOF)
		  unget_input(ch);
	      }
	    else
	      {
		obstack_1grow (&token_stack, ch);
	      }

	    type = M4_TOKEN_WORD;
	  }
	else
	  {
	    type = M4_TOKEN_SIMPLE;	/* escape before eof */
	  }
      }
    else if (M4_IS_ALPHA (ch))
      {
	obstack_1grow (&token_stack, ch);
	while ((ch = next_char ()) != CHAR_EOF && (M4_IS_ALNUM(ch)))
	  {
	    obstack_1grow (&token_stack, ch);
	  }
	if (ch != CHAR_EOF)
	  unget_input(ch);

	type = m4__use_macro_escape ? M4_TOKEN_STRING : M4_TOKEN_WORD;
      }
    else if (M4_IS_LQUOTE(ch))		/* QUOTED STRING, SINGLE QUOTES */
      {
	const char *current_file = m4_current_file;
	int current_line = m4_current_line;
	quote_level = 1;
	while (1)
	  {
	    ch = next_char ();
	    if (ch == CHAR_EOF)
	      error_at_line (EXIT_FAILURE, 0,
			      current_file, current_line,
			      _("EOF in string"));

	    if (M4_IS_RQUOTE(ch))
	      {
		if (--quote_level == 0)
		  break;
		obstack_1grow (&token_stack, ch);
	      }
	    else if (M4_IS_LQUOTE(ch))
	      {
		quote_level++;
		obstack_1grow (&token_stack, ch);
	      }
	    else
	      obstack_1grow (&token_stack, ch);
	  }
	type = M4_TOKEN_STRING;
      }
					/* QUOTED STRING, LONGER QUOTES */
    else if (!m4__single_quotes && MATCH (ch, lquote.string))
      {
	const char *current_file = m4_current_file;
	int current_line = m4_current_line;
	quote_level = 1;
	while (1)
	  {
	    ch = next_char ();
	    if (ch == CHAR_EOF)
	      error_at_line (EXIT_FAILURE, 0,
			      current_file, current_line,
			      _("EOF in string"));
	    if (MATCH (ch, rquote.string))
	      {
		if (--quote_level == 0)
		  break;
		obstack_grow (&token_stack, rquote.string, rquote.length);
	      }
	    else if (MATCH (ch, lquote.string))
	      {
		quote_level++;
		obstack_grow (&token_stack, lquote.string, lquote.length);
	      }
	    else
	      obstack_1grow (&token_stack, ch);
	  }
	type = M4_TOKEN_STRING;
      }
    else if (m4__single_quotes && m4__single_comments) /* EVERYTHING ELSE */
      {
	obstack_1grow (&token_stack, ch);

	if (M4_IS_OTHER(ch) || M4_IS_NUM(ch))
	  {
	    while ((ch = next_char()) != CHAR_EOF
		   && (M4_IS_OTHER(ch) || M4_IS_NUM(ch)))
	      obstack_1grow (&token_stack, ch);

	    if (ch != CHAR_EOF)
	      unget_input(ch);
	    type = M4_TOKEN_STRING;
	  }
	else if (M4_IS_SPACE(ch))
	  {
	    if (!interactive)
	      {
		while ((ch = next_char()) != CHAR_EOF && M4_IS_SPACE(ch))
		  obstack_1grow (&token_stack, ch);

		if (ch != CHAR_EOF)
		  unget_input(ch);
	      }
	    type = M4_TOKEN_SPACE;
	  }
	else if (M4_IS_ACTIVE(ch))
	  type = M4_TOKEN_WORD;
	else
	  type = M4_TOKEN_SIMPLE;
      }
    else				/* EVERYTHING ELSE */
      {
	obstack_1grow (&token_stack, ch);

	if (M4_IS_OTHER(ch) || M4_IS_NUM(ch))
	  type = M4_TOKEN_STRING;
	else if (M4_IS_SPACE(ch))
	  type = M4_TOKEN_SPACE;
	else if (M4_IS_ACTIVE(ch))
	  type = M4_TOKEN_WORD;
	else
	  type = M4_TOKEN_SIMPLE;
      }
  } while (type == M4_TOKEN_NONE);

  obstack_1grow (&token_stack, '\0');

  bzero (td, sizeof (m4_token));

  TOKEN_TYPE (td)	= M4_TOKEN_TEXT;
  TOKEN_TEXT (td)	= obstack_finish (&token_stack);
  TOKEN_MIN_ARGS (td)	= -1;
  TOKEN_MAX_ARGS (td)	= -1;

#ifdef DEBUG_INPUT
  print_token("next_token", type, td);
#endif

  return type;
}

void
m4_token_copy (m4_token *dest, m4_token *src)
{
  TOKEN_TYPE (dest)	= TOKEN_TYPE (src);
  TOKEN_FUNC (dest)	= TOKEN_FUNC (src);
  TOKEN_HANDLE (dest)   = TOKEN_HANDLE (src);
  TOKEN_FLAGS (dest)    = TOKEN_FLAGS (src);
  TOKEN_MIN_ARGS (dest) = TOKEN_MIN_ARGS (src);
  TOKEN_MAX_ARGS (dest) = TOKEN_MAX_ARGS (src);
}


#ifdef DEBUG_INPUT

static	void  lex_debug	(void);

int
m4_print_token (const char *s, m4_token_t t, m4_token *td)
{
  fprintf (stderr, "%s: ", s);
  switch (t)
    {				/* TOKSW */
    case M4_TOKEN_SIMPLE:
      fprintf (stderr,	"char\t\"%s\"\n",	TOKEN_TEXT (td));
      break;

    case M4_TOKEN_WORD:
      fprintf (stderr,	"word\t\"%s\"\n",	TOKEN_TEXT (td));
      break;

    case M4_TOKEN_STRING:
      fprintf (stderr,	"string\t\"%s\"\n",	TOKEN_TEXT (td));
      break;

    case M4_TOKEN_SPACE:
      fprintf (stderr,	"space\t\"%s\"\n",	TOKEN_TEXT (td));
      break;

    case M4_TOKEN_MACDEF:
      fprintf (stderr,	"builtin 0x%x\n", 	(int) TOKEN_FUNC (td));
      break;

    case M4_TOKEN_EOF:
      fprintf (stderr,	"eof\n");
      break;

    case M4_TOKEN_NONE:
      fprintf (stderr,	"none\n");
      break;
    }
  return 0;
}

static void
lex_debug (void)
{
  m4_token_t t;
  m4_token td;

  while ((t = next_token (&td)) != NULL)
    print_token ("lex", t, &td);
}
#endif

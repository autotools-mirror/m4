/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

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
   push_string (), push_wrapup () (for wrapup text), and push_macro ()
   (for macro definitions).  Because macro expansion needs direct
   access to the current input obstack (for optimization), push_string
   () is split in two functions, push_string_init (), which returns a
   pointer to the current input stack, and push_string_finish (),
   which returns a pointer to the final text.  The input_block *next
   is used to manage the coordination between the different push
   routines.

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

/* Type of an input block.  */
enum input_type
{
  INPUT_STRING,		/* String resulting from macro expansion.  */
  INPUT_FILE,		/* File from command line or include.  */
  INPUT_MACRO,		/* Builtin resulting from defn.  */
  INPUT_CHAIN		/* FIFO chain of separate strings and $@ refs.  */
};

typedef enum input_type input_type;

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
      builtin_func *func;	/* INPUT_MACRO */
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

/* Pointer to top of current_input.  */
static input_block *isp;

/* Pointer to top of wrapup_stack.  */
static input_block *wsp;

/* Aux. for handling split push_string ().  */
static input_block *next;

/* Flag for next_char () to increment current_line.  */
static bool start_of_input_line;

/* Flag for next_char () to recognize change in input block.  */
static bool input_change;

#define CHAR_EOF	256	/* Character return on EOF.  */
#define CHAR_MACRO	257	/* Character return for MACRO token.  */

/* Quote chars.  */
STRING rquote;
STRING lquote;

/* Comment chars.  */
STRING bcomm;
STRING ecomm;

#ifdef ENABLE_CHANGEWORD

# define DEFAULT_WORD_REGEXP "[_a-zA-Z][_a-zA-Z0-9]*"

/* Table of characters that can start a word.  */
static char *word_start;

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
static void
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
      chain->str = str;
      chain->len = len;
      chain->level = -1;
      chain->argv = NULL;
      chain->index = 0;
      chain->flatten = false;
    }
}

/*-------------------------------------------------------------------.
| push_file () pushes an input file on the input stack, saving the   |
| current file name and line number.  If next is non-NULL, this push |
| invalidates a call to push_string_init (), whose storage is        |
| consequently released.  If CLOSE, then close FP after EOF is       |
| detected.  TITLE is used as the location for text parsed from the  |
| file (not necessarily the file name).                              |
`-------------------------------------------------------------------*/

void
push_file (FILE *fp, const char *title, bool close)
{
  input_block *i;

  if (next != NULL)
    {
      obstack_free (current_input, next);
      next = NULL;
    }

  if (debug_level & DEBUG_TRACE_INPUT)
    DEBUG_MESSAGE1 ("input read from %s", title);

  i = (input_block *) obstack_alloc (current_input, sizeof *i);
  i->type = INPUT_FILE;
  i->file = (char *) obstack_copy0 (&file_names, title, strlen (title));
  i->line = 1;
  input_change = true;

  i->u.u_f.fp = fp;
  i->u.u_f.end = false;
  i->u.u_f.close = close;
  i->u.u_f.advance = start_of_input_line;
  output_current_line = -1;

  i->prev = isp;
  isp = i;
}

/*-----------------------------------------------------------------.
| push_macro () pushes the builtin macro FUNC on the input stack.  |
| If next is non-NULL, this push invalidates a call to             |
| push_string_init (), whose storage is consequently released.     |
`-----------------------------------------------------------------*/

void
push_macro (builtin_func *func)
{
  input_block *i;

  if (next != NULL)
    {
      obstack_free (current_input, next);
      next = NULL;
    }

  i = (input_block *) obstack_alloc (current_input, sizeof *i);
  i->type = INPUT_MACRO;
  i->file = current_file;
  i->line = current_line;
  input_change = true;

  i->u.func = func;
  i->prev = isp;
  isp = i;
}

/*--------------------------------------------------------------.
| First half of push_string ().  The return value points to the |
| obstack where expansion text should be placed.                |
`--------------------------------------------------------------*/

struct obstack *
push_string_init (void)
{
  /* Free any memory occupied by completely parsed strings.  */
  assert (next == NULL);
  while (isp && pop_input (false));

  /* Reserve the next location on the obstack.  */
  next = (input_block *) obstack_alloc (current_input, sizeof *next);
  next->type = INPUT_STRING;
  next->file = current_file;
  next->line = current_line;

  return current_input;
}

/*-------------------------------------------------------------------.
| If TOKEN contains text, then convert the current string into a     |
| chain if it is not one already, and add the contents of TOKEN as a |
| new link in the chain.  LEVEL describes the current expansion      |
| level, or -1 if the contents of TOKEN reside entirely on the       |
| current_input stack and TOKEN lives in temporary storage.  Allows  |
| gathering input from multiple locations, rather than copying       |
| everything consecutively onto the input stack.  Must be called     |
| between push_string_init and push_string_finish.  Return true only |
| if LEVEL is non-negative, and a reference was created to TOKEN.    |
`-------------------------------------------------------------------*/
bool
push_token (token_data *token, int level)
{
  token_chain *chain;

  assert (next);
  // TODO - also accept TOKEN_COMP chains
  assert (TOKEN_DATA_TYPE (token) == TOKEN_TEXT);
  if (TOKEN_DATA_LEN (token) == 0)
    return false;

  if (next->type == INPUT_STRING)
    {
      next->type = INPUT_CHAIN;
      next->u.u_c.chain = next->u.u_c.end = NULL;
    }
  make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);
  chain = (token_chain *) obstack_alloc (current_input, sizeof *chain);
  if (next->u.u_c.end)
    next->u.u_c.end->next = chain;
  else
    next->u.u_c.chain = chain;
  next->u.u_c.end = chain;
  chain->next = NULL;
  if (level >= 0)
    // TODO - use token as-is, rather than copying data.  This implies
    // lengthening lifetime of $@ arguments until the rescan is complete,
    // rather than the current approach of freeing them during expand_macro
    chain->str = (char *) obstack_copy (current_input, TOKEN_DATA_TEXT (token),
					TOKEN_DATA_LEN (token));
  else
    chain->str = TOKEN_DATA_TEXT (token);
  chain->len = TOKEN_DATA_LEN (token);
  chain->level = -1;
  // chain->level = level;
  chain->argv = NULL;
  chain->index = 0;
  chain->flatten = false;
  return false; // no reference exists when text is copied
  // return true;
}

/*-------------------------------------------------------------------.
| Last half of push_string ().  If next is now NULL, a call to       |
| push_file () or push_macro () has invalidated the previous call to |
| push_string_init (), so we just give up.  If the new object is     |
| void, we do not push it.  The function push_string_finish ()       |
| returns an opaque pointer to the finished object, which can then   |
| be printed with input_print when tracing is enabled.  This pointer |
| is only for temporary use, since reading the next token will       |
| invalidate the object.                                             |
`-------------------------------------------------------------------*/

const input_block *
push_string_finish (void)
{
  input_block *ret = NULL;
  size_t len = obstack_object_size (current_input);

  if (next == NULL)
    {
      assert (!len);
      return NULL;
    }

  if (len || next->type == INPUT_CHAIN)
    {
      if (next->type == INPUT_STRING)
	{
	  next->u.u_s.str = (char *) obstack_finish (current_input);
	  next->u.u_s.len = len;
	}
      else
	make_text_link (current_input, &next->u.u_c.chain, &next->u.u_c.end);
      next->prev = isp;
      isp = next;
      input_change = true;
      ret = isp;
    }
  else
    obstack_free (current_input, next);
  next = NULL;
  return ret;
}

/*------------------------------------------------------------------.
| The function push_wrapup () pushes a string on the wrapup stack.  |
| When the normal input stack gets empty, the wrapup stack will     |
| become the input stack, and push_string () and push_file () will  |
| operate on wrapup_stack.  Push_wrapup should be done as           |
| push_string (), but this will suffice, as long as arguments to    |
| m4_m4wrap () are moderate in size.                                |
`------------------------------------------------------------------*/

void
push_wrapup (const char *s)
{
  input_block *i;
  i = (input_block *) obstack_alloc (wrapup_stack, sizeof *i);
  i->prev = wsp;
  i->type = INPUT_STRING;
  i->file = current_file;
  i->line = current_line;
  i->u.u_s.len = strlen (s);
  i->u.u_s.str = (char *) obstack_copy (wrapup_stack, s, i->u.u_s.len);
  wsp = i;
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

    case INPUT_MACRO:
      if (!cleanup)
	return false;
      break;

    case INPUT_CHAIN:
      chain = isp->u.u_c.chain;
      assert (!chain || !cleanup);
      while (chain)
	{
	  if (chain->str)
	    {
	      if (chain->len)
		return false;
	    }
	  else
	    {
	      // TODO - peek into argv
	      assert (!"implemented yet");
	      abort ();
	    }
	  if (chain->level >= 0)
	    adjust_refcount (chain->level, false);
	  isp->u.u_c.chain = chain = chain->next;
	}
      break;

    case INPUT_FILE:
      if (!cleanup)
	return false;
      if (debug_level & DEBUG_TRACE_INPUT)
	{
	  if (tmp)
	    DEBUG_MESSAGE2 ("input reverted to %s, line %d",
			    tmp->file, tmp->line);
	  else
	    DEBUG_MESSAGE ("input exhausted");
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

    default:
      assert (!"pop_input");
      abort ();
    }
  obstack_free (current_input, isp);
  next = NULL;			/* might be set in push_string_init () */

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
  next = NULL;
  obstack_free (current_input, NULL);
  free (current_input);

  if (wsp == NULL)
    {
      /* End of the program.  Free all memory even though we are about
	 to exit, since it makes leak detection easier.  */
      obstack_free (&token_stack, NULL);
      obstack_free (&file_names, NULL);
      obstack_free (wrapup_stack, NULL);
      free (wrapup_stack);
      return false;
    }

  current_input = wrapup_stack;
  wrapup_stack = (struct obstack *) xmalloc (sizeof *wrapup_stack);
  obstack_init (wrapup_stack);

  isp = wsp;
  wsp = NULL;
  input_change = true;

  return true;
}

/*-------------------------------------------------------------------.
| When a MACRO token is seen, next_token () uses init_macro_token () |
| to retrieve the value of the function pointer and store it in TD.  |
`-------------------------------------------------------------------*/

static void
init_macro_token (token_data *td)
{
  assert (isp->type == INPUT_MACRO);
  TOKEN_DATA_TYPE (td) = TOKEN_FUNC;
  TOKEN_DATA_FUNC (td) = isp->u.func;
}

/*--------------------------------------------------------------.
| Dump a representation of INPUT to the obstack OBS, for use in |
| tracing.                                                      |
`--------------------------------------------------------------*/
void
input_print (struct obstack *obs, const input_block *input)
{
  int maxlen = max_debug_argument_length;
  token_chain *chain;

  assert (input);
  switch (input->type)
    {
    case INPUT_STRING:
      obstack_print (obs, input->u.u_s.str, input->u.u_s.len, &maxlen);
      break;
    case INPUT_FILE:
      obstack_grow (obs, "<file: ", strlen ("<file: "));
      obstack_grow (obs, input->file, strlen (input->file));
      obstack_1grow (obs, '>');
      break;
    case INPUT_MACRO:
      {
	const builtin *bp = find_builtin_by_addr (input->u.func);
	assert (bp);
	obstack_1grow (obs, '<');
	obstack_grow (obs, bp->name, strlen (bp->name));
	obstack_1grow (obs, '>');
      }
      break;
    case INPUT_CHAIN:
      chain = input->u.u_c.chain;
      while (chain)
	{
	  // TODO support argv refs as well
	  assert (chain->str);
	  if (obstack_print (obs, chain->str, chain->len, &maxlen))
	    return;
	  chain = chain->next;
	}
      break;
    default:
      assert (!"input_print");
      abort ();
    }
}


/*-----------------------------------------------------------------.
| Low level input is done a character at a time.  The function     |
| peek_input () is used to look at the next character in the input |
| stream.  At any given time, it reads from the input_block on the |
| top of the current input stack.  The return value is an unsigned |
| char, or CHAR_EOF if there is no more input, or CHAR_MACRO if a  |
| builtin token occurs next.                                       |
`-----------------------------------------------------------------*/

static int
peek_input (void)
{
  int ch;
  input_block *block = isp;
  token_chain *chain;

  while (1)
    {
      if (block == NULL)
	return CHAR_EOF;

      switch (block->type)
	{
	case INPUT_STRING:
	  if (!block->u.u_s.len)
	    break;
	  return to_uchar (block->u.u_s.str[0]);

	case INPUT_FILE:
	  ch = getc (block->u.u_f.fp);
	  if (ch != EOF)
	    {
	      ungetc (ch, block->u.u_f.fp);
	      return ch;
	    }
	  block->u.u_f.end = true;
	  break;

	case INPUT_MACRO:
	  return CHAR_MACRO;

	case INPUT_CHAIN:
	  chain = block->u.u_c.chain;
	  while (chain)
	    {
	      if (chain->str)
		{
		  if (chain->len)
		    return to_uchar (chain->str[0]);
		}
	      else
		{
		  // TODO - peek into argv
		  assert (!"implemented yet");
		  abort ();
		}
	      chain = chain->next;
	    }
	  break;

	default:
	  assert (!"peek_input");
	  abort ();
	}
      block = block->prev;
    }
}

/*-------------------------------------------------------------------------.
| The function next_char () is used to read and advance the input to the   |
| next character.  It also manages line numbers for error messages, so	   |
| they do not get wrong, due to lookahead.  The token consisting of a	   |
| newline alone is taken as belonging to the line it ends, and the current |
| line number is not incremented until the next character is read.	   |
| 99.9% of all calls will read from a string, so factor that out into a    |
| macro for speed.                                                         |
`-------------------------------------------------------------------------*/

#define next_char() \
  (isp && isp->type == INPUT_STRING && isp->u.u_s.len && !input_change	\
   ? (isp->u.u_s.len--, to_uchar (*isp->u.u_s.str++))			\
   : next_char_1 ())

static int
next_char_1 (void)
{
  int ch;
  token_chain *chain;

  while (1)
    {
      if (isp == NULL)
	{
	  current_file = "";
	  current_line = 0;
	  return CHAR_EOF;
	}

      if (input_change)
	{
	  current_file = isp->file;
	  current_line = isp->line;
	  input_change = false;
	}

      switch (isp->type)
	{
	case INPUT_STRING:
	  if (!isp->u.u_s.len)
	    break;
	  isp->u.u_s.len--;
	  return to_uchar (*isp->u.u_s.str++);

	case INPUT_FILE:
	  if (start_of_input_line)
	    {
	      start_of_input_line = false;
	      current_line = ++isp->line;
	    }

	  /* If stdin is a terminal, calling getc after peek_input
	     already called it would make the user have to hit ^D
	     twice to quit.  */
	  ch = isp->u.u_f.end ? EOF : getc (isp->u.u_f.fp);
	  if (ch != EOF)
	    {
	      if (ch == '\n')
		start_of_input_line = true;
	      return ch;
	    }
	  break;

	case INPUT_MACRO:
	  /* INPUT_MACRO input sources has only one token */
	  pop_input (true);
	  return CHAR_MACRO;

	case INPUT_CHAIN:
	  chain = isp->u.u_c.chain;
	  while (chain)
	    {
	      if (chain->str)
		{
		  if (chain->len)
		    {
		      chain->len--;
		      return to_uchar (*chain->str++);
		    }
		}
	      else
		{
		  // TODO - read from argv
		  assert (!"implemented yet");
		  abort ();
		}
	      if (chain->level >= 0)
		adjust_refcount (chain->level, false);
	      isp->u.u_c.chain = chain = chain->next;
	    }
	  break;

	default:
	  assert (!"next_char_1");
	  abort ();
	}

      /* End of input source --- pop one level.  */
      pop_input (true);
    }
}

/*-------------------------------------------------------------------.
| skip_line () simply discards all immediately following characters, |
| up to the first newline.  It is only used from m4_dnl ().  Report  |
| warnings on behalf of NAME.                                        |
`-------------------------------------------------------------------*/

void
skip_line (const char *name)
{
  int ch;
  const char *file = current_file;
  int line = current_line;

  while ((ch = next_char ()) != CHAR_EOF && ch != '\n')
    ;
  if (ch == CHAR_EOF)
    /* current_file changed to "" if we see CHAR_EOF, use the
       previous value we stored earlier.  */
    m4_warn_at_line (0, file, line, name,
		     _("end of file treated as newline"));
  /* On the rare occasion that dnl crosses include file boundaries
     (either the input file did not end in a newline, or changeword
     was used), calling next_char can update current_file and
     current_line, and that update will be undone as we return to
     expand_macro.  This informs next_char to fix things again.  */
  if (file != current_file || line != current_line)
    input_change = true;
}


/*------------------------------------------------------------------.
| This function is for matching a string against a prefix of the    |
| input stream.  If the string S matches the input and CONSUME is   |
| true, the input is discarded; otherwise any characters read are   |
| pushed back again.  The function is used only when multicharacter |
| quotes or comment delimiters are used.                            |
`------------------------------------------------------------------*/

static bool
match_input (const char *s, bool consume)
{
  int n;			/* number of characters matched */
  int ch;			/* input character */
  const char *t;
  bool result = false;

  ch = peek_input ();
  if (ch != to_uchar (*s))
    return false;			/* fail */

  if (s[1] == '\0')
    {
      if (consume)
	(void) next_char ();
      return true;			/* short match */
    }

  (void) next_char ();
  for (n = 1, t = s++; (ch = peek_input ()) == to_uchar (*s++); )
    {
      (void) next_char ();
      n++;
      if (*s == '\0')		/* long match */
	{
	  if (consume)
	    return true;
	  result = true;
	  break;
	}
    }

  /* Failed or shouldn't consume, push back input.  */
  push_string_init ();
  obstack_grow (current_input, t, n);
  push_string_finish ();
  return result;
}

/*--------------------------------------------------------------------.
| The macro MATCH() is used to match a string S against the input.    |
| The first character is handled inline, for speed.  Hopefully, this  |
| will not hurt efficiency too much when single character quotes and  |
| comment delimiters are used.  If CONSUME, then CH is the result of  |
| next_char, and a successful match will discard the matched string.  |
| Otherwise, CH is the result of peek_input, and the input stream is  |
| effectively unchanged.                                              |
`--------------------------------------------------------------------*/

#define MATCH(ch, s, consume)                                           \
  (to_uchar ((s)[0]) == (ch)                                            \
   && (ch) != '\0'                                                      \
   && ((s)[1] == '\0' || (match_input ((s) + (consume), consume))))


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
  obstack_alloc (&token_stack, 1);
  token_bottom = obstack_base (&token_stack);

  isp = NULL;
  wsp = NULL;
  next = NULL;

  start_of_input_line = false;

  lquote.string = xstrdup (DEF_LQUOTE);
  lquote.length = strlen (lquote.string);
  rquote.string = xstrdup (DEF_RQUOTE);
  rquote.length = strlen (rquote.string);
  bcomm.string = xstrdup (DEF_BCOMM);
  bcomm.length = strlen (bcomm.string);
  ecomm.string = xstrdup (DEF_ECOMM);
  ecomm.length = strlen (ecomm.string);

#ifdef ENABLE_CHANGEWORD
  set_word_regexp (NULL, user_word_regexp);
#endif /* ENABLE_CHANGEWORD */

  set_quote_age ();
}


/*--------------------------------------------------------------------.
| Set the quote delimiters to LQ and RQ.  Used by m4_changequote ().  |
| Pass NULL if the argument was not present, to distinguish from an   |
| explicit empty string.                                              |
`--------------------------------------------------------------------*/

void
set_quotes (const char *lq, const char *rq)
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
      rq = DEF_RQUOTE;
    }
  else if (!rq || (*lq && !*rq))
    rq = DEF_RQUOTE;

  if (strcmp (lquote.string, lq) == 0 && strcmp (rquote.string, rq) == 0)
    return;

  free (lquote.string);
  free (rquote.string);
  lquote.string = xstrdup (lq);
  lquote.length = strlen (lquote.string);
  rquote.string = xstrdup (rq);
  rquote.length = strlen (rquote.string);
  set_quote_age ();
}

/*--------------------------------------------------------------------.
| Set the comment delimiters to BC and EC.  Used by m4_changecom ().  |
| Pass NULL if the argument was not present, to distinguish from an   |
| explicit empty string.                                              |
`--------------------------------------------------------------------*/

void
set_comment (const char *bc, const char *ec)
{
  /* POSIX requires no arguments to disable comments.  It requires
     empty arguments to be used as-is, but this is counter to
     traditional behavior, because a non-null begin and null end makes
     it impossible to end a comment.  An aardvark has been filed:
     http://www.opengroup.org/austin/mailarchives/ag-review/msg02168.html
     This implementation assumes the aardvark will be approved.  See
     the texinfo for what some other implementations do.  */
  if (!bc)
    bc = ec = "";
  else if (!ec || (*bc && !*ec))
    ec = DEF_ECOMM;

  if (strcmp (bcomm.string, bc) == 0 && strcmp (ecomm.string, ec) == 0)
    return;

  free (bcomm.string);
  free (ecomm.string);
  bcomm.string = xstrdup (bc);
  bcomm.length = strlen (bcomm.string);
  ecomm.string = xstrdup (ec);
  ecomm.length = strlen (ecomm.string);
  set_quote_age ();
}

#ifdef ENABLE_CHANGEWORD

/*-------------------------------------------------------------------.
| Set the regular expression for recognizing words to REGEXP, and    |
| report errors on behalf of CALLER.  If REGEXP is NULL, revert back |
| to the default parsing rules.                                      |
`-------------------------------------------------------------------*/

void
set_word_regexp (const char *caller, const char *regexp)
{
  int i;
  char test[2];
  const char *msg;
  struct re_pattern_buffer new_word_regexp;

  if (!*regexp || !strcmp (regexp, DEFAULT_WORD_REGEXP))
    {
      default_word_regexp = true;
      set_quote_age ();
      return;
    }

  /* Dry run to see whether the new expression is compilable.  */
  init_pattern_buffer (&new_word_regexp, NULL);
  msg = re_compile_pattern (regexp, strlen (regexp), &new_word_regexp);
  regfree (&new_word_regexp);

  if (msg != NULL)
    {
      m4_warn (0, caller, _("bad regular expression `%s': %s"), regexp, msg);
      return;
    }

  /* If compilation worked, retry using the word_regexp struct.
     Can't rely on struct assigns working, so redo the compilation.  */
  regfree (&word_regexp);
  msg = re_compile_pattern (regexp, strlen (regexp), &word_regexp);
  assert (!msg);
  re_set_registers (&word_regexp, &regs, regs.num_regs, regs.start, regs.end);

  default_word_regexp = false;
  set_quote_age ();

  if (word_start == NULL)
    word_start = (char *) xmalloc (256);

  word_start[0] = '\0';
  test[1] = '\0';
  for (i = 1; i < 256; i++)
    {
      test[0] = i;
      word_start[i] = re_search (&word_regexp, test, 1, 0, 0, NULL) >= 0;
    }
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

  /* Hueristic of characters that might impact rescan if they appear in
     a quote delimiter.  */
#define Letters "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
  static const char unsafe[] = Letters "_0123456789(,) \t\n\r\f\v";
#undef Letters

  if (lquote.length == 1 && rquote.length == 1
      && strpbrk(lquote.string, unsafe) == NULL
      && strpbrk(rquote.string, unsafe) == NULL
      && default_word_regexp && *lquote.string != *rquote.string
      && *bcomm.string != '(' && *bcomm.string != ','
      && *bcomm.string != ')' && *bcomm.string != *lquote.string)
    current_quote_age = (((*lquote.string & 0xff) << 8)
			 | (*rquote.string & 0xff));
  else
    current_quote_age = 0;
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


/*--------------------------------------------------------------------.
| Parse and return a single token from the input stream.  A token     |
| can either be TOKEN_EOF, if the input_stack is empty; it can be     |
| TOKEN_STRING for a quoted string; TOKEN_WORD for something that is  |
| a potential macro name; and TOKEN_SIMPLE for any single character   |
| that is not a part of any of the previous types.  If LINE is not    |
| NULL, set *LINE to the line where the token starts.  Report errors  |
| (unterminated comments or strings) on behalf of CALLER, if	      |
| non-NULL.							      |
|								      |
| Next_token () returns the token type, and passes back a pointer to  |
| the token data through TD.  The token text is collected on the      |
| obstack token_stack, which never contains more than one token text  |
| at a time.  The storage pointed to by the fields in TD is	      |
| therefore subject to change the next time next_token () is called.  |
`--------------------------------------------------------------------*/

token_type
next_token (token_data *td, int *line, const char *caller)
{
  int ch;
  int quote_level;
  token_type type;
#ifdef ENABLE_CHANGEWORD
  int startpos;
  char *orig_text = NULL;
#endif /* ENABLE_CHANGEWORD */
  const char *file;
  int dummy;

  obstack_free (&token_stack, token_bottom);
  if (!line)
    line = &dummy;

  /* Can't consume character until after CHAR_MACRO is handled.  */
  ch = peek_input ();
  if (ch == CHAR_EOF)
    {
#ifdef DEBUG_INPUT
      xfprintf (stderr, "next_token -> EOF\n");
#endif /* DEBUG_INPUT */
      next_char ();
      return TOKEN_EOF;
    }
  if (ch == CHAR_MACRO)
    {
      init_macro_token (td);
      next_char ();
#ifdef DEBUG_INPUT
      xfprintf (stderr, "next_token -> MACDEF (%s)\n",
		find_builtin_by_addr (TOKEN_DATA_FUNC (td))->name);
#endif /* DEBUG_INPUT */
      return TOKEN_MACDEF;
    }

  next_char (); /* Consume character we already peeked at.  */
  file = current_file;
  *line = current_line;
  if (MATCH (ch, bcomm.string, true))
    {
      obstack_grow (&token_stack, bcomm.string, bcomm.length);
      while ((ch = next_char ()) != CHAR_EOF
	     && !MATCH (ch, ecomm.string, true))
	obstack_1grow (&token_stack, ch);
      if (ch != CHAR_EOF)
	obstack_grow (&token_stack, ecomm.string, ecomm.length);
      else
	/* Current_file changed to "" if we see CHAR_EOF, use the
	   previous value we stored earlier.  */
	m4_error_at_line (EXIT_FAILURE, 0, file, *line, caller,
			  _("end of file in comment"));

      type = TOKEN_STRING;
    }
  else if (default_word_regexp && (isalpha (ch) || ch == '_'))
    {
      obstack_1grow (&token_stack, ch);
      while ((ch = peek_input ()) != CHAR_EOF && (isalnum (ch) || ch == '_'))
	{
	  obstack_1grow (&token_stack, ch);
	  (void) next_char ();
	}
      type = TOKEN_WORD;
    }

#ifdef ENABLE_CHANGEWORD

  else if (!default_word_regexp && word_start[ch])
    {
      obstack_1grow (&token_stack, ch);
      while (1)
	{
	  ch = peek_input ();
	  if (ch == CHAR_EOF)
	    break;
	  obstack_1grow (&token_stack, ch);
	  startpos = re_search (&word_regexp,
				(char *) obstack_base (&token_stack),
				obstack_object_size (&token_stack), 0, 0,
				&regs);
	  if (startpos != 0 ||
	      regs.end [0] != obstack_object_size (&token_stack))
	    {
	      *(((char *) obstack_base (&token_stack)
		 + obstack_object_size (&token_stack)) - 1) = '\0';
	      break;
	    }
	  next_char ();
	}

      obstack_1grow (&token_stack, '\0');
      orig_text = (char *) obstack_finish (&token_stack);

      if (regs.start[1] != -1)
	obstack_grow (&token_stack, orig_text + regs.start[1],
		      regs.end[1] - regs.start[1]);
      else
	obstack_grow (&token_stack, orig_text, regs.end[0]);

      type = TOKEN_WORD;
    }

#endif /* ENABLE_CHANGEWORD */

  else if (!MATCH (ch, lquote.string, true))
    {
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
      quote_level = 1;
      while (1)
	{
	  ch = next_char ();
	  if (ch == CHAR_EOF)
	    /* Current_file changed to "" if we see CHAR_EOF, use
	       the previous value we stored earlier.  */
	    m4_error_at_line (EXIT_FAILURE, 0, file, *line, caller,
			      _("end of file in string"));

	  if (MATCH (ch, rquote.string, true))
	    {
	      if (--quote_level == 0)
		break;
	      obstack_grow (&token_stack, rquote.string, rquote.length);
	    }
	  else if (MATCH (ch, lquote.string, true))
	    {
	      quote_level++;
	      obstack_grow (&token_stack, lquote.string, lquote.length);
	    }
	  else
	    obstack_1grow (&token_stack, ch);
	}
      type = TOKEN_STRING;
    }

  TOKEN_DATA_TYPE (td) = TOKEN_TEXT;
  TOKEN_DATA_LEN (td) = obstack_object_size (&token_stack);
  obstack_1grow (&token_stack, '\0');
  TOKEN_DATA_TEXT (td) = (char *) obstack_finish (&token_stack);
  TOKEN_DATA_QUOTE_AGE (td) = current_quote_age;
#ifdef ENABLE_CHANGEWORD
  if (orig_text == NULL)
    TOKEN_DATA_ORIG_TEXT (td) = TOKEN_DATA_TEXT (td);
  else
    {
      TOKEN_DATA_ORIG_TEXT (td) = orig_text;
      TOKEN_DATA_LEN (td) = strlen (orig_text);
    }
#endif /* ENABLE_CHANGEWORD */
#ifdef DEBUG_INPUT
  xfprintf (stderr, "next_token -> %s (%s), len %zu\n",
	    token_type_string (type), TOKEN_DATA_TEXT (td),
	    TOKEN_DATA_LEN (td));
#endif /* DEBUG_INPUT */
  return type;
}

/*-----------------------------------------------.
| Peek at the next token from the input stream.  |
`-----------------------------------------------*/

token_type
peek_token (void)
{
  token_type result;
  int ch = peek_input ();

  if (ch == CHAR_EOF)
    {
      result = TOKEN_EOF;
    }
  else if (ch == CHAR_MACRO)
    {
      result = TOKEN_MACDEF;
    }
  else if (MATCH (ch, bcomm.string, false))
    {
      result = TOKEN_STRING;
    }
  else if ((default_word_regexp && (isalpha (ch) || ch == '_'))
#ifdef ENABLE_CHANGEWORD
      || (!default_word_regexp && word_start[ch])
#endif /* ENABLE_CHANGEWORD */
      )
    {
      result = TOKEN_WORD;
    }
  else if (MATCH (ch, lquote.string, false))
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

    case TOKEN_MACDEF:
      xfprintf (stderr, "macro: %p\n", TOKEN_DATA_FUNC (td));
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

  while ((t = next_token (&td, NULL, "<debug>")) != TOKEN_EOF)
    print_token ("lex", t, &td);
}
#endif /* DEBUG_INPUT */

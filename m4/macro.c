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

/* This file contains the functions, that performs the basic argument
   parsing and macro expansion.  */

#include "m4.h"
#include "m4private.h"

static void expand_macro (m4_symbol *);
static void expand_token (struct obstack *, m4_token_t, m4_token_data *);

/* Current recursion level in expand_macro ().  */
int m4_expansion_level = 0;

/* The number of the current call of expand_macro ().  */
static int macro_call_id = 0;

/* This function read all input, and expands each token, one at a time.  */
void
m4_expand_input (void)
{
  m4_token_t t;
  m4_token_data td;

  while ((t = m4_next_token (&td)) != M4_TOKEN_EOF)
    expand_token ((struct obstack *) NULL, t, &td);
}


/* Expand one token, according to its type.  Potential macro names
   (TOKEN_WORD) are looked up in the symbol table, to see if they have a
   macro definition.  If they have, they are expanded as macros, otherwise
   the text are just copied to the output.  */
static void
expand_token (struct obstack *obs, m4_token_t t, m4_token_data *td)
{
  m4_symbol *symbol;
  char *text = M4_TOKEN_DATA_TEXT (td);

  switch (t)
    {				/* TOKSW */
    case M4_TOKEN_EOF:
    case M4_TOKEN_MACDEF:
      break;

    case M4_TOKEN_SIMPLE:
    case M4_TOKEN_STRING:
    case M4_TOKEN_SPACE:
      m4_shipout_text (obs, text, strlen (text));
      break;

    case M4_TOKEN_WORD:
      if (M4_IS_ESCAPE(*text))
	text++;

      symbol = m4_lookup_symbol (text, M4_SYMBOL_LOOKUP);
      if (symbol == NULL || M4_SYMBOL_TYPE (symbol) == M4_TOKEN_VOID
	  || (M4_SYMBOL_TYPE (symbol) == M4_TOKEN_FUNC
	      && M4_SYMBOL_BLIND_NO_ARGS (symbol)
	      && !M4_IS_OPEN(m4_peek_input ())))
	{
	  m4_shipout_text (obs, M4_TOKEN_DATA_TEXT (td),
			   strlen (M4_TOKEN_DATA_TEXT (td)));
	}
      else
	expand_macro (symbol);
      break;

    default:
      M4ERROR ((warning_status, 0,
		_("INTERNAL ERROR: Bad token type in expand_token ()")));
      abort ();
    }
}


/* This function parses one argument to a macro call.  It expects the first
   left parenthesis, or the separating comma to have been read by the
   caller.  It skips leading whitespace, and reads and expands tokens,
   until it finds a comma or an right parenthesis at the same level of
   parentheses.  It returns a flag indicating whether the argument read are
   the last for the active macro call.  The argument are build on the
   obstack OBS, indirectly through expand_token ().	 */
static boolean
expand_argument (struct obstack *obs, m4_token_data *argp)
{
  m4_token_t t;
  m4_token_data td;
  char *text;
  int paren_level;

  M4_TOKEN_DATA_TYPE (argp) = M4_TOKEN_VOID;

  /* Skip leading white space.  */
  do
    {
      t = m4_next_token (&td);
    }
  while (t == M4_TOKEN_SPACE);

  paren_level = 0;

  while (1)
    {

      switch (t)
	{			/* TOKSW */
	case M4_TOKEN_SIMPLE:
	  text = M4_TOKEN_DATA_TEXT (&td);
	  if ((M4_IS_COMMA(*text) || M4_IS_CLOSE(*text)) && paren_level == 0)
	    {

	      /* The argument MUST be finished, whether we want it or not.  */
	      obstack_1grow (obs, '\0');
	      text = obstack_finish (obs);

	      if (M4_TOKEN_DATA_TYPE (argp) == M4_TOKEN_VOID)
		{
		  M4_TOKEN_DATA_TYPE (argp) = M4_TOKEN_TEXT;
		  M4_TOKEN_DATA_TEXT (argp) = text;
		}
	      return (boolean) (M4_IS_COMMA(*M4_TOKEN_DATA_TEXT (&td)));
	    }

	  if (M4_IS_OPEN(*text))
	    paren_level++;
	  else if (M4_IS_CLOSE(*text))
	    paren_level--;
	  expand_token (obs, t, &td);
	  break;

	case M4_TOKEN_EOF:
	  M4ERROR ((EXIT_FAILURE, 0,
		    _("ERROR: EOF in argument list")));
	  break;

	case M4_TOKEN_WORD:
	case M4_TOKEN_SPACE:
	case M4_TOKEN_STRING:
	  expand_token (obs, t, &td);
	  break;

	case M4_TOKEN_MACDEF:
	  if (obstack_object_size (obs) == 0)
	    {
	      M4_TOKEN_DATA_TYPE (argp) = M4_TOKEN_FUNC;
	      M4_TOKEN_DATA_HANDLE (argp) = M4_TOKEN_DATA_HANDLE (&td);
	      M4_TOKEN_DATA_FUNC (argp) = M4_TOKEN_DATA_FUNC (&td);
	      M4_TOKEN_DATA_FUNC_TRACED (argp) = M4_TOKEN_DATA_FUNC_TRACED (&td);
	    }
	  break;

	default:
	  M4ERROR ((warning_status, 0, _("\
INTERNAL ERROR: Bad token type in expand_argument ()")));
	  abort ();
	}

      t = m4_next_token (&td);
    }
}

/* Collect all the arguments to a call of the macro SYM.  The arguments are
   stored on the obstack ARGUMENTS and a table of pointers to the arguments
   on the obstack ARGPTR.  */
static void
collect_arguments (m4_symbol *symbol, struct obstack *argptr,
		   struct obstack *arguments)
{
  int ch;			/* lookahead for ( */
  m4_token_data td;
  m4_token_data *tdp;
  boolean more_args;
  boolean groks_macro_args = M4_SYMBOL_MACRO_ARGS (symbol);

  M4_TOKEN_DATA_TYPE (&td) = M4_TOKEN_TEXT;
  M4_TOKEN_DATA_TEXT (&td) = M4_SYMBOL_NAME (symbol);
  tdp = (m4_token_data *) obstack_copy (arguments, (void *) &td, sizeof (td));
  obstack_grow (argptr, (void *) &tdp, sizeof (tdp));

  ch = m4_peek_input ();
  if (M4_IS_OPEN(ch))
    {
      m4_next_token (&td);		/* gobble parenthesis */
      do
	{
	  more_args = expand_argument (arguments, &td);

	  if (!groks_macro_args && M4_TOKEN_DATA_TYPE (&td) == M4_TOKEN_FUNC)
	    {
	      M4_TOKEN_DATA_TYPE (&td) = M4_TOKEN_TEXT;
	      M4_TOKEN_DATA_TEXT (&td) = "";
	    }
	  tdp = (m4_token_data *)
	    obstack_copy (arguments, (void *) &td, sizeof (td));
	  obstack_grow (argptr, (void *) &tdp, sizeof (tdp));
	}
      while (more_args);
    }
}



/* The actual call of a macro is handled by m4_call_macro ().
   m4_call_macro () is passed a symbol SYM, whose type is used to
   call either a builtin function, or the user macro expansion
   function expand_predefine () (lives in builtin.c).  There are ARGC
   arguments to the call, stored in the ARGV table.  The expansion is
   left on the obstack EXPANSION.  Macro tracing is also handled here.  */
void
m4_call_macro (m4_symbol *symbol, int argc, m4_token_data **argv,
	       struct obstack *expansion)
{
  switch (M4_SYMBOL_TYPE (symbol))
    {
    case M4_TOKEN_FUNC:
      (*M4_SYMBOL_FUNC (symbol)) (expansion, argc, argv);
      break;

    case M4_TOKEN_TEXT:
      m4_process_macro (expansion, symbol, argc, argv);
      break;

    default:
      M4ERROR ((warning_status, 0,
		_("INTERNAL ERROR: Bad symbol type in call_macro ()")));
      abort ();
    }
}

/* The macro expansion is handled by expand_macro ().  It parses the
   arguments, using collect_arguments (), and builds a table of pointers to
   the arguments.  The arguments themselves are stored on a local obstack.
   Expand_macro () uses call_macro () to do the call of the macro.

   Expand_macro () is potentially recursive, since it calls expand_argument
   (), which might call expand_token (), which might call expand_macro ().  */
static void
expand_macro (m4_symbol *symbol)
{
  struct obstack arguments;
  struct obstack argptr;
  m4_token_data **argv;
  int argc;
  struct obstack *expansion;
  const char *expanded;
  boolean traced;
  int my_call_id;

  m4_expansion_level++;
  if (m4_expansion_level > nesting_limit)
    M4ERROR ((EXIT_FAILURE, 0, _("\
ERROR: Recursion limit of %d exceeded, use -L<N> to change it"),
	      nesting_limit));

  macro_call_id++;
  my_call_id = macro_call_id;

  traced = (boolean) ((debug_level & M4_DEBUG_TRACE_ALL) || M4_SYMBOL_TRACED (symbol));

  obstack_init (&argptr);
  obstack_init (&arguments);

  if (traced && (debug_level & M4_DEBUG_TRACE_CALL))
    m4_trace_prepre (M4_SYMBOL_NAME (symbol), my_call_id);

  collect_arguments (symbol, &argptr, &arguments);

  argc = obstack_object_size (&argptr) / sizeof (m4_token_data *);
  argv = (m4_token_data **) obstack_finish (&argptr);

  if (traced)
    m4_trace_pre (M4_SYMBOL_NAME (symbol), my_call_id, argc, argv);

  expansion = m4_push_string_init ();
  m4_call_macro (symbol, argc, argv, expansion);
  expanded = m4_push_string_finish ();

  if (traced)
    m4_trace_post (M4_SYMBOL_NAME (symbol), my_call_id, argc, argv, expanded);

  --m4_expansion_level;

  obstack_free (&arguments, NULL);
  obstack_free (&argptr, NULL);
}

/* This function handles all expansion of user defined and predefined
   macros.  It is called with an obstack OBS, where the macros expansion
   will be placed, as an unfinished object.  SYMBOL points to the macro
   definition, giving the expansion text.  ARGC and ARGV are the arguments,
   as usual.  */
void
m4_process_macro (struct obstack *obs, m4_symbol *symbol, int argc,
		  m4_token_data **argv)
{
  const unsigned char *text;
  int i;

  for (text = M4_SYMBOL_TEXT (symbol); *text != '\0';)
    {
      if (*text != '$')
	{
	  obstack_1grow (obs, *text);
	  text++;
	  continue;
	}
      text++;
      switch (*text)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  if (no_gnu_extensions || !isdigit(text[1]))
	    {
	      i = *text++ - '0';
	    }
	  else
	    {
	      char *endp;
	      i = (int)strtol (text, &endp, 10);
	      text = endp;
	    }
	  if (i < argc)
	    m4_shipout_string (obs, M4_TOKEN_DATA_TEXT (argv[i]), 0, FALSE);
	  break;

	case '#':		/* number of arguments */
	  m4_shipout_int (obs, argc - 1);
	  text++;
	  break;

	case '*':		/* all arguments */
	case '@':		/* ... same, but quoted */
	  m4_dump_args (obs, argc, argv, ",", *text == '@');
	  text++;
	  break;

	default:
	  obstack_1grow (obs, '$');
	  break;
	}
    }
}

/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2006, 2007 Free
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This file contains the functions, that performs the basic argument
   parsing and macro expansion.  */

#include "m4.h"

/* Opaque structure describing all arguments to a macro, including the
   macro name at index 0.  */
struct macro_arguments
{
  /* Number of arguments owned by this object, may be larger than
     arraylen since the array can refer to multiple arguments via a
     single $@ reference.  */
  unsigned int argc;
  /* False unless the macro expansion refers to $@, determines whether
     this object can be freed at end of macro expansion or must wait
     until next byte read from file.  */
  bool_bitfield inuse : 1;
  /* False if all arguments are just text or func, true if this argv
     refers to another one.  */
  bool_bitfield has_ref : 1;
  const char *argv0; /* The macro name being expanded.  */
  size_t argv0_len; /* Length of argv0.  */
  /* The value of quote_age used when parsing all arguments in this
     object, or 0 if quote_age changed during parsing or if any of the
     arguments might contain content that can affect rescan.  */
  unsigned int quote_age;
  size_t arraylen; /* True length of allocated elements in array.  */
  /* Used as a variable-length array, storing information about each
     argument.  */
  token_data *array[FLEXIBLE_ARRAY_MEMBER];
};

static void expand_macro (symbol *);
static bool expand_token (struct obstack *, token_type, token_data *, int,
			  bool);

/* Current recursion level in expand_macro ().  */
int expansion_level = 0;

/* The number of the current call of expand_macro ().  */
static int macro_call_id = 0;

/* The shared stack of collected arguments for macro calls; as each
   argument is collected, it is finished and its location stored in
   argv_stack.  This stack can be used simultaneously by multiple
   macro calls, using obstack_regrow to handle partial objects
   embedded in the stack.  */
static struct obstack argc_stack;

/* The shared stack of pointers to collected arguments for macro
   calls.  This stack can be used simultaneously by multiple macro
   calls, using obstack_regrow to handle partial objects embedded in
   the stack.  */
static struct obstack argv_stack;

/* The empty string token.  */
static token_data empty_token;

/*----------------------------------------------------------------.
| This function reads all input, and expands each token, one at a |
| time.                                                           |
`----------------------------------------------------------------*/

void
expand_input (void)
{
  token_type t;
  token_data td;
  int line;

  obstack_init (&argc_stack);
  obstack_init (&argv_stack);

  TOKEN_DATA_TYPE (&empty_token) = TOKEN_TEXT;
  TOKEN_DATA_TEXT (&empty_token) = "";
  TOKEN_DATA_LEN (&empty_token) = 0;
#ifdef ENABLE_CHANGEWORD
  TOKEN_DATA_ORIG_TEXT (&empty_token) = "";
#endif

  while ((t = next_token (&td, &line, NULL)) != TOKEN_EOF)
    expand_token ((struct obstack *) NULL, t, &td, line, true);

  obstack_free (&argc_stack, NULL);
  obstack_free (&argv_stack, NULL);
}


/*-------------------------------------------------------------------.
| Expand one token TD onto the stack OBS, according to its type T,   |
| which began parsing on the specified LINE.  If OBS is NULL, output |
| the data.  If FIRST, there is no previous text in the current	     |
| argument.  Potential macro names (TOKEN_WORD) are looked up in the |
| symbol table, to see if they have a macro definition.  If they     |
| have, they are expanded as macros, otherwise the text is just	     |
| copied to the output.  Return true if the result is guaranteed to  |
| give the same parse on rescan in a quoted context, provided	     |
| quoting doesn't change.  Returning false is always safe, although  |
| it may lead to slower performance.				     |
`-------------------------------------------------------------------*/

static bool
expand_token (struct obstack *obs, token_type t, token_data *td, int line,
	      bool first)
{
  symbol *sym;
  bool result;
  int ch;

  switch (t)
    {				/* TOKSW */
    case TOKEN_EOF:
    case TOKEN_MACDEF:
      /* Always safe, since there is no text to rescan.  */
      return true;

    case TOKEN_STRING:
      /* Tokens and comments are safe in isolation (since quote_age()
	 detects any change in delimiters).  But if other text is
	 already present, multi-character delimiters could be an
	 issue, so use a conservative heuristic.  */
      result = first || safe_quotes ();
      break;

    case TOKEN_OPEN:
    case TOKEN_COMMA:
    case TOKEN_CLOSE:
      /* Conservative heuristic; thanks to multi-character delimiter
	 concatenation.  */
      result = safe_quotes ();
      break;

    case TOKEN_SIMPLE:
      /* Conservative heuristic; if these characters are whitespace or
	 numeric, then behavior of safe_quotes is applicable.
	 Otherwise, assume these characters have a high likelihood of
	 use in quote delimiters.  */
      ch = to_uchar (*TOKEN_DATA_TEXT (td));
      result = (isspace (ch) || isdigit (ch)) && safe_quotes ();
      break;

    case TOKEN_WORD:
      sym = lookup_symbol (TOKEN_DATA_TEXT (td), SYMBOL_LOOKUP);
      if (sym == NULL || SYMBOL_TYPE (sym) == TOKEN_VOID
	  || (SYMBOL_TYPE (sym) == TOKEN_FUNC
	      && SYMBOL_BLIND_NO_ARGS (sym)
	      && peek_token () != TOKEN_OPEN))
	{
#ifdef ENABLE_CHANGEWORD
	  shipout_text (obs, TOKEN_DATA_ORIG_TEXT (td),
			TOKEN_DATA_LEN (td), line);
#else
	  shipout_text (obs, TOKEN_DATA_TEXT (td), TOKEN_DATA_LEN (td), line);
#endif /* !ENABLE_CHANGEWORD */
	  /* The word just appended is unquoted, but the heuristics of
	     safe_quote are applicable.  */
	  return safe_quotes();
	}
      expand_macro (sym);
      /* Expanding a macro creates new tokens to scan, and those new
	 tokens may append unsafe text later; but we did not append
	 any text now.  */
      return true;

    default:
      assert (!"expand_token");
      abort ();
    }
  shipout_text (obs, TOKEN_DATA_TEXT (td), TOKEN_DATA_LEN (td), line);
  return result;
}


/*---------------------------------------------------------------.
| Helper function to print warning about concatenating FUNC with |
| text.                                                          |
`---------------------------------------------------------------*/
static void
warn_builtin_concat (const char *caller, builtin_func *func)
{
  const builtin *bp = find_builtin_by_addr (func);
  assert (bp);
  m4_warn (0, caller, _("cannot concatenate builtin `%s'"), bp->name);
}

/*-------------------------------------------------------------------.
| This function parses one argument to a macro call.  It expects the |
| first left parenthesis or the separating comma to have been read   |
| by the caller.  It skips leading whitespace, then reads and        |
| expands tokens, until it finds a comma or right parenthesis at the |
| same level of parentheses.  It returns a flag indicating whether   |
| the argument read is the last for the active macro call.  The      |
| argument is built on the obstack OBS, indirectly through           |
| expand_token ().  Report errors on behalf of CALLER.               |
`-------------------------------------------------------------------*/

static bool
expand_argument (struct obstack *obs, token_data *argp, const char *caller)
{
  token_type t;
  token_data td;
  int paren_level;
  const char *file = current_file;
  int line = current_line;
  unsigned int age = quote_age ();
  bool first = true;

  TOKEN_DATA_TYPE (argp) = TOKEN_VOID;

  /* Skip leading white space.  */
  do
    {
      t = next_token (&td, NULL, caller);
    }
  while (t == TOKEN_SIMPLE && isspace (to_uchar (*TOKEN_DATA_TEXT (&td))));

  paren_level = 0;

  while (1)
    {

      switch (t)
	{			/* TOKSW */
	case TOKEN_COMMA:
	case TOKEN_CLOSE:
	  if (paren_level == 0)
	    {
	      if (TOKEN_DATA_TYPE (argp) == TOKEN_FUNC)
		{
		  if (obstack_object_size (obs) == 0)
		    return t == TOKEN_COMMA;
		  warn_builtin_concat (caller, TOKEN_DATA_FUNC (argp));
		}
	      TOKEN_DATA_TYPE (argp) = TOKEN_TEXT;
	      TOKEN_DATA_LEN (argp) = obstack_object_size (obs);
	      obstack_1grow (obs, '\0');
	      TOKEN_DATA_TEXT (argp) = (char *) obstack_finish (obs);
	      TOKEN_DATA_QUOTE_AGE (argp) = age;
	      return t == TOKEN_COMMA;
	    }
	  /* fallthru */
	case TOKEN_OPEN:
	case TOKEN_SIMPLE:
	  if (t == TOKEN_OPEN)
	    paren_level++;
	  else if (t == TOKEN_CLOSE)
	    paren_level--;
	  if (!expand_token (obs, t, &td, line, first))
	    age = 0;
	  break;

	case TOKEN_EOF:
	  /* Current_file changed to "" if we see TOKEN_EOF, use the
	     previous value we stored earlier.  */
	  m4_error_at_line (EXIT_FAILURE, 0, file, line, caller,
			    _("end of file in argument list"));
	  break;

	case TOKEN_WORD:
	case TOKEN_STRING:
	  if (!expand_token (obs, t, &td, line, first))
	    age = 0;
	  break;

	case TOKEN_MACDEF:
	  if (TOKEN_DATA_TYPE (argp) == TOKEN_VOID
	      && obstack_object_size (obs) == 0)
	    {
	      TOKEN_DATA_TYPE (argp) = TOKEN_FUNC;
	      TOKEN_DATA_FUNC (argp) = TOKEN_DATA_FUNC (&td);
	    }
	  else
	    {
	      if (TOKEN_DATA_TYPE (argp) == TOKEN_FUNC)
		warn_builtin_concat (caller, TOKEN_DATA_FUNC (argp));
	      warn_builtin_concat (caller, TOKEN_DATA_FUNC (&td));
	      TOKEN_DATA_TYPE (argp) = TOKEN_TEXT;
	    }
	  break;

	default:
	  assert (!"expand_argument");
	  abort ();
	}

      if (TOKEN_DATA_TYPE (argp) != TOKEN_VOID || obstack_object_size (obs))
	first = false;
      t = next_token (&td, NULL, caller);
    }
}

/*-------------------------------------------------------------------------.
| Collect all the arguments to a call of the macro SYM.  The arguments are |
| stored on the obstack ARGUMENTS and a table of pointers to the arguments |
| on the obstack argv_stack.						   |
`-------------------------------------------------------------------------*/

static macro_arguments *
collect_arguments (symbol *sym, struct obstack *arguments)
{
  token_data td;
  token_data *tdp;
  bool more_args;
  bool groks_macro_args = SYMBOL_MACRO_ARGS (sym);
  macro_arguments args;
  macro_arguments *argv;

  args.argc = 1;
  args.inuse = false;
  args.has_ref = false;
  args.argv0 = SYMBOL_NAME (sym);
  args.argv0_len = strlen (args.argv0);
  args.quote_age = quote_age ();
  args.arraylen = 0;
  obstack_grow (&argv_stack, &args, offsetof (macro_arguments, array));

  if (peek_token () == TOKEN_OPEN)
    {
      next_token (&td, NULL, SYMBOL_NAME (sym)); /* gobble parenthesis */
      do
	{
	  more_args = expand_argument (arguments, &td, SYMBOL_NAME (sym));

	  if ((TOKEN_DATA_TYPE (&td) == TOKEN_TEXT && !TOKEN_DATA_LEN (&td))
	      || (!groks_macro_args && TOKEN_DATA_TYPE (&td) == TOKEN_FUNC))
	    tdp = &empty_token;
	  else
	    tdp = (token_data *) obstack_copy (arguments, &td, sizeof td);
	  obstack_ptr_grow (&argv_stack, tdp);
	  args.arraylen++;
	  args.argc++;
	  /* Be conservative - any change in quoting while collecting
	     arguments, or any argument that consists of unsafe text,
	     will require a rescan if $@ is reused.  */
	  if (TOKEN_DATA_TYPE (tdp) == TOKEN_TEXT
	      && TOKEN_DATA_LEN (tdp) > 0
	      && TOKEN_DATA_QUOTE_AGE (tdp) != args.quote_age)
	    args.quote_age = 0;
	}
      while (more_args);
    }
  argv = (macro_arguments *) obstack_finish (&argv_stack);
  argv->argc = args.argc;
  if (args.quote_age != quote_age ())
    argv->quote_age = 0;
  argv->arraylen = args.arraylen;
  return argv;
}


/*-----------------------------------------------------------------.
| Call the macro SYM, which is either a builtin function or a user |
| macro (via the expansion function expand_user_macro () in        |
| builtin.c).  There are ARGC arguments to the call, stored in the |
| ARGV table.  The expansion is left on the obstack EXPANSION.     |
`-----------------------------------------------------------------*/

void
call_macro (symbol *sym, int argc, macro_arguments *argv,
	    struct obstack *expansion)
{
  switch (SYMBOL_TYPE (sym))
    {
    case TOKEN_FUNC:
      SYMBOL_FUNC (sym) (expansion, argc, argv);
      break;

    case TOKEN_TEXT:
      expand_user_macro (expansion, sym, argc, argv);
      break;

    default:
      assert (!"call_macro");
      abort ();
    }
}

/*-------------------------------------------------------------------------.
| The macro expansion is handled by expand_macro ().  It parses the	   |
| arguments, using collect_arguments (), and builds a table of pointers to |
| the arguments.  The arguments themselves are stored on a local obstack.  |
| Expand_macro () uses call_macro () to do the call of the macro.	   |
|									   |
| Expand_macro () is potentially recursive, since it calls expand_argument |
| (), which might call expand_token (), which might call expand_macro ().  |
`-------------------------------------------------------------------------*/

static void
expand_macro (symbol *sym)
{
  void *argc_base = NULL;	/* Base of argc_stack on entry.  */
  void *argv_base = NULL;	/* Base of argv_stack on entry.  */
  unsigned int argc_size;	/* Size of argc_stack on entry.  */
  unsigned int argv_size;	/* Size of argv_stack on entry.  */
  macro_arguments *argv;
  struct obstack *expansion;
  const char *expanded;
  bool traced;
  int my_call_id;

  /* Report errors at the location where the open parenthesis (if any)
     was found, but after expansion, restore global state back to the
     location of the close parenthesis.  This is safe since we
     guarantee that macro expansion does not alter the state of
     current_file/current_line (dnl, include, and sinclude are special
     cased in the input engine to ensure this fact).  */
  const char *loc_open_file = current_file;
  int loc_open_line = current_line;
  const char *loc_close_file;
  int loc_close_line;

  SYMBOL_PENDING_EXPANSIONS (sym)++;
  expansion_level++;
  if (nesting_limit > 0 && expansion_level > nesting_limit)
    m4_error (EXIT_FAILURE, 0, NULL,
	      _("recursion limit of %d exceeded, use -L<N> to change it"),
	      nesting_limit);

  macro_call_id++;
  my_call_id = macro_call_id;

  traced = (debug_level & DEBUG_TRACE_ALL) || SYMBOL_TRACED (sym);

  argc_size = obstack_object_size (&argc_stack);
  argv_size = obstack_object_size (&argv_stack);
  argc_base = obstack_finish (&argc_stack);
  if (0 < argv_size)
    argv_base = obstack_finish (&argv_stack);

  if (traced && (debug_level & DEBUG_TRACE_CALL))
    trace_prepre (SYMBOL_NAME (sym), my_call_id);

  argv = collect_arguments (sym, &argc_stack);

  loc_close_file = current_file;
  loc_close_line = current_line;
  current_file = loc_open_file;
  current_line = loc_open_line;

  if (traced)
    trace_pre (SYMBOL_NAME (sym), my_call_id, argv->argc, argv);

  expansion = push_string_init ();
  call_macro (sym, argv->argc, argv, expansion);
  expanded = push_string_finish ();

  if (traced)
    trace_post (SYMBOL_NAME (sym), my_call_id, argv->argc, argv, expanded);

  current_file = loc_close_file;
  current_line = loc_close_line;

  --expansion_level;
  --SYMBOL_PENDING_EXPANSIONS (sym);

  if (SYMBOL_DELETED (sym))
    free_symbol (sym);

  /* TODO pay attention to argv->inuse, in case someone is depending on $@.  */
  if (0 < argc_size)
    obstack_regrow (&argc_stack, argc_base, argc_size);
  else
    obstack_free (&argc_stack, argc_base);
  if (0 < argv_size)
    obstack_regrow (&argv_stack, argv_base, argv_size);
  else
    obstack_free (&argv_stack, argv);
}


/* Given ARGV, return the token_data that contains argument INDEX;
   INDEX must be > 0, < argv->argc.  */
static token_data *
arg_token (macro_arguments *argv, unsigned int index)
{
  unsigned int i;
  token_data *token;

  assert (index && index < argv->argc);
  if (!argv->has_ref)
    return argv->array[index - 1];
  /* Must cycle through all tokens, until we find index, since a ref
     may occupy multiple indices.  */
  for (i = 0; i < argv->arraylen; i++)
    {
      token = argv->array[i];
      if (TOKEN_DATA_TYPE (token) == TOKEN_COMP)
	{
	  token_chain *chain = token->u.chain;
	  /* TODO - for now we support only a single-length $@ chain.  */
	  assert (!chain->next && !chain->str);
	  if (index < chain->argv->argc - (chain->index - 1))
	    {
	      token = arg_token (chain->argv, chain->index - 1 + index);
	      if (chain->flatten && TOKEN_DATA_TYPE (token) == TOKEN_FUNC)
		token = &empty_token;
	      break;
	    }
	  index -= chain->argv->argc - chain->index;
	}
      else if (--index == 0)
	break;
    }
  return token;
}

/* Given ARGV, return how many arguments it refers to.  */
unsigned int
arg_argc (macro_arguments *argv)
{
  return argv->argc;
}

/* Given ARGV, return the type of argument INDEX.  Index 0 is always
   text, and indices beyond argc are likewise treated as text.  */
token_data_type
arg_type (macro_arguments *argv, unsigned int index)
{
  token_data_type type;
  token_data *token;

  if (index == 0 || index >= argv->argc)
    return TOKEN_TEXT;
  token = arg_token (argv, index);
  type = TOKEN_DATA_TYPE (token);
  assert (type != TOKEN_COMP);
  return type;
}

/* Given ARGV, return the text at argument INDEX.  Abort if the
   argument is not text.  Index 0 is always text, and indices beyond
   argc return the empty string.  */
const char *
arg_text (macro_arguments *argv, unsigned int index)
{
  token_data *token;

  if (index == 0)
    return argv->argv0;
  if (index >= argv->argc)
    return "";
  token = arg_token (argv, index);
  switch (TOKEN_DATA_TYPE (token))
    {
    case TOKEN_TEXT:
      return TOKEN_DATA_TEXT (token);
    case TOKEN_COMP:
      /* TODO - how to concatenate multiple arguments?  For now, we expect
	 only one element in the chain, and arg_token dereferences it.  */
    default:
      break;
    }
  assert (!"arg_text");
  abort ();
}

/* Given ARGV, compare text arguments INDEXA and INDEXB for equality.
   Both indices must be non-zero and less than argc.  Return true if
   the arguments contain the same contents; often more efficient than
   strcmp (arg_text (argv, indexa), arg_text (argv, indexb)) == 0.  */
bool
arg_equal (macro_arguments *argv, unsigned int indexa, unsigned int indexb)
{
  token_data *ta = arg_token (argv, indexa);
  token_data *tb = arg_token (argv, indexb);

  if (ta == &empty_token || tb == &empty_token)
    return ta == tb;
  /* TODO - allow builtin tokens in the comparison?  */
  assert (TOKEN_DATA_TYPE (ta) == TOKEN_TEXT
	  && TOKEN_DATA_TYPE (tb) == TOKEN_TEXT);
  return (TOKEN_DATA_LEN (ta) == TOKEN_DATA_LEN (tb)
	  && strcmp (TOKEN_DATA_TEXT (ta), TOKEN_DATA_TEXT (tb)) == 0);
}

/* Given ARGV, return true if argument INDEX is the empty string.
   This gives the same result as comparing arg_len against 0, but is
   often faster.  */
bool
arg_empty (macro_arguments *argv, unsigned int index)
{
  if (index == 0)
    return argv->argv0_len == 0;
  if (index >= argv->argc)
    return true;
  return arg_token (argv, index) == &empty_token;
}

/* Given ARGV, return the length of argument INDEX.  Abort if the
   argument is not text.  Indices beyond argc return 0.  */
size_t
arg_len (macro_arguments *argv, unsigned int index)
{
  token_data *token;

  if (index == 0)
    return argv->argv0_len;
  if (index >= argv->argc)
    return 0;
  token = arg_token (argv, index);
  switch (TOKEN_DATA_TYPE (token))
    {
    case TOKEN_TEXT:
      assert ((token == &empty_token) == (TOKEN_DATA_LEN (token) == 0));
      return TOKEN_DATA_LEN (token);
    case TOKEN_COMP:
      /* TODO - how to concatenate multiple arguments?  For now, we expect
	 only one element in the chain, and arg_token dereferences it.  */
    default:
      break;
    }
  assert (!"arg_len");
  abort ();
}

/* Given ARGV, return the builtin function referenced by argument
   INDEX.  Abort if it is not a builtin in isolation.  */
builtin_func *
arg_func (macro_arguments *argv, unsigned int index)
{
  token_data *token;

  token = arg_token (argv, index);
  assert (TOKEN_DATA_TYPE (token) == TOKEN_FUNC);
  return TOKEN_DATA_FUNC (token);
}

/* Create a new argument object using the same obstack as ARGV; thus,
   the new object will automatically be freed when the original is
   freed.  Explicitly set the macro name (argv[0]) from ARGV0 with
   length ARGV0_LEN.  If SKIP, set argv[1] of the new object to
   argv[2] of the old, otherwise the objects share all arguments.  If
   FLATTEN, any non-text in ARGV is flattened to an empty string when
   referenced through the new object.  */
macro_arguments *
make_argv_ref (macro_arguments *argv, const char *argv0, size_t argv0_len,
	       bool skip, bool flatten)
{
  macro_arguments *new_argv;
  token_data *token;
  token_chain *chain;
  unsigned int index = skip ? 2 : 1;

  assert (obstack_object_size (&argv_stack) == 0);
  /* When making a reference through a reference, point to the
     original if possible.  */
  if (argv->has_ref)
    {
      /* TODO - for now we support only a single-length $@ chain.  */
      assert (argv->arraylen == 1
	      && TOKEN_DATA_TYPE (argv->array[0]) == TOKEN_COMP);
      chain = argv->array[0]->u.chain;
      assert (!chain->next && !chain->str);
      argv = chain->argv;
      index += chain->index - 1;
    }
  if (argv->argc <= index)
    {
      new_argv = (macro_arguments *)
	obstack_alloc (&argv_stack, offsetof (macro_arguments, array));
      new_argv->arraylen = 0;
      new_argv->has_ref = false;
    }
  else
    {
      new_argv = (macro_arguments *)
	obstack_alloc (&argv_stack,
		       offsetof (macro_arguments, array) + sizeof token);
      token = (token_data *) obstack_alloc (&argv_stack, sizeof *token);
      chain = (token_chain *) obstack_alloc (&argv_stack, sizeof *chain);
      new_argv->arraylen = 1;
      new_argv->array[0] = token;
      new_argv->has_ref = true;
      TOKEN_DATA_TYPE (token) = TOKEN_COMP;
      token->u.chain = chain;
      chain->next = NULL;
      chain->str = NULL;
      chain->len = 0;
      chain->argv = argv;
      chain->index = index;
      chain->flatten = flatten;
    }
  /* TODO - should argv->inuse be set?  */
  new_argv->argc = argv->argc - (index - 1);
  new_argv->inuse = false;
  new_argv->argv0 = argv0;
  new_argv->argv0_len = argv0_len;
  new_argv->quote_age = argv->quote_age;
  return new_argv;
}

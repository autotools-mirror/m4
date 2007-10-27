/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2006, 2007, 2008 Free
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

#ifndef DEBUG_MACRO
# define DEBUG_MACRO 0
#endif /* DEBUG_MACRO */

/* Opaque structure describing all arguments to a macro, including the
   macro name at index 0.  The lifetime of argv0 is only guaranteed
   within a call to expand_macro, whereas the lifetime of the array
   members is guaranteed as long as the input engine can parse text
   with a reference to $@.  */
struct macro_arguments
{
  /* Number of arguments owned by this object, may be larger than
     arraylen since the array can refer to multiple arguments via a
     single $@ reference.  */
  unsigned int argc;
  /* False unless the macro expansion refers to $@; determines whether
     this object can be freed immediately at the end of expand_macro,
     or must wait until all recursion has completed.  */
  bool_bitfield inuse : 1;
  /* False if all arguments are just text or func, true if this argv
     refers to another one.  */
  bool_bitfield wrapper : 1;
  /* False if all arguments belong to this argv, true if some of them
     include references to another.  */
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

/* Internal struct to maintain list of argument stacks.  Within a
   recursion level, consecutive macros can share a stack, but distinct
   recursion levels need different stacks since the nested macro is
   interrupting the argument collection of the outer level.  Note that
   a reference can live as long as the expansion containing the
   reference can participate as an argument in a future macro call.

   Therefore, we implement a reference counter for each expansion
   level, tracking how many references exist into the obstack, as well
   as associate a level with each reference.  Of course, expand_macro
   is actively using argv, so it increments the refcount on entry and
   decrements it on exit.  Additionally, any time the input engine is
   handed a reference that it does not inline, it increases the
   refcount in push_token, then decreases it in pop_input once the
   reference has been rescanned.  Finally, when the input engine hands
   a reference back to expand_argument, the refcount increases, which
   is then cleaned up at the end of expand_macro.

   For a running example, consider this input:

     define(a,A)define(b,`a(`$1')')define(c,$*)dnl
     define(x,`a(1)`'c($@')define(y,`$@)')dnl
     x(a(`b')``a'')y(`b')(`a')
     => AAaA

   Assuming all arguments are large enough to exceed the inlining
   thresholds of the input engine, the interesting sequence of events
   is as follows:

				     stacks[0]             refs stacks[1] refs
     after second dnl ends:          `'                    0    `'        0
     expand_macro for x, level 0:    `'                    1    `'        0
     expand_macro for a, level 1:    `'                    1    `'        1
     after collect_arguments for a:  `'                    1    `b'       1
     push `A' to input stack:        `'                    1    `b'       1
     exit expand_macro for a:        `'                    1    `'        0
     after collect_arguments for x:  `A`a''                1    `'        0
     push `a(1)`'c(' to input stack: `A`a''                1    `'        0
     push_token saves $@(x) ref:     `A`a''                2    `'        0
     exit expand_macro for x:        `A`a''                1    `'        0
     expand_macro for a, level 0:    `A`a''                2    `'        0
     after collect_arguments for a:  `A`a''`1'             2    `'        0
     push `A' to input stack:        `A`a''`1'             2    `'        0
     exit expand_macro for a:        `A`a''                1    `'        0
     output `A':                     `A`a''                1    `'        0
     expand_macro for c, level 0:    `A`a''                2    `'        0
     expand_argument gets $@(x) ref: `A`a''`$@(x)'         3    `'        0
     pop_input ends $@(x) ref:       `A`a''`$@(x)'         2    `'        0
     expand_macro for y, level 1:    `A`a''`$@(x)'         2    `'        1
     after collect_arguments for y:  `A`a''`$@(x)'         2    `b'       1
     push_token saves $@(y) ref:     `A`a''`$@(x)'         2    `b'       2
     push `)' to input stack:        `A`a''`$@(x)'         2    `b'       2
     exit expand_macro for y:        `A`a''`$@(x)'         2    `b'       1
     expand_argument gets $@(y) ref: `A`a''`$@(x)$@(y)'    2    `b'       2
     pop_input ends $@(y) ref:       `A`a''`$@(x)$@(y)'    2    `b'       1
     after collect_arguments for c:  `A`a''`$@(x)$@(y)'    2    `b'       1
     push_token saves $*(c) ref:     `A`a''`$@(x)$@(y)'    3    `b'       2
     expand_macro frees $@(x) ref:   `A`a''`$@(x)$@(y)'    2    `b'       2
     expand_macro frees $@(y) ref:   `A`a''`$@(x)$@(y)'    2    `b'       1
     exit expand_macro for c:        `A`a''`$@(x)$@(y)'    1    `b'       1
     output `Aa':                    `A`a''`$@(x)$@(y)'    0    `b'       1
     pop_input ends $*(c)$@(x) ref:  `'                    0    `b'       1
     expand_macro for b, level 0:    `'                    1    `b'       1
     pop_input ends $*(c)$@(y) ref:  `'                    1    `'        0
     after collect_arguments for b:  `a'                   1    `'        0
     push `a(`' to input stack:      `a'                   1    `'        0
     push_token saves $1(b) ref:     `a'                   2    `'        0
     push `')' to input stack:       `a'                   2    `'        0
     exit expand_macro for b:        `a'                   1    `'        0
     expand_macro for a, level 0 :   `a'                   2    `'        0
     expand_argument gets $1(b) ref: `a'`$1(b)'            3    `'        0
     pop_input ends $1(b) ref:       `a'`$1(b)'            2    `'        0
     after collect_arguments for a:  `a'`$1(b)'            2    `'        0
     push `A' to input stack:        `a'`$1(b)'            2    `'        0
     expand_macro frees $1(b) ref:   `a'`$1(b)'            1    `'        0
     exit expand_macro for a:        `'                    0    `'        0
     output `A':                     `'                    0    `'        0

   An obstack is only completely cleared when its refcount reaches
   zero.  However, as an optimization, expand_macro also frees
   anything that it added to the obstack if no additional references
   were added at the current expansion level, to reduce the amount of
   memory left on the obstack while waiting for refcounts to drop.
*/
struct macro_arg_stacks
{
  size_t refcount;	/* Number of active $@ references at this level.  */
  size_t argcount;	/* Number of argv at this level.  */
  struct obstack *args;	/* Content of arguments.  */
  struct obstack *argv;	/* Argv pointers into args.  */
  void *args_base;	/* Location for clearing the args obstack.  */
  void *argv_base;	/* Location for clearing the argv obstack.  */
};

typedef struct macro_arg_stacks macro_arg_stacks;

static void expand_macro (symbol *);
static bool expand_token (struct obstack *, token_type, token_data *, int,
			  bool);

/* Array of stacks, one element per macro recursion level.  */
static macro_arg_stacks *stacks;

/* Current size of stacks array.  */
static size_t stacks_count;

/* Current recursion level in expand_macro ().  */
int expansion_level = 0;

/* The number of the current call of expand_macro ().  */
static int macro_call_id = 0;

/* The empty string token.  */
static token_data empty_token;

#if DEBUG_MACRO
/* True if significant changes to stacks should be printed to the
   trace stream.  Primarily useful for debugging $@ ref memory leaks,
   and controlled by M4_DEBUG_MACRO environment variable.  */
static int debug_macro_level;
#else
# define debug_macro_level 0
#endif /* !DEBUG_MACRO */
#define PRINT_ARGCOUNT_CHANGES 1
#define PRINT_REFCOUNT_INCREASE 2
#define PRINT_REFCOUNT_DECREASE 4


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
  size_t i;

#if DEBUG_MACRO
  const char *s = getenv ("M4_DEBUG_MACRO");
  if (s)
    debug_macro_level = strtol (s, NULL, 0);
#endif /* DEBUG_MACRO */

  TOKEN_DATA_TYPE (&empty_token) = TOKEN_TEXT;
  TOKEN_DATA_TEXT (&empty_token) = "";
  TOKEN_DATA_LEN (&empty_token) = 0;
#ifdef ENABLE_CHANGEWORD
  TOKEN_DATA_ORIG_TEXT (&empty_token) = "";
#endif

  while ((t = next_token (&td, &line, NULL, NULL)) != TOKEN_EOF)
    expand_token (NULL, t, &td, line, true);

  for (i = 0; i < stacks_count; i++)
    {
      assert (stacks[i].refcount == 0 && stacks[i].argcount == 0);
      if (stacks[i].args)
	{
	  obstack_free (stacks[i].args, NULL);
	  free (stacks[i].args);
	  obstack_free (stacks[i].argv, NULL);
	  free (stacks[i].argv);
	}
    }
  free (stacks);
  stacks = NULL;
  stacks_count = 0;
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
	 issue, so use a conservative heuristic.  If obstack is
	 provided, the string was already expanded into it during
	 next_token.  */
      result = first || safe_quotes ();
      if (obs)
	return result;
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
      t = next_token (&td, NULL, obs, caller);
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
	      size_t len = obstack_object_size (obs);
	      if (TOKEN_DATA_TYPE (argp) == TOKEN_FUNC)
		{
		  if (!len)
		    return t == TOKEN_COMMA;
		  warn_builtin_concat (caller, TOKEN_DATA_FUNC (argp));
		}
	      if (TOKEN_DATA_TYPE (argp) != TOKEN_COMP)
		{
		  obstack_1grow (obs, '\0');
		  TOKEN_DATA_TYPE (argp) = TOKEN_TEXT;
		  TOKEN_DATA_TEXT (argp) = (char *) obstack_finish (obs);
		  TOKEN_DATA_LEN (argp) = len;
		  TOKEN_DATA_QUOTE_AGE (argp) = age;
		}
	      else
		make_text_link (obs, NULL, &argp->u.u_c.end);
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
	  if (TOKEN_DATA_TYPE (&td) == TOKEN_COMP)
	    {
	      if (TOKEN_DATA_TYPE (argp) != TOKEN_COMP)
		{
		  if (TOKEN_DATA_TYPE (argp) == TOKEN_FUNC)
		    warn_builtin_concat (caller, TOKEN_DATA_FUNC (argp));
		  TOKEN_DATA_TYPE (argp) = TOKEN_COMP;
		  argp->u.u_c.chain = td.u.u_c.chain;
		  argp->u.u_c.end = td.u.u_c.end;
		}
	      else
		{
		  assert (argp->u.u_c.end);
		  argp->u.u_c.end->next = td.u.u_c.chain;
		  argp->u.u_c.end = td.u.u_c.end;
		}
	    }
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
      t = next_token (&td, NULL, obs, caller);
    }
}

/*-------------------------------------------------------------------------.
| Collect all the arguments to a call of the macro SYM.  The arguments are |
| stored on the obstack ARGUMENTS and a table of pointers to the arguments |
| on the obstack argv_stack.						   |
`-------------------------------------------------------------------------*/

static macro_arguments *
collect_arguments (symbol *sym, struct obstack *arguments,
		   struct obstack *argv_stack)
{
  token_data td;
  token_data *tdp;
  bool more_args;
  bool groks_macro_args = SYMBOL_MACRO_ARGS (sym);
  macro_arguments args;
  macro_arguments *argv;

  args.argc = 1;
  args.inuse = false;
  args.wrapper = false;
  args.has_ref = false;
  args.argv0 = SYMBOL_NAME (sym);
  args.argv0_len = strlen (args.argv0);
  args.quote_age = quote_age ();
  args.arraylen = 0;
  obstack_grow (argv_stack, &args, offsetof (macro_arguments, array));

  if (peek_token () == TOKEN_OPEN)
    {
      next_token (&td, NULL, NULL, SYMBOL_NAME (sym)); /* gobble parenthesis */
      do
	{
	  tdp = (token_data *) obstack_alloc (arguments, sizeof *tdp);
	  more_args = expand_argument (arguments, tdp, SYMBOL_NAME (sym));

	  if ((TOKEN_DATA_TYPE (tdp) == TOKEN_TEXT && !TOKEN_DATA_LEN (tdp))
	      || (!groks_macro_args && TOKEN_DATA_TYPE (tdp) == TOKEN_FUNC))
	    {
	      obstack_free (arguments, tdp);
	      tdp = &empty_token;
	    }
	  obstack_ptr_grow (argv_stack, tdp);
	  args.arraylen++;
	  args.argc++;
	  /* Be conservative - any change in quoting while collecting
	     arguments, or any argument that consists of unsafe text,
	     will require a rescan if $@ is reused.  */
	  if (TOKEN_DATA_TYPE (tdp) == TOKEN_TEXT
	      && TOKEN_DATA_LEN (tdp) > 0
	      && TOKEN_DATA_QUOTE_AGE (tdp) != args.quote_age)
	    args.quote_age = 0;
	  else if (TOKEN_DATA_TYPE (tdp) == TOKEN_COMP)
	    args.has_ref = true;
	}
      while (more_args);
    }
  argv = (macro_arguments *) obstack_finish (argv_stack);
  argv->argc = args.argc;
  argv->has_ref = args.has_ref;
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
  void *args_base;		/* Base of stacks[i].args on entry.  */
  void *args_scratch;		/* Base of scratch space for call_macro.  */
  void *argv_base;		/* Base of stacks[i].argv on entry.  */
  macro_arguments *argv;	/* Arguments to the called macro.  */
  struct obstack *expansion;	/* Collects the macro's expansion.  */
  const input_block *expanded;	/* The resulting expansion, for tracing.  */
  bool traced;			/* True if this macro is traced.  */
  int my_call_id;		/* Sequence id for this macro.  */
  int level = expansion_level;	/* Expansion level of this macro.  */

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

  /* Obstack preparation.  */
  if (level >= stacks_count)
    {
      size_t old_count = stacks_count;
      stacks = (macro_arg_stacks *) x2nrealloc (stacks, &stacks_count,
						sizeof *stacks);
      memset (&stacks[old_count], 0,
	      sizeof *stacks * (stacks_count - old_count));
    }
  if (!stacks[level].args)
    {
      assert (!stacks[level].refcount);
      stacks[level].args = xmalloc (sizeof (struct obstack));
      stacks[level].argv = xmalloc (sizeof (struct obstack));
      obstack_init (stacks[level].args);
      obstack_init (stacks[level].argv);
      stacks[level].args_base = obstack_finish (stacks[level].args);
      stacks[level].argv_base = obstack_finish (stacks[level].argv);
    }
  assert (obstack_object_size (stacks[level].args) == 0
	  && obstack_object_size (stacks[level].argv) == 0);
  args_base = obstack_finish (stacks[level].args);
  argv_base = obstack_finish (stacks[level].argv);
  adjust_refcount (level, true);
  stacks[level].argcount++;

  /* Prepare for argument collection.  */
  SYMBOL_PENDING_EXPANSIONS (sym)++;
  expansion_level++;
  if (nesting_limit > 0 && expansion_level > nesting_limit)
    m4_error (EXIT_FAILURE, 0, NULL,
	      _("recursion limit of %d exceeded, use -L<N> to change it"),
	      nesting_limit);

  macro_call_id++;
  my_call_id = macro_call_id;

  traced = (debug_level & DEBUG_TRACE_ALL) || SYMBOL_TRACED (sym);
  if (traced && (debug_level & DEBUG_TRACE_CALL))
    trace_prepre (SYMBOL_NAME (sym), my_call_id);

  argv = collect_arguments (sym, stacks[level].args, stacks[level].argv);
  args_scratch = obstack_finish (stacks[level].args);

  /* The actual macro call.  */
  loc_close_file = current_file;
  loc_close_line = current_line;
  current_file = loc_open_file;
  current_line = loc_open_line;

  if (traced)
    trace_pre (SYMBOL_NAME (sym), my_call_id, argv);

  expansion = push_string_init ();
  call_macro (sym, argv->argc, argv, expansion);
  expanded = push_string_finish ();

  if (traced)
    trace_post (SYMBOL_NAME (sym), my_call_id, argv, expanded);

  /* Cleanup.  */
  current_file = loc_close_file;
  current_line = loc_close_line;

  --expansion_level;
  --SYMBOL_PENDING_EXPANSIONS (sym);

  if (SYMBOL_DELETED (sym))
    free_symbol (sym);

  /* If argv contains references, those refcounts must be reduced now.  */
  if (argv->has_ref)
    {
      token_chain *chain;
      size_t i;
      for (i = 0; i < argv->arraylen; i++)
	if (TOKEN_DATA_TYPE (argv->array[i]) == TOKEN_COMP)
	  {
	    chain = argv->array[i]->u.u_c.chain;
	    while (chain)
	      {
		if (chain->level >= 0)
		  adjust_refcount (chain->level, false);
		chain = chain->next;
	      }
	  }
    }

  /* We no longer need argv, so reduce the refcount.  Additionally, if
     no other references to argv were created, we can free our portion
     of the obstack, although we must leave earlier content alone.  A
     refcount of 0 implies that adjust_refcount already freed the
     entire stack.  */
  if (adjust_refcount (level, false))
    {
      if (argv->inuse)
	{
	  obstack_free (stacks[level].args, args_scratch);
	  if (debug_macro_level & PRINT_ARGCOUNT_CHANGES)
	    xfprintf (debug, "m4debug: -%d- `%s' in use, level=%d, "
		      "refcount=%zu, argcount=%zu\n", my_call_id, argv->argv0,
		      level, stacks[level].refcount, stacks[level].argcount);
	}
      else
	{
	  obstack_free (stacks[level].args, args_base);
	  obstack_free (stacks[level].argv, argv_base);
	  stacks[level].argcount--;
	}
    }
}

/* Adjust the refcount of argument stack LEVEL.  If INCREASE, then
   increase the count, otherwise decrease the count and clear the
   entire stack if the new count is zero.  Return the new
   refcount.  */
size_t
adjust_refcount (int level, bool increase)
{
  assert (level >= 0 && level < stacks_count && stacks[level].args);
  assert (increase || stacks[level].refcount);
  if (increase)
    stacks[level].refcount++;
  else if (--stacks[level].refcount == 0)
    {
      obstack_free (stacks[level].args, stacks[level].args_base);
      obstack_free (stacks[level].argv, stacks[level].argv_base);
      if ((debug_macro_level & PRINT_ARGCOUNT_CHANGES)
	  && stacks[level].argcount > 1)
	xfprintf (debug, "m4debug: -%d- freeing %zu args, level=%d\n",
		  macro_call_id, stacks[level].argcount, level);
      stacks[level].argcount = 0;
    }
  if (debug_macro_level
      & (increase ? PRINT_REFCOUNT_INCREASE : PRINT_REFCOUNT_DECREASE))
    xfprintf (debug, "m4debug: level %d refcount=%d\n", level,
	      stacks[level].refcount);
  return stacks[level].refcount;
}


/* Given ARGV, return the token_data that contains argument INDEX;
   INDEX must be > 0, < argv->argc.  */
static token_data *
arg_token (macro_arguments *argv, unsigned int index)
{
  unsigned int i;
  token_data *token;

  assert (index && index < argv->argc);
  if (!argv->wrapper)
    return argv->array[index - 1];
  /* Must cycle through all tokens, until we find index, since a ref
     may occupy multiple indices.  */
  for (i = 0; i < argv->arraylen; i++)
    {
      token = argv->array[i];
      if (TOKEN_DATA_TYPE (token) == TOKEN_COMP)
	{
	  token_chain *chain = token->u.u_c.chain;
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

/* Mark ARGV as being in use, along with any $@ references that it
   wraps.  */
static void
arg_mark (macro_arguments *argv)
{
  argv->inuse = true;
  if (argv->wrapper)
    {
      /* TODO for now we support only a single-length $@ chain.  */
      assert (argv->arraylen == 1
	      && TOKEN_DATA_TYPE (argv->array[0]) == TOKEN_COMP
	      && !argv->array[0]->u.u_c.chain->next
	      && !argv->array[0]->u.u_c.chain->str);
      argv->array[0]->u.u_c.chain->argv->inuse = true;
    }
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
  /* Composite tokens are currently sequences of text only.  */
  if (type == TOKEN_COMP)
    type = TOKEN_TEXT;
  return type;
}

/* Given ARGV, return the text at argument INDEX.  Abort if the
   argument is not text.  Index 0 is always text, and indices beyond
   argc return the empty string.  The result is always NUL-terminated,
   even if it includes embedded NUL characters.  */
const char *
arg_text (macro_arguments *argv, unsigned int index)
{
  token_data *token;
  token_chain *chain;
  struct obstack *obs;

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
      /* TODO - concatenate multiple arguments?  For now, we assume
	 all elements are text.  */
      chain = token->u.u_c.chain;
      obs = arg_scratch ();
      while (chain)
	{
	  assert (chain->str);
	  obstack_grow (obs, chain->str, chain->len);
	  chain = chain->next;
	}
      obstack_1grow (obs, '\0');
      return (char *) obstack_finish (obs);
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
  token_chain tmpa;
  token_chain tmpb;
  token_chain *ca = &tmpa;
  token_chain *cb = &tmpb;

  /* Quick tests.  */
  if (ta == &empty_token || tb == &empty_token)
    return ta == tb;
  if (TOKEN_DATA_TYPE (ta) == TOKEN_TEXT
      && TOKEN_DATA_TYPE (tb) == TOKEN_TEXT)
    return (TOKEN_DATA_LEN (ta) == TOKEN_DATA_LEN (tb)
	    && memcmp (TOKEN_DATA_TEXT (ta), TOKEN_DATA_TEXT (tb),
		       TOKEN_DATA_LEN (ta)) == 0);

  /* Convert both arguments to chains, if not one already.  */
  /* TODO - allow builtin tokens in the comparison?  */
  if (TOKEN_DATA_TYPE (ta) == TOKEN_TEXT)
    {
      tmpa.next = NULL;
      tmpa.str = TOKEN_DATA_TEXT (ta);
      tmpa.len = TOKEN_DATA_LEN (ta);
    }
  else
    {
      assert (TOKEN_DATA_TYPE (ta) == TOKEN_COMP);
      ca = ta->u.u_c.chain;
    }
  if (TOKEN_DATA_TYPE (tb) == TOKEN_TEXT)
    {
      tmpb.next = NULL;
      tmpb.str = TOKEN_DATA_TEXT (tb);
      tmpb.len = TOKEN_DATA_LEN (tb);
    }
  else
    {
      assert (TOKEN_DATA_TYPE (tb) == TOKEN_COMP);
      cb = tb->u.u_c.chain;
    }

  /* Compare each link of the chain.  */
  while (ca && cb)
    {
      /* TODO support comparison against $@ refs.  */
      assert (ca->str && cb->str);
      if (ca->len == cb->len)
	{
	  if (memcmp (ca->str, cb->str, ca->len) != 0)
	    return false;
	  ca = ca->next;
	  cb = cb->next;
	}
      else if (ca->len < cb->len)
	{
	  if (memcmp (ca->str, cb->str, ca->len) != 0)
	    return false;
	  tmpb.next = cb->next;
	  tmpb.str = cb->str + ca->len;
	  tmpb.len = cb->len - ca->len;
	  ca = ca->next;
	  cb = &tmpb;
	}
      else
	{
	  assert (ca->len > cb->len);
	  if (memcmp (ca->str, cb->str, cb->len) != 0)
	    return false;
	  tmpa.next = ca->next;
	  tmpa.str = ca->str + cb->len;
	  tmpa.len = ca->len - cb->len;
	  ca = &tmpa;
	  cb = cb->next;
	}
    }

  /* If we get this far, the two tokens are equal only if both chains
     are exhausted.  */
  assert (ca != cb || ca == NULL);
  return ca == cb;
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
  token_chain *chain;
  size_t len;

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
      /* TODO - concatenate multiple arguments?  For now, we assume
	 all elements are text.  */
      chain = token->u.u_c.chain;
      len = 0;
      while (chain)
	{
	  assert (chain->str);
	  len += chain->len;
	  chain = chain->next;
	}
      assert (len);
      return len;
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

/* Return an obstack useful for scratch calculations that will not
   interfere with macro expansion.  The obstack will be reset when
   expand_macro completes.  */
struct obstack *
arg_scratch (void)
{
  assert (obstack_object_size (stacks[expansion_level - 1].args) == 0);
  return stacks[expansion_level - 1].args;
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
  struct obstack *obs = arg_scratch ();

  /* When making a reference through a reference, point to the
     original if possible.  */
  if (argv->wrapper)
    {
      /* TODO - for now we support only a single-length $@ chain.  */
      assert (argv->arraylen == 1
	      && TOKEN_DATA_TYPE (argv->array[0]) == TOKEN_COMP);
      chain = argv->array[0]->u.u_c.chain;
      assert (!chain->next && !chain->str);
      argv = chain->argv;
      index += chain->index - 1;
    }
  if (argv->argc <= index)
    {
      new_argv = (macro_arguments *)
	obstack_alloc (obs, offsetof (macro_arguments, array));
      new_argv->arraylen = 0;
      new_argv->wrapper = false;
      new_argv->has_ref = false;
    }
  else
    {
      new_argv = (macro_arguments *)
	obstack_alloc (obs,
		       offsetof (macro_arguments, array) + sizeof token);
      token = (token_data *) obstack_alloc (obs, sizeof *token);
      chain = (token_chain *) obstack_alloc (obs, sizeof *chain);
      new_argv->arraylen = 1;
      new_argv->array[0] = token;
      new_argv->wrapper = true;
      new_argv->has_ref = true;
      TOKEN_DATA_TYPE (token) = TOKEN_COMP;
      token->u.u_c.chain = token->u.u_c.end = chain;
      chain->next = NULL;
      chain->quote_age = argv->quote_age;
      chain->str = NULL;
      chain->len = 0;
      chain->level = expansion_level - 1;
      chain->argv = argv;
      chain->index = index;
      chain->flatten = flatten;
    }
  new_argv->argc = argv->argc - (index - 1);
  new_argv->inuse = false;
  new_argv->argv0 = argv0;
  new_argv->argv0_len = argv0_len;
  new_argv->quote_age = argv->quote_age;
  return new_argv;
}

/* Push argument INDEX from ARGV, which must be a text token, onto the
   expansion stack OBS for rescanning.  */
void
push_arg (struct obstack *obs, macro_arguments *argv, unsigned int index)
{
  token_data *token;

  if (index == 0)
    {
      /* Always push copy of arg 0, since its lifetime is not
	 guaranteed beyond expand_macro.  */
      obstack_grow (obs, argv->argv0, argv->argv0_len);
      return;
    }
  if (index >= argv->argc)
    return;
  token = arg_token (argv, index);
  /* TODO handle func tokens?  */
  if (TOKEN_DATA_TYPE (token) == TOKEN_TEXT)
    {
      if (push_token (token, expansion_level - 1))
	arg_mark (argv);
    }
  else if (TOKEN_DATA_TYPE (token) == TOKEN_COMP)
    {
      /* TODO - concatenate multiple arguments?  For now, we assume
	 all elements are text.  */
      token_chain *chain = token->u.u_c.chain;
      while (chain)
	{
	  assert (chain->str);
	  obstack_grow (obs, chain->str, chain->len);
	  chain = chain->next;
	}
    }
}

/* Push series of comma-separated arguments from ARGV, which should
   all be text, onto the expansion stack OBS for rescanning.  If SKIP,
   then don't push the first argument.  If QUOTE, the rescan also
   includes quoting around each arg.  */
void
push_args (struct obstack *obs, macro_arguments *argv, bool skip, bool quote)
{
  token_data *token;
  token_chain *chain;
  unsigned int i = skip ? 2 : 1;
  const char *sep = ",";
  size_t sep_len = 1;
  bool use_sep = false;
  bool inuse = false;
  struct obstack *scratch = arg_scratch ();

  if (i >= argv->argc)
    return;

  if (i + 1 == argv->argc)
    {
      if (quote)
	obstack_grow (obs, lquote.string, lquote.length);
      push_arg (obs, argv, i);
      if (quote)
	obstack_grow (obs, rquote.string, rquote.length);
      return;
    }

  /* Compute the separator in the scratch space.  */
  if (quote)
    {
      obstack_grow (obs, lquote.string, lquote.length);
      obstack_grow (scratch, rquote.string, rquote.length);
      obstack_1grow (scratch, ',');
      obstack_grow0 (scratch, lquote.string, lquote.length);
      sep = (char *) obstack_finish (scratch);
      sep_len += lquote.length + rquote.length;
    }
  /* TODO push entire $@ reference, rather than pushing each arg.  */
  for ( ; i < argv->argc; i++)
    {
      token = arg_token (argv, i);
      if (use_sep)
	obstack_grow (obs, sep, sep_len);
      else
	use_sep = true;
      /* TODO handle func tokens?  */
      if (TOKEN_DATA_TYPE (token) == TOKEN_TEXT)
	inuse |= push_token (token, expansion_level - 1);
      else
	{
	  /* TODO - handle composite text in push_token.  */
	  assert (TOKEN_DATA_TYPE (token) == TOKEN_COMP);
	  chain = token->u.u_c.chain;
	  while (chain)
	    {
	      assert (chain->str);
	      obstack_grow (obs, chain->str, chain->len);
	      chain = chain->next;
	    }
	}
    }
  if (quote)
    obstack_grow (obs, rquote.string, rquote.length);
  if (inuse)
    arg_mark (argv);
}

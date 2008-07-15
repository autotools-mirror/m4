/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2006, 2007, 2008,
   2009 Free Software Foundation, Inc.

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
   macro name at index 0.  The lifetime of info is only guaranteed
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
  /* True to flatten builtins contained in references.  */
  bool_bitfield flatten : 1;
  /* True if any token contains builtins.  */
  bool_bitfield has_func : 1;
  /* The value of quote_age used when parsing all arguments in this
     object, or 0 if quote_age changed during parsing or if any of the
     arguments might contain content that can affect rescan.  */
  unsigned int quote_age;
  /* The context of this macro call during expansion, and NULL in a
     back-reference.  */
  call_info *info;
  int level; /* Which obstack owns this argv.  */
  unsigned int arraylen; /* True length of allocated elements in array.  */
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
static int macro_call_id;

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
  TOKEN_DATA_ORIG_LEN (&empty_token) = 0;
#endif

  while ((t = next_token (&td, &line, NULL, false, NULL)) != TOKEN_EOF)
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
  bool result = false;

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
    case TOKEN_COMMENT:
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
      result = *TOKEN_DATA_TEXT (td) != *curr_quote.str2 && safe_quotes ();
      if (result)
	assert (*TOKEN_DATA_TEXT (td) != *curr_quote.str1);
      break;

    case TOKEN_WORD:
      sym = lookup_symbol (TOKEN_DATA_TEXT (td), TOKEN_DATA_LEN (td),
			   SYMBOL_LOOKUP);
      if (sym == NULL || SYMBOL_TYPE (sym) == TOKEN_VOID
	  || (SYMBOL_TYPE (sym) == TOKEN_FUNC
	      && SYMBOL_BLIND_NO_ARGS (sym)
	      && peek_token () != TOKEN_OPEN))
	{
#ifdef ENABLE_CHANGEWORD
	  divert_text (obs, TOKEN_DATA_ORIG_TEXT (td),
		       TOKEN_DATA_ORIG_LEN (td), line);
#else
	  divert_text (obs, TOKEN_DATA_TEXT (td), TOKEN_DATA_LEN (td), line);
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
  divert_text (obs, TOKEN_DATA_TEXT (td), TOKEN_DATA_LEN (td), line);
  return result;
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
expand_argument (struct obstack *obs, token_data *argp,
		 const call_info *caller)
{
  token_type t;
  token_data td;
  int paren_level;
  int line = current_line;
  unsigned int age = quote_age ();
  bool first = true;

  TOKEN_DATA_TYPE (argp) = TOKEN_VOID;

  /* Skip leading white space.  */
  do
    {
      t = next_token (&td, NULL, obs, true, caller);
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
	      assert (TOKEN_DATA_TYPE (argp) != TOKEN_FUNC);
	      if (TOKEN_DATA_TYPE (argp) != TOKEN_COMP)
		{
		  size_t len = obstack_object_size (obs);
		  TOKEN_DATA_TYPE (argp) = TOKEN_TEXT;
		  if (len)
		    {
		      obstack_1grow (obs, '\0');
		      TOKEN_DATA_TEXT (argp) = (char *) obstack_finish (obs);
		    }
		  else
		    TOKEN_DATA_TEXT (argp) = NULL;
		  TOKEN_DATA_LEN (argp) = len;
		  TOKEN_DATA_QUOTE_AGE (argp) = age;
		}
	      else
		{
		  make_text_link (obs, NULL, &argp->u.u_c.end);
		  if (argp->u.u_c.chain == argp->u.u_c.end
		      && argp->u.u_c.chain->type == CHAIN_FUNC)
		    {
		      builtin_func *func = argp->u.u_c.chain->u.func;
		      TOKEN_DATA_TYPE (argp) = TOKEN_FUNC;
		      TOKEN_DATA_FUNC (argp) = func;
		    }
		}
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
	  m4_error (EXIT_FAILURE, 0, caller,
		    _("end of file in argument list"));
	  break;

	case TOKEN_WORD:
	case TOKEN_STRING:
	case TOKEN_COMMENT:
	case TOKEN_MACDEF:
	  if (!expand_token (obs, t, &td, line, first))
	    age = 0;
	  if (TOKEN_DATA_TYPE (&td) == TOKEN_COMP)
	    {
	      if (TOKEN_DATA_TYPE (argp) != TOKEN_COMP)
		{
		  TOKEN_DATA_TYPE (argp) = TOKEN_COMP;
		  argp->u.u_c.chain = td.u.u_c.chain;
		  argp->u.u_c.wrapper = argp->u.u_c.has_func = false;
		}
	      else
		{
		  assert (argp->u.u_c.end);
		  argp->u.u_c.end->next = td.u.u_c.chain;
		}
	      argp->u.u_c.end = td.u.u_c.end;
	      if (td.u.u_c.has_func)
		argp->u.u_c.has_func = true;
	    }
	  break;

	case TOKEN_ARGV:
	  assert (paren_level == 0 && TOKEN_DATA_TYPE (argp) == TOKEN_VOID
		  && obstack_object_size (obs) == 0
		  && td.u.u_c.chain == td.u.u_c.end
		  && td.u.u_c.chain->quote_age == age
		  && td.u.u_c.chain->type == CHAIN_ARGV);
	  TOKEN_DATA_TYPE (argp) = TOKEN_COMP;
	  argp->u.u_c.chain = argp->u.u_c.end = td.u.u_c.chain;
	  argp->u.u_c.wrapper = true;
	  argp->u.u_c.has_func = td.u.u_c.has_func;
	  t = next_token (&td, NULL, NULL, false, caller);
	  if (argp->u.u_c.chain->u.u_a.skip_last)
	    assert (t == TOKEN_COMMA);
	  else
	    assert (t == TOKEN_COMMA || t == TOKEN_CLOSE);
	  return t == TOKEN_COMMA;

	default:
	  assert (!"expand_argument");
	  abort ();
	}

      if (TOKEN_DATA_TYPE (argp) != TOKEN_VOID || obstack_object_size (obs))
	first = false;
      t = next_token (&td, NULL, obs, first, caller);
    }
}

/*-------------------------------------------------------------------------.
| Collect all the arguments to a call of the macro SYM.  The arguments are |
| stored on the obstack ARGUMENTS and a table of pointers to the arguments |
| on the obstack argv_stack.						   |
`-------------------------------------------------------------------------*/

static macro_arguments *
collect_arguments (symbol *sym, call_info *info, struct obstack *arguments,
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
  args.flatten = !groks_macro_args;
  args.has_func = false;
  args.quote_age = quote_age ();
  args.info = info;
  args.level = expansion_level - 1;
  args.arraylen = 0;
  obstack_grow (argv_stack, &args, offsetof (macro_arguments, array));

  if (peek_token () == TOKEN_OPEN)
    {
      /* gobble parenthesis */
      next_token (&td, NULL, NULL, false, info);
      do
	{
	  tdp = (token_data *) obstack_alloc (arguments, sizeof *tdp);
	  more_args = expand_argument (arguments, tdp, info);

	  if ((TOKEN_DATA_TYPE (tdp) == TOKEN_TEXT && !TOKEN_DATA_LEN (tdp))
	      || (!groks_macro_args && TOKEN_DATA_TYPE (tdp) == TOKEN_FUNC))
	    {
	      obstack_free (arguments, tdp);
	      tdp = &empty_token;
	    }
	  obstack_ptr_grow (argv_stack, tdp);
	  args.arraylen++;
	  args.argc++;
	  switch (TOKEN_DATA_TYPE (tdp))
	    {
	    case TOKEN_TEXT:
	      /* Be conservative - any change in quoting while
		 collecting arguments, or any argument that consists
		 of unsafe text, will require a rescan if $@ is
		 reused.  */
	      if (TOKEN_DATA_LEN (tdp) > 0
		  && TOKEN_DATA_QUOTE_AGE (tdp) != args.quote_age)
		args.quote_age = 0;
	      break;
	    case TOKEN_FUNC:
	      args.has_func = true;
	      break;
	    case TOKEN_COMP:
	      args.has_ref = true;
	      if (tdp->u.u_c.wrapper)
		{
		  assert (tdp->u.u_c.chain->type == CHAIN_ARGV
			  && !tdp->u.u_c.chain->next);
		  args.argc += (tdp->u.u_c.chain->u.u_a.argv->argc
				- tdp->u.u_c.chain->u.u_a.index
				- tdp->u.u_c.chain->u.u_a.skip_last - 1);
		  args.wrapper = true;
		}
	      if (tdp->u.u_c.has_func)
		args.has_func = true;
	      break;
	    default:
	      assert (!"expand_argument");
	      abort ();
	    }
	}
      while (more_args);
    }
  argv = (macro_arguments *) obstack_finish (argv_stack);
  argv->argc = args.argc;
  argv->wrapper = args.wrapper;
  argv->has_ref = args.has_ref;
  argv->has_func = args.has_func;
  if (args.quote_age != quote_age ())
    argv->quote_age = 0;
  argv->arraylen = args.arraylen;
  return argv;
}


/*-------------------------------------------------------------------.
| Call the macro SYM, which is either a builtin function or a user   |
| macro (via the expansion function expand_user_macro () in          |
| builtin.c).  The arguments are provided by ARGV.  The expansion is |
| left on the obstack EXPANSION.  Macro tracing is also handled      |
| here.                                                              |
`-------------------------------------------------------------------*/

void
call_macro (symbol *sym, macro_arguments *argv, struct obstack *expansion)
{
  unsigned int trace_start = 0;

  if (argv->info->trace)
    trace_start = trace_pre (argv);
  switch (SYMBOL_TYPE (sym))
    {
    case TOKEN_FUNC:
      SYMBOL_FUNC (sym) (expansion, argv->argc, argv);
      break;

    case TOKEN_TEXT:
      expand_user_macro (expansion, sym, argv->argc, argv);
      break;

    default:
      assert (!"call_macro");
      abort ();
    }
  if (argv->info->trace)
    trace_post (trace_start, argv->info);
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
  int level = expansion_level;	/* Expansion level of this macro.  */
  call_info my_call_info;	/* Context of this macro.  */

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
      stacks[level].args =
	(struct obstack *) xmalloc (sizeof *stacks[level].args);
      stacks[level].argv =
	(struct obstack *) xmalloc (sizeof *stacks[level].argv);
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

  /* Collect context in effect at start of macro, even if global state
     changes in the meantime.  */
  my_call_info.file = current_file;
  my_call_info.line = current_line;
  my_call_info.call_id = ++macro_call_id;
  my_call_info.trace = (debug_level & DEBUG_TRACE_ALL) || SYMBOL_TRACED (sym);
  my_call_info.debug_level = debug_level;
  my_call_info.name = SYMBOL_NAME (sym);
  my_call_info.name_len = SYMBOL_NAME_LEN (sym);
  trace_prepre (&my_call_info);

  /* Collect the arguments.  */
  argv = collect_arguments (sym, &my_call_info, stacks[level].args,
			    stacks[level].argv);
  args_scratch = obstack_finish (stacks[level].args);

  /* The actual macro call.  */
  expansion = push_string_init (my_call_info.file, my_call_info.line);
  call_macro (sym, argv, expansion);
  push_string_finish ();

  /* Cleanup.  */
  argv->info = NULL;
  --expansion_level;
  assert (SYMBOL_PENDING_EXPANSIONS (sym));
  --SYMBOL_PENDING_EXPANSIONS (sym);

  if (SYMBOL_DELETED (sym))
    free_symbol (sym);

  /* We no longer need argv, so reduce the refcount.  Additionally, if
     no other references to argv were created, we can free our portion
     of the obstack, although we must leave earlier content alone.  A
     refcount of 0 implies that adjust_refcount already freed the
     entire stack.  */
  arg_adjust_refcount (argv, false);
  if (stacks[level].refcount)
    {
      if (argv->inuse)
	{
	  obstack_free (stacks[level].args, args_scratch);
	  if (debug_macro_level & PRINT_ARGCOUNT_CHANGES)
	    xfprintf (debug, "m4debug: -%d- `%s' in use, level=%d, "
		      "refcount=%zu, argcount=%zu\n", my_call_info.call_id,
		      my_call_info.name, level, stacks[level].refcount,
		      stacks[level].argcount);
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

/* Given ARGV, adjust the refcount of every reference it contains in
   the direction decided by INCREASE.  Return true if increasing
   references to ARGV implies the first use of ARGV.  */
bool
arg_adjust_refcount (macro_arguments *argv, bool increase)
{
  unsigned int i;
  token_chain *chain;
  bool result = !argv->inuse;

  if (argv->has_ref)
    for (i = 0; i < argv->arraylen; i++)
      if (TOKEN_DATA_TYPE (argv->array[i]) == TOKEN_COMP)
	{
	  chain = argv->array[i]->u.u_c.chain;
	  while (chain)
	    {
	      switch (chain->type)
		{
		case CHAIN_STR:
		  if (chain->u.u_s.level >= 0)
		    {
		      assert (!chain->u.u_s.sym);
		      adjust_refcount (chain->u.u_s.level, increase);
		    }
		  else if (chain->u.u_s.sym)
		    {
		      symbol *sym = chain->u.u_s.sym;
		      if (increase)
			SYMBOL_PENDING_EXPANSIONS (sym)++;
		      else
			{
			  assert (SYMBOL_PENDING_EXPANSIONS (sym));
			  --SYMBOL_PENDING_EXPANSIONS (sym);
			  if (SYMBOL_DELETED (sym))
			    free_symbol (sym);
			}
		    }
		  break;
		case CHAIN_FUNC:
		  break;
		case CHAIN_ARGV:
		  assert (chain->u.u_a.argv->inuse);
		  arg_adjust_refcount (chain->u.u_a.argv, increase);
		  break;
		default:
		  assert (!"arg_adjust_refcount");
		  abort ();
		}
	      chain = chain->next;
	    }
	}
  adjust_refcount (argv->level, increase);
  return result;
}


/* Given ARGV, return the token_data that contains argument ARG; ARG
   must be > 0, < argv->argc.  If LEVEL is non-NULL, *LEVEL is set to
   the obstack level that contains the token (which is not necessarily
   the level of ARGV).  If FLATTEN, avoid returning a builtin
   function.  */
static token_data *
arg_token (macro_arguments *argv, unsigned int arg, int *level, bool flatten)
{
  unsigned int i;
  token_data *token = NULL;

  assert (arg && arg < argv->argc);
  if (level)
    *level = argv->level;
  flatten |= argv->flatten;
  if (!argv->wrapper)
    {
      token = argv->array[arg - 1];
      if (flatten && TOKEN_DATA_TYPE (token) == TOKEN_FUNC)
	token = &empty_token;
      return token;
    }

  /* Must cycle through all tokens, until we find arg, since a ref
     may occupy multiple indices.  */
  for (i = 0; i < argv->arraylen; i++)
    {
      token = argv->array[i];
      if (TOKEN_DATA_TYPE (token) == TOKEN_COMP && token->u.u_c.wrapper)
	{
	  token_chain *chain = token->u.u_c.chain;
	  assert (!chain->next && chain->type == CHAIN_ARGV);
	  if (arg <= (chain->u.u_a.argv->argc - chain->u.u_a.index
		      - chain->u.u_a.skip_last))
	    {
	      token = arg_token (chain->u.u_a.argv,
				 chain->u.u_a.index - 1 + arg, level,
				 flatten || chain->u.u_a.flatten);
	      break;
	    }
	  arg -= (chain->u.u_a.argv->argc - chain->u.u_a.index
		  - chain->u.u_a.skip_last);
	}
      else if (--arg == 0)
	break;
    }
  return token;
}

/* Mark ARGV as being in use, along with any $@ references that it
   wraps.  */
static void
arg_mark (macro_arguments *argv)
{
  unsigned int i;
  token_chain *chain;

  if (argv->inuse)
    return;
  argv->inuse = true;
  if (argv->wrapper)
    for (i = 0; i < argv->arraylen; i++)
      if (TOKEN_DATA_TYPE (argv->array[i]) == TOKEN_COMP
	  && argv->array[i]->u.u_c.wrapper)
	{
	  chain = argv->array[i]->u.u_c.chain;
	  assert (!chain->next && chain->type == CHAIN_ARGV);
	  if (!chain->u.u_a.argv->inuse)
	    arg_mark (chain->u.u_a.argv);
	}
}

/* Given ARGV, return how many arguments it refers to.  */
unsigned int
arg_argc (macro_arguments *argv)
{
  return argv->argc;
}

/* Given ARGV, return the call context in effect when argument
   collection began.  Only safe to call while the macro is being
   expanded.  */
const call_info *
arg_info (macro_arguments *argv)
{
  assert (argv->info);
  return argv->info;
}

/* Given ARGV, return the quote age in effect when argument collection
   completed, or zero if all arguments do not have the same quote
   age.  */
unsigned int
arg_quote_age (macro_arguments *argv)
{
  return argv->quote_age;
}

/* Given ARGV, return the type of argument ARG.  Arg 0 is always text,
   and indices beyond argc are likewise treated as text.  */
token_data_type
arg_type (macro_arguments *argv, unsigned int arg)
{
  token_data_type type;
  token_data *token;

  if (argv->flatten || !argv->has_func || arg == 0 || arg >= argv->argc)
    return TOKEN_TEXT;
  token = arg_token (argv, arg, NULL, false);
  type = TOKEN_DATA_TYPE (token);
  if (type == TOKEN_COMP && !token->u.u_c.has_func)
    type = TOKEN_TEXT;
  if (type != TOKEN_TEXT)
    assert (argv->has_func);
  return type;
}

/* Given a CHAIN, convert its links into text on OBS.  If FLATTEN,
   builtins are ignored.  */
static void
collect_chain (struct obstack *obs, token_chain *chain, bool flatten)
{
  while (chain)
    {
      switch (chain->type)
	{
	case CHAIN_STR:
	  obstack_grow (obs, chain->u.u_s.str, chain->u.u_s.len);
	  break;
	case CHAIN_FUNC:
	  if (flatten)
	    break;
	  assert (!"flatten_chain");
	  abort ();
	case CHAIN_ARGV:
	  assert (!chain->u.u_a.has_func || flatten);
	  arg_print (obs, chain->u.u_a.argv, chain->u.u_a.index,
		     quote_cache (NULL, chain->quote_age, chain->u.u_a.quotes),
		     flatten || chain->u.u_a.flatten, NULL, NULL, NULL, false);
	  break;
	default:
	  assert (!"flatten_chain");
	  abort ();
	}
      chain = chain->next;
    }
}

/* Given ARGV, return the text at argument ARG.  Abort if the argument
   is not text.  Arg 0 is always text, and indices beyond argc return
   the empty string.  If FLATTEN, builtins are ignored.  The result is
   always NUL-terminated, even if it includes embedded NUL
   characters.  */
const char *
arg_text (macro_arguments *argv, unsigned int arg, bool flatten)
{
  token_data *token;
  token_chain *chain;
  struct obstack *obs; /* Scratch space; cleaned at end of macro_expand.  */

  if (arg == 0)
    {
      assert (argv->info);
      return argv->info->name;
    }
  if (arg >= argv->argc)
    return "";
  token = arg_token (argv, arg, NULL, flatten);
  switch (TOKEN_DATA_TYPE (token))
    {
    case TOKEN_TEXT:
      return TOKEN_DATA_TEXT (token);
    case TOKEN_COMP:
      chain = token->u.u_c.chain;
      obs = arg_scratch ();
      collect_chain (obs, chain, flatten || argv->flatten);
      obstack_1grow (obs, '\0');
      return (char *) obstack_finish (obs);
    case TOKEN_FUNC:
    default:
      break;
    }
  assert (!"arg_text");
  abort ();
}

/* Given ARGV, compare text arguments INDEXA and INDEXB for equality.
   Both indices must be non-zero and less than argc.  Return true if
   the arguments contain the same contents; often more efficient than
   strcmp (arg_text (argv, a, 1), arg_text (argv, b, 1)) == 0.  */
bool
arg_equal (macro_arguments *argv, unsigned int indexa, unsigned int indexb)
{
  token_data *ta = arg_token (argv, indexa, NULL, false);
  token_data *tb = arg_token (argv, indexb, NULL, false);
  token_chain tmpa;
  token_chain tmpb;
  token_chain *ca = &tmpa;
  token_chain *cb = &tmpb;
  token_chain *chain;
  struct obstack *obs = arg_scratch ();

  /* Quick tests.  */
  if (ta == &empty_token || tb == &empty_token)
    return ta == tb;
  if (TOKEN_DATA_TYPE (ta) == TOKEN_TEXT
      && TOKEN_DATA_TYPE (tb) == TOKEN_TEXT)
    return (TOKEN_DATA_LEN (ta) == TOKEN_DATA_LEN (tb)
	    && memcmp (TOKEN_DATA_TEXT (ta), TOKEN_DATA_TEXT (tb),
		       TOKEN_DATA_LEN (ta)) == 0);

  /* Convert both arguments to chains, if not one already.  */
  switch (TOKEN_DATA_TYPE (ta))
    {
    case TOKEN_TEXT:
      tmpa.next = NULL;
      tmpa.type = CHAIN_STR;
      tmpa.u.u_s.str = TOKEN_DATA_TEXT (ta);
      tmpa.u.u_s.len = TOKEN_DATA_LEN (ta);
      break;
    case TOKEN_FUNC:
      tmpa.next = NULL;
      tmpa.type = CHAIN_FUNC;
      tmpa.u.func = TOKEN_DATA_FUNC (ta);
      break;
    case TOKEN_COMP:
      ca = ta->u.u_c.chain;
      break;
    default:
      assert (!"arg_equal");
      abort ();
    }
  switch (TOKEN_DATA_TYPE (tb))
    {
    case TOKEN_TEXT:
      tmpb.next = NULL;
      tmpb.type = CHAIN_STR;
      tmpb.u.u_s.str = TOKEN_DATA_TEXT (tb);
      tmpb.u.u_s.len = TOKEN_DATA_LEN (tb);
      break;
    case TOKEN_FUNC:
      tmpb.next = NULL;
      tmpb.type = CHAIN_FUNC;
      tmpb.u.func = TOKEN_DATA_FUNC (tb);
      break;
    case TOKEN_COMP:
      cb = tb->u.u_c.chain;
      break;
    default:
      assert (!"arg_equal");
      abort ();
    }

  /* Compare each link of the chain.  */
  while (ca && cb)
    {
      if (ca->type == CHAIN_ARGV)
	{
	  tmpa.next = NULL;
	  tmpa.type = CHAIN_STR;
	  tmpa.u.u_s.str = NULL;
	  tmpa.u.u_s.len = 0;
	  chain = &tmpa;
	  arg_print (obs, ca->u.u_a.argv, ca->u.u_a.index,
		     quote_cache (NULL, ca->quote_age, ca->u.u_a.quotes),
		     argv->flatten || ca->u.u_a.flatten, &chain, NULL, NULL,
		     false);
	  assert (obstack_object_size (obs) == 0 && chain != &tmpa);
	  chain->next = ca->next;
	  ca = tmpa.next;
	  continue;
	}
      if (cb->type == CHAIN_ARGV)
	{
	  tmpb.next = NULL;
	  tmpb.type = CHAIN_STR;
	  tmpb.u.u_s.str = NULL;
	  tmpb.u.u_s.len = 0;
	  chain = &tmpb;
	  arg_print (obs, cb->u.u_a.argv, cb->u.u_a.index,
		     quote_cache (NULL, cb->quote_age, cb->u.u_a.quotes),
		     argv->flatten || cb->u.u_a.flatten, &chain, NULL, NULL,
		     false);
	  assert (obstack_object_size (obs) == 0 && chain != &tmpb);
	  chain->next = cb->next;
	  cb = tmpb.next;
	  continue;
	}
      if (ca->type == CHAIN_FUNC)
	{
	  if (cb->type != CHAIN_FUNC || ca->u.func != cb->u.func)
	    return false;
	  ca = ca->next;
	  cb = cb->next;
	  continue;
	}
      assert (ca->type == CHAIN_STR && cb->type == CHAIN_STR);
      if (ca->u.u_s.len == cb->u.u_s.len)
	{
	  if (memcmp (ca->u.u_s.str, cb->u.u_s.str, ca->u.u_s.len) != 0)
	    return false;
	  ca = ca->next;
	  cb = cb->next;
	}
      else if (ca->u.u_s.len < cb->u.u_s.len)
	{
	  if (memcmp (ca->u.u_s.str, cb->u.u_s.str, ca->u.u_s.len) != 0)
	    return false;
	  tmpb.next = cb->next;
	  tmpb.type = CHAIN_STR;
	  tmpb.u.u_s.str = cb->u.u_s.str + ca->u.u_s.len;
	  tmpb.u.u_s.len = cb->u.u_s.len - ca->u.u_s.len;
	  ca = ca->next;
	  cb = &tmpb;
	}
      else
	{
	  assert (ca->u.u_s.len > cb->u.u_s.len);
	  if (memcmp (ca->u.u_s.str, cb->u.u_s.str, cb->u.u_s.len) != 0)
	    return false;
	  tmpa.next = ca->next;
	  tmpa.type = CHAIN_STR;
	  tmpa.u.u_s.str = ca->u.u_s.str + cb->u.u_s.len;
	  tmpa.u.u_s.len = ca->u.u_s.len - cb->u.u_s.len;
	  ca = &tmpa;
	  cb = cb->next;
	}
    }

  /* If we get this far, the two tokens are equal only if both chains
     are exhausted.  */
  assert (ca != cb || ca == NULL);
  return ca == cb;
}

/* Given index ARG within ARGV, if the argument is determined to be
   appending text onto the existing definition DEFN of length LEN,
   then return only the text occurring after LEN.  Otherwise, return
   NULL.  This is useful for optimizing the builtin define, making
   appending O(n) rather than O(n^2).  */
const char *
arg_has_prefix (macro_arguments *argv, unsigned int arg, const char *defn,
		size_t len)
{
  token_data *token;
  token_chain *chain;
  struct obstack *obs; /* Scratch space; cleaned at end of macro_expand.  */

  if (arg == 0 || arg >= argv->argc)
    return NULL;
  token = arg_token (argv, arg, NULL, false);
  if (TOKEN_DATA_TYPE (token) != TOKEN_COMP
      || (!argv->flatten && token->u.u_c.has_func))
    return NULL;
  chain = token->u.u_c.chain;
  assert (chain);
  if (chain->type == CHAIN_STR && chain->u.u_s.str == defn
      && chain->u.u_s.len == len)
    {
      obs = arg_scratch ();
      collect_chain (obs, chain->next, argv->flatten);
      return (char *) obstack_finish (obs);
    }
  return NULL;
}

/* Given ARGV, return true if argument ARG is the empty string.  This
   gives the same result as comparing arg_len against 0, but is often
   faster.  */
bool
arg_empty (macro_arguments *argv, unsigned int arg)
{
  if (arg == 0)
    {
      assert (argv->info);
      return argv->info->name_len == 0;
    }
  if (arg >= argv->argc)
    return true;
  return arg_token (argv, arg, NULL, false) == &empty_token;
}

/* Given ARGV, return the length of argument ARG.  Abort if the
   argument is not text.  Indices beyond argc return 0.  If FLATTEN,
   builtins are ignored.  */
size_t
arg_len (macro_arguments *argv, unsigned int arg, bool flatten)
{
  token_data *token;
  token_chain *chain;
  size_t len;

  if (arg == 0)
    {
      assert (argv->info);
      return argv->info->name_len;
    }
  if (arg >= argv->argc)
    return 0;
  token = arg_token (argv, arg, NULL, flatten);
  switch (TOKEN_DATA_TYPE (token))
    {
    case TOKEN_TEXT:
      assert ((token == &empty_token) == (TOKEN_DATA_LEN (token) == 0));
      return TOKEN_DATA_LEN (token);
    case TOKEN_COMP:
      chain = token->u.u_c.chain;
      len = 0;
      while (chain)
	{
	  unsigned int i;
	  unsigned int limit;
	  const string_pair *quotes;
	  switch (chain->type)
	    {
	    case CHAIN_STR:
	      len += chain->u.u_s.len;
	      break;
	    case CHAIN_FUNC:
	      assert (flatten);
	      break;
	    case CHAIN_ARGV:
	      i = chain->u.u_a.index;
	      limit = chain->u.u_a.argv->argc - i - chain->u.u_a.skip_last;
	      quotes = quote_cache (NULL, chain->quote_age,
				    chain->u.u_a.quotes);
	      assert (limit);
	      if (quotes)
		len += (quotes->len1 + quotes->len2) * limit;
	      len += limit - 1;
	      while (limit--)
		len += arg_len (chain->u.u_a.argv, i++,
				flatten || chain->u.u_a.flatten);
	      break;
	    default:
	      assert (!"arg_len");
	      abort ();
	    }
	  chain = chain->next;
	}
      assert (len || flatten);
      return len;
    case TOKEN_FUNC:
    default:
      break;
    }
  assert (!"arg_len");
  abort ();
}

/* Given ARGV, return the builtin function referenced by argument ARG.
   Abort if it is not a builtin in isolation.  */
builtin_func *
arg_func (macro_arguments *argv, unsigned int arg)
{
  token_data *token;

  token = arg_token (argv, arg, NULL, false);
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

/* Dump a representation of ARGV to the obstack OBS, starting with
   argument ARG.  If QUOTES is non-NULL, each argument is displayed
   with those quotes.  If FLATTEN, builtins are converted to empty
   quotes; if CHAINP, *CHAINP is updated with macro tokens; otherwise,
   builtins are represented by their name.  Separate arguments with
   SEP, which defaults to a comma.  If MAX_LEN is non-NULL, truncate
   the output after *MAX_LEN bytes are output and return true;
   otherwise, return false, and reduce *MAX_LEN by the number of bytes
   output.  If QUOTE_EACH, the truncation length is reset for each
   argument, quotes do not count against length, and all arguments are
   printed; otherwise, quotes count against the length and trailing
   arguments may be discarded.  MAX_LEN and CHAINP may not both be
   specified.  */
bool
arg_print (struct obstack *obs, macro_arguments *argv, unsigned int arg,
	   const string_pair *quotes, bool flatten, token_chain **chainp,
	   const char *sep, size_t *max_len, bool quote_each)
{
  int len = max_len ? *max_len : INT_MAX;
  unsigned int i;
  token_data *token;
  token_chain *chain;
  bool use_sep = false;
  bool done;
  size_t sep_len;
  size_t *plen = quote_each ? NULL : &len;

  flatten |= argv->flatten;
  if (chainp)
    assert (!max_len && *chainp);
  if (!sep)
    sep = ",";
  sep_len = strlen (sep);
  for (i = arg; i < argv->argc; i++)
    {
      if (quote_each && max_len)
	len = *max_len;
      if (use_sep && shipout_string_trunc (obs, sep, sep_len, plen))
	return true;
      use_sep = true;
      token = arg_token (argv, i, NULL, flatten);
      switch (TOKEN_DATA_TYPE (token))
	{
	case TOKEN_TEXT:
	  if (quotes && shipout_string_trunc (obs, quotes->str1, quotes->len1,
					      plen))
	    return true;
	  if (shipout_string_trunc (obs, TOKEN_DATA_TEXT (token),
				    TOKEN_DATA_LEN (token), &len)
	      && !quote_each)
	    return true;
	  if (quotes && shipout_string_trunc (obs, quotes->str2, quotes->len2,
					      plen))
	    return true;
	  break;
	case TOKEN_COMP:
	  if (quotes && shipout_string_trunc (obs, quotes->str1, quotes->len1,
					      plen))
	    return true;
	  chain = token->u.u_c.chain;
	  done = false;
	  while (chain && !done)
	    {
	      switch (chain->type)
		{
		case CHAIN_STR:
		  if (shipout_string_trunc (obs, chain->u.u_s.str,
					    chain->u.u_s.len, &len))
		    done = true;
		  break;
		case CHAIN_FUNC:
		  func_print (obs, find_builtin_by_addr (chain->u.func),
			      flatten, chainp, quotes);
		  break;
		case CHAIN_ARGV:
		  if (arg_print (obs, chain->u.u_a.argv, chain->u.u_a.index,
				 quote_cache (NULL, chain->quote_age,
					      chain->u.u_a.quotes),
				 flatten, chainp, NULL, &len, false))
		    done = true;
		  break;
		default:
		  assert (!"arg_print");
		  abort ();
		}
	      chain = chain->next;
	    }
	  if (done && !quote_each)
	    return true;
	  if (quotes && shipout_string_trunc (obs, quotes->str2, quotes->len2,
					      plen))
	    return true;
	  break;
	case TOKEN_FUNC:
	  func_print (obs, find_builtin_by_addr (TOKEN_DATA_FUNC (token)),
		      flatten, chainp, quotes);
	  break;
	default:
	  assert (!"arg_print");
	  abort ();
	}
    }
  if (max_len)
    *max_len = len;
  else if (chainp)
    make_text_link (obs, NULL, chainp);
  return false;
}

/* Populate the new TOKEN as a wrapper to ARGV, starting with argument
   ARG.  Allocate any data on OBS, owned by a given expansion LEVEL.
   FLATTEN determines whether to allow builtins, and QUOTES determines
   whether all arguments are quoted.  Return TOKEN when successful,
   NULL when wrapping ARGV is trivially empty.  */
static token_data *
make_argv_ref_token (token_data *token, struct obstack *obs, int level,
		     macro_arguments *argv, unsigned int arg, bool flatten,
		     const string_pair *quotes)
{
  token_chain *chain;

  if (arg >= argv->argc)
    return NULL;
  TOKEN_DATA_TYPE (token) = TOKEN_COMP;
  token->u.u_c.chain = token->u.u_c.end = NULL;

  /* Cater to the common idiom of $0(`$1',shift(shift($@))), by
     inlining the first few arguments and reusing the original $@ ref,
     rather than creating another layer of wrappers.  */
  while (argv->wrapper)
    {
      unsigned int i;
      for (i = 0; i < argv->arraylen; i++)
	{
	  if ((TOKEN_DATA_TYPE (argv->array[i]) == TOKEN_COMP
	       && argv->array[i]->u.u_c.wrapper)
	      || level >= 0)
	    break;
	  if (arg == 1)
	    {
	      push_arg_quote (obs, argv, i + 1, quotes);
	      obstack_1grow (obs, ',');
	    }
	  else
	    arg--;
	}
      assert (i < argv->arraylen);
      if (i + 1 == argv->arraylen)
	{
	  assert (TOKEN_DATA_TYPE (argv->array[i]) == TOKEN_COMP
		  && argv->array[i]->u.u_c.wrapper);
	  chain = argv->array[i]->u.u_c.chain;
	  assert (!chain->next && chain->type == CHAIN_ARGV
		  && !chain->u.u_a.skip_last);
	  argv = chain->u.u_a.argv;
	  arg += chain->u.u_a.index - 1;
	}
      else
	{
	  arg += i;
	  break;
	}
    }

  make_text_link (obs, &token->u.u_c.chain, &token->u.u_c.end);
  chain = (token_chain *) obstack_alloc (obs, sizeof *chain);
  if (token->u.u_c.end)
    token->u.u_c.end->next = chain;
  else
    token->u.u_c.chain = chain;
  token->u.u_c.end = chain;
  token->u.u_c.wrapper = true;
  token->u.u_c.has_func = argv->has_func;
  chain->next = NULL;
  chain->type = CHAIN_ARGV;
  chain->quote_age = argv->quote_age;
  chain->u.u_a.argv = argv;
  chain->u.u_a.index = arg;
  chain->u.u_a.flatten = flatten;
  chain->u.u_a.has_func = argv->has_func;
  chain->u.u_a.comma = false;
  chain->u.u_a.skip_last = false;
  chain->u.u_a.quotes = quote_cache (obs, chain->quote_age, quotes);
  return token;
}

/* Create a new argument object using the same obstack as ARGV; thus,
   the new object will automatically be freed when the original is
   freed.  Explicitly set the macro name (argv[0]) from ARGV0 with
   length ARGV0_LEN, and discard argv[1] of the wrapped ARGV.  If
   FLATTEN, any non-text in ARGV is flattened to an empty string when
   referenced through the new object.  If TRACE, then trace the macro,
   regardless of global trace state.  */
macro_arguments *
make_argv_ref (macro_arguments *argv, const char *argv0, size_t argv0_len,
	       bool flatten, bool trace)
{
  macro_arguments *new_argv;
  token_data *token;
  token_data *new_token;
  struct obstack *obs = arg_scratch ();
  call_info *info;

  info = (call_info *) obstack_copy (obs, argv->info, sizeof *info);
  new_token = (token_data *) obstack_alloc (obs, sizeof *token);
  token = make_argv_ref_token (new_token, obs, expansion_level - 1, argv, 2,
			       flatten, NULL);
  if (!token)
    {
      obstack_free (obs, new_token);
      new_argv = (macro_arguments *)
	obstack_alloc (obs, offsetof (macro_arguments, array));
      new_argv->arraylen = 0;
      new_argv->wrapper = false;
      new_argv->has_ref = false;
      new_argv->flatten = false;
      new_argv->has_func = false;
    }
  else
    {
      new_argv = (macro_arguments *)
	obstack_alloc (obs, offsetof (macro_arguments, array) + sizeof token);
      new_argv->arraylen = 1;
      new_argv->array[0] = token;
      new_argv->wrapper = true;
      new_argv->has_ref = argv->has_ref;
      new_argv->flatten = flatten;
      new_argv->has_func = argv->has_func;
    }
  new_argv->argc = argv->argc - 1;
  new_argv->inuse = false;
  new_argv->quote_age = argv->quote_age;
  new_argv->info = info;
  info->trace = (argv->info->debug_level & DEBUG_TRACE_ALL) || trace;
  info->name = argv0;
  info->name_len = argv0_len;
  new_argv->level = argv->level;
  return new_argv;
}

/* Push argument ARG from ARGV onto the expansion stack OBS for
   rescanning.  */
void
push_arg (struct obstack *obs, macro_arguments *argv, unsigned int arg)
{
  if (arg == 0)
    {
      /* Always push copy of arg 0, since its lifetime is not
	 guaranteed beyond expand_macro.  */
      assert (argv->info);
      obstack_grow (obs, argv->info->name, argv->info->name_len);
      return;
    }
  if (arg >= argv->argc)
    return;
  push_arg_quote (obs, argv, arg, NULL);
}

/* Push argument ARG from ARGV onto the expansion stack OBS for
   rescanning.  ARG must be > 0, < argc.  QUOTES determines any quote
   delimiters that were in effect when the reference was created.  */
void
push_arg_quote (struct obstack *obs, macro_arguments *argv, unsigned int arg,
		const string_pair *quotes)
{
  int level;
  token_data *token = arg_token (argv, arg, &level, false);

  if (quotes)
    obstack_grow (obs, quotes->str1, quotes->len1);
  if (push_token (token, level, argv->inuse))
    arg_mark (argv);
  if (quotes)
    obstack_grow (obs, quotes->str2, quotes->len2);
}

/* Push series of comma-separated arguments from ARGV, which can
   include builtins, onto the expansion stack OBS for rescanning.  If
   SKIP, then don't push the first argument.  If QUOTE, the rescan
   also includes quoting around each arg.  */
void
push_args (struct obstack *obs, macro_arguments *argv, bool skip, bool quote)
{
  unsigned int i = skip ? 2 : 1;
  token_data td;
  token_data *token;

  if (i >= argv->argc)
    return;

  if (i + 1 == argv->argc)
    {
      push_arg_quote (obs, argv, i, quote ? &curr_quote : NULL);
      return;
    }

  token = make_argv_ref_token (&td, obs, -1, argv, i, argv->flatten,
			       quote ? &curr_quote : NULL);
  assert (token);
  if (push_token (token, -1, argv->inuse))
    arg_mark (argv);
}

/* Push arguments from ARGV, which can include builtins, onto the wrap
   stack for later rescanning.  If GNU extensions are disabled, only
   the first argument is pushed; otherwise, all arguments are pushed
   and separated with a space.  */
void
wrap_args (macro_arguments *argv)
{
  int i;
  struct obstack *obs;
  token_data *token;
  token_chain *chain;
  token_chain **end;

  if ((argv->argc == 2 || no_gnu_extensions) && arg_empty (argv, 1))
    return;

  obs = push_wrapup_init (argv->info, &end);
  for (i = 1; i < (no_gnu_extensions ? 2 : argv->argc); i++)
    {
      if (i != 1)
	obstack_1grow (obs, ' ');
      token = arg_token (argv, i, NULL, false);
      switch (TOKEN_DATA_TYPE (token))
	{
	case TOKEN_TEXT:
	  obstack_grow (obs, TOKEN_DATA_TEXT (token), TOKEN_DATA_LEN (token));
	  break;
	case TOKEN_FUNC:
	  append_macro (obs, TOKEN_DATA_FUNC (token), NULL, end);
	  break;
	case TOKEN_COMP:
	  chain = token->u.u_c.chain;
	  while (chain)
	    {
	      switch (chain->type)
		{
		case CHAIN_STR:
		  obstack_grow (obs, chain->u.u_s.str, chain->u.u_s.len);
		  break;
		case CHAIN_FUNC:
		  append_macro (obs, chain->u.func, NULL, end);
		  break;
		case CHAIN_ARGV:
		  arg_print (obs, chain->u.u_a.argv, chain->u.u_a.index,
			     quote_cache (NULL, chain->quote_age,
					  chain->u.u_a.quotes),
			     chain->u.u_a.flatten, end, NULL, NULL, false);
		  break;
		default:
		  assert (!"wrap_args");
		  abort ();
		}
	      chain = chain->next;
	    }
	  break;
	default:
	  assert (!"wrap_args");
	  abort ();
	}
    }
  push_wrapup_finish ();
}

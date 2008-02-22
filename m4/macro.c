/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2001, 2006, 2007,
   2008 Free Software Foundation, Inc.

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

/* This file contains the functions that perform the basic argument
   parsing and macro expansion.  */

#include <config.h>

#include <stdarg.h>

#include "m4private.h"

#include "intprops.h"

/* Define this to 1 see runtime debug info.  Implied by DEBUG.  */
/*#define DEBUG_INPUT 1 */
#ifndef DEBUG_MACRO
# define DEBUG_MACRO 0
#endif /* DEBUG_MACRO */

/* A note on argument memory lifetimes: We use an internal struct
   (m4__macro_args_stacks) to maintain list of argument obstacks.
   Within a recursion level, consecutive macros can share a stack, but
   distinct recursion levels need different stacks since the nested
   macro is interrupting the argument collection of the outer level.
   Note that a reference can live as long as the expansion containing
   the reference can participate as an argument in a future macro
   call.

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

static m4_macro_args *collect_arguments (m4 *, const char *, size_t,
					 m4_symbol *, m4_obstack *,
					 m4_obstack *);
static void    expand_macro      (m4 *, const char *, size_t, m4_symbol *);
static bool    expand_token      (m4 *, m4_obstack *, m4__token_type,
				  m4_symbol_value *, int, bool);
static bool    expand_argument   (m4 *, m4_obstack *, m4_symbol_value *,
				  const char *);
static void    process_macro	 (m4 *, m4_symbol_value *, m4_obstack *, int,
				  m4_macro_args *);

static void    trace_prepre	 (m4 *, const char *, size_t,
				  m4_symbol_value *);
static void    trace_pre	 (m4 *, size_t, m4_macro_args *);
static void    trace_post	 (m4 *, size_t, m4_macro_args *,
				  m4_input_block *, bool);

static void    trace_format	 (m4 *, const char *, ...)
  M4_GNUC_PRINTF (2, 3);
static void    trace_header	 (m4 *, size_t);
static void    trace_flush	 (m4 *);


/* The number of the current call of expand_macro ().  */
static size_t macro_call_id = 0;

/* A placeholder symbol value representing the empty string, used to
   optimize checks for emptiness.  */
static m4_symbol_value empty_symbol;

#if DEBUG_MACRO
/* True if significant changes to stacks should be printed to the
   trace stream.  Primarily useful for debugging $@ ref memory leaks,
   and controlled by M4_DEBUG_MACRO environment variable.  */
static int debug_macro_level;
#else
# define debug_macro_level 0
#endif /* !DEBUG_MACRO */
#define PRINT_ARGCOUNT_CHANGES	1	/* Any change to argcount > 1.  */
#define PRINT_REFCOUNT_INCREASE	2	/* Any increase to refcount.  */
#define PRINT_REFCOUNT_DECREASE	4	/* Any decrease to refcount.  */



/* This function reads all input, and expands each token, one at a time.  */
void
m4_macro_expand_input (m4 *context)
{
  m4__token_type type;
  m4_symbol_value token;
  int line;

#if DEBUG_MACRO
  const char *s = getenv ("M4_DEBUG_MACRO");
  if (s)
    debug_macro_level = strtol (s, NULL, 0);
#endif /* DEBUG_MACRO */

  m4_set_symbol_value_text (&empty_symbol, "", 0, 0);
  VALUE_MAX_ARGS (&empty_symbol) = -1;

  while ((type = m4__next_token (context, &token, &line, NULL, false, NULL))
	 != M4_TOKEN_EOF)
    expand_token (context, NULL, type, &token, line, true);
}


/* Expand one token onto OBS, according to its type.  If OBS is NULL,
   output the expansion to the current diversion.  TYPE determines the
   contents of TOKEN.  Potential macro names (a TYPE of M4_TOKEN_WORD)
   are looked up in the symbol table, to see if they have a macro
   definition.  If they have, they are expanded as macros, otherwise
   the text are just copied to the output.  LINE determines where
   TOKEN began.  FIRST is true if there is no prior content in the
   current macro argument.  Return true if the result is guranteed to
   give the same parse on rescan in a quoted context with the same
   quote age.  Returning false is always safe, although it may lead to
   slower performance.  */
static bool
expand_token (m4 *context, m4_obstack *obs, m4__token_type type,
	      m4_symbol_value *token, int line, bool first)
{
  m4_symbol *symbol;
  bool result;
  const char *text = (m4_is_symbol_value_text (token)
		      ? m4_get_symbol_value_text (token) : NULL);

  switch (type)
    {				/* TOKSW */
    case M4_TOKEN_EOF:
    case M4_TOKEN_MACDEF:
      /* Always safe, since there is no text to rescan.  */
      return true;

    case M4_TOKEN_STRING:
      /* Tokens and comments are safe in isolation (since quote_age
	 detects any change in delimiters).  This is also returned for
	 sequences of benign characters, such as digits.  But if other
	 text is already present, multi-character delimiters could be
	 formed by concatenation, so use a conservative heuristic.  If
	 obstack was provided, the string was already expanded into it
	 during m4__next_token.  */
      result = first || m4__safe_quotes (M4SYNTAX);
      if (obs)
	return result;
      break;

    case M4_TOKEN_OPEN:
    case M4_TOKEN_COMMA:
    case M4_TOKEN_CLOSE:
    case M4_TOKEN_SPACE:
      /* Conservative heuristic, thanks to multi-character delimiter
	 concatenation.  */
      result = m4__safe_quotes (M4SYNTAX);
      break;

    case M4_TOKEN_SIMPLE:
      /* No guarantees here.  */
      assert (m4_get_symbol_value_len (token) == 1);
      result = false;
      break;

    case M4_TOKEN_WORD:
      {
	const char *textp = text;
	size_t len = m4_get_symbol_value_len (token);
	size_t len2 = len;

	if (m4_has_syntax (M4SYNTAX, *textp, M4_SYNTAX_ESCAPE))
	  {
	    textp++;
	    len2--;
	  }

	symbol = m4_symbol_lookup (M4SYMTAB, textp);
	assert (!symbol || !m4_is_symbol_void (symbol));
	if (symbol == NULL
	    || (symbol->value->type == M4_SYMBOL_FUNC
		&& BIT_TEST (SYMBOL_FLAGS (symbol), VALUE_BLIND_ARGS_BIT)
		&& !m4__next_token_is_open (context)))
	  {
	    m4_divert_text (context, obs, text, len, line);
	    /* The word just output is unquoted, but we can trust the
	       heuristics of safe_quote.  */
	    return m4__safe_quotes (M4SYNTAX);
	  }
	expand_macro (context, textp, len2, symbol);
	/* Expanding a macro may create new tokens to scan, and those
	   tokens may generate unsafe text, but we did not append any
	   text now.  */
	return true;
      }

    default:
      assert (!"INTERNAL ERROR: bad token type in expand_token ()");
      abort ();
    }
  m4_divert_text (context, obs, text, m4_get_symbol_value_len (token), line);
  return result;
}


/* This function parses one argument to a macro call.  It expects the
   first left parenthesis or the separating comma to have been read by
   the caller.  It skips leading whitespace, then reads and expands
   tokens, until it finds a comma or a right parenthesis at the same
   level of parentheses.  It returns a flag indicating whether the
   argument read is the last for the active macro call.  The arguments
   are built on the obstack OBS, indirectly through expand_token ().
   Report errors on behalf of CALLER.  */
static bool
expand_argument (m4 *context, m4_obstack *obs, m4_symbol_value *argp,
		 const char *caller)
{
  m4__token_type type;
  m4_symbol_value token;
  int paren_level = 0;
  const char *file = m4_get_current_file (context);
  int line = m4_get_current_line (context);
  size_t len;
  unsigned int age = m4__quote_age (M4SYNTAX);
  bool first = true;

  memset (argp, '\0', sizeof *argp);
  VALUE_MAX_ARGS (argp) = -1;

  /* Skip leading white space.  */
  do
    {
      type = m4__next_token (context, &token, NULL, obs, true, caller);
    }
  while (type == M4_TOKEN_SPACE);

  while (1)
    {
      if (VALUE_MIN_ARGS (argp) < VALUE_MIN_ARGS (&token))
	VALUE_MIN_ARGS (argp) = VALUE_MIN_ARGS (&token);
      if (VALUE_MAX_ARGS (&token) < VALUE_MAX_ARGS (argp))
	VALUE_MAX_ARGS (argp) = VALUE_MAX_ARGS (&token);
      switch (type)
	{			/* TOKSW */
	case M4_TOKEN_COMMA:
	case M4_TOKEN_CLOSE:
	  if (paren_level == 0)
	    {
	      /* FIXME - For now, we match the behavior of the branch,
		 except we don't issue warnings.  But in the future,
		 we want to allow concatenation of builtins and
		 text.  */
	      len = obstack_object_size (obs);
	      if (argp->type == M4_SYMBOL_FUNC && !len)
		return type == M4_TOKEN_COMMA;
	      if (argp->type != M4_SYMBOL_COMP)
		{
		  VALUE_MODULE (argp) = NULL;
		  if (len)
		    {
		      obstack_1grow (obs, '\0');
		      m4_set_symbol_value_text (argp, obstack_finish (obs),
						len, age);
		    }
		  else
		    m4_set_symbol_value_text (argp, "", len, 0);
		}
	      else
		m4__make_text_link (obs, NULL, &argp->u.u_c.end);
	      return type == M4_TOKEN_COMMA;
	    }
	  /* fallthru */
	case M4_TOKEN_OPEN:
	case M4_TOKEN_SIMPLE:
	  if (type == M4_TOKEN_OPEN)
	    paren_level++;
	  else if (type == M4_TOKEN_CLOSE)
	    paren_level--;
	  if (!expand_token (context, obs, type, &token, line, first))
	    age = 0;
	  break;

	case M4_TOKEN_EOF:
	  m4_error_at_line (context, EXIT_FAILURE, 0, file, line, caller,
			    _("end of file in argument list"));
	  break;

	case M4_TOKEN_WORD:
	case M4_TOKEN_SPACE:
	case M4_TOKEN_STRING:
	  if (!expand_token (context, obs, type, &token, line, first))
	    age = 0;
	  if (token.type == M4_SYMBOL_COMP)
	    {
	      if (argp->type != M4_SYMBOL_COMP)
		{
		  argp->type = M4_SYMBOL_COMP;
		  argp->u.u_c.chain = token.u.u_c.chain;
		  argp->u.u_c.wrapper = argp->u.u_c.has_func = false;
		}
	      else
		{
		  assert (argp->u.u_c.end);
		  argp->u.u_c.end->next = token.u.u_c.chain;
		}
	      argp->u.u_c.end = token.u.u_c.end;
	      if (token.u.u_c.has_func)
		argp->u.u_c.has_func = true;
	    }
	  break;

	case M4_TOKEN_MACDEF:
	  if (argp->type == M4_SYMBOL_VOID && obstack_object_size (obs) == 0)
	    m4_symbol_value_copy (argp, &token);
	  else
	    argp->type = M4_SYMBOL_TEXT;
	  break;

	case M4_TOKEN_ARGV:
	  assert (paren_level == 0 && argp->type == M4_SYMBOL_VOID
		  && obstack_object_size (obs) == 0
		  && token.u.u_c.chain == token.u.u_c.end
		  && token.u.u_c.chain->type == M4__CHAIN_ARGV);
	  argp->type = M4_SYMBOL_COMP;
	  argp->u.u_c.chain = argp->u.u_c.end = token.u.u_c.chain;
	  argp->u.u_c.wrapper = true;
	  argp->u.u_c.has_func = token.u.u_c.has_func;
	  type = m4__next_token (context, &token, NULL, NULL, false, caller);
	  if (argp->u.u_c.chain->u.u_a.skip_last)
	    assert (type == M4_TOKEN_COMMA);
	  else
	    assert (type == M4_TOKEN_COMMA || type == M4_TOKEN_CLOSE);
	  return type == M4_TOKEN_COMMA;

	default:
	  assert (!"expand_argument");
	  abort ();
	}

      if (argp->type != M4_SYMBOL_VOID || obstack_object_size (obs))
	first = false;
      type = m4__next_token (context, &token, NULL, obs, first, caller);
    }
}


/* The macro expansion is handled by expand_macro ().  It parses the
   arguments, using collect_arguments (), and builds a table of pointers to
   the arguments.  The arguments themselves are stored on a local obstack.
   Expand_macro () uses m4_macro_call () to do the call of the macro.

   Expand_macro () is potentially recursive, since it calls expand_argument
   (), which might call expand_token (), which might call expand_macro ().

   NAME points to storage on the token stack, so it is only valid
   until a call to collect_arguments parses more tokens.  SYMBOL is
   the result of the symbol table lookup on NAME.  */
static void
expand_macro (m4 *context, const char *name, size_t len, m4_symbol *symbol)
{
  void *args_base;		/* Base of stack->args on entry.  */
  void *args_scratch;		/* Base of scratch space for m4_macro_call.  */
  void *argv_base;		/* Base of stack->argv on entry.  */
  m4_macro_args *argv;		/* Arguments to the called macro.  */
  m4_obstack *expansion;	/* Collects the macro's expansion.  */
  m4_input_block *expanded;	/* The resulting expansion, for tracing.  */
  bool traced;			/* True if this macro is traced.  */
  bool trace_expansion = false;	/* True if trace and debugmode(`e').  */
  size_t my_call_id;		/* Sequence id for this macro.  */
  m4_symbol_value *value;	/* Original value of this macro.  */
  size_t level;			/* Expansion level of this macro.  */
  m4__macro_arg_stacks *stack;	/* Storage for this macro.  */

  /* Report errors at the location where the open parenthesis (if any)
     was found, but after expansion, restore global state back to the
     location of the close parenthesis.  This is safe since we
     guarantee that macro expansion does not alter the state of
     current_file/current_line (dnl, include, and sinclude are special
     cased in the input engine to ensure this fact).  */
  const char *loc_open_file = m4_get_current_file (context);
  int loc_open_line = m4_get_current_line (context);
  const char *loc_close_file;
  int loc_close_line;

  /* Obstack preparation.  */
  level = context->expansion_level;
  if (context->stacks_count <= level)
    {
      size_t count = context->stacks_count;
      context->arg_stacks
	= (m4__macro_arg_stacks *) x2nrealloc (context->arg_stacks,
					       &context->stacks_count,
					       sizeof *context->arg_stacks);
      memset (&context->arg_stacks[count], 0,
	      sizeof *context->arg_stacks * (context->stacks_count - count));
    }
  stack = &context->arg_stacks[level];
  if (!stack->args)
    {
      assert (!stack->refcount);
      stack->args = xmalloc (sizeof *stack->args);
      stack->argv = xmalloc (sizeof *stack->argv);
      obstack_init (stack->args);
      obstack_init (stack->argv);
      stack->args_base = obstack_finish (stack->args);
      stack->argv_base = obstack_finish (stack->argv);
    }
  assert (obstack_object_size (stack->args) == 0
	  && obstack_object_size (stack->argv) == 0);
  args_base = obstack_finish (stack->args);
  argv_base = obstack_finish (stack->argv);
  m4__adjust_refcount (context, level, true);
  stack->argcount++;

  /* Grab the current value of this macro, because it may change while
     collecting arguments.  Likewise, grab any state needed during
     tracing.  */
  value = m4_get_symbol_value (symbol);
  traced = (m4_is_debug_bit (context, M4_DEBUG_TRACE_ALL)
	    || m4_get_symbol_traced (symbol));
  if (traced)
    trace_expansion = m4_is_debug_bit (context, M4_DEBUG_TRACE_EXPANSION);

  /* Prepare for macro expansion.  */
  VALUE_PENDING (value)++;
  if (m4_get_nesting_limit_opt (context) < ++context->expansion_level)
    m4_error (context, EXIT_FAILURE, 0, NULL, _("\
recursion limit of %zu exceeded, use -L<N> to change it"),
	      m4_get_nesting_limit_opt (context));
  my_call_id = ++macro_call_id;

  if (traced && m4_is_debug_bit (context, M4_DEBUG_TRACE_CALL))
    trace_prepre (context, name, my_call_id, value);

  argv = collect_arguments (context, name, len, symbol, stack->args,
			    stack->argv);
  /* Since collect_arguments can invalidate stack by reallocating
     context->arg_stacks during a recursive expand_macro call, we must
     reset it here.  */
  stack = &context->arg_stacks[level];
  args_scratch = obstack_finish (stack->args);

  /* The actual macro call.  */
  loc_close_file = m4_get_current_file (context);
  loc_close_line = m4_get_current_line (context);
  m4_set_current_file (context, loc_open_file);
  m4_set_current_line (context, loc_open_line);

  if (traced)
    trace_pre (context, my_call_id, argv);

  expansion = m4_push_string_init (context);
  m4_macro_call (context, value, expansion, argv->argc, argv);
  expanded = m4_push_string_finish ();

  if (traced)
    trace_post (context, my_call_id, argv, expanded, trace_expansion);

  /* Cleanup.  */
  m4_set_current_file (context, loc_close_file);
  m4_set_current_line (context, loc_close_line);

  --context->expansion_level;
  --VALUE_PENDING (value);
  if (BIT_TEST (VALUE_FLAGS (value), VALUE_DELETED_BIT))
    m4_symbol_value_delete (value);

  /* We no longer need argv, so reduce the refcount.  Additionally, if
     no other references to argv were created, we can free our portion
     of the obstack, although we must leave earlier content alone.  A
     refcount of 0 implies that adjust_refcount already freed the
     entire stack.  */
  m4__arg_adjust_refcount (context, argv, false);
  if (stack->refcount)
    {
      if (argv->inuse)
	{
	  obstack_free (stack->args, args_scratch);
	  if (debug_macro_level & PRINT_ARGCOUNT_CHANGES)
	    xfprintf (stderr, "m4debug: -%d- `%s' in use, level=%d, "
		      "refcount=%zu, argcount=%zu\n", my_call_id, argv->argv0,
		      level, stack->refcount, stack->argcount);
	}
      else
	{
	  obstack_free (stack->args, args_base);
	  obstack_free (stack->argv, argv_base);
	  stack->argcount--;
	}
    }
}

/* Collect all the arguments to a call of the macro SYMBOL (called
   NAME, with length LEN).  The arguments are stored on the obstack
   ARGUMENTS and a table of pointers to the arguments on the obstack
   argv_stack.  Return the object describing all of the macro
   arguments.  */
static m4_macro_args *
collect_arguments (m4 *context, const char *name, size_t len,
		   m4_symbol *symbol, m4_obstack *arguments,
		   m4_obstack *argv_stack)
{
  m4_symbol_value token;
  m4_symbol_value *tokenp;
  bool more_args;
  bool groks_macro_args;
  m4_macro_args args;
  m4_macro_args *argv;

  groks_macro_args = BIT_TEST (SYMBOL_FLAGS (symbol), VALUE_MACRO_ARGS_BIT);

  args.argc = 1;
  args.inuse = false;
  args.wrapper = false;
  args.has_ref = false;
  args.flatten = !groks_macro_args;
  args.has_func = false;
  /* Must copy here, since we are consuming tokens, and since symbol
     table can be changed during argument collection.  */
  args.argv0 = (char *) obstack_copy0 (arguments, name, len);
  args.argv0_len = len;
  args.quote_age = m4__quote_age (M4SYNTAX);
  args.level = context->expansion_level - 1;
  args.arraylen = 0;
  obstack_grow (argv_stack, &args, offsetof (m4_macro_args, array));
  name = args.argv0;

  if (m4__next_token_is_open (context))
    {
      /* Gobble parenthesis, then collect arguments.  */
      m4__next_token (context, &token, NULL, NULL, false, name);
      do
	{
	  tokenp = (m4_symbol_value *) obstack_alloc (arguments,
						      sizeof *tokenp);
	  more_args = expand_argument (context, arguments, tokenp, name);

	  if ((m4_is_symbol_value_text (tokenp)
	       && !m4_get_symbol_value_len (tokenp))
	      || (!groks_macro_args && m4_is_symbol_value_func (tokenp)))
	    {
	      obstack_free (arguments, tokenp);
	      tokenp = &empty_symbol;
	    }
	  obstack_ptr_grow (argv_stack, tokenp);
	  args.arraylen++;
	  args.argc++;
	  switch (tokenp->type)
	    {
	    case M4_SYMBOL_TEXT:
	      /* Be conservative - any change in quoting while
		 collecting arguments, or any unsafe argument, will
		 require a rescan if $@ is reused.  */
	      if (m4_get_symbol_value_len (tokenp)
		  && m4_get_symbol_value_quote_age (tokenp) != args.quote_age)
		args.quote_age = 0;
	      break;
	    case M4_SYMBOL_FUNC:
	      args.has_func = true;
	      break;
	    case M4_SYMBOL_COMP:
	      args.has_ref = true;
	      if (tokenp->u.u_c.wrapper)
		{
		  assert (tokenp->u.u_c.chain->type == M4__CHAIN_ARGV
			  && !tokenp->u.u_c.chain->next);
		  args.argc += (tokenp->u.u_c.chain->u.u_a.argv->argc
				- tokenp->u.u_c.chain->u.u_a.index
				- tokenp->u.u_c.chain->u.u_a.skip_last - 1);
		  args.wrapper = true;
		}
	      if (tokenp->u.u_c.has_func)
		args.has_func = true;
	      break;
	    default:
	      assert (!"expand_argument");
	      abort ();
	    }
	}
      while (more_args);
    }
  argv = (m4_macro_args *) obstack_finish (argv_stack);
  argv->argc = args.argc;
  argv->wrapper = args.wrapper;
  argv->has_ref = args.has_ref;
  argv->has_func = args.has_func;
  if (args.quote_age != m4__quote_age (M4SYNTAX))
    argv->quote_age = 0;
  argv->arraylen = args.arraylen;
  return argv;
}


/* The actual call of a macro is handled by m4_macro_call ().
   m4_macro_call () is passed a SYMBOL, whose type is used to
   call either a builtin function, or the user macro expansion
   function process_macro ().  There are ARGC arguments to
   the call, stored in the ARGV table.  The expansion is left on
   the obstack EXPANSION.  Macro tracing is also handled here.  */
void
m4_macro_call (m4 *context, m4_symbol_value *value, m4_obstack *expansion,
	       size_t argc, m4_macro_args *argv)
{
  if (m4_bad_argc (context, argc, argv->argv0,
		   VALUE_MIN_ARGS (value), VALUE_MAX_ARGS (value),
		   BIT_TEST (VALUE_FLAGS (value),
			     VALUE_SIDE_EFFECT_ARGS_BIT)))
    return;
  if (m4_is_symbol_value_text (value))
    process_macro (context, value, expansion, argc, argv);
  else if (m4_is_symbol_value_func (value))
    m4_get_symbol_value_func (value) (context, expansion, argc, argv);
  else if (m4_is_symbol_value_placeholder (value))
    m4_warn (context, 0, M4ARG (0),
	     _("builtin `%s' requested by frozen file not found"),
	     m4_get_symbol_value_placeholder (value));
  else
    {
      assert (!"m4_macro_call");
      abort ();
    }
}

/* This function handles all expansion of user defined and predefined
   macros.  It is called with an obstack OBS, where the macros expansion
   will be placed, as an unfinished object.  SYMBOL points to the macro
   definition, giving the expansion text.  ARGC and ARGV are the arguments,
   as usual.  */
static void
process_macro (m4 *context, m4_symbol_value *value, m4_obstack *obs,
	       int argc, m4_macro_args *argv)
{
  const char *text = m4_get_symbol_value_text (value);
  size_t len = m4_get_symbol_value_len (value);
  int i;

  while (len--)
    {
      char ch;

      if (!m4_has_syntax (M4SYNTAX, *text, M4_SYNTAX_DOLLAR) || !len)
	{
	  obstack_1grow (obs, *text);
	  text++;
	  continue;
	}
      ch = *text++;
      switch (*text)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  /* FIXME - multidigit arguments should convert over to ${10}
	     syntax instead of $10; see
	     http://lists.gnu.org/archive/html/m4-discuss/2006-08/msg00028.html
	     for more discussion.  */
	  if (m4_get_posixly_correct_opt (context) || !isdigit(text[1]))
	    {
	      i = *text++ - '0';
	      len--;
	    }
	  else
	    {
	      char *endp;
	      i = (int) strtol (text, &endp, 10);
	      len -= endp - text;
	      text = endp;
	    }
	  if (i < argc)
	    m4_push_arg (context, obs, argv, i);
	  break;

	case '#':		/* number of arguments */
	  m4_shipout_int (obs, argc - 1);
	  text++;
	  len--;
	  break;

	case '*':		/* all arguments */
	case '@':		/* ... same, but quoted */
	  m4_push_args (context, obs, argv, false, *text == '@');
	  text++;
	  len--;
	  break;

	default:
	  if (m4_get_posixly_correct_opt (context)
	      || !VALUE_ARG_SIGNATURE (value))
	    {
	      obstack_1grow (obs, ch);
	    }
	  else
	    {
	      size_t len1 = 0;
	      const char *endp;
	      char *key;

	      for (endp = ++text;
		   len1 < len && m4_has_syntax (M4SYNTAX, *endp,
						(M4_SYNTAX_OTHER
						 | M4_SYNTAX_ALPHA
						 | M4_SYNTAX_NUM));
		   ++endp)
		{
		  ++len1;
		}
	      key = xstrndup (text, len1);

	      if (*endp)
		{
		  struct m4_symbol_arg **arg
		    = (struct m4_symbol_arg **)
		      m4_hash_lookup (VALUE_ARG_SIGNATURE (value), key);

		  if (arg)
		    {
		      i = SYMBOL_ARG_INDEX (*arg);
		      assert (i < argc);
		      m4_shipout_string (context, obs, M4ARG (i), M4ARGLEN (i),
					 false);
		    }
		}
	      else
		{
		  m4_error (context, 0, 0, M4ARG (0),
			    _("unterminated parameter reference: %s"),
			    key);
		}

	      len -= endp - text;
	      text = endp;

	      free (key);
	      break;
	    }
	  break;
	}
    }
}



/* The next portion of this file contains the functions for macro
   tracing output.  All tracing output for a macro call is collected
   on an obstack TRACE, and printed whenever the line is complete.
   This prevents tracing output from interfering with other debug
   messages generated by the various builtins.  */

/* Tracing output is formatted here, by a simplified printf-to-obstack
   function trace_format ().  Understands only %s, %d, %zu (size_t
   value).  */
static void
trace_format (m4 *context, const char *fmt, ...)
{
  va_list args;
  char ch;
  const char *s;
  char nbuf[INT_BUFSIZE_BOUND (sizeof (int) > sizeof (size_t)
			       ? sizeof (int) : sizeof (size_t))];

  va_start (args, fmt);

  while (true)
    {
      while ((ch = *fmt++) != '\0' && ch != '%')
	obstack_1grow (&context->trace_messages, ch);

      if (ch == '\0')
	break;

      switch (*fmt++)
	{
	case 's':
	  s = va_arg (args, const char *);
	  break;

	case 'd':
	  {
	    int d = va_arg (args, int);

	    sprintf (nbuf, "%d", d);
	    s = nbuf;
	  }
	  break;

	case 'z':
	  ch = *fmt++;
	  assert (ch == 'u');
	  {
	    size_t z = va_arg (args, size_t);

	    sprintf (nbuf, "%zu", z);
	    s = nbuf;
	  }
	  break;

	default:
	  abort ();
	  break;
	}

      obstack_grow (&context->trace_messages, s, strlen (s));
    }

  va_end (args);
}

/* Format the standard header attached to all tracing output lines.  */
static void
trace_header (m4 *context, size_t id)
{
  trace_format (context, "m4trace:");
  if (m4_get_current_line (context))
    {
      if (m4_is_debug_bit (context, M4_DEBUG_TRACE_FILE))
	trace_format (context, "%s:", m4_get_current_file (context));
      if (m4_is_debug_bit (context, M4_DEBUG_TRACE_LINE))
	trace_format (context, "%d:", m4_get_current_line (context));
    }
  trace_format (context, " -%zu- ", context->expansion_level);
  if (m4_is_debug_bit (context, M4_DEBUG_TRACE_CALLID))
    trace_format (context, "id %zu: ", id);
}

/* Print current tracing line, and clear the obstack.  */
static void
trace_flush (m4 *context)
{
  char *line;

  obstack_1grow (&context->trace_messages, '\n');
  obstack_1grow (&context->trace_messages, '\0');
  line = obstack_finish (&context->trace_messages);
  if (m4_get_debug_file (context))
    fputs (line, m4_get_debug_file (context));
  obstack_free (&context->trace_messages, line);
}

/* Do pre-argument-collection tracing for macro NAME.  Used from
   expand_macro ().  */
static void
trace_prepre (m4 *context, const char *name, size_t id, m4_symbol_value *value)
{
  const m4_string_pair *quotes = NULL;
  size_t arg_length = m4_get_max_debug_arg_length_opt (context);
  bool module = m4_is_debug_bit (context, M4_DEBUG_TRACE_MODULE);

  if (m4_is_debug_bit (context, M4_DEBUG_TRACE_QUOTE))
    quotes = m4_get_syntax_quotes (M4SYNTAX);
  trace_header (context, id);
  trace_format (context, "%s ... = ", name);
  m4_symbol_value_print (context, value, &context->trace_messages, quotes,
			 false, &arg_length, module);
  trace_flush (context);
}

/* Format the parts of a trace line, that can be made before the macro is
   actually expanded.  Used from expand_macro ().  */
static void
trace_pre (m4 *context, size_t id, m4_macro_args *argv)
{
  trace_header (context, id);
  trace_format (context, "%s", M4ARG (0));

  if (1 < m4_arg_argc (argv) && m4_is_debug_bit (context, M4_DEBUG_TRACE_ARGS))
    {
      const m4_string_pair *quotes = NULL;
      size_t arg_length = m4_get_max_debug_arg_length_opt (context);
      bool module = m4_is_debug_bit (context, M4_DEBUG_TRACE_MODULE);

      if (m4_is_debug_bit (context, M4_DEBUG_TRACE_QUOTE))
	quotes = m4_get_syntax_quotes (M4SYNTAX);
      trace_format (context, "(");
      m4_arg_print (context, &context->trace_messages, argv, 1, quotes, false,
		    ", ", &arg_length, true, module);
      trace_format (context, ")");
    }
}

/* Format the final part of a trace line and print it all.  Used from
   expand_macro ().  */
static void
trace_post (m4 *context, size_t id, m4_macro_args *argv,
	    m4_input_block *expanded, bool trace_expansion)
{
  if (trace_expansion)
    {
      trace_format (context, " -> ");
      m4_input_print (context, &context->trace_messages, expanded);
    }

  trace_flush (context);
}


/* Accessors into m4_macro_args.  */

/* Adjust the refcount of argument stack LEVEL.  If INCREASE, then
   increase the count, otherwise decrease the count and clear the
   entire stack if the new count is zero.  Return the new
   refcount.  */
size_t
m4__adjust_refcount (m4 *context, size_t level, bool increase)
{
  m4__macro_arg_stacks *stack = &context->arg_stacks[level];
  assert (level < context->stacks_count && stack->args
	  && (increase || stack->refcount));
  if (increase)
    stack->refcount++;
  else if (--stack->refcount == 0)
    {
      obstack_free (stack->args, stack->args_base);
      obstack_free (stack->argv, stack->argv_base);
      if ((debug_macro_level & PRINT_ARGCOUNT_CHANGES) && 1 < stack->argcount)
	xfprintf (stderr, "m4debug: -%d- freeing %zu args, level=%d\n",
		  macro_call_id, stack->argcount, level);
      stack->argcount = 0;
    }
  if (debug_macro_level
      & (increase ? PRINT_REFCOUNT_INCREASE : PRINT_REFCOUNT_DECREASE))
    xfprintf (stderr, "m4debug: level %d refcount=%d\n", level,
	      stack->refcount);
  return stack->refcount;
}

/* Given ARGV, adjust the refcount of every reference it contains in
   the direction decided by INCREASE.  Return true if increasing
   references to ARGV implies the first use of ARGV.  */
bool
m4__arg_adjust_refcount (m4 *context, m4_macro_args *argv, bool increase)
{
  size_t i;
  m4__symbol_chain *chain;
  bool result = !argv->inuse;

  if (argv->has_ref)
    for (i = 0; i < argv->arraylen; i++)
      if (argv->array[i]->type == M4_SYMBOL_COMP)
	{
	  chain = argv->array[i]->u.u_c.chain;
	  while (chain)
	    {
	      switch (chain->type)
		{
		case M4__CHAIN_STR:
		  if (chain->u.u_s.level < SIZE_MAX)
		    m4__adjust_refcount (context, chain->u.u_s.level,
					 increase);
		  break;
		case M4__CHAIN_ARGV:
		  assert (chain->u.u_a.argv->inuse);
		  m4__arg_adjust_refcount (context, chain->u.u_a.argv,
					   increase);
		  break;
		default:
		  assert (!"m4__arg_adjust_refcount");
		  abort ();
		}
	      chain = chain->next;
	    }
	}
  m4__adjust_refcount (context, argv->level, increase);
  return result;
}

/* Mark ARGV as being in use, along with any $@ references that it
   wraps.  */
static void
arg_mark (m4_macro_args *argv)
{
  size_t i;
  m4__symbol_chain *chain;

  if (argv->inuse)
    return;
  argv->inuse = true;
  if (argv->wrapper)
    {
      for (i = 0; i < argv->arraylen; i++)
	if (argv->array[i]->type == M4_SYMBOL_COMP
	    && argv->array[i]->u.u_c.wrapper)
	  {
	    chain = argv->array[i]->u.u_c.chain;
	    assert (!chain->next && chain->type == M4__CHAIN_ARGV);
	    if (!chain->u.u_a.argv->inuse)
	      arg_mark (chain->u.u_a.argv);
	  }
    }
}

/* Populate the newly-allocated VALUE as a wrapper around ARGV,
   starting with argument INDEX.  Allocate any data on OBS, owned by a
   given expansion LEVEL.  FLATTEN determines whether to allow
   builtins, and QUOTES determines whether all arguments are quoted.
   Return TOKEN when successful, NULL when wrapping ARGV is trivially
   empty.  */
static m4_symbol_value *
make_argv_ref (m4 *context, m4_symbol_value *value, m4_obstack *obs,
	       size_t level, m4_macro_args *argv, size_t index, bool flatten,
	       const m4_string_pair *quotes)
{
  m4__symbol_chain *chain;

  assert (obstack_object_size (obs) == 0);
  /* TODO reuse $@ even if argv has multiple array slots.  */
  if (argv->wrapper && argv->arraylen == 1)
    {
      assert (argv->array[0]->type == M4_SYMBOL_COMP
	      && argv->array[0]->u.u_c.wrapper);
      chain= argv->array[0]->u.u_c.chain;
      assert (!chain->next && chain->type == M4__CHAIN_ARGV
	      && !chain->u.u_a.skip_last);
      argv = chain->u.u_a.argv;
      index += chain->u.u_a.index - 1;
    }
  if (argv->argc <= index)
    return NULL;

  chain = (m4__symbol_chain *) obstack_alloc (obs, sizeof *chain);
  value->type = M4_SYMBOL_COMP;
  value->u.u_c.chain = value->u.u_c.end = chain;
  value->u.u_c.wrapper = true;
  value->u.u_c.has_func = argv->has_func;
  chain->next = NULL;
  chain->type = M4__CHAIN_ARGV;
  chain->quote_age = argv->quote_age;
  chain->u.u_a.argv = argv;
  chain->u.u_a.index = index;
  chain->u.u_a.flatten = flatten;
  chain->u.u_a.has_func = argv->has_func;
  chain->u.u_a.comma = false;
  chain->u.u_a.skip_last = false;
  chain->u.u_a.quotes = m4__quote_cache (M4SYNTAX, obs, chain->quote_age,
					 quotes);
  return value;
}

/* Given ARGV, return the symbol value at the specified INDEX, which
   must be non-zero.  *LEVEL is set to the obstack level that contains
   the symbol (which is not necessarily the level of ARGV).  */
static m4_symbol_value *
arg_symbol (m4_macro_args *argv, size_t index, size_t *level)
{
  size_t i;
  m4_symbol_value *value;

  assert (index);
  *level = argv->level;
  if (argv->argc <= index)
    return &empty_symbol;
  if (!argv->wrapper)
    return argv->array[index - 1];

  /* Must cycle through all array slots until we find index, since
     wrappers can contain multiple arguments.  */
  for (i = 0; i < argv->arraylen; i++)
    {
      value = argv->array[i];
      if (value->type == M4_SYMBOL_COMP && value->u.u_c.wrapper)
	{
	  m4__symbol_chain *chain = value->u.u_c.chain;
	  assert (!chain->next && chain->type == M4__CHAIN_ARGV);
	  if (index <= (chain->u.u_a.argv->argc - chain->u.u_a.index
			- chain->u.u_a.skip_last))
	    {
	      value = arg_symbol (chain->u.u_a.argv,
				  chain->u.u_a.index - 1 + index, level);
	      if (chain->u.u_a.flatten && m4_is_symbol_value_func (value))
		value = &empty_symbol;
	      break;
	    }
	  index -= (chain->u.u_a.argv->argc - chain->u.u_a.index
		    - chain->u.u_a.skip_last);
	}
      else if (--index == 0)
	break;
    }
  return value;
}

/* Given ARGV, return the symbol value at the specified INDEX, which
   must be non-zero.  */
m4_symbol_value *
m4_arg_symbol (m4_macro_args *argv, size_t index)
{
  size_t dummy;
  return arg_symbol (argv, index, &dummy);
}

/* Given ARGV, return true if argument INDEX is text.  Index 0 is
   always text, as are indices beyond argc.  */
bool
m4_is_arg_text (m4_macro_args *argv, size_t index)
{
  m4_symbol_value *value;
  if (index == 0 || argv->argc <= index || argv->flatten || !argv->has_func)
    return true;
  value = m4_arg_symbol (argv, index);
  if (m4_is_symbol_value_text (value)
      || (value->type == M4_SYMBOL_COMP && !value->u.u_c.has_func))
    return true;
  return false;
}

/* TODO - add m4_is_arg_comp to distinguish concatenation of builtins.  */
/* Given ARGV, return true if argument INDEX is a single builtin
   function.  Only non-zero indices less than argc can return
   true.  */
bool
m4_is_arg_func (m4_macro_args *argv, size_t index)
{
  if (index == 0 || argv->argc <= index || argv->flatten || !argv->has_func)
    return false;
  return m4_is_symbol_value_func (m4_arg_symbol (argv, index));
}

/* Given ARGV, return the text at argument INDEX.  Abort if the
   argument is not text.  Index 0 is always text, and indices beyond
   argc return the empty string.  The result is always NUL-terminated,
   even if it includes embedded NUL characters.  */
const char *
m4_arg_text (m4 *context, m4_macro_args *argv, size_t index)
{
  m4_symbol_value *value;
  m4__symbol_chain *chain;
  m4_obstack *obs;

  if (index == 0)
    return argv->argv0;
  if (argv->argc <= index)
    return "";
  value = m4_arg_symbol (argv, index);
  if (m4_is_symbol_value_text (value))
    return m4_get_symbol_value_text (value);
  /* TODO - concatenate functions.  */
  assert (value->type == M4_SYMBOL_COMP);
  chain = value->u.u_c.chain;
  obs = m4_arg_scratch (context);
  while (chain)
    {
      switch (chain->type)
	{
	case M4__CHAIN_STR:
	  obstack_grow (obs, chain->u.u_s.str, chain->u.u_s.len);
	  break;
	case M4__CHAIN_ARGV:
	  m4_arg_print (context, obs, chain->u.u_a.argv, chain->u.u_a.index,
			m4__quote_cache (M4SYNTAX, NULL, chain->quote_age,
					 chain->u.u_a.quotes),
			chain->u.u_a.flatten, NULL, NULL, false, false);
	  break;
	default:
	  assert (!"m4_arg_text");
	  abort ();
	}
      chain = chain->next;
    }
  obstack_1grow (obs, '\0');
  return (char *) obstack_finish (obs);
}

/* Given ARGV, compare text arguments INDEXA and INDEXB for equality.
   Both indices must be non-zero.  Return true if the arguments
   contain the same contents; often more efficient than
   !strcmp (m4_arg_text (context, argv, indexa),
	    m4_arg_text (context, argv, indexb)).  */
bool
m4_arg_equal (m4 *context, m4_macro_args *argv, size_t indexa, size_t indexb)
{
  m4_symbol_value *sa = m4_arg_symbol (argv, indexa);
  m4_symbol_value *sb = m4_arg_symbol (argv, indexb);
  m4__symbol_chain tmpa;
  m4__symbol_chain tmpb;
  m4__symbol_chain *ca = &tmpa;
  m4__symbol_chain *cb = &tmpb;
  m4_obstack *obs = m4_arg_scratch (context);

  /* Quick tests.  */
  if (sa == &empty_symbol || sb == &empty_symbol)
    return sa == sb;
  if (m4_is_symbol_value_text (sa) && m4_is_symbol_value_text (sb))
    return (m4_get_symbol_value_len (sa) == m4_get_symbol_value_len (sb)
	    && memcmp (m4_get_symbol_value_text (sa),
		       m4_get_symbol_value_text (sb),
		       m4_get_symbol_value_len (sa)) == 0);

  /* Convert both arguments to chains, if not one already.  */
  /* TODO - allow builtin tokens in the comparison?  */
  if (m4_is_symbol_value_text (sa))
    {
      tmpa.next = NULL;
      tmpa.type = M4__CHAIN_STR;
      tmpa.u.u_s.str = m4_get_symbol_value_text (sa);
      tmpa.u.u_s.len = m4_get_symbol_value_len (sa);
    }
  else
    {
      assert (sa->type == M4_SYMBOL_COMP);
      ca = sa->u.u_c.chain;
    }
  if (m4_is_symbol_value_text (sb))
    {
      tmpb.next = NULL;
      tmpb.type = M4__CHAIN_STR;
      tmpb.u.u_s.str = m4_get_symbol_value_text (sb);
      tmpb.u.u_s.len = m4_get_symbol_value_len (sb);
    }
  else
    {
      assert (sb->type == M4_SYMBOL_COMP);
      cb = sb->u.u_c.chain;
    }

  /* Compare each link of the chain.  */
  while (ca && cb)
    {
      if (ca->type == M4__CHAIN_ARGV)
	{
	  tmpa.next = ca->next;
	  tmpa.type = M4__CHAIN_STR;
	  /* TODO support funcs in $@.  */
	  assert (!ca->u.u_a.has_func || argv->flatten || ca->u.u_a.flatten);
	  m4_arg_print (context, obs, ca->u.u_a.argv, ca->u.u_a.index,
			m4__quote_cache (M4SYNTAX, NULL, ca->quote_age,
					 ca->u.u_a.quotes),
			argv->flatten || ca->u.u_a.flatten, NULL, NULL, false,
			false);
	  tmpa.u.u_s.len = obstack_object_size (obs);
	  tmpa.u.u_s.str = (char *) obstack_finish (obs);
	  ca = &tmpa;
	  continue;
	}
      if (cb->type == M4__CHAIN_ARGV)
	{
	  tmpb.next = cb->next;
	  tmpb.type = M4__CHAIN_STR;
	  /* TODO support funcs in $@.  */
	  assert (!cb->u.u_a.has_func || argv->flatten || cb->u.u_a.flatten);
	  m4_arg_print (context, obs, cb->u.u_a.argv, cb->u.u_a.index,
			m4__quote_cache (M4SYNTAX, NULL, cb->quote_age,
					 cb->u.u_a.quotes),
			argv->flatten || cb->u.u_a.flatten, NULL, NULL, false,
			false);
	  tmpb.u.u_s.len = obstack_object_size (obs);
	  tmpb.u.u_s.str = (char *) obstack_finish (obs);
	  cb = &tmpb;
	  continue;
	}
      assert (ca->type == M4__CHAIN_STR && cb->type == M4__CHAIN_STR);
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
	  tmpb.u.u_s.str = cb->u.u_s.str + ca->u.u_s.len;
	  tmpb.u.u_s.len = cb->u.u_s.len - ca->u.u_s.len;
	  ca = ca->next;
	  cb = &tmpb;
	}
      else
	{
	  assert (cb->u.u_s.len < ca->u.u_s.len);
	  if (memcmp (ca->u.u_s.str, cb->u.u_s.str, cb->u.u_s.len) != 0)
	    return false;
	  tmpa.next = ca->next;
	  tmpa.u.u_s.str = ca->u.u_s.str + cb->u.u_s.len;
	  tmpa.u.u_s.len = ca->u.u_s.len - cb->u.u_s.len;
	  ca = &tmpa;
	  cb = cb->next;
	}
    }

  /* If we get this far, the two arguments are equal only if both
     chains are exhausted.  */
  assert (ca != cb || !ca);
  return ca == cb;
}

/* Given ARGV, return true if argument INDEX is the empty string.
   This gives the same result as comparing m4_arg_len against 0, but
   is often faster.  */
bool
m4_arg_empty (m4_macro_args *argv, size_t index)
{
  return (index ? m4_arg_symbol (argv, index) == &empty_symbol
	  : !argv->argv0_len);
}

/* Given ARGV, return the length of argument INDEX.  Abort if the
   argument is not text.  Indices beyond argc return 0.  */
size_t
m4_arg_len (m4 *context, m4_macro_args *argv, size_t index)
{
  m4_symbol_value *value;
  m4__symbol_chain *chain;
  size_t len;

  if (index == 0)
    return argv->argv0_len;
  if (argv->argc <= index)
    return 0;
  value = m4_arg_symbol (argv, index);
  if (m4_is_symbol_value_text (value))
    return m4_get_symbol_value_len (value);
  assert (value->type == M4_SYMBOL_COMP);
  chain = value->u.u_c.chain;
  len = 0;
  while (chain)
    {
      size_t i;
      size_t limit;
      const m4_string_pair *quotes;
      switch (chain->type)
	{
	case M4__CHAIN_STR:
	  len += chain->u.u_s.len;
	  break;
	case M4__CHAIN_ARGV:
	  i = chain->u.u_a.index;
	  limit = chain->u.u_a.argv->argc - i - chain->u.u_a.skip_last;
	  quotes = m4__quote_cache (M4SYNTAX, NULL, chain->quote_age,
				    chain->u.u_a.quotes);
	  assert (limit);
	  if (quotes)
	    len += (quotes->len1 + quotes->len2) * limit;
	  len += limit - 1;
	  while (limit--)
	    {
	      /* TODO handle concatenation of builtins.  */
	      if (m4_is_symbol_value_func (m4_arg_symbol (chain->u.u_a.argv,
							  i)))
		assert (argv->flatten);
	      else
		len += m4_arg_len (context, chain->u.u_a.argv, i);
	      i++;
	    }
	  break;
	default:
	  assert (!"m4_arg_len");
	  abort ();
	}
      chain = chain->next;
    }
  assert (len);
  return len;
}

/* Given ARGV, return the builtin function referenced by argument
   INDEX.  Abort if it is not a single builtin.  */
m4_builtin_func *
m4_arg_func (m4_macro_args *argv, size_t index)
{
  return m4_get_symbol_value_func (m4_arg_symbol (argv, index));
}

/* Dump a representation of ARGV to the obstack OBS, starting with
   argument INDEX.  If QUOTES is non-NULL, each argument is displayed
   with those quotes.  If FLATTEN, builtins are ignored.  Separate
   arguments with SEP, which defaults to a comma.  If MAX_LEN is
   non-NULL, truncate the output after *MAX_LEN bytes are output and
   return true; otherwise, return false, and reduce *MAX_LEN by the
   number of bytes output.  If QUOTE_EACH, the truncation length is
   reset for each argument, quotes do not count against length, and
   all arguments are printed; otherwise, quotes count against the
   length and trailing arguments may be discarded.  If MODULE, print
   any details about originating modules; modules do not count against
   truncation length.  */
bool
m4_arg_print (m4 *context, m4_obstack *obs, m4_macro_args *argv, size_t index,
	      const m4_string_pair *quotes, bool flatten, const char *sep,
	      size_t *max_len, bool quote_each, bool module)
{
  size_t len = max_len ? *max_len : SIZE_MAX;
  size_t i;
  bool use_sep = false;
  size_t sep_len;
  size_t *plen = quote_each ? NULL : &len;

  if (!sep)
    sep = ",";
  sep_len = strlen (sep);
  for (i = index; i < argv->argc; i++)
    {
      if (quote_each && max_len)
	len = *max_len;
      if (use_sep && m4_shipout_string_trunc (obs, sep, sep_len, NULL, plen))
	return true;
      use_sep = true;
      if (quotes && !quote_each
	  && m4_shipout_string_trunc (obs, quotes->str1, quotes->len1, NULL,
				      plen))
	return true;
      if (m4_symbol_value_print (context, m4_arg_symbol (argv, i), obs,
				 quote_each ? quotes : NULL, flatten, &len,
				 module))
	return true;
      if (quotes && !quote_each
	  && m4_shipout_string_trunc (obs, quotes->str2, quotes->len2, NULL,
				      plen))
	return true;
    }
  if (max_len)
    *max_len = len;
  return false;
}

/* Create a new argument object using the same obstack as ARGV; thus,
   the new object will automatically be freed when the original is
   freed.  Explicitly set the macro name (argv[0]) from ARGV0 with
   length ARGV0_LEN.  If SKIP, set argv[1] of the new object to
   argv[2] of the old, otherwise the objects share all arguments.  If
   FLATTEN, any builtins in ARGV are flattened to an empty string when
   referenced through the new object.  */
m4_macro_args *
m4_make_argv_ref (m4 *context, m4_macro_args *argv, const char *argv0,
		  size_t argv0_len, bool skip, bool flatten)
{
  m4_macro_args *new_argv;
  m4_symbol_value *value;
  m4_symbol_value *new_value;
  size_t index = skip ? 2 : 1;
  m4_obstack *obs = m4_arg_scratch (context);

  new_value = (m4_symbol_value *) obstack_alloc (obs, sizeof *value);
  value = make_argv_ref (context, new_value, obs, context->expansion_level - 1,
			 argv, index, flatten, NULL);
  if (!value)
    {
      obstack_free (obs, new_value);
      new_argv = (m4_macro_args *) obstack_alloc (obs, offsetof (m4_macro_args,
								 array));
      new_argv->arraylen = 0;
      new_argv->wrapper = false;
      new_argv->has_ref = false;
      new_argv->flatten = false;
      new_argv->has_func = false;
    }
  else
    {
      new_argv = (m4_macro_args *) obstack_alloc (obs, (offsetof (m4_macro_args,
								  array)
							+ sizeof value));
      new_argv->arraylen = 1;
      new_argv->array[0] = value;
      new_argv->wrapper = true;
      new_argv->has_ref = argv->has_ref;
      new_argv->flatten = flatten;
      new_argv->has_func = argv->has_func;
    }
  new_argv->argc = argv->argc - (index - 1);
  new_argv->inuse = false;
  new_argv->argv0 = argv0;
  new_argv->argv0_len = argv0_len;
  new_argv->quote_age = argv->quote_age;
  new_argv->level = argv->level;
  return new_argv;
}

/* Push argument INDEX from ARGV, which must be a text token, onto the
   expansion stack OBS for rescanning.  */
void
m4_push_arg (m4 *context, m4_obstack *obs, m4_macro_args *argv, size_t index)
{
  m4_symbol_value value;

  if (index == 0)
    {
      m4_set_symbol_value_text (&value, argv->argv0, argv->argv0_len, 0);
      if (m4__push_symbol (context, &value, context->expansion_level - 1,
			   argv->inuse))
	arg_mark (argv);
    }
  else
    m4__push_arg_quote (context, obs, argv, index, NULL);
}

/* Push argument INDEX from ARGV, which must be a text token, onto the
   expansion stack OBS for rescanning.  INDEX must be non-zero.
   QUOTES determines any quote delimiters that were in effect when the
   reference was created.  */
void
m4__push_arg_quote (m4 *context, m4_obstack *obs, m4_macro_args *argv,
		    size_t index, const m4_string_pair *quotes)
{
  size_t level;
  m4_symbol_value *value = arg_symbol (argv, index, &level);

  /* TODO handle builtin tokens?  */
  if (quotes)
    obstack_grow (obs, quotes->str1, quotes->len1);
  if (value != &empty_symbol
      && m4__push_symbol (context, value, level, argv->inuse))
    arg_mark (argv);
  if (quotes)
    obstack_grow (obs, quotes->str2, quotes->len2);
}

/* Push series of comma-separated arguments from ARGV onto the
   expansion stack OBS for rescanning.  If SKIP, then don't push the
   first argument.  If QUOTE, also push quoting around each arg.  */
void
m4_push_args (m4 *context, m4_obstack *obs, m4_macro_args *argv, bool skip,
	      bool quote)
{
  m4_symbol_value tmp;
  m4_symbol_value *value;
  m4__symbol_chain *chain;
  size_t i = skip ? 2 : 1;
  const m4_string_pair *quotes = m4_get_syntax_quotes (M4SYNTAX);
  char *str = NULL;
  size_t len = obstack_object_size (obs);

  if (argv->argc <= i)
    return;

  if (argv->argc == i + 1)
    {
      m4__push_arg_quote (context, obs, argv, i, quote ? quotes : NULL);
      return;
    }

  /* Since make_argv_ref puts data on obs, we must first close any
     pending data.  The resulting symbol contents live entirely on
     obs, so we call push_symbol with a level of -1.  */
  if (len)
    {
      obstack_1grow (obs, '\0');
      str = (char *) obstack_finish (obs);
    }

  /* TODO allow shift, $@, to push builtins without flatten.  */
  value = make_argv_ref (context, &tmp, obs, -1, argv, i, true,
			 quote ? quotes : NULL);
  assert (value == &tmp);
  if (len)
    {
      chain = (m4__symbol_chain *) obstack_alloc (obs, sizeof *chain);
      chain->next = value->u.u_c.chain;
      value->u.u_c.chain = chain;
      chain->type = M4__CHAIN_STR;
      chain->quote_age = 0;
      chain->u.u_s.str = str;
      chain->u.u_s.len = len;
      chain->u.u_s.level = SIZE_MAX;
    }
  if (m4__push_symbol (context, value, -1, argv->inuse))
    arg_mark (argv);
}


/* Define these last, so that earlier uses can benefit from the macros
   in m4private.h.  */

/* Given ARGV, return one greater than the number of arguments it
   describes.  */
#undef m4_arg_argc
size_t
m4_arg_argc (m4_macro_args *argv)
{
  return argv->argc;
}

/* Return an obstack useful for scratch calculations, and which will
   not interfere with macro expansion.  The obstack will be reset when
   expand_macro completes.  */
#undef m4_arg_scratch
m4_obstack *
m4_arg_scratch (m4 *context)
{
  m4__macro_arg_stacks *stack
    = &context->arg_stacks[context->expansion_level - 1];
  assert (obstack_object_size (stack->args) == 0);
  return stack->args;
}

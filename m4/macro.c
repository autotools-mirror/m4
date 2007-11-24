/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2001, 2006, 2007
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

/* This file contains the functions that perform the basic argument
   parsing and macro expansion.  */

#include <config.h>

#include <stdarg.h>

#include "m4private.h"

#include "intprops.h"

static void    collect_arguments (m4 *, const char *, m4_symbol *,
				  m4_obstack *, m4_obstack *);
static void    expand_macro      (m4 *, const char *, m4_symbol *);
static void    expand_token      (m4 *, m4_obstack *, m4__token_type,
				  m4_symbol_value *, int);
static bool    expand_argument   (m4 *, m4_obstack *, m4_symbol_value *,
				  const char *);
static void    process_macro	 (m4 *, m4_symbol_value *, m4_obstack *, int,
				  m4_symbol_value **);

static void    trace_prepre	 (m4 *, const char *, size_t,
				  m4_symbol_value *);
static void    trace_pre	 (m4 *, const char *, size_t, int,
				  m4_symbol_value **);
static void    trace_post	 (m4 *, const char *, size_t, int,
				  m4_symbol_value **, m4_input_block *, bool);

static void    trace_format	 (m4 *, const char *, ...)
  M4_GNUC_PRINTF (2, 3);
static void    trace_header	 (m4 *, size_t);
static void    trace_flush	 (m4 *);


/* Current recursion level in expand_macro ().  */
static size_t expansion_level = 0;

/* The number of the current call of expand_macro ().  */
static size_t macro_call_id = 0;

/* The shared stack of collected arguments for macro calls; as each
   argument is collected, it is finished and its location stored in
   argv_stack.  This stack can be used simultaneously by multiple
   macro calls, using obstack_regrow to handle partial objects
   embedded in the stack.  */
static struct obstack argc_stack;

/* The shared stack of pointers to collected arguments for macro
   calls.  This object is never finished; we exploit the fact that
   obstack_blank is documented to take a negative size to reduce the
   size again.  */
static struct obstack argv_stack;

/* This function reads all input, and expands each token, one at a time.  */
void
m4_macro_expand_input (m4 *context)
{
  m4__token_type type;
  m4_symbol_value token;
  int line;

  obstack_init (&argc_stack);
  obstack_init (&argv_stack);

  while ((type = m4__next_token (context, &token, &line, NULL))
	 != M4_TOKEN_EOF)
    expand_token (context, (m4_obstack *) NULL, type, &token, line);

  obstack_free (&argc_stack, NULL);
  obstack_free (&argv_stack, NULL);
}


/* Expand one token, according to its type.  Potential macro names
   (M4_TOKEN_WORD) are looked up in the symbol table, to see if they have a
   macro definition.  If they have, they are expanded as macros, otherwise
   the text are just copied to the output.  */
static void
expand_token (m4 *context, m4_obstack *obs,
	      m4__token_type type, m4_symbol_value *token, int line)
{
  m4_symbol *symbol;
  const char *text = (m4_is_symbol_value_text (token)
		      ? m4_get_symbol_value_text (token) : NULL);

  switch (type)
    {				/* TOKSW */
    case M4_TOKEN_EOF:
    case M4_TOKEN_MACDEF:
      break;

    case M4_TOKEN_OPEN:
    case M4_TOKEN_COMMA:
    case M4_TOKEN_CLOSE:
    case M4_TOKEN_SIMPLE:
    case M4_TOKEN_STRING:
    case M4_TOKEN_SPACE:
      m4_shipout_text (context, obs, text, strlen (text), line);
      break;

    case M4_TOKEN_WORD:
      {
	const char *textp = text;

	if (m4_has_syntax (M4SYNTAX, to_uchar (*textp), M4_SYNTAX_ESCAPE))
	  ++textp;

	symbol = m4_symbol_lookup (M4SYMTAB, textp);
	assert (! symbol || ! m4_is_symbol_void (symbol));
	if (symbol == NULL
	    || (symbol->value->type == M4_SYMBOL_FUNC
		&& BIT_TEST (SYMBOL_FLAGS (symbol), VALUE_BLIND_ARGS_BIT)
		&& ! m4__next_token_is_open (context)))
	  {
	    m4_shipout_text (context, obs, text, strlen (text), line);
	  }
	else
	  expand_macro (context, textp, symbol);
      }
      break;

    default:
      assert (!"INTERNAL ERROR: bad token type in expand_token ()");
      abort ();
    }
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

  argp->type = M4_SYMBOL_VOID;

  /* Skip leading white space.  */
  do
    {
      type = m4__next_token (context, &token, NULL, caller);
    }
  while (type == M4_TOKEN_SPACE);

  while (1)
    {
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
	      if (argp->type == M4_SYMBOL_FUNC
		  && obstack_object_size (obs) == 0)
		return type == M4_TOKEN_COMMA;
	      obstack_1grow (obs, '\0');
	      VALUE_MODULE (argp) = NULL;
	      m4_set_symbol_value_text (argp, obstack_finish (obs));
	      return type == M4_TOKEN_COMMA;
	    }
	  /* fallthru */
	case M4_TOKEN_OPEN:
	case M4_TOKEN_SIMPLE:
	  if (type == M4_TOKEN_OPEN)
	    paren_level++;
	  else if (type == M4_TOKEN_CLOSE)
	    paren_level--;
	  expand_token (context, obs, type, &token, line);
	  break;

	case M4_TOKEN_EOF:
	  m4_error_at_line (context, EXIT_FAILURE, 0, file, line, caller,
			    _("end of file in argument list"));
	  break;

	case M4_TOKEN_WORD:
	case M4_TOKEN_SPACE:
	case M4_TOKEN_STRING:
	  expand_token (context, obs, type, &token, line);
	  break;

	case M4_TOKEN_MACDEF:
	  if (argp->type == M4_SYMBOL_VOID && obstack_object_size (obs) == 0)
	    m4_symbol_value_copy (argp, &token);
	  else
	    argp->type = M4_SYMBOL_TEXT;
	  break;

	default:
	  assert (!"INTERNAL ERROR: bad token type in expand_argument ()");
	  abort ();
	}

      type = m4__next_token (context, &token, NULL, caller);
    }
}


/* The macro expansion is handled by expand_macro ().  It parses the
   arguments, using collect_arguments (), and builds a table of pointers to
   the arguments.  The arguments themselves are stored on a local obstack.
   Expand_macro () uses call_macro () to do the call of the macro.

   Expand_macro () is potentially recursive, since it calls expand_argument
   (), which might call expand_token (), which might call expand_macro ().

   NAME points to storage on the token stack, so it is only valid
   until a call to collect_arguments parses more tokens.  SYMBOL is
   the result of the symbol table lookup on NAME.  */
static void
expand_macro (m4 *context, const char *name, m4_symbol *symbol)
{
  char *argc_base = NULL;	/* Base of argc_stack on entry.  */
  unsigned int argc_size;	/* Size of argc_stack on entry.  */
  unsigned int argv_size;	/* Size of argv_stack on entry.  */
  m4_symbol_value **argv;
  int argc;
  m4_obstack *expansion;
  m4_input_block *expanded;
  bool traced;
  bool trace_expansion = false;
  size_t my_call_id;
  m4_symbol_value *value;

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
  expansion_level++;
  if (m4_get_nesting_limit_opt (context) > 0
      && expansion_level > m4_get_nesting_limit_opt (context))
    m4_error (context, EXIT_FAILURE, 0, NULL, _("\
recursion limit of %zu exceeded, use -L<N> to change it"),
	      m4_get_nesting_limit_opt (context));

  macro_call_id++;
  my_call_id = macro_call_id;

  argc_size = obstack_object_size (&argc_stack);
  argv_size = obstack_object_size (&argv_stack);
  if (0 < argc_size)
    argc_base = obstack_finish (&argc_stack);

  if (traced && m4_is_debug_bit (context, M4_DEBUG_TRACE_CALL))
    trace_prepre (context, name, my_call_id, value);

  collect_arguments (context, name, symbol, &argv_stack, &argc_stack);

  argc = ((obstack_object_size (&argv_stack) - argv_size)
	  / sizeof (m4_symbol_value *));
  argv = (m4_symbol_value **) ((char *) obstack_base (&argv_stack)
			       + argv_size);
  /* Calling collect_arguments invalidated name, but we copied it as
     argv[0].  */
  name = m4_get_symbol_value_text (argv[0]);

  loc_close_file = m4_get_current_file (context);
  loc_close_line = m4_get_current_line (context);
  m4_set_current_file (context, loc_open_file);
  m4_set_current_line (context, loc_open_line);

  if (traced)
    trace_pre (context, name, my_call_id, argc, argv);

  expansion = m4_push_string_init (context);
  m4_macro_call (context, value, expansion, argc, argv);
  expanded = m4_push_string_finish ();

  if (traced)
    trace_post (context, name, my_call_id, argc, argv, expanded,
		trace_expansion);

  m4_set_current_file (context, loc_close_file);
  m4_set_current_line (context, loc_close_line);

  --expansion_level;
  --VALUE_PENDING (value);
  if (BIT_TEST (VALUE_FLAGS (value), VALUE_DELETED_BIT))
    m4_symbol_value_delete (value);

  if (0 < argc_size)
    obstack_regrow (&argc_stack, argc_base, argc_size);
  else
    obstack_free (&argc_stack, argv[0]);
  obstack_blank (&argv_stack, -argc * sizeof (m4_symbol_value *));
}

/* Collect all the arguments to a call of the macro SYMBOL (called NAME).
   The arguments are stored on the obstack ARGUMENTS and a table of pointers
   to the arguments on the obstack ARGPTR.  */
static void
collect_arguments (m4 *context, const char *name, m4_symbol *symbol,
		   m4_obstack *argptr, m4_obstack *arguments)
{
  m4_symbol_value token;
  m4_symbol_value *tokenp;
  bool more_args;
  bool groks_macro_args;

  groks_macro_args = BIT_TEST (SYMBOL_FLAGS (symbol), VALUE_MACRO_ARGS_BIT);

  tokenp = (m4_symbol_value *) obstack_alloc (arguments, sizeof *tokenp);
  m4_set_symbol_value_text (tokenp, (char *) obstack_copy0 (arguments, name,
							    strlen (name)));
  name = m4_get_symbol_value_text (tokenp);
  obstack_ptr_grow (argptr, tokenp);

  if (m4__next_token_is_open (context))
    {
      m4__next_token (context, &token, NULL, name); /* gobble parenthesis */
      do
	{
	  more_args = expand_argument (context, arguments, &token, name);

	  if (!groks_macro_args && m4_is_symbol_value_func (&token))
	    {
	      VALUE_MODULE (&token) = NULL;
	      m4_set_symbol_value_text (&token, "");
	    }
	  tokenp = (m4_symbol_value *) obstack_copy (arguments, &token,
						     sizeof token);
	  obstack_ptr_grow (argptr, tokenp);
	}
      while (more_args);
    }
}


/* The actual call of a macro is handled by m4_macro_call ().
   m4_macro_call () is passed a SYMBOL, whose type is used to
   call either a builtin function, or the user macro expansion
   function process_macro ().  There are ARGC arguments to
   the call, stored in the ARGV table.  The expansion is left on
   the obstack EXPANSION.  Macro tracing is also handled here.  */
void
m4_macro_call (m4 *context, m4_symbol_value *value, m4_obstack *expansion,
	       int argc, m4_symbol_value **argv)
{
  if (m4_bad_argc (context, argc, m4_get_symbol_value_text (argv[0]),
		   VALUE_MIN_ARGS (value), VALUE_MAX_ARGS (value),
		   BIT_TEST (VALUE_FLAGS (value),
			     VALUE_SIDE_EFFECT_ARGS_BIT)))
    return;
  if (m4_is_symbol_value_text (value))
    {
      process_macro (context, value, expansion, argc, argv);
    }
  else if (m4_is_symbol_value_func (value))
    {
      (*m4_get_symbol_value_func (value)) (context, expansion, argc, argv);
    }
  else if (m4_is_symbol_value_placeholder (value))
    {
      m4_warn (context, 0, M4ARG (0),
	       _("builtin `%s' requested by frozen file not found"),
	       m4_get_symbol_value_placeholder (value));
    }
  else
    {
      assert (!"INTERNAL ERROR: bad symbol type in call_macro ()");
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
	       int argc, m4_symbol_value **argv)
{
  const char *text;
  int i;

  for (text = m4_get_symbol_value_text (value); *text != '\0';)
    {
      char ch;

      if (!m4_has_syntax (M4SYNTAX, to_uchar (*text), M4_SYNTAX_DOLLAR))
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
	    }
	  else
	    {
	      char *endp;
	      i = (int) strtol (text, &endp, 10);
	      text = endp;
	    }
	  if (i < argc)
	    m4_shipout_string (context, obs, M4ARG (i), 0, false);
	  break;

	case '#':		/* number of arguments */
	  m4_shipout_int (obs, argc - 1);
	  text++;
	  break;

	case '*':		/* all arguments */
	case '@':		/* ... same, but quoted */
	  m4_dump_args (context, obs, argc, argv, ",", *text == '@');
	  text++;
	  break;

	default:
	  if (m4_get_posixly_correct_opt (context)
	      || !VALUE_ARG_SIGNATURE (value))
	    {
	      obstack_1grow (obs, ch);
	    }
	  else
	    {
	      size_t len  = 0;
	      const char *endp;
	      const char *key;

	      for (endp = ++text;
		   *endp && m4_has_syntax (M4SYNTAX, to_uchar (*endp),
					   (M4_SYNTAX_OTHER | M4_SYNTAX_ALPHA
					    | M4_SYNTAX_NUM));
		   ++endp)
		{
		  ++len;
		}
	      key = xstrndup (text, len);

	      if (*endp)
		{
		  struct m4_symbol_arg **arg
		    = (struct m4_symbol_arg **)
		      m4_hash_lookup (VALUE_ARG_SIGNATURE (value), key);

		  if (arg)
		    {
		      i = SYMBOL_ARG_INDEX (*arg);

		      if (i < argc)
			m4_shipout_string (context, obs, M4ARG (i), 0, false);
		      else
			{
			  assert (!"INTERNAL ERROR: out of range reference");
			  abort ();
			}
		    }
		}
	      else
		{
		  m4_error (context, 0, 0, M4ARG (0),
			    _("unterminated parameter reference: %s"),
			    key);
		}

	      text = *endp ? 1 + endp : endp;

	      free ((char *) key);
	      break;
	    }
	  break;
	}
    }
}



/* The rest of this file contains the functions for macro tracing output.
   All tracing output for a macro call is collected on an obstack TRACE,
   and printed whenever the line is complete.  This prevents tracing
   output from interfering with other debug messages generated by the
   various builtins.  */

/* Tracing output is formatted here, by a simplified printf-to-obstack
   function trace_format ().  Understands only %s, %d, %zu (size_t
   value).  */
static void
trace_format (m4 *context, const char *fmt, ...)
{
  va_list args;
  char ch;
  const char *s;

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
	    char nbuf[INT_BUFSIZE_BOUND (int)];

	    sprintf (nbuf, "%d", d);
	    s = nbuf;
	  }
	  break;

	case 'z':
	  ch = *fmt++;
	  assert (ch == 'u');
	  {
	    size_t z = va_arg (args, size_t);
	    char nbuf[INT_BUFSIZE_BOUND (size_t)];

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
  trace_format (context, " -%zu- ", expansion_level);
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

/* Do pre-argument-collction tracing for macro NAME.  Used from
   expand_macro ().  */
static void
trace_prepre (m4 *context, const char *name, size_t id, m4_symbol_value *value)
{
  bool quote = m4_is_debug_bit (context, M4_DEBUG_TRACE_QUOTE);
  const char *lquote = m4_get_syntax_lquote (M4SYNTAX);
  const char *rquote = m4_get_syntax_rquote (M4SYNTAX);
  size_t arg_length = m4_get_max_debug_arg_length_opt (context);
  bool module = m4_is_debug_bit (context, M4_DEBUG_TRACE_MODULE);

  trace_header (context, id);
  trace_format (context, "%s ... = ", name);
  m4_symbol_value_print (value, &context->trace_messages,
			 quote, lquote, rquote, arg_length, module);
  trace_flush (context);
}

/* Format the parts of a trace line, that can be made before the macro is
   actually expanded.  Used from expand_macro ().  */
static void
trace_pre (m4 *context, const char *name, size_t id,
	   int argc, m4_symbol_value **argv)
{
  int i;

  trace_header (context, id);
  trace_format (context, "%s", name);

  if ((argc > 1) && m4_is_debug_bit (context, M4_DEBUG_TRACE_ARGS))
    {
      bool quote = m4_is_debug_bit (context, M4_DEBUG_TRACE_QUOTE);
      const char *lquote = m4_get_syntax_lquote (M4SYNTAX);
      const char *rquote = m4_get_syntax_rquote (M4SYNTAX);
      size_t arg_length = m4_get_max_debug_arg_length_opt (context);
      bool module = m4_is_debug_bit (context, M4_DEBUG_TRACE_MODULE);

      trace_format (context, "(");
      for (i = 1; i < argc; i++)
	{
	  if (i != 1)
	    trace_format (context, ", ");

	  m4_symbol_value_print (argv[i], &context->trace_messages,
				 quote, lquote, rquote, arg_length, module);
	}
      trace_format (context, ")");
    }
}

/* Format the final part of a trace line and print it all.  Used from
   expand_macro ().  */
static void
trace_post (m4 *context, const char *name, size_t id,
	    int argc, m4_symbol_value **argv, m4_input_block *expanded,
	    bool trace_expansion)
{
  if (trace_expansion)
    {
      trace_format (context, " -> ");
      m4_input_print (context, &context->trace_messages, expanded);
    }

  trace_flush (context);
}

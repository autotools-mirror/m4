/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2001, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

/* This file contains the functions that perform the basic argument
   parsing and macro expansion.  */

#include <stdarg.h>

#include "m4private.h"

static void    collect_arguments (m4 *context, const char *name,
				  m4_symbol *symbol, m4_obstack *argptr,
				  m4_obstack *arguments);
static void    expand_macro      (m4 *context, const char *name,
				  m4_symbol *symbol);
static void    expand_token      (m4 *context, m4_obstack *obs,
				  m4__token_type type, m4_symbol_value *token);
static bool    expand_argument   (m4 *context, m4_obstack *obs,
				  m4_symbol_value *argp);

static void    process_macro	 (m4 *context, m4_symbol *symbol,
				  m4_obstack *expansion, int argc,
				  m4_symbol_value **argv);

static void    trace_prepre	 (m4 *context, const char *, int);
static void    trace_pre	 (m4 *context, const char *, int, int,
				  m4_symbol_value **);
static void    trace_post	 (m4 *context, const char *, int, int,
				  m4_symbol_value **, const char *);

/* It would be nice if we could use M4_GNUC_PRINTF(2, 3) on
   trace_format, but since we don't accept the same set of modifiers,
   it would lead to compiler warnings.  */
static void    trace_format	 (m4 *context, const char *fmt, ...);

static void    trace_header	 (m4 *, int);
static void    trace_flush	 (m4 *);


/* Current recursion level in expand_macro ().  */
static int expansion_level = 0;

/* The number of the current call of expand_macro ().  */
static int macro_call_id = 0;

/* This function reads all input, and expands each token, one at a time.  */
void
m4_macro_expand_input (m4 *context)
{
  m4__token_type type;
  m4_symbol_value token;

  while ((type = m4__next_token (context, &token)) != M4_TOKEN_EOF)
    expand_token (context, (m4_obstack *) NULL, type, &token);
}


/* Expand one token, according to its type.  Potential macro names
   (M4_TOKEN_WORD) are looked up in the symbol table, to see if they have a
   macro definition.  If they have, they are expanded as macros, otherwise
   the text are just copied to the output.  */
static void
expand_token (m4 *context, m4_obstack *obs,
	      m4__token_type type, m4_symbol_value *token)
{
  m4_symbol *symbol;
  char *text = xstrdup (m4_get_symbol_value_text (token));

  switch (type)
    {				/* TOKSW */
    case M4_TOKEN_EOF:
    case M4_TOKEN_MACDEF:
      break;

    case M4_TOKEN_SIMPLE:
    case M4_TOKEN_STRING:
    case M4_TOKEN_SPACE:
      m4_shipout_text (context, obs, text, strlen (text));
      break;

    case M4_TOKEN_WORD:
      {
	char *textp = text;
	int ch;

	if (m4_has_syntax (M4SYNTAX, *textp, M4_SYNTAX_ESCAPE))
	  ++textp;

	symbol = m4_symbol_lookup (M4SYMTAB, textp);
	if (symbol == NULL
	    || symbol->value->type == M4_SYMBOL_VOID
	    || (symbol->value->type == M4_SYMBOL_FUNC
		&& BIT_TEST (SYMBOL_FLAGS (symbol), VALUE_BLIND_ARGS_BIT)
		&& (ch = m4_peek_input (context)) < CHAR_EOF
		&& !m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_OPEN)))
	  {
	    m4_shipout_text (context, obs, text, strlen (text));
	  }
	else
	  expand_macro (context, textp, symbol);
      }
      break;

    default:
      assert (!"INTERNAL ERROR: bad token type in expand_token ()");
      abort ();
    }

  free (text);
}


/* This function parses one argument to a macro call.  It expects the first
   left parenthesis, or the separating comma to have been read by the
   caller.  It skips leading whitespace, and reads and expands tokens,
   until it finds a comma or a right parenthesis at the same level of
   parentheses.  It returns a flag indicating whether the argument read is
   the last for the active macro call.  The arguments are built on the
   obstack OBS, indirectly through expand_token ().	 */
static bool
expand_argument (m4 *context, m4_obstack *obs, m4_symbol_value *argp)
{
  m4__token_type type;
  m4_symbol_value token;
  char *text;
  int paren_level = 0;
  const char *current_file = m4_get_current_file (context);
  int current_line = m4_get_current_line (context);

  argp->type = M4_SYMBOL_VOID;

  /* Skip leading white space.  */
  do
    {
      type = m4__next_token (context, &token);
    }
  while (type == M4_TOKEN_SPACE);

  while (1)
    {
      switch (type)
	{			/* TOKSW */
	case M4_TOKEN_SIMPLE:
	  text = m4_get_symbol_value_text (&token);
	  if ((m4_has_syntax (M4SYNTAX, *text,
			      M4_SYNTAX_COMMA|M4_SYNTAX_CLOSE))
	      && paren_level == 0)
	    {

	      /* The argument MUST be finished, whether we want it or not.  */
	      obstack_1grow (obs, '\0');
	      text = obstack_finish (obs);

	      if (argp->type == M4_SYMBOL_VOID)
		{
		  m4_set_symbol_value_text (argp, text);
		}
	      return (bool) (m4_has_syntax (M4SYNTAX, *m4_get_symbol_value_text (&token), M4_SYNTAX_COMMA));
	    }

	  if (m4_has_syntax (M4SYNTAX, *text, M4_SYNTAX_OPEN))
	    paren_level++;
	  else if (m4_has_syntax (M4SYNTAX, *text, M4_SYNTAX_CLOSE))
	    paren_level--;
	  expand_token (context, obs, type, &token);
	  break;

	case M4_TOKEN_EOF:
	  error_at_line (EXIT_FAILURE, 0, current_file, current_line,
			 _("end of file in argument list"));
	  break;

	case M4_TOKEN_WORD:
	case M4_TOKEN_SPACE:
	case M4_TOKEN_STRING:
	  expand_token (context, obs, type, &token);
	  break;

	case M4_TOKEN_MACDEF:
	  if (obstack_object_size (obs) == 0)
	    m4_symbol_value_copy (argp, &token);
	  break;

	default:
	  assert (!"INTERNAL ERROR: bad token type in expand_argument ()");
	  abort ();
	}

      type = m4__next_token (context, &token);
    }
}


/* The macro expansion is handled by expand_macro ().  It parses the
   arguments, using collect_arguments (), and builds a table of pointers to
   the arguments.  The arguments themselves are stored on a local obstack.
   Expand_macro () uses call_macro () to do the call of the macro.

   Expand_macro () is potentially recursive, since it calls expand_argument
   (), which might call expand_token (), which might call expand_macro ().  */
static void
expand_macro (m4 *context, const char *name, m4_symbol *symbol)
{
  m4_obstack arguments;
  m4_obstack argptr;
  m4_symbol_value **argv;
  int argc;
  m4_obstack *expansion;
  const char *expanded;
  bool traced;
  int my_call_id;

  expansion_level++;
  if (expansion_level > m4_get_nesting_limit_opt (context))
    m4_error (context, EXIT_FAILURE, 0, _("\
recursion limit of %d exceeded, use -L<N> to change it"),
	      m4_get_nesting_limit_opt (context));

  macro_call_id++;
  my_call_id = macro_call_id;

  traced = (bool) (m4_is_debug_bit (context, M4_DEBUG_TRACE_ALL)
		      || m4_get_symbol_traced (symbol));

  obstack_init (&argptr);
  obstack_init (&arguments);

  if (traced && m4_is_debug_bit (context, M4_DEBUG_TRACE_CALL))
    trace_prepre (context, name, my_call_id);

  collect_arguments (context, name, symbol, &argptr, &arguments);

  argc = obstack_object_size (&argptr) / sizeof (m4_symbol_value *);
  argv = (m4_symbol_value **) obstack_finish (&argptr);

  if (traced)
    trace_pre (context, name, my_call_id, argc, argv);

  expansion = m4_push_string_init (context);
  m4_macro_call (context, symbol, expansion, argc, argv);
  expanded = m4_push_string_finish ();

  if (traced)
    trace_post (context, name, my_call_id, argc, argv, expanded);

  --expansion_level;

  obstack_free (&arguments, NULL);
  obstack_free (&argptr, NULL);
}

/* Collect all the arguments to a call of the macro SYMBOL (called NAME).
   The arguments are stored on the obstack ARGUMENTS and a table of pointers
   to the arguments on the obstack ARGPTR.  */
static void
collect_arguments (m4 *context, const char *name, m4_symbol *symbol,
		   m4_obstack *argptr, m4_obstack *arguments)
{
  int ch;			/* lookahead for ( */
  m4_symbol_value token;
  m4_symbol_value *tokenp;
  bool more_args;
  bool groks_macro_args;

  groks_macro_args = BIT_TEST (SYMBOL_FLAGS (symbol), VALUE_MACRO_ARGS_BIT);

  m4_set_symbol_value_text (&token, (char *) name);
  tokenp = (m4_symbol_value *) obstack_copy (arguments, (void *) &token,
				      sizeof (token));
  obstack_grow (argptr, (void *) &tokenp, sizeof (tokenp));

  ch = m4_peek_input (context);
  if ((ch < CHAR_EOF) && m4_has_syntax (M4SYNTAX, ch, M4_SYNTAX_OPEN))
    {
      m4__next_token (context, &token);		/* gobble parenthesis */
      do
	{
	  more_args = expand_argument (context, arguments, &token);

	  if (!groks_macro_args && m4_is_symbol_value_func (&token))
	    {
	      m4_set_symbol_value_text (&token, "");
	    }
	  tokenp = (m4_symbol_value *)
	    obstack_copy (arguments, (void *) &token, sizeof (token));
	  obstack_grow (argptr, (void *) &tokenp, sizeof (tokenp));
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
m4_macro_call (m4 *context, m4_symbol *symbol, m4_obstack *expansion,
	       int argc, m4_symbol_value **argv)
{
  if (m4_bad_argc (context, argc, argv,
		   SYMBOL_MIN_ARGS (symbol), SYMBOL_MAX_ARGS (symbol),
		   BIT_TEST (SYMBOL_FLAGS (symbol),
			     VALUE_SIDE_EFFECT_ARGS_BIT)))
    return;
  if (m4_is_symbol_text (symbol))
    {
      process_macro (context, symbol, expansion, argc, argv);
    }
  else if (m4_is_symbol_func (symbol))
    {
      (*m4_get_symbol_func (symbol)) (context, expansion, argc, argv);
    }
  else if (m4_is_symbol_placeholder (symbol))
    {
      m4_warn (context, 0,
	       _("%s: builtin `%s' requested by frozen file not found"),
	       M4ARG (0), m4_get_symbol_placeholder (symbol));
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
process_macro (m4 *context, m4_symbol *symbol, m4_obstack *obs,
	       int argc, m4_symbol_value **argv)
{
  const unsigned char *text;
  int i;

  for (text = m4_get_symbol_text (symbol); *text != '\0';)
    {
      char ch;

      if (!m4_has_syntax (M4SYNTAX, *text, M4_SYNTAX_DOLLAR))
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
	  if (m4_get_posixly_correct_opt (context) || !isdigit(text[1]))
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
	      || !SYMBOL_ARG_SIGNATURE (symbol))
	    {
	      obstack_1grow (obs, ch);
	    }
	  else
	    {
	      size_t       len  = 0;
	      const char * endp;
	      const char * key;

	      for (endp = ++text;
		   *endp && m4_has_syntax (M4SYNTAX, *endp,
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
		      m4_hash_lookup (SYMBOL_ARG_SIGNATURE (symbol), key);

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
		  m4_error (context, 0, 0,
			    _("%s: unterminated parameter reference: %s"),
			    M4ARG (0), key);
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
  function trace_format ().  Understands only %S, %s, %d, %l (optional
  left quote) and %r (optional right quote).  */
static void
trace_format (m4 *context, const char *fmt, ...)
{
  va_list args;
  char ch;

  int d;
  char nbuf[32];
  const char *s;
  int slen;
  int maxlen;

  va_start (args, fmt);

  while (true)
    {
      while ((ch = *fmt++) != '\0' && ch != '%')
	obstack_1grow (&context->trace_messages, ch);

      if (ch == '\0')
	break;

      maxlen = 0;
      switch (*fmt++)
	{
	case 'S':
	  maxlen = m4_get_max_debug_arg_length_opt (context);
	  /* fall through */

	case 's':
	  s = va_arg (args, const char *);
	  break;

	case 'l':
	  s = m4_is_debug_bit (context, M4_DEBUG_TRACE_QUOTE)
		? m4_get_syntax_lquote (M4SYNTAX)
		: "";
	  break;

	case 'r':
	  s = m4_is_debug_bit(context, M4_DEBUG_TRACE_QUOTE)
		? m4_get_syntax_rquote (M4SYNTAX)
		: "";
	  break;

	case 'd':
	  d = va_arg (args, int);
	  sprintf (nbuf, "%d", d);
	  s = nbuf;
	  break;

	default:
	  s = "";
	  break;
	}

      slen = strlen (s);
      if (maxlen == 0 || maxlen > slen)
	obstack_grow (&context->trace_messages, s, slen);
      else
	{
	  obstack_grow (&context->trace_messages, s, maxlen);
	  obstack_grow (&context->trace_messages, "...", 3);
	}
    }

  va_end (args);
}

/* Format the standard header attached to all tracing output lines.  */
static void
trace_header (m4 *context, int id)
{
  trace_format (context, "m4trace:");
  if (m4_get_current_line (context))
    {
      if (m4_is_debug_bit (context, M4_DEBUG_TRACE_FILE))
	trace_format (context, "%s:", m4_get_current_file (context));
      if (m4_is_debug_bit (context, M4_DEBUG_TRACE_LINE))
	trace_format (context, "%d:", m4_get_current_line (context));
    }
  trace_format (context, " -%d- ", expansion_level);
  if (m4_is_debug_bit (context, M4_DEBUG_TRACE_CALLID))
    trace_format (context, "id %d: ", id);
}

/* Print current tracing line, and clear the obstack.  */
static void
trace_flush (m4 *context)
{
  char *line;

  obstack_1grow (&context->trace_messages, '\0');
  line = obstack_finish (&context->trace_messages);
  if (m4_get_debug_file (context))
    fprintf (m4_get_debug_file (context), "%s\n", line);
  obstack_free (&context->trace_messages, line);
}

/* Do pre-argument-collction tracing for macro NAME.  Used from
   expand_macro ().  */
static void
trace_prepre (m4 *context, const char *name, int id)
{
  trace_header (context, id);
  trace_format (context, "%s ...", name);
  trace_flush  (context);
}

/* Format the parts of a trace line, that can be made before the macro is
   actually expanded.  Used from expand_macro ().  */
static void
trace_pre (m4 *context, const char *name, int id,
	   int argc, m4_symbol_value **argv)
{
  int i;
  const m4_builtin *bp;

  trace_header (context, id);
  trace_format (context, "%s", name);

  if ((argc > 1) && m4_is_debug_bit (context, M4_DEBUG_TRACE_ARGS))
    {
      trace_format (context, "(");

      for (i = 1; i < argc; i++)
	{
	  if (i != 1)
	    trace_format (context, ", ");

	  if (m4_is_symbol_value_text (argv[i]))
	    {
	      trace_format (context, "%l%S%r", M4ARG (i));
	    }
	  else if (m4_is_symbol_value_func (argv[i]))
	    {
	      bp = m4_builtin_find_by_func (NULL,
					    m4_get_symbol_value_func(argv[i]));
	      if (bp == NULL)
		{
		  assert (!"INTERNAL ERROR: builtin not found in table!");
		  abort ();
		}
	      trace_format (context, "<%s>", bp->name);
	    }
	  else if (m4_is_symbol_value_placeholder (argv[i]))
	    {
	      trace_format (context, "<placeholder for %s>",
			    m4_get_symbol_value_placeholder (argv[i]));
	    }
	  else
	    {
	      assert (!"INTERNAL ERROR: bad token data type (trace_pre ())");
	      abort ();
	    }
	}
      trace_format (context, ")");
    }

  if (m4_is_debug_bit (context, M4_DEBUG_TRACE_CALL))
    {
      trace_format (context, " -> ???");
      trace_flush  (context);
    }
}

/* Format the final part of a trace line and print it all.  Used from
   expand_macro ().  */
static void
trace_post (m4 *context, const char *name, int id,
	    int argc, m4_symbol_value **argv, const char *expanded)
{
  if (m4_is_debug_bit (context, M4_DEBUG_TRACE_CALL))
    {
      trace_header (context, id);
      trace_format (context, "%s%s", name, (argc > 1) ? "(...)" : "");
    }

  if (expanded && m4_is_debug_bit (context, M4_DEBUG_TRACE_EXPANSION))
    {
      trace_format (context, " -> %l%S%r", expanded);
    }

  trace_flush (context);
}

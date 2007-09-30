/* GNU m4 -- A simple macro processor
   Copyright (C) 2000, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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

#include <config.h>

/* Build using only the exported interfaces, unles NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

#include "modules/m4.h"

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	gnu_LTX_m4_builtin_table
#define m4_macro_table		gnu_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions					\
  BUILTIN (__file__,	false,	false,	false,	0,	0 )	\
  BUILTIN (__line__,	false,	false,	false,	0,	0  )	\
  BUILTIN (__program__,	false,	false,	false,	0,	0  )	\
  BUILTIN (builtin,	true,	true,	false,	1,	-1 )	\
  BUILTIN (changeresyntax,false,true,	false,	1,	1  )	\
  BUILTIN (changesyntax,false,	true,	false,	1,	-1 )	\
  BUILTIN (debugfile,	false,	false,	false,	0,	1  )	\
  BUILTIN (debuglen,	false,	true,	false,	1,	1  )	\
  BUILTIN (debugmode,	false,	false,	false,	0,	1  )	\
  BUILTIN (esyscmd,	false,	true,	true,	1,	1  )	\
  BUILTIN (format,	false,	true,	false,	1,	-1 )	\
  BUILTIN (indir,	true,	true,	false,	1,	-1 )	\
  BUILTIN (mkdtemp,	false,	true,	false,	1,	1  )	\
  BUILTIN (patsubst,	false,	true,	true,	2,	4  )	\
  BUILTIN (regexp,	false,	true,	true,	2,	4  )	\
  BUILTIN (renamesyms,	false,	true,	false,	2,	3  )	\
  BUILTIN (m4symbols,	false,	false,	false,	0,	-1 )	\
  BUILTIN (syncoutput,	false,  true,	false,	1,	1  )	\


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros, blind, side, min, max)  M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)	\
  { CONC(builtin_, handler), STR(handler),		\
    ((macros ? M4_BUILTIN_GROKS_MACRO : 0)		\
     | (blind ? M4_BUILTIN_BLIND : 0)			\
     | (side ? M4_BUILTIN_SIDE_EFFECT : 0)),		\
    min, max },
  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};


/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  /* name		text */
#if UNIX
  { "__unix__",		"" },
#elif W32_NATIVE
  { "__windows__",	"" },
#elif OS2
  { "__os2__",		"" },
#else
# warning Platform macro not provided
#endif
  { "__gnu__",		"" },
  { "__m4_version__",	VERSION/**/TIMESTAMP },

  { NULL, NULL },
};



/* The regs_allocated field in an re_pattern_buffer refers to the
   state of the re_registers struct used in successive matches with
   the same compiled pattern:  */

typedef struct {
  struct re_pattern_buffer pat;	/* compiled regular expression */
  struct re_registers regs;	/* match registers */
} m4_pattern_buffer;

static m4_pattern_buffer gnu_buf;	/* compiled regular expression */

/* Compile a REGEXP using the RESYNTAX bits, and return the buffer.
   Report errors on behalf of CALLER.  If NO_SUB, optimize the
   compilation to skip filling out the regs member of the buffer.  */

static m4_pattern_buffer *
m4_regexp_compile (m4 *context, const char *caller,
		   const char *regexp, int resyntax, bool no_sub)
{
  /* gnu_buf is guaranteed to start life 0-initialized, which works in the
     below algorithm.

     FIXME - this method is not reentrant, since re_compile_pattern
     mallocs memory, depends on the global variable re_syntax_options
     for its syntax (but at least the compiled regex remembers its
     syntax even if the global variable changes later), and since we
     use a static variable.  To be reentrant, we would need a mutex in
     this method, and move the storage for gnu_buf into context.  */

  const char *msg;		/* error message from re_compile_pattern */

  re_set_syntax (resyntax);
  regfree (&gnu_buf.pat);
  gnu_buf.pat.no_sub = no_sub;
  msg = re_compile_pattern (regexp, strlen (regexp), &gnu_buf.pat);

  if (msg != NULL)
    {
      m4_error (context, 0, 0, _("%s: bad regular expression `%s': %s"),
		caller, regexp, msg);
      return NULL;
    }

  re_set_registers (&gnu_buf.pat, &gnu_buf.regs, gnu_buf.regs.num_regs,
		    gnu_buf.regs.start, gnu_buf.regs.end);
  return &gnu_buf;
}


/* Wrap up GNU Regex re_search call to work with an m4_pattern_buffer.  */

static int
m4_regexp_search (m4_pattern_buffer *buf, const char *string,
		  const int size, const int start, const int range)
{
  return re_search (&buf->pat, string, size, start, range, &buf->regs);
}


/* Function to perform substitution by regular expressions.  Used by the
   builtins regexp, patsubst and renamesyms.  The changed text is placed on
   the obstack.  The substitution is REPL, with \& substituted by this part
   of VICTIM matched by the last whole regular expression, and \N
   substituted by the text matched by the Nth parenthesized sub-expression.  */

static void
substitute (m4 *context, m4_obstack *obs, const char *caller,
	    const char *victim, const char *repl, m4_pattern_buffer *buf)
{
  int ch;

  for (;;)
    {
      while ((ch = *repl++) != '\\')
	{
	  if (ch == '\0')
	    return;
	  obstack_1grow (obs, ch);
	}

      switch ((ch = *repl++))
	{
	case '&':
	  obstack_grow (obs, victim + buf->regs.start[0],
			buf->regs.end[0] - buf->regs.start[0]);
	  break;

	case '1': case '2': case '3': case '4': case '5': case '6':
	case '7': case '8': case '9':
	  ch -= '0';
	  if (buf->pat.re_nsub < ch)
	    m4_warn (context, 0, _("%s: sub-expression %d not present"),
		     caller, ch);
	  else if (buf->regs.end[ch] > 0)
	    obstack_grow (obs, victim + buf->regs.start[ch],
			  buf->regs.end[ch] - buf->regs.start[ch]);
	  break;

	case '\0':
	  m4_warn (context, 0, _("%s: trailing \\ ignored in replacement"),
		   caller);
	  return;

	default:
	  obstack_1grow (obs, ch);
	  break;
	}
    }
}


/* For each match against compiled REGEXP (held in BUF -- as returned
   by m4_regexp_compile) in VICTIM, substitute REPLACE.  Non-matching
   characters are copied verbatim, and the result copied to the
   obstack.  Errors are reported on behalf of CALLER.  Return true if
   a substitution was made.  If IGNORE_DUPLICATES is set, don't worry
   about completing the obstack when returning false.  */

static bool
m4_regexp_substitute (m4 *context, m4_obstack *obs, const char *caller,
		      const char *victim, const char *regexp,
		      m4_pattern_buffer *buf, const char *replace,
		      bool ignore_duplicates)
{
  int matchpos	= 0;		/* start position of match */
  int offset	= 0;		/* current match offset */
  int length	= strlen (victim);
  bool subst	= false;	/* if a substitution has been made */

  while (offset <= length)
    {
      matchpos = m4_regexp_search (buf, victim, length,
				   offset, length - offset);

      if (matchpos < 0)
	{

	  /* Match failed -- either error or there is no match in the
	     rest of the string, in which case the rest of the string is
	     copied verbatim.  */

	  if (matchpos == -2)
	    m4_error (context, 0, 0,
		      _("%s: error matching regular expression `%s'"),
		      caller, regexp);
	  else if (!ignore_duplicates && (offset < length))
	    obstack_grow (obs, victim + offset, length - offset);
	  break;
	}

      /* Copy the part of the string that was skipped by re_search ().  */

      if (matchpos > offset)
	obstack_grow (obs, victim + offset, matchpos - offset);

      /* Handle the part of the string that was covered by the match.  */

      substitute (context, obs, caller, victim, replace, buf);
      subst = true;

      /* Update the offset to the end of the match.  If the regexp
	 matched a null string, advance offset one more, to avoid
	 infinite loops.  */

      offset = buf->regs.end[0];
      if (buf->regs.start[0] == buf->regs.end[0])
	{
	  obstack_1grow (obs, victim[offset]);
	  offset++;
	}
    }

  if (!ignore_duplicates || subst)
    obstack_1grow (obs, '\0');

  return subst;
}


/* Reclaim memory used by this module.  */
M4FINISH_HANDLER(gnu)
{
  regfree (&gnu_buf.pat);
  free (gnu_buf.regs.start);
  free (gnu_buf.regs.end);
  /* If this module was preloaded, then we need to explicitly reset
     the memory in case it gets reloaded.  */
  memset (&gnu_buf, 0, sizeof gnu_buf);
}



/**
 * __file__
 **/
M4BUILTIN_HANDLER (__file__)
{
  m4_shipout_string (context, obs, m4_get_current_file (context), 0, true);
}


/**
 * __line__
 **/
M4BUILTIN_HANDLER (__line__)
{
  m4_shipout_int (obs, m4_get_current_line (context));
}


/**
 * __program__
 **/
M4BUILTIN_HANDLER (__program__)
{
  m4_shipout_string (context, obs, m4_get_program_name (), 0, true);
}


/* The builtin "builtin" allows calls to builtin macros, even if their
   definition has been overridden or shadowed.  It is thus possible to
   redefine builtins, and still access their original definition.  A
   special form allows one to retrieve the special token that defn
   would normally return, even if that builtin is not currently
   defined and hence can't be passed to defn.  */

/**
 * builtin(MACRO, [...])
 * builtin(defn(`builtin'), MACRO)
 **/
M4BUILTIN_HANDLER (builtin)
{
  const char *name;
  m4_symbol_value *value;

  if (! m4_is_symbol_value_text (argv[1]))
    {
      if (m4_is_symbol_value_func (argv[1])
	 && m4_get_symbol_value_func (argv[1]) == builtin_builtin)
       {
	 if (m4_bad_argc (context, argc, argv, 2, 2, false))
	   return;
	 if (! m4_is_symbol_value_text (argv[2]))
	   {
	     m4_warn (context, 0, _("%s: invalid macro name ignored"),
		      M4ARG (0));
	     return;
	   }
	 name = M4ARG (2);
	 value = m4_builtin_find_by_name (NULL, name);
	 if (value == NULL)
	   m4_warn (context, 0, _("%s: undefined builtin `%s'"), M4ARG (0),
		    name);
	 else
	   {
	     m4_push_builtin (context, value);
	     free (value);
	   }
       }
      else
       m4_warn (context, 0, _("%s: invalid macro name ignored"), M4ARG (0));
    }
  else
    {
      name = M4ARG (1);
      value = m4_builtin_find_by_name (NULL, name);
      if (value == NULL)
	m4_warn (context, 0, _("%s: undefined builtin `%s'"), M4ARG (0), name);
      else
	{
	  const m4_builtin *bp = m4_get_symbol_value_builtin (value);
	  if (!m4_bad_argc (context, argc - 1, argv + 1,
			    bp->min_args, bp->max_args,
			    (bp->flags & M4_BUILTIN_SIDE_EFFECT) != 0))
	    {
	      int i;
	      if ((bp->flags & M4_BUILTIN_GROKS_MACRO) == 0)
		for (i = 2; i < argc; i++)
		  if (! m4_is_symbol_value_text (argv[i]))
		    m4_set_symbol_value_text (argv[i], "");
	      bp->func (context, obs, argc - 1, argv + 1);
	    }
	  free (value);
	}
    }
}


/* Change the current regexp syntax.  Currently this affects the
   builtins: `patsubst', `regexp' and `renamesyms'.  */

static int
m4_resyntax_encode_safe (m4 *context, const char *caller, const char *spec)
{
  int resyntax = m4_regexp_syntax_encode (spec);

  if (resyntax < 0)
    m4_warn (context, 0, _("%s: bad syntax-spec: `%s'"), caller, spec);

  return resyntax;
}


/**
 * changeresyntax(RESYNTAX-SPEC)
 **/
M4BUILTIN_HANDLER (changeresyntax)
{
  int resyntax = m4_resyntax_encode_safe (context, M4ARG (0), M4ARG (1));

  if (resyntax >= 0)
    m4_set_regexp_syntax_opt (context, resyntax);
}


/* Change the current input syntax.  The function m4_set_syntax ()
   lives in syntax.c.  Any changes to comment delimiters and quotes
   made here will be overridden by a call to `changecom' or
   `changequote'.  */

/**
 * changesyntax(SYNTAX-SPEC, ...)
 **/
M4BUILTIN_HANDLER (changesyntax)
{
  M4_MODULE_IMPORT (m4, m4_expand_ranges);

  if (m4_expand_ranges)
    {
      int i;
      for (i = 1; i < argc; i++)
	{
	  const char *spec = M4ARG (i);
	  char key = *spec++;
	  char action = key ? *spec : '\0';
	  switch (action)
	    {
	    case '-':
	    case '+':
	    case '=':
	      spec++;
	      break;
	    case '\0':
	      break;
	    default:
	      action = '=';
	      break;
	    }
	  if (m4_set_syntax (M4SYNTAX, key, action,
			     key ? m4_expand_ranges (spec, obs) : "") < 0)
	    m4_warn (context, 0, _("%s: undefined syntax code: `%c'"),
		     M4ARG (0), key);
	}
    }
  else
    assert (!"Unable to import from m4 module");
}


/* Specify the destination of the debugging output.  With one argument, the
   argument is taken as a file name, with no arguments, revert to stderr.  */

/**
 * debugfile([FILENAME])
 **/
M4BUILTIN_HANDLER (debugfile)
{
  if (argc == 1)
    m4_debug_set_output (context, NULL);
  else if (m4_get_safer_opt (context) && *M4ARG (1))
    m4_error (context, 0, 0, _("%s: disabled by --safer"), M4ARG (0));
  else if (!m4_debug_set_output (context, M4ARG (1)))
    m4_error (context, 0, errno, _("%s: cannot set debug file `%s'"),
	      M4ARG (0), M4ARG (1));
}


/* On-the-fly control of debug length.  It takes one integer
   argument.  */

/**
 * debuglen(LEN)
 **/
M4BUILTIN_HANDLER (debuglen)
{
  int i;
  if (!m4_numeric_arg (context, argc, argv, 1, &i))
    return;
  /* FIXME - make m4_numeric_arg more powerful - we want to accept
     suffixes, and limit the result to size_t.  */
  m4_set_max_debug_arg_length_opt (context, i);
}

/* On-the-fly control of the format of the tracing output.  It takes one
   argument, which is a character string like given to the -d option, or
   none in which case the debug_level is zeroed.  */

/**
 * debugmode([FLAGS])
 **/
M4BUILTIN_HANDLER (debugmode)
{
  int debug_level = m4_get_debug_level_opt (context);
  int new_debug_level;

  if (argc == 1)
    m4_set_debug_level_opt (context, 0);
  else
    {
      new_debug_level = m4_debug_decode (context, debug_level, M4ARG (1));
      if (new_debug_level < 0)
	m4_error (context, 0, 0, _("%s: bad debug flags: `%s'"),
		  M4ARG (0), M4ARG (1));
      else
	m4_set_debug_level_opt (context, new_debug_level);
    }
}


/* Same as the sysymd builtin from m4.c module, but expand to the
   output of SHELL-COMMAND. */

/**
 * esyscmd(SHELL-COMMAND)
 **/

M4BUILTIN_HANDLER (esyscmd)
{
  M4_MODULE_IMPORT (m4, m4_set_sysval);
  M4_MODULE_IMPORT (m4, m4_sysval_flush);

  if (m4_set_sysval && m4_sysval_flush)
    {
      FILE *pin;
      int ch;

      if (m4_get_safer_opt (context))
	{
	  m4_error (context, 0, 0, _("%s: disabled by --safer"), M4ARG (0));
	  return;
	}

      /* Optimize the empty command.  */
      if (*M4ARG (1) == '\0')
	{
	  m4_set_sysval (0);
	  return;
	}

      m4_sysval_flush (context, false);
      errno = 0;
      pin = popen (M4ARG (1), "r");
      if (pin == NULL)
	{
	  m4_error (context, 0, errno,
		    _("%s: cannot open pipe to command `%s'"),
		    M4ARG (0), M4ARG (1));
	  m4_set_sysval (-1);
	}
      else
	{
	  while ((ch = getc (pin)) != EOF)
	    obstack_1grow (obs, (char) ch);
	  m4_set_sysval (pclose (pin));
	}
    }
  else
    assert (!"Unable to import from m4 module");
}


/* Frontend for printf like formatting.  The function format () lives in
   the file format.c.  */

#include "format.c"

/**
 * format(FORMAT-STRING, [...])
 **/
M4BUILTIN_HANDLER (format)
{
  format (context, obs, argc, argv);
}


/* The builtin "indir" allows indirect calls to macros, even if their name
   is not a proper macro name.  It is thus possible to define macros with
   ill-formed names for internal use in larger macro packages.  */

/**
 * indir(MACRO, [...])
 **/
M4BUILTIN_HANDLER (indir)
{
  if (! m4_is_symbol_value_text (argv[1]))
    m4_warn (context, 0, _("%s: invalid macro name ignored"), M4ARG (0));
  else
    {
      const char *name = M4ARG (1);
      m4_symbol *symbol = m4_symbol_lookup (M4SYMTAB, name);

      if (symbol == NULL)
	m4_warn (context, 0, _("%s: undefined macro `%s'"), M4ARG (0), name);
      else
	{
	  int i;
	  if (! m4_symbol_groks_macro (symbol))
	    for (i = 2; i < argc; i++)
	      if (! m4_is_symbol_value_text (argv[i]))
		m4_set_symbol_value_text (argv[i], "");
	  m4_macro_call (context, m4_get_symbol_value (symbol), obs,
			 argc - 1, argv + 1);
	}
    }
}


/* The builtin "mkdtemp" allows creation of temporary directories.  */

/**
 * mkdtemp(TEMPLATE)
 **/
M4BUILTIN_HANDLER (mkdtemp)
{
  M4_MODULE_IMPORT (m4, m4_make_temp);

  if (m4_make_temp)
    m4_make_temp (context, obs, M4ARG (0), M4ARG (1), true);
  else
    assert (!"Unable to import from m4 module");
}


/* Substitute all matches of a regexp occuring in a string.  Each match of
   the second argument (a regexp) in the first argument is changed to the
   third argument, with \& substituted by the matched text, and \N
   substituted by the text matched by the Nth parenthesized sub-expression.  */

/**
 * patsubst(VICTIM, REGEXP, [REPLACEMENT], [RESYNTAX])
 **/
M4BUILTIN_HANDLER (patsubst)
{
  const char *me;		/* name of this macro */
  const char *pattern;		/* regular expression */
  const char *replace;		/* replacement */
  m4_pattern_buffer *buf;	/* compiled regular expression */
  int resyntax;

  me = M4ARG (0);
  pattern = M4ARG (2);
  replace = M4ARG (3);

  resyntax = m4_get_regexp_syntax_opt (context);
  if (argc >= 5)		/* additional args ignored */
    {
      resyntax = m4_resyntax_encode_safe (context, me, M4ARG (4));
      if (resyntax < 0)
	return;
    }

  /* The empty regex matches everywhere, but if there is no
     replacement, we need not waste time with it.  */
  if (!*pattern && !*replace)
    {
      const char *str = M4ARG (1);
      obstack_grow (obs, str, strlen (str));
      return;
    }

  buf = m4_regexp_compile (context, me, pattern, resyntax, false);
  if (!buf)
    return;

  m4_regexp_substitute (context, obs, me, M4ARG (1), pattern, buf,
			replace, false);
}


/* Regular expression version of index.  Given two arguments, expand to the
   index of the first match of the second argument (a regexp) in the first.
   Expand to -1 if here is no match.  Given a third argument, it changes
   the expansion to this argument.  */

/**
 * regexp(VICTIM, REGEXP, RESYNTAX)
 * regexp(VICTIM, REGEXP, [REPLACEMENT], [RESYNTAX])
 **/
M4BUILTIN_HANDLER (regexp)
{
  const char *me;		/* name of this macro */
  const char *pattern;		/* regular expression */
  const char *replace;		/* optional replacement string */
  m4_pattern_buffer *buf;	/* compiled regular expression */
  int startpos;			/* start position of match */
  int length;			/* length of first argument */
  int resyntax;

  me = M4ARG (0);
  pattern = M4ARG (2);
  replace = M4ARG (3);
  resyntax = m4_get_regexp_syntax_opt (context);

  if (argc == 4)
    {
      resyntax = m4_regexp_syntax_encode (replace);

      /* The first case is the most difficult, because the empty string
	 is a valid RESYNTAX, yet we want `regexp(aab, a*, )' to return
	 an empty string as per M4 1.4.x!  */

      if ((*replace == '\0') || (resyntax < 0))
	/* regexp(VICTIM, REGEXP, REPLACEMENT) */
	resyntax = m4_get_regexp_syntax_opt (context);
      else
	/* regexp(VICTIM, REGEXP, RESYNTAX) */
	replace = NULL;
    }
  else if (argc >= 5)
    {
      /* regexp(VICTIM, REGEXP, REPLACEMENT, RESYNTAX) */
      resyntax = m4_resyntax_encode_safe (context, me, M4ARG (4));
      if (resyntax < 0)
	return;
    }
  else
    /* regexp(VICTIM, REGEXP)  */
    replace = NULL;

  if (!*pattern)
    {
      /* The empty regex matches everything.  */
      if (replace)
	obstack_grow (obs, replace, strlen (replace));
      else
	m4_shipout_int (obs, 0);
      return;
    }

  buf = m4_regexp_compile (context, me, pattern, resyntax, replace == NULL);
  if (!buf)
    return;

  length = strlen (M4ARG (1));
  startpos = m4_regexp_search (buf, M4ARG (1), length, 0, length);

  if (startpos == -2)
    {
      m4_error (context, 0, 0, _("%s: error matching regular expression `%s'"),
		me, pattern);
      return;
    }

  if (replace == NULL)
    m4_shipout_int (obs, startpos);
  else if (startpos >= 0)
    substitute (context, obs, me, M4ARG (1), replace, buf);
}


/* Rename all current symbols that match REGEXP according to the
   REPLACEMENT specification.  */

/**
 * renamesyms(REGEXP, REPLACEMENT, [RESYNTAX])
 **/
M4BUILTIN_HANDLER (renamesyms)
{
  M4_MODULE_IMPORT (m4, m4_dump_symbols);

  if (m4_dump_symbols)
    {
      const char *me;		/* name of this macro */
      const char *regexp;	/* regular expression string */
      const char *replace;	/* replacement expression string */

      m4_pattern_buffer *buf;	/* compiled regular expression */

      m4_dump_symbol_data	data;
      m4_obstack		rename_obs;

      int resyntax;

      me      = M4ARG (0);
      regexp  = M4ARG (1);
      replace = M4ARG (2);

      resyntax = m4_get_regexp_syntax_opt (context);
      if (argc >= 4)
	{
	  resyntax = m4_resyntax_encode_safe (context, me, M4ARG (3));
	  if (resyntax < 0)
	    return;
	}

      buf = m4_regexp_compile (context, me, regexp, resyntax, false);
      if (!buf)
	return;

      obstack_init (&rename_obs);
      data.obs = obs;

      m4_dump_symbols (context, &data, 1, argv, false);

      for (; data.size > 0; --data.size, data.base++)
	{
	  const char *name = data.base[0];

	  if (m4_regexp_substitute (context, &rename_obs, me, name, regexp,
				    buf, replace, true))
	    {
	      const char *renamed = obstack_finish (&rename_obs);

	      m4_symbol_rename (M4SYMTAB, name, renamed);
	    }
	}

      obstack_free (&rename_obs, NULL);
    }
  else
    assert (!"Unable to import from m4 module");
}


/* Implementation of "m4symbols".  It builds up a table of pointers to
   symbols, sorts it and ships out the symbol names.  */

/**
 * m4symbols([...])
 **/
M4BUILTIN_HANDLER (m4symbols)
{
  M4_MODULE_IMPORT (m4, m4_dump_symbols);

  if (m4_dump_symbols)
    {
      m4_dump_symbol_data data;
      m4_obstack data_obs;

      obstack_init (&data_obs);
      data.obs = &data_obs;
      m4_dump_symbols (context, &data, argc, argv, false);

      for (; data.size > 0; --data.size, data.base++)
	{
	  m4_shipout_string (context, obs, data.base[0], 0, true);
	  if (data.size > 1)
	    obstack_1grow (obs, ',');
	}
      obstack_free (&data_obs, NULL);
    }
  else
    assert (!"Unable to import from m4 module");
}


/* This contains macro which implements syncoutput() which takes one arg
     1, on, yes - turn on sync lines
     0, off, no, blank - turn off sync lines
     everything else is silently ignored  */

/**
 * syncoutput(TRUTH)
 **/
M4BUILTIN_HANDLER (syncoutput)
{
  bool value = m4_get_syncoutput_opt (context);
  value = m4_parse_truth_arg (context, M4ARG (1), M4ARG (0), value);
  m4_set_syncoutput_opt (context, value);
}

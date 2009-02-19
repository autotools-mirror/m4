/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2000, 2004, 2006,
   2007, 2008, 2009 Free Software Foundation, Inc.

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

/* Code for all builtin macros, initialization of symbol table, and
   expansion of user defined macros.  */

#include "m4.h"

#include "memchr2.h"
#include "regex.h"

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

/* Initialization of builtin and predefined macros.  The table
   "builtin_tab" is both used for initialization, and by the "builtin"
   builtin.  */

#define DECLARE(name) \
  static void name (struct obstack *, int, macro_arguments *)

DECLARE (m4___file__);
DECLARE (m4___line__);
DECLARE (m4___program__);
DECLARE (m4_builtin);
DECLARE (m4_changecom);
DECLARE (m4_changequote);
#ifdef ENABLE_CHANGEWORD
DECLARE (m4_changeword);
#endif
DECLARE (m4_debugmode);
DECLARE (m4_debugfile);
DECLARE (m4_decr);
DECLARE (m4_define);
DECLARE (m4_defn);
DECLARE (m4_divert);
DECLARE (m4_divnum);
DECLARE (m4_dnl);
DECLARE (m4_dumpdef);
DECLARE (m4_errprint);
DECLARE (m4_esyscmd);
DECLARE (m4_eval);
DECLARE (m4_format);
DECLARE (m4_ifdef);
DECLARE (m4_ifelse);
DECLARE (m4_include);
DECLARE (m4_incr);
DECLARE (m4_index);
DECLARE (m4_indir);
DECLARE (m4_len);
DECLARE (m4_m4exit);
DECLARE (m4_m4wrap);
DECLARE (m4_maketemp);
DECLARE (m4_mkstemp);
DECLARE (m4_patsubst);
DECLARE (m4_popdef);
DECLARE (m4_pushdef);
DECLARE (m4_regexp);
DECLARE (m4_shift);
DECLARE (m4_sinclude);
DECLARE (m4_substr);
DECLARE (m4_syscmd);
DECLARE (m4_sysval);
DECLARE (m4_traceoff);
DECLARE (m4_traceon);
DECLARE (m4_translit);
DECLARE (m4_undefine);
DECLARE (m4_undivert);

#undef DECLARE

static builtin const builtin_tab[] =
{

  /* name		GNUext	macros	blind	function */

  { "__file__",		true,	false,	false,	m4___file__ },
  { "__line__",		true,	false,	false,	m4___line__ },
  { "__program__",	true,	false,	false,	m4___program__ },
  { "builtin",		true,	true,	true,	m4_builtin },
  { "changecom",	false,	false,	false,	m4_changecom },
  { "changequote",	false,	false,	false,	m4_changequote },
#ifdef ENABLE_CHANGEWORD
  { "changeword",	true,	false,	true,	m4_changeword },
#endif
  { "debugmode",	true,	false,	false,	m4_debugmode },
  { "debugfile",	true,	false,	false,	m4_debugfile },
  { "decr",		false,	false,	true,	m4_decr },
  { "define",		false,	true,	true,	m4_define },
  { "defn",		false,	true,	true,	m4_defn },
  { "divert",		false,	false,	false,	m4_divert },
  { "divnum",		false,	false,	false,	m4_divnum },
  { "dnl",		false,	false,	false,	m4_dnl },
  { "dumpdef",		false,	true,	false,	m4_dumpdef },
  { "errprint",		false,	false,	true,	m4_errprint },
  { "esyscmd",		true,	false,	true,	m4_esyscmd },
  { "eval",		false,	false,	true,	m4_eval },
  { "format",		true,	false,	true,	m4_format },
  { "ifdef",		false,	true,	true,	m4_ifdef },
  { "ifelse",		false,	true,	true,	m4_ifelse },
  { "include",		false,	false,	true,	m4_include },
  { "incr",		false,	false,	true,	m4_incr },
  { "index",		false,	false,	true,	m4_index },
  { "indir",		true,	true,	true,	m4_indir },
  { "len",		false,	false,	true,	m4_len },
  { "m4exit",		false,	false,	false,	m4_m4exit },
  { "m4wrap",		false,	true,	true,	m4_m4wrap },
  { "maketemp",		false,	false,	true,	m4_maketemp },
  { "mkstemp",		false,	false,	true,	m4_mkstemp },
  { "patsubst",		true,	false,	true,	m4_patsubst },
  { "popdef",		false,	true,	true,	m4_popdef },
  { "pushdef",		false,	true,	true,	m4_pushdef },
  { "regexp",		true,	false,	true,	m4_regexp },
  { "shift",		false,	true,	true,	m4_shift },
  { "sinclude",		false,	false,	true,	m4_sinclude },
  { "substr",		false,	false,	true,	m4_substr },
  { "syscmd",		false,	false,	true,	m4_syscmd },
  { "sysval",		false,	false,	false,	m4_sysval },
  { "traceoff",		false,	true,	false,	m4_traceoff },
  { "traceon",		false,	true,	false,	m4_traceon },
  { "translit",		false,	false,	true,	m4_translit },
  { "undefine",		false,	true,	true,	m4_undefine },
  { "undivert",		false,	false,	false,	m4_undivert },

  { 0,			false,	false,	false,	0 },

  /* placeholder is intentionally stuck after the table end delimiter,
     so that we can easily find it, while not treating it as a real
     builtin.  */
  { "placeholder",	true,	false,	false,	m4_placeholder },
};

static predefined const predefined_tab[] =
{
#if UNIX
  { "unix",	"__unix__",	"" },
#endif
#if W32_NATIVE
  { "windows",	"__windows__",	"" },
#endif
#if OS2
  { "os2",	"__os2__",	"" },
#endif
#if !UNIX && !W32_NATIVE && !OS2
# warning Platform macro not provided
#endif
  { NULL,	"__gnu__",	"" },
  { NULL,	"__m4_version__", VERSION },

  { NULL,	NULL,		NULL },
};

/*----------------------------------------.
| Find the builtin, which lives on ADDR.  |
`----------------------------------------*/

const builtin *
find_builtin_by_addr (builtin_func *func)
{
  const builtin *bp;

  for (bp = &builtin_tab[0]; bp->name != NULL; bp++)
    if (bp->func == func)
      return bp;
  if (func == m4_placeholder)
    return bp + 1;
  return NULL;
}

/*----------------------------------------------------------.
| Find the builtin, which has NAME.  On failure, return the |
| placeholder builtin.                                      |
`----------------------------------------------------------*/

const builtin *
find_builtin_by_name (const char *name)
{
  const builtin *bp;

  for (bp = &builtin_tab[0]; bp->name != NULL; bp++)
    if (strcmp (bp->name, name) == 0)
      return bp;
  return bp + 1;
}

/*------------------------------------------------------------------.
| Print a representation of FUNC to OBS.  If FLATTEN, output QUOTES |
| around an empty string instead; else if CHAIN, append the builtin |
| to the chain; otherwise print the name of FUNC.                   |
`------------------------------------------------------------------*/
void
func_print (struct obstack *obs, const builtin *func, bool flatten,
	    token_chain **chain, const string_pair *quotes)
{
  assert (func);
  if (flatten)
    {
      if (quotes)
	{
	  obstack_grow (obs, quotes->str1, quotes->len1);
	  obstack_grow (obs, quotes->str2, quotes->len2);
	}
    }
  else if (chain)
    append_macro (obs, func->func, NULL, chain);
  else
    {
      obstack_1grow (obs, '<');
      obstack_grow (obs, func->name, strlen (func->name));
      obstack_1grow (obs, '>');
    }
}

/*----------------------------------------------------------------.
| Install a builtin macro with name NAME and length LEN, bound to |
| the C function given in BP.  MODE is SYMBOL_INSERT or		  |
| SYMBOL_PUSHDEF.						  |
`----------------------------------------------------------------*/

void
define_builtin (const char *name, size_t len, const builtin *bp,
		symbol_lookup mode)
{
  symbol *sym;

  sym = lookup_symbol (name, len, mode);
  SYMBOL_TYPE (sym) = TOKEN_FUNC;
  SYMBOL_MACRO_ARGS (sym) = bp->groks_macro_args;
  SYMBOL_BLIND_NO_ARGS (sym) = bp->blind_if_no_args;
  SYMBOL_FUNC (sym) = bp->func;
}

/* Storage for the compiled regular expression of
   --warn-macro-sequence.  */
static struct re_pattern_buffer macro_sequence_buf;

/* Storage for the matches of --warn-macro-sequence.  */
static struct re_registers macro_sequence_regs;

/* True if --warn-macro-sequence is in effect.  */
static bool macro_sequence_inuse;

/* Maybe this is worth making runtime tunable.  Too small, and nothing
   gets cached because the working set of active regex is larger than
   the cache, and we are always swapping out entries.  Too large, and
   the time spent searching the cache for a match overtakes the time
   saved by caching.  For now, this size proved reasonable for the
   typical working set of Autoconf 2.62.  */
#define REGEX_CACHE_SIZE 16

/* Structure for caching compiled regex.  */
struct m4_regex {
  unsigned count;			/* usage counter */
  size_t len;				/* length of string */
  char *str;				/* copy of compiled string */
  struct re_pattern_buffer *buf;	/* compiled regex, allocated */
  struct re_registers regs;		/* match registers, reused */
};
typedef struct m4_regex m4_regex;

/* Storage for the cache of regular expressions.  */
static m4_regex regex_cache[REGEX_CACHE_SIZE];

#ifdef DEBUG_REGEX
extern FILE *trace_file;
#endif /* DEBUG_REGEX */

/*------------------------------------------------------------------.
| Compile STR, with length LEN, into a regex.  On success, set BUF  |
| and REGS to the compiled regex.  Compilation is cached, so do not |
| free the results here; rather, use free_regex at the end of the   |
| program.  Return NULL on success, or an error message.	    |
`------------------------------------------------------------------*/
static const char *
compile_pattern (const char *str, size_t len, struct re_pattern_buffer **buf,
		 struct re_registers **regs)
{
  int i;
  m4_regex *victim;
  unsigned victim_count;
  struct re_pattern_buffer *new_buf;
  struct re_registers *new_regs;
  const char *msg;

  /* First, check if STR is already cached.  If so, increase its use
     count and return it.  */
  for (i = 0; i < REGEX_CACHE_SIZE; i++)
    if (len == regex_cache[i].len && regex_cache[i].str
	&& memcmp (str, regex_cache[i].str, len) == 0)
      {
	*buf = regex_cache[i].buf;
	*regs = &regex_cache[i].regs;
	regex_cache[i].count++;
#ifdef DEBUG_REGEX
	if (trace_file)
	  {
	    fputs ("cached:{", trace_file);
	    fwrite (str, 1, len, trace_file);
	    fputs ("}\n", trace_file);
	  }
#endif /* DEBUG_REGEX */
	return NULL;
      }

  /* Next, check if STR can be compiled.  */
  new_buf = (struct re_pattern_buffer *) xzalloc (sizeof *new_buf);
  msg = re_compile_pattern (str, len, new_buf);
#ifdef DEBUG_REGEX
  if (trace_file)
    {
      fputs ("compile:{", trace_file);
      fwrite (str, 1, len, trace_file);
      fputs ("}\n", trace_file);
    }
#endif /* DEBUG_REGEX */
  if (msg)
    {
      regfree (new_buf);
      free (new_buf);
      return msg;
    }
  /* Use a fastmap for speed; it is freed by regfree.  */
  new_buf->fastmap = xcharalloc (UCHAR_MAX + 1);

  /* Now, find a victim slot.  Decrease the count of all entries, then
     prime the count of the victim slot at REGEX_CACHE_SIZE.  This
     way, frequently used entries and newly created entries are least
     likely to be victims next time we have a cache miss.  */
  victim = regex_cache;
  victim_count = victim->count;
  if (victim_count)
    victim->count--;
  for (i = 1; i < REGEX_CACHE_SIZE; i++)
    {
      if (regex_cache[i].count < victim_count)
	{
	  victim_count = regex_cache[i].count;
	  victim = &regex_cache[i];
	}
      if (regex_cache[i].count)
	regex_cache[i].count--;
    }
  victim->count = REGEX_CACHE_SIZE;
  victim->len = len;
  if (victim->str)
    {
#ifdef DEBUG_REGEX
      if (trace_file)
	{
	  fputs ("flush:{", trace_file);
	  fwrite (victim->str, 1, victim->len, trace_file);
	  fputs ("}\n", trace_file);
	}
#endif /* DEBUG_REGEX */
      free (victim->str);
      regfree (victim->buf);
      free (victim->buf);
    }
  victim->str = xstrdup (str);
  victim->buf = new_buf;
  new_regs = &victim->regs;
  re_set_registers (new_buf, new_regs, new_regs->num_regs,
		    new_regs->start, new_regs->end);
  *buf = new_buf;
  *regs = new_regs;
  return NULL;
}

/*----------------------------------------.
| Clean up regular expression variables.  |
`----------------------------------------*/

static void
free_pattern_buffer (struct re_pattern_buffer *buf, struct re_registers *regs)
{
  regfree (buf);
  free (regs->start);
  free (regs->end);
}

/*-----------------------------------------------------------------.
| Set the regular expression of --warn-macro-sequence that will be |
| checked during define and pushdef.  Exit on failure.             |
`-----------------------------------------------------------------*/
void
set_macro_sequence (const char *regexp)
{
  const char *msg;

  if (!regexp)
    regexp = DEFAULT_MACRO_SEQUENCE;
  else if (regexp[0] == '\0')
    {
      macro_sequence_inuse = false;
      return;
    }

  msg = re_compile_pattern (regexp, strlen (regexp), &macro_sequence_buf);
  if (msg != NULL)
    m4_error (EXIT_FAILURE, 0, NULL,
	      _("--warn-macro-sequence: bad regular expression %s: %s"),
	      quotearg_style (locale_quoting_style, regexp), msg);
  re_set_registers (&macro_sequence_buf, &macro_sequence_regs,
		    macro_sequence_regs.num_regs,
		    macro_sequence_regs.start, macro_sequence_regs.end);
  macro_sequence_inuse = true;
}

/*------------------------------------------------------.
| Free dynamic memory utilized by regular expressions.  |
`------------------------------------------------------*/
void
free_regex (void)
{
  int i;
  free_pattern_buffer (&macro_sequence_buf, &macro_sequence_regs);
  for (i = 0; i < REGEX_CACHE_SIZE; i++)
    if (regex_cache[i].str)
      {
	free (regex_cache[i].str);
	free_pattern_buffer (regex_cache[i].buf, &regex_cache[i].regs);
	free (regex_cache[i].buf);
      }
}

/*------------------------------------------------------------------.
| Define a predefined or user-defined macro, with name NAME of      |
| length NAME_LEN, and expansion TEXT of length LEN.  LEN may be    |
| SIZE_MAX, to use the string length of TEXT instead.  MODE is      |
| SYMBOL_INSERT for "define" or SYMBOL_PUSHDEF for "pushdef".  This |
| function is also used from main ().                               |
`------------------------------------------------------------------*/

void
define_user_macro (const char *name, size_t name_len, const char *text,
		   size_t len, symbol_lookup mode)
{
  symbol *s;
  char *defn;

  assert (text);
  if (len == SIZE_MAX)
    len = strlen (text);
  defn = xmemdup (text, len);
  s = lookup_symbol (name, name_len, mode);
  if (SYMBOL_TYPE (s) == TOKEN_TEXT)
    free (SYMBOL_TEXT (s));

  SYMBOL_TYPE (s) = TOKEN_TEXT;
  SYMBOL_TEXT (s) = defn;
  SYMBOL_TEXT_LEN (s) = len;
  SYMBOL_MACRO_ARGS (s) = true;

  /* Implement --warn-macro-sequence.  */
  if (macro_sequence_inuse && text)
    {
      regoff_t offset = 0;
      struct re_registers *regs = &macro_sequence_regs;

      while (offset < len
	     && (offset = re_search (&macro_sequence_buf, defn, len, offset,
				     len - offset, regs)) >= 0)
	{
	  /* Skip empty matches.  */
	  if (regs->start[0] == regs->end[0])
	    offset++;
	  else
	    {
	      offset = regs->end[0];
	      /* Safe to use slot 1 since we don't pass a macro name
		 to m4_warn.  */
	      m4_warn (0, NULL, _("definition of %s contains sequence %s"),
		       quotearg_style_mem (locale_quoting_style, name,
					   name_len),
		       quotearg_n_style_mem (1, locale_quoting_style,
					     defn + regs->start[0],
					     regs->end[0] - regs->start[0]));
	    }
	}
      if (offset == -2)
	m4_warn (0, NULL,
		 _("problem checking --warn-macro-sequence for macro %s"),
		 quotearg_style_mem (locale_quoting_style, name, name_len));
    }
}

/*-----------------------------------------------.
| Initialize all builtin and predefined macros.	 |
`-----------------------------------------------*/

void
builtin_init (void)
{
  const builtin *bp;
  const predefined *pp;
  char *string;

  for (bp = &builtin_tab[0]; bp->name != NULL; bp++)
    if (!no_gnu_extensions || !bp->gnu_extension)
      {
	size_t len = strlen (bp->name);
	if (prefix_all_builtins)
	  {
	    string = xasprintf ("m4_%s", bp->name);
	    define_builtin (string, len + 3, bp, SYMBOL_INSERT);
	    free (string);
	  }
	else
	  define_builtin (bp->name, len, bp, SYMBOL_INSERT);
      }

  for (pp = &predefined_tab[0]; pp->func != NULL; pp++)
    if (no_gnu_extensions)
      {
	if (pp->unix_name != NULL)
	  define_user_macro (pp->unix_name, strlen (pp->unix_name),
			     pp->func, SIZE_MAX, SYMBOL_INSERT);
      }
    else
      {
	if (pp->gnu_name != NULL)
	  define_user_macro (pp->gnu_name, strlen (pp->gnu_name),
			     pp->func, SIZE_MAX, SYMBOL_INSERT);
      }
}

/*------------------------------------------------------------------.
| Give friendly warnings if a builtin macro is passed an            |
| inappropriate number of arguments.  NAME is macro name for        |
| messages.  ARGC is one more than the number of arguments.  MIN is |
| the 0-based minimum number of acceptable arguments.  MAX is the   |
| 0-based maximum number of arguments, UINT_MAX if not applicable.  |
| Return true if there are not enough arguments.                    |
`------------------------------------------------------------------*/

bool
bad_argc (const call_info *name, int argc, unsigned int min, unsigned int max)
{
  if (argc - 1 < min)
    {
      m4_warn (0, name, _("too few arguments: %d < %d"), argc - 1, min);
      return true;
    }
  if (argc - 1 > max)
    m4_warn (0, name, _("extra arguments ignored: %d > %d"), argc - 1, max);
  return false;
}

/*------------------------------------------------------------------.
| The function numeric_arg () converts ARG of length LEN to an int  |
| pointed to by VALUEP.  If the conversion fails, print error       |
| message on behalf of NAME.  Return true iff conversion succeeds.  |
`------------------------------------------------------------------*/

static bool
numeric_arg (const call_info *name, const char *arg, size_t len, int *valuep)
{
  char *endp;

  if (!len)
    {
      *valuep = 0;
      m4_warn (0, name, _("empty string treated as 0"));
    }
  else
    {
      errno = 0;
      *valuep = strtol (arg, &endp, 10);
      if (endp - arg != len)
	{
	  m4_warn (0, name, _("non-numeric argument %s"),
		   quotearg_style_mem (locale_quoting_style, arg, len));
	  return false;
	}
      if (isspace (to_uchar (*arg)))
	m4_warn (0, name, _("leading whitespace ignored"));
      else if (errno == ERANGE)
	m4_warn (0, name, _("numeric overflow detected"));
    }
  return true;
}

/*------------------------------------------------------------------------.
| The function ntoa () converts VALUE to a signed ASCII representation in |
| radix RADIX.								  |
`------------------------------------------------------------------------*/

/* Digits for number to ASCII conversions.  */
static char const digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static const char *
ntoa (int32_t value, int radix)
{
  bool negative;
  uint32_t uvalue;
  /* Sized for radix 2, plus sign and trailing NUL.  */
  static char str[sizeof (value) * CHAR_BIT + 2];
  char *s = &str[sizeof str];

  *--s = '\0';

  if (value < 0)
    {
      negative = true;
      uvalue = -(uint32_t) value;
    }
  else
    {
      negative = false;
      uvalue = (uint32_t) value;
    }

  do
    {
      *--s = digits[uvalue % radix];
      uvalue /= radix;
    }
  while (uvalue > 0);

  if (negative)
    *--s = '-';
  return s;
}

/*----------------------------------------------------------------------.
| Format an int VAL, and stuff it into an obstack OBS.  Used for macros |
| expanding to numbers.						        |
`----------------------------------------------------------------------*/

static void
shipout_int (struct obstack *obs, int val)
{
  const char *s;

  s = ntoa ((int32_t) val, 10);
  obstack_grow (obs, s, strlen (s));
}


/* The rest of this file is code for builtins and expansion of user
   defined macros.  All the functions for builtins have a prototype as:

     void m4_MACRONAME (struct obstack *obs, int argc, macro_arguments *argv);

   The functions are expected to leave their expansion on the obstack OBS,
   as an unfinished object.  ARGV is an object representing ARGC pointers
   to the individual arguments to the macro; the object may be compressed
   due to references to $@ expansions, so accessors should be used.  Please
   note that in general argv[argc] != NULL.  */

/* The first section are macros for defining, undefining, examining,
   changing, ... other macros.  */

/*-------------------------------------------------------------------------.
| The function define_macro is common for the builtins "define",	   |
| "undefine", "pushdef" and "popdef".  ARGC and ARGV is as for the caller, |
| and MODE argument determines how the macro name is entered into the	   |
| symbol table.								   |
`-------------------------------------------------------------------------*/

static void
define_macro (int argc, macro_arguments *argv, symbol_lookup mode)
{
  const builtin *bp;
  const call_info *me = arg_info (argv);

  if (bad_argc (me, argc, 1, 2))
    return;

  if (arg_type (argv, 1) != TOKEN_TEXT)
    {
      m4_warn (0, me, _("invalid macro name ignored"));
      return;
    }

  if (argc == 2)
    {
      define_user_macro (ARG (1), ARG_LEN (1), "", 0, mode);
      return;
    }

  switch (arg_type (argv, 2))
    {
    case TOKEN_COMP:
      m4_warn (0, me, _("cannot concatenate builtins"));
      /* fallthru */
    case TOKEN_TEXT:
      define_user_macro (ARG (1), ARG_LEN (1), arg_text (argv, 2, true),
			 arg_len (argv, 2, true), mode);
      break;

    case TOKEN_FUNC:
      bp = find_builtin_by_addr (arg_func (argv, 2));
      if (bp == NULL)
	return;
      else
	define_builtin (ARG (1), ARG_LEN (1), bp, mode);
      break;

    default:
      assert (!"define_macro");
      abort ();
    }
}

static void
m4_define (struct obstack *obs, int argc, macro_arguments *argv)
{
  define_macro (argc, argv, SYMBOL_INSERT);
}

static void
m4_undefine (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int i;
  if (bad_argc (me, argc, 1, -1))
    return;
  for (i = 1; i < argc; i++)
    if (arg_type (argv, i) != TOKEN_TEXT)
      m4_warn (0, me, _("invalid macro name ignored"));
    else if (!lookup_symbol (ARG (i), ARG_LEN (i), SYMBOL_DELETE)
	     && (debug_level & DEBUG_TRACE_DEREF))
      m4_warn (0, me, _("undefined macro %s"),
	       quotearg_style_mem (locale_quoting_style, ARG (i),
				   ARG_LEN (i)));
}

static void
m4_pushdef (struct obstack *obs, int argc, macro_arguments *argv)
{
  define_macro (argc, argv, SYMBOL_PUSHDEF);
}

static void
m4_popdef (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int i;
  if (bad_argc (me, argc, 1, -1))
    return;
  for (i = 1; i < argc; i++)
    if (arg_type (argv, i) != TOKEN_TEXT)
      m4_warn (0, me, _("invalid macro name ignored"));
    else if (!lookup_symbol (ARG (i), ARG_LEN (i), SYMBOL_POPDEF)
	     && (debug_level & DEBUG_TRACE_DEREF))
      m4_warn (0, me, _("undefined macro %s"),
	       quotearg_style_mem (locale_quoting_style, ARG (i),
				   ARG_LEN (i)));
}

/*---------------------.
| Conditionals of m4.  |
`---------------------*/

static void
m4_ifdef (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  symbol *s;

  if (bad_argc (me, argc, 2, 3))
    return;
  if (arg_type (argv, 1) != TOKEN_TEXT)
    {
      m4_warn (0, me, _("invalid macro name ignored"));
      push_arg (obs, argv, 3);
      return;
    }
  s = lookup_symbol (ARG (1), ARG_LEN (1), SYMBOL_LOOKUP);
  push_arg (obs, argv, (s && SYMBOL_TYPE (s) != TOKEN_VOID) ? 2 : 3);
}

static void
m4_ifelse (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int i;

  if (argc == 2 || bad_argc (me, argc, 3, -1))
    return;
  else if (argc % 3 == 0)
    /* Diagnose excess arguments if 5, 8, 11, etc., actual arguments.  */
    bad_argc (me, argc, 0, argc - 2);

  i = 1;
  argc--;

  while (true)
    {
      if (arg_equal (argv, i, i + 1))
	{
	  push_arg (obs, argv, i + 2);
	  return;
	}
      switch (argc)
	{
	case 3:
	  return;

	case 4:
	case 5:
	  push_arg (obs, argv, i + 3);
	  return;

	default:
	  argc -= 3;
	  i += 3;
	}
    }
}

/*---------------------------------------------------------------------.
| The function dump_symbol () is for use by "dumpdef".  It builds up a |
| table of all defined, un-shadowed, symbols.			       |
`---------------------------------------------------------------------*/

/* The structure dump_symbol_data is used to pass the information needed
   from call to call to dump_symbol.  */

struct dump_symbol_data
{
  struct obstack *obs;		/* obstack for table */
  symbol **base;		/* base of table */
  int size;			/* size of table */
};

static void
dump_symbol (symbol *sym, void *arg)
{
  struct dump_symbol_data *data = (struct dump_symbol_data *) arg;
  if (SYMBOL_TYPE (sym) != TOKEN_VOID)
    {
      obstack_blank (data->obs, sizeof (symbol *));
      data->base = (symbol **) obstack_base (data->obs);
      data->base[data->size++] = sym;
    }
}

/*------------------------------------------------------------------------.
| qsort comparison routine, for sorting the table made in m4_dumpdef ().  |
`------------------------------------------------------------------------*/

static int
dumpdef_cmp (const void *s1, const void *s2)
{
  const symbol *sym1 = *(const symbol **) s1;
  const symbol *sym2 = *(const symbol **) s2;
  size_t len1 = SYMBOL_NAME_LEN (sym1);
  size_t len2 = SYMBOL_NAME_LEN (sym2);
  int result = memcmp (SYMBOL_NAME (sym1), SYMBOL_NAME (sym2),
		       len1 < len2 ? len1 : len2);
  if (!result)
    result = len1 < len2 ? -1 : len2 < len1;
  return result;
}

/*-------------------------------------------------------------------------.
| Implementation of "dumpdef" itself.  It builds up a table of pointers to |
| symbols, sorts it and prints the sorted table.			   |
`-------------------------------------------------------------------------*/

static void
m4_dumpdef (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  symbol *s;
  int i;
  struct dump_symbol_data data;
  const builtin *bp;
  FILE *output = (debug_level & DEBUG_TRACE_OUTPUT_DUMPDEF) ? stderr : debug;


  /* If there's no debug stream to dump to, skip all of this work.  */
  if (!output)
    return;

  data.obs = obs;
  data.base = (symbol **) obstack_base (obs);
  data.size = 0;

  if (argc == 1)
    {
      hack_all_symbols (dump_symbol, &data);
    }
  else
    {
      for (i = 1; i < argc; i++)
	{
	  if (arg_type (argv, i) != TOKEN_TEXT)
	    {
	      m4_warn (0, me, _("invalid macro name ignored"));
	      continue;
	    }
	  s = lookup_symbol (ARG (i), ARG_LEN (i), SYMBOL_LOOKUP);
	  if (s != NULL && SYMBOL_TYPE (s) != TOKEN_VOID)
	    dump_symbol (s, &data);
	  else if (debug_level & DEBUG_TRACE_DEREF)
	    m4_warn (0, me, _("undefined macro %s"),
		     quotearg_style_mem (locale_quoting_style, ARG (i),
					 ARG_LEN (i)));
	}
    }

  /* Make table of symbols invisible to expand_macro ().  */
  obstack_finish (obs);

  qsort (data.base, data.size, sizeof (symbol *), dumpdef_cmp);
  for (; data.size > 0; --data.size, data.base++)
    {
      /* TODO - add debugmode(b) option to control quoting style?  */
      fwrite (SYMBOL_NAME (data.base[0]), 1, SYMBOL_NAME_LEN (data.base[0]),
	      output);
      fputc (':', output);
      fputc ('\t', output);

      switch (SYMBOL_TYPE (data.base[0]))
	{
	case TOKEN_TEXT:
	  if (debug_level & DEBUG_TRACE_QUOTE)
	    fwrite (curr_quote.str1, 1, curr_quote.len1, output);
	  fwrite (SYMBOL_TEXT (data.base[0]), 1,
		  SYMBOL_TEXT_LEN (data.base[0]), output);
	  if (debug_level & DEBUG_TRACE_QUOTE)
	    fwrite (curr_quote.str2, 1, curr_quote.len2, output);
	  break;

	case TOKEN_FUNC:
	  bp = find_builtin_by_addr (SYMBOL_FUNC (data.base[0]));
	  if (bp == NULL)
	    {
	      assert (!"m4_dumpdef");
	      abort ();
	    }
	  xfprintf (output, "<%s>", bp->name);
	  break;

	default:
	  assert (!"m4_dumpdef");
	  abort ();
	  break;
	}
      fputc ('\n', output);
    }
}

/*---------------------------------------------------------------------.
| The builtin "builtin" allows calls to builtin macros, even if their  |
| definition has been overridden or shadowed.  It is thus possible to  |
| redefine builtins, and still access their original definition.  This |
| macro is not available in compatibility mode.			       |
`---------------------------------------------------------------------*/

static void
m4_builtin (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  const builtin *bp;
  const char *name;

  if (bad_argc (me, argc, 1, -1))
    return;
  if (arg_type (argv, 1) != TOKEN_TEXT)
    {
      m4_warn (0, me, _("invalid macro name ignored"));
      return;
    }

  name = ARG (1);
  bp = find_builtin_by_name (name);
  if (bp->func == m4_placeholder)
    {
      if (debug_level & DEBUG_TRACE_DEREF)
	m4_warn (0, me, _("undefined builtin %s"),
		 quotearg_style_mem (locale_quoting_style, name, ARG_LEN (1)));
    }
  else
    {
      macro_arguments *new_argv = make_argv_ref (argv, name, ARG_LEN (1),
						 !bp->groks_macro_args, false);
      bp->func (obs, argc - 1, new_argv);
    }
}

/*------------------------------------------------------------------------.
| The builtin "indir" allows indirect calls to macros, even if their name |
| is not a proper macro name.  It is thus possible to define macros with  |
| ill-formed names for internal use in larger macro packages.  This macro |
| is not available in compatibility mode.				  |
`------------------------------------------------------------------------*/

static void
m4_indir (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  symbol *s;
  const char *name;
  size_t len;

  if (bad_argc (me, argc, 1, -1))
    return;
  if (arg_type (argv, 1) != TOKEN_TEXT)
    {
      m4_warn (0, me, _("invalid macro name ignored"));
      return;
    }

  name = ARG (1);
  len = ARG_LEN (1);
  s = lookup_symbol (name, len, SYMBOL_LOOKUP);
  if (s == NULL || SYMBOL_TYPE (s) == TOKEN_VOID)
    {
      if (debug_level & DEBUG_TRACE_DEREF)
	m4_warn (0, me, _("undefined macro %s"),
		 quotearg_style_mem (locale_quoting_style, name, len));
    }
  else
    {
      macro_arguments *new_argv = make_argv_ref (argv, name, len,
						 !SYMBOL_MACRO_ARGS (s),
						 SYMBOL_TRACED (s));
      trace_prepre (arg_info (new_argv));
      call_macro (s, new_argv, obs);
    }
}

/*-------------------------------------------------------------------------.
| The macro "defn" returns the quoted definition of the macro named by the |
| first argument.  If the macro is builtin, it will push a special	   |
| macro-definition token on the input stack.				   |
`-------------------------------------------------------------------------*/

static void
m4_defn (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  symbol *s;
  builtin_func *b;
  int i;

  if (bad_argc (me, argc, 1, -1))
    return;

  for (i = 1; i < argc; i++)
    {
      if (arg_type (argv, i) != TOKEN_TEXT)
	{
	  m4_warn (0, me, _("invalid macro name ignored"));
	  continue;
	}
      s = lookup_symbol (ARG (i), ARG_LEN (i), SYMBOL_LOOKUP);
      if (s == NULL || SYMBOL_TYPE (s) == TOKEN_VOID)
	{
	  if (debug_level & DEBUG_TRACE_DEREF)
	    m4_warn (0, me, _("undefined macro %s"),
		     quotearg_style_mem (locale_quoting_style, ARG (i),
					 ARG_LEN (i)));
	  continue;
	}

      switch (SYMBOL_TYPE (s))
	{
	case TOKEN_TEXT:
	  obstack_grow (obs, curr_quote.str1, curr_quote.len1);
	  obstack_grow (obs, SYMBOL_TEXT (s), SYMBOL_TEXT_LEN (s));
	  obstack_grow (obs, curr_quote.str2, curr_quote.len2);
	  break;

	case TOKEN_FUNC:
	  b = SYMBOL_FUNC (s);
	  if (b == m4_placeholder)
	    m4_warn (0, me,
		     _("builtin %s requested by frozen file not found"),
		     quotearg_style_mem (locale_quoting_style, ARG (i),
					 ARG_LEN (i)));
	  else
	    push_macro (obs, b);
	  break;

	default:
	  assert (!"m4_defn");
	  abort ();
	}
    }
}

/*------------------------------------------------------------------------.
| This section contains macros to handle the builtins "syscmd", "esyscmd" |
| and "sysval".  "esyscmd" is GNU specific.				  |
`------------------------------------------------------------------------*/

/* Helper macros for readability.  */
#if UNIX || defined WEXITSTATUS
# define M4SYSVAL_EXITBITS(status)			 \
  (WIFEXITED (status) ? WEXITSTATUS (status) : 0)
# define M4SYSVAL_TERMSIGBITS(status)			 \
  (WIFSIGNALED (status) ? WTERMSIG (status) << 8 : 0)

#else /* !UNIX && !defined WEXITSTATUS */
/* Platforms such as MinGW do not support the notion of reporting
   which signal terminated a process.  Furthermore if WEXITSTATUS was
   not provided, then the exit value is in the low eight bits.  */
# define M4SYSVAL_EXITBITS(status) status
# define M4SYSVAL_TERMSIGBITS(status) 0
#endif /* !UNIX && !defined WEXITSTATUS */

/* Fallback definitions if <stdlib.h> or <sys/wait.h> are inadequate.  */
#ifndef WEXITSTATUS
# define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#endif
#ifndef WTERMSIG
# define WTERMSIG(status) ((status) & 0x7f)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(status) (WTERMSIG (status) != 0)
#endif
#ifndef WIFEXITED
# define WIFEXITED(status) (WTERMSIG (status) == 0)
#endif

/* Exit code from last "syscmd" command.  */
static int sysval;

static void
m4_syscmd (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  const char *cmd = ARG (1);
  size_t len = ARG_LEN (1);

  if (strlen (cmd) != len)
    m4_warn (0, me, _("argument %s truncated"),
	     quotearg_style_mem (locale_quoting_style, cmd, len));
  if (bad_argc (me, argc, 1, 1) || !*cmd)
    {
      /* The empty command is successful.  */
      sysval = 0;
      return;
    }

  debug_flush_files ();
  sysval = system (cmd);
#if FUNC_SYSTEM_BROKEN
  /* OS/2 has a buggy system() that returns exit status in the lowest eight
     bits, although pclose() and WEXITSTATUS are defined to return exit
     status in the next eight bits.  This approach can't detect signals, but
     at least syscmd(`ls') still works when stdout is a terminal.  An
     alternate approach is popen/insert_file/pclose, but that makes stdout
     a pipe, which can change how some child processes behave.  */
  if (sysval != -1)
    sysval <<= 8;
#endif /* FUNC_SYSTEM_BROKEN */
}

static void
m4_esyscmd (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  const char *cmd = ARG (1);
  size_t len = ARG_LEN (1);
  FILE *pin;

  if (strlen (cmd) != len)
    m4_warn (0, me, _("argument %s truncated"),
	     quotearg_style_mem (locale_quoting_style, cmd, len));
  if (bad_argc (me, argc, 1, 1) || !*cmd)
    {
      /* The empty command is successful.  */
      sysval = 0;
      return;
    }

  debug_flush_files ();
  errno = 0;
  pin = popen (cmd, "r");
  if (pin == NULL)
    {
      m4_warn (errno, me, _("cannot open pipe to command %s"),
	       quotearg_style (locale_quoting_style, cmd));
      sysval = -1;
    }
  else
    {
      while (1)
	{
	  size_t avail = obstack_room (obs);
	  if (!avail)
	    {
	      int ch = getc (pin);
	      if (ch == EOF)
		break;
	      obstack_1grow (obs, ch);
	    }
	  else
	    {
	      size_t len = fread (obstack_next_free (obs), 1, avail, pin);
	      if (len <= 0)
		break;
	      obstack_blank_fast (obs, len);
	    }
	}
      if (ferror (pin))
	m4_warn (errno, me, _("cannot read pipe to command %s"),
		 quotearg_style (locale_quoting_style, cmd));
      sysval = pclose (pin);
    }
}

static void
m4_sysval (struct obstack *obs, int argc, macro_arguments *argv)
{
  shipout_int (obs, (sysval == -1 ? 127
		     : (M4SYSVAL_EXITBITS (sysval)
			| M4SYSVAL_TERMSIGBITS (sysval))));
}

/*-------------------------------------------------------------------------.
| This section contains the top level code for the "eval" builtin.  The	   |
| actual work is done in the function evaluate (), which lives in eval.c.  |
`-------------------------------------------------------------------------*/

static void
m4_eval (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int32_t value = 0;
  int radix = 10;
  int min = 1;
  const char *s;
  size_t len;

  if (bad_argc (me, argc, 1, 3))
    return;

  if (!arg_empty (argv, 2) && !numeric_arg (me, ARG (2), ARG_LEN (2), &radix))
    return;

  if (radix < 1 || radix > 36)
    {
      m4_warn (0, me, _("radix out of range: %d"), radix);
      return;
    }

  if (argc >= 4 && !numeric_arg (me, ARG (3), ARG_LEN (3), &min))
    return;
  if (min < 0)
    {
      m4_warn (0, me, _("negative width: %d"), min);
      return;
    }

  if (evaluate (me, ARG (1), ARG_LEN (1), &value))
    return;

  if (radix == 1)
    {
      if (value < 0)
	{
	  obstack_1grow (obs, '-');
	  value = -value;
	}
      if ((uint32_t) value < min)
	{
	  obstack_blank (obs, min - value);
	  memset ((char *) obstack_next_free (obs) - (min - value), '0',
		  min - value);
	}
      obstack_blank (obs, value);
      memset ((char *) obstack_next_free (obs) - value, '1', value);
      return;
    }

  s = ntoa (value, radix);

  if (*s == '-')
    {
      obstack_1grow (obs, '-');
      s++;
    }
  len = strlen (s);
  if (len < min)
    {
      min -= len;
      obstack_blank (obs, min);
      memset ((char *) obstack_next_free (obs) - min, '0', min);
    }
  obstack_grow (obs, s, len);
}

static void
m4_incr (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int value;

  if (bad_argc (me, argc, 1, 1))
    return;

  if (!numeric_arg (me, ARG (1), ARG_LEN (1), &value))
    return;

  shipout_int (obs, value + 1);
}

static void
m4_decr (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int value;

  if (bad_argc (me, argc, 1, 1))
    return;

  if (!numeric_arg (me, ARG (1), ARG_LEN (1), &value))
    return;

  shipout_int (obs, value - 1);
}

/* This section contains the macros "divert", "undivert" and "divnum" for
   handling diversion.  The utility functions used lives in output.c.  */

/*-----------------------------------------------------------------------.
| Divert further output to the diversion given by ARGV[1].  Out of range |
| means discard further output.						 |
`-----------------------------------------------------------------------*/

static void
m4_divert (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int i = 0;

  bad_argc (me, argc, 0, 2);
  if (argc >= 2 && !numeric_arg (me, ARG (1), ARG_LEN (1), &i))
    return;

  make_diversion (i);
  divert_text (NULL, ARG (2), ARG_LEN (2), current_line);
}

/*-----------------------------------------------------.
| Expand to the current diversion number, -1 if none.  |
`-----------------------------------------------------*/

static void
m4_divnum (struct obstack *obs, int argc, macro_arguments *argv)
{
  bad_argc (arg_info (argv), argc, 0, 0);
  shipout_int (obs, current_diversion);
}

/*-----------------------------------------------------------------------.
| Bring back the diversion given by the argument list.  If none is	 |
| specified, bring back all diversions.  GNU specific is the option of	 |
| undiverting named files, by passing a non-numeric argument to undivert |
| ().									 |
`-----------------------------------------------------------------------*/

static void
m4_undivert (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int i;
  int file;
  FILE *fp;
  char *endp;

  if (argc == 1)
    undivert_all ();
  else
    for (i = 1; i < argc; i++)
      {
	const char *str = ARG (i);
	size_t len = ARG_LEN (i);
	file = strtol (str, &endp, 10);
	if (endp - str == len && !isspace (to_uchar (*str)))
	  insert_diversion (file);
	else if (no_gnu_extensions)
	  m4_warn (0, me, _("non-numeric argument %s"),
		   quotearg_style_mem (locale_quoting_style, str, len));
	else if (strlen (str) != len)
	  m4_warn (0, me, _("invalid file name %s"),
		   quotearg_style_mem (locale_quoting_style, str, len));
	else
	  {
	    fp = m4_path_search (str, NULL);
	    if (fp != NULL)
	      {
		insert_file (fp);
		if (fclose (fp) == EOF)
		  m4_warn (errno, me, _("error undiverting %s"),
			   quotearg_style (locale_quoting_style, str));
	      }
	    else
	      m4_warn (errno, me, _("cannot undivert %s"),
		       quotearg_style (locale_quoting_style, str));
	  }
      }
}

/* This section contains various macros, which does not fall into any
   specific group.  These are "dnl", "shift", "changequote", "changecom"
   and "changeword".  */

/*------------------------------------------------------------------------.
| Delete all subsequent whitespace from input.  The function skip_line () |
| lives in input.c.							  |
`------------------------------------------------------------------------*/

static void
m4_dnl (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);

  bad_argc (me, argc, 0, 0);
  skip_line (me);
}

/*--------------------------------------------------------------------.
| Shift all arguments one to the left, discarding the first	      |
| argument.  Each output argument is quoted with the current quotes.  |
`--------------------------------------------------------------------*/

static void
m4_shift (struct obstack *obs, int argc, macro_arguments *argv)
{
  if (bad_argc (arg_info (argv), argc, 1, -1))
    return;
  push_args (obs, argv, true, true);
}

/*--------------------------------------------------------------------------.
| Change the current quotes.  The function set_quotes () lives in input.c.  |
`--------------------------------------------------------------------------*/

static void
m4_changequote (struct obstack *obs, int argc, macro_arguments *argv)
{
  bad_argc (arg_info (argv), argc, 0, 2);

  /* Explicit NULL distinguishes between empty and missing argument.  */
  set_quotes ((argc >= 2) ? ARG (1) : NULL, ARG_LEN (1),
	      (argc >= 3) ? ARG (2) : NULL, ARG_LEN (2));
}

/*--------------------------------------------------------------------.
| Change the current comment delimiters.  The function set_comment () |
| lives in input.c.						      |
`--------------------------------------------------------------------*/

static void
m4_changecom (struct obstack *obs, int argc, macro_arguments *argv)
{
  bad_argc (arg_info (argv), argc, 0, 2);

  /* Explicit NULL distinguishes between empty and missing argument.  */
  set_comment ((argc >= 2) ? ARG (1) : NULL, ARG_LEN (1),
	       (argc >= 3) ? ARG (2) : NULL, ARG_LEN (2));
}

#ifdef ENABLE_CHANGEWORD

/*-----------------------------------------------------------------------.
| Change the regular expression used for breaking the input into words.	 |
| The function set_word_regexp () lives in input.c.			 |
`-----------------------------------------------------------------------*/

static void
m4_changeword (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);

  if (bad_argc (me, argc, 1, 1))
    return;
  set_word_regexp (me, ARG (1), ARG_LEN (1));
}

#endif /* ENABLE_CHANGEWORD */

/* This section contains macros for inclusion of other files -- "include"
   and "sinclude".  This differs from bringing back diversions, in that
   the input is scanned before being copied to the output.  */

/*-------------------------------------------------------------------------.
| Generic include function.  Include the file given by the first argument, |
| if it exists.  Complain about inaccessible files iff SILENT is false.    |
`-------------------------------------------------------------------------*/

static void
include (int argc, macro_arguments *argv, bool silent)
{
  const call_info *me = arg_info (argv);
  FILE *fp;
  char *name;
  const char *arg;
  size_t len;

  if (bad_argc (me, argc, 1, 1))
    return;

  arg = ARG (1);
  len = ARG_LEN (1);
  if (strlen (arg) != len)
    m4_warn (0, me, _("argument %s truncated"),
	     quotearg_style_mem (locale_quoting_style, arg, len));
  fp = m4_path_search (arg, &name);
  if (fp == NULL)
    {
      if (!silent)
	m4_error (0, errno, me, _("cannot open %s"),
		  quotearg_style (locale_quoting_style, arg));
      return;
    }

  push_file (fp, name, true);
  free (name);
}

/*------------------------------------------------.
| Include a file, complaining in case of errors.  |
`------------------------------------------------*/

static void
m4_include (struct obstack *obs, int argc, macro_arguments *argv)
{
  include (argc, argv, false);
}

/*----------------------------------.
| Include a file, ignoring errors.  |
`----------------------------------*/

static void
m4_sinclude (struct obstack *obs, int argc, macro_arguments *argv)
{
  include (argc, argv, true);
}

/* More miscellaneous builtins -- "maketemp", "errprint", "__file__",
   "__line__", and "__program__".  The last three are GNU specific.  */

/*-----------------------------------------------------------------.
| Use the first argument as a template for a temporary file name.  |
`-----------------------------------------------------------------*/

/* Add trailing 'X' to PATTERN of length LEN as necessary, then
   securely create the file, and place the quoted new file name on
   OBS.  Report errors on behalf of ME.  */
static void
mkstemp_helper (struct obstack *obs, const call_info *me, const char *pattern,
		size_t len)
{
  int fd;
  int i;
  char *name;

  /* Guarantee that there are six trailing 'X' characters, even if the
     user forgot to supply them.  Output must be quoted if
     successful.  */
  obstack_grow (obs, curr_quote.str1, curr_quote.len1);
  if (strlen (pattern) < len)
    {
      m4_warn (0, me, _("argument %s truncated"),
	       quotearg_style_mem (locale_quoting_style, pattern, len));
      len = strlen (pattern);
    }
  obstack_grow (obs, pattern, len);
  for (i = 0; len > 0 && i < 6; i++)
    if (pattern[--len] != 'X')
      break;
  obstack_grow0 (obs, "XXXXXX", 6 - i);
  name = (char *) obstack_base (obs) + curr_quote.len1;

  errno = 0;
  fd = mkstemp (name);
  if (fd < 0)
    {
      m4_warn (errno, me, _("cannot create file from template %s"),
	       quotearg_style (locale_quoting_style, pattern));
      obstack_free (obs, obstack_finish (obs));
    }
  else
    {
      close (fd);
      /* Remove NUL, then finish quote.  */
      obstack_blank (obs, -1);
      obstack_grow (obs, curr_quote.str2, curr_quote.len2);
    }
}

static void
m4_maketemp (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);

  if (bad_argc (me, argc, 1, 1))
    return;
  m4_warn (0, me, _("recommend using mkstemp instead"));
  if (no_gnu_extensions)
    {
      /* POSIX states "any trailing 'X' characters [are] replaced with
	 the current process ID as a string", without referencing the
	 file system.  Horribly insecure, but we have to do it when we
	 are in traditional mode.

	 For reference, Solaris m4 does:
	   maketemp() -> `'
	   maketemp(X) -> `X'
	   maketemp(XX) -> `Xn', where n is last digit of pid
	   maketemp(XXXXXXXX) -> `X00nnnnn', where nnnnn is 16-bit pid
      */
      const char *str = ARG (1);
      size_t len = ARG_LEN (1);
      size_t i;
      struct obstack *scratch = arg_scratch ();
      size_t pid_len = obstack_printf (scratch, "%lu",
				       (unsigned long) getpid ());
      char *pid = (char *) obstack_copy0 (scratch, "", 0);

      for (i = len; i > 1; i--)
	if (str[i - 1] != 'X')
	  break;
      obstack_grow (obs, str, i);
      if (len - i < pid_len)
	obstack_grow (obs, pid + pid_len - (len - i), len - i);
      else
	obstack_printf (obs, "%.*d%s", len - i - pid_len, 0, pid);
    }
  else
    mkstemp_helper (obs, me, ARG (1), ARG_LEN (1));
}

static void
m4_mkstemp (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);

  if (bad_argc (me, argc, 1, 1))
    return;
  mkstemp_helper (obs, me, ARG (1), ARG_LEN (1));
}

/*----------------------------------------.
| Print all arguments on standard error.  |
`----------------------------------------*/

static void
m4_errprint (struct obstack *obs, int argc, macro_arguments *argv)
{
  size_t len;

  if (bad_argc (arg_info (argv), argc, 1, -1))
    return;
  arg_print (obs, argv, 1, NULL, true, NULL, " ", NULL, false);
  debug_flush_files ();
  len = obstack_object_size (obs);
  /* The close_stdin module makes it safe to skip checking the return
     value here.  */
  fwrite (obstack_finish (obs), 1, len, stderr);
  fflush (stderr);
}

static void
m4___file__ (struct obstack *obs, int argc, macro_arguments *argv)
{
  bad_argc (arg_info (argv), argc, 0, 0);
  obstack_grow (obs, curr_quote.str1, curr_quote.len1);
  obstack_grow (obs, current_file, strlen (current_file));
  obstack_grow (obs, curr_quote.str2, curr_quote.len2);
}

static void
m4___line__ (struct obstack *obs, int argc, macro_arguments *argv)
{
  bad_argc (arg_info (argv), argc, 0, 0);
  shipout_int (obs, current_line);
}

static void
m4___program__ (struct obstack *obs, int argc, macro_arguments *argv)
{
  bad_argc (arg_info (argv), argc, 0, 0);
  obstack_grow (obs, curr_quote.str1, curr_quote.len1);
  obstack_grow (obs, program_name, strlen (program_name));
  obstack_grow (obs, curr_quote.str2, curr_quote.len2);
}

/* This section contains various macros for exiting, saving input until
   EOF is seen, and tracing macro calls.  That is: "m4exit", "m4wrap",
   "traceon" and "traceoff".  */

/*-------------------------------------------------------------------------.
| Exit immediately, with exitcode specified by the first argument, 0 if no |
| arguments are present.						   |
`-------------------------------------------------------------------------*/

static void
m4_m4exit (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int exit_code = EXIT_SUCCESS;

  /* Warn on bad arguments, but still exit.  */
  bad_argc (me, argc, 0, 1);
  if (argc >= 2 && !numeric_arg (me, ARG (1), ARG_LEN (1), &exit_code))
    exit_code = EXIT_FAILURE;
  if (exit_code < 0 || exit_code > 255)
    {
      m4_warn (0, me, _("exit status out of range: %d"), exit_code);
      exit_code = EXIT_FAILURE;
    }
  /* Change debug stream back to stderr, to force flushing debug stream and
     detect any errors it might have encountered.  */
  debug_set_output (me, NULL);
  debug_flush_files ();
  if (exit_code == EXIT_SUCCESS && retcode != EXIT_SUCCESS)
    exit_code = retcode;
  /* Propagate non-zero status to atexit handlers.  */
  if (exit_code != EXIT_SUCCESS)
    exit_failure = exit_code;
  exit (exit_code);
}

/*-----------------------------------------------------------------.
| Save the argument text in FIFO order until EOF has been seen,    |
| allowing for user specified cleanup action.  Extra arguments are |
| saved when not in POSIX mode.                                    |
`-----------------------------------------------------------------*/

static void
m4_m4wrap (struct obstack *obs, int argc, macro_arguments *argv)
{
  if (bad_argc (arg_info (argv), argc, 1, -1))
    return;
  wrap_args (argv);
}


/* Enable tracing of all specified macros, or all, if none is specified.
   Tracing is disabled by default, when a macro is defined.  This can be
   overridden by the "t" debug flag.  */

/*-----------------------------------------------------------------------.
| Set_trace () is used by "traceon" and "traceoff" to enable and disable |
| tracing of a macro.  It disables tracing if DATA is NULL, otherwise it |
| enable tracing.							 |
`-----------------------------------------------------------------------*/

static void
set_trace (symbol *sym, void *data)
{
  SYMBOL_TRACED (sym) = data != NULL;
  /* Remove placeholder from table if macro is undefined and untraced.  */
  if (SYMBOL_TYPE (sym) == TOKEN_VOID && data == NULL)
    lookup_symbol (SYMBOL_NAME (sym), SYMBOL_NAME_LEN (sym), SYMBOL_POPDEF);
}

static void
m4_traceon (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  symbol *s;
  int i;

  if (argc == 1)
    hack_all_symbols (set_trace, obs);
  else
    for (i = 1; i < argc; i++)
      {
	if (arg_type (argv, i) != TOKEN_TEXT)
	  {
	    m4_warn (0, me, _("invalid macro name ignored"));
	    continue;
	  }
	s = lookup_symbol (ARG (i), ARG_LEN (i), SYMBOL_LOOKUP);
	if (!s)
	  s = lookup_symbol (ARG (i), ARG_LEN (i), SYMBOL_INSERT);
	set_trace (s, obs);
      }
}

/*------------------------------------------------------------------------.
| Disable tracing of all specified macros, or all, if none is specified.  |
`------------------------------------------------------------------------*/

static void
m4_traceoff (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  symbol *s;
  int i;

  if (argc == 1)
    hack_all_symbols (set_trace, NULL);
  else
    for (i = 1; i < argc; i++)
      {
	if (arg_type (argv, i) != TOKEN_TEXT)
	  {
	    m4_warn (0, me, _("invalid macro name ignored"));
	    continue;
	  }
	s = lookup_symbol (ARG (i), ARG_LEN (i), SYMBOL_LOOKUP);
	if (s != NULL)
	  set_trace (s, NULL);
      }
}

/*----------------------------------------------------------------------.
| On-the-fly control of the format of the tracing output.  It takes one |
| argument, which is a character string like given to the -d option, or |
| none in which case the debug_level is zeroed.			        |
`----------------------------------------------------------------------*/

static void
m4_debugmode (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  const char *str = ARG (1);
  size_t len = ARG_LEN (1);

  bad_argc (me, argc, 0, 1);

  if (argc == 1)
    debug_level = 0;
  else if (debug_decode (str, len) < 0)
    m4_warn (0, me, _("bad debug flags: %s"),
	     quotearg_style_mem (locale_quoting_style, str, len));
}

/*-------------------------------------------------------------------------.
| Specify the destination of the debugging output.  With one argument, the |
| argument is taken as a file name, with no arguments, revert to stderr.   |
`-------------------------------------------------------------------------*/

static void
m4_debugfile (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);

  bad_argc (me, argc, 0, 1);

  if (argc == 1)
    debug_set_output (me, NULL);
  else
    {
      const char *str = ARG (1);
      size_t len = ARG_LEN (1);
      if (strlen (str) < len)
	m4_warn (0, me, _("argument %s truncated"),
		 quotearg_style_mem (locale_quoting_style, str, len));
      if (!debug_set_output (me, str))
	m4_warn (errno, me, _("cannot set debug file %s"),
		 quotearg_style (locale_quoting_style, str));
    }
}

/* This section contains text processing macros: "len", "index",
   "substr", "translit", "format", "regexp" and "patsubst".  The last
   three are GNU specific.  */

/*---------------------------------------------.
| Expand to the length of the first argument.  |
`---------------------------------------------*/

static void
m4_len (struct obstack *obs, int argc, macro_arguments *argv)
{
  if (bad_argc (arg_info (argv), argc, 1, 1))
    return;
  shipout_int (obs, ARG_LEN (1));
}

/*-------------------------------------------------------------------.
| The macro expands to the first index of the second argument in the |
| first argument.  As an extension, start the search at the index    |
| indicated by the third argument.				     |
`-------------------------------------------------------------------*/

static void
m4_index (struct obstack *obs, int argc, macro_arguments *argv)
{
  const char *haystack;
  size_t haystack_len;
  const char *needle;
  const char *result = NULL;
  int offset = 0;
  int retval = -1;

  if (bad_argc (arg_info (argv), argc, 2, 3))
    {
      /* builtin(`index') is blank, but index(`abc') is 0.  */
      if (argc == 2)
	shipout_int (obs, 0);
      return;
    }

  haystack = ARG (1);
  haystack_len = ARG_LEN (1);
  needle = ARG (2);
  if (!arg_empty (argv, 3)
      && !numeric_arg (arg_info (argv), ARG (3), ARG_LEN (3), &offset))
    return;
  if (offset < 0)
    {
      offset += haystack_len;
      if (offset < 0)
	offset = 0;
    }
  else if (haystack_len < offset)
    {
      shipout_int (obs, -1);
      return;
    }

  /* Rely on the optimizations guaranteed by gnulib's memmem
     module.  */
  result = (char *) memmem (haystack + offset, haystack_len - offset, needle,
			    ARG_LEN (2));
  if (result)
    retval = result - haystack;

  shipout_int (obs, retval);
}

/*-------------------------------------------------------------------.
| The macro "substr" extracts substrings from the first argument,    |
| starting from the index given by the second argument, extending    |
| for a length given by the third argument.  If the third argument   |
| is missing or empty, the substring extends to the end of the first |
| argument.  As an extension, negative arguments are treated as	     |
| indices relative to the string length.  Also, if a fourth argument |
| is supplied, the original string is output with the selected	     |
| substring replaced by the argument.				     |
`-------------------------------------------------------------------*/

static void
m4_substr (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  int start = 0;
  int end;
  int length;

  if (bad_argc (me, argc, 2, 4))
    {
      /* builtin(`substr') is blank, but substr(`abc') is abc.  */
      if (argc == 2)
	push_arg (obs, argv, 1);
      return;
    }

  length = ARG_LEN (1);
  if (!arg_empty (argv, 2) && !numeric_arg (me, ARG (2), ARG_LEN (2), &start))
    return;
  if (start < 0)
    start += length;

  if (arg_empty (argv, 3))
    end = length;
  else
    {
      if (!numeric_arg (me, ARG (3), ARG_LEN (3), &end))
	return;
      if (end < 0)
	end += length;
      else
	end += start;
    }

  if (argc >= 5)
    {
      /* Replacement text provided.  */
      if (end < start)
	end = start;
      if (end < 0 || length < start)
	{
	  m4_warn (0, me, _("substring out of range"));
	  return;
	}
      if (start < 0)
	start = 0;
      if (length < end)
	end = length;
      obstack_grow (obs, ARG (1), start);
      push_arg (obs, argv, 4);
      obstack_grow (obs, ARG (1) + end, length - end);
      return;
    }

  if (start < 0)
    start = 0;
  if (length < end)
    end = length;
  if (end <= start)
    return;

  obstack_grow (obs, ARG (1) + start, end - start);
}

/*------------------------------------------------------------------.
| For "translit", ranges are allowed in the second and third        |
| argument.  They are expanded in the following function, and the   |
| expanded strings, without any ranges left, are used to translate  |
| the characters of the first argument.  A single - (dash) can be   |
| included in the strings by being the first or the last character  |
| in the string.  If the first character in a range is after the    |
| first in the character set, the range is made backwards, thus 9-0 |
| is the string 9876543210.  This function expands S of length *LEN |
| using OBS for the expansion, sets *LEN to the new length, and     |
| returns the expansion.                                            |
`------------------------------------------------------------------*/

static const char *
expand_ranges (const char *s, size_t *len, struct obstack *obs)
{
  unsigned char from;
  unsigned char to;
  const char *end = s + *len;

  assert (s != end);
  from = *s++;
  obstack_1grow (obs, from);

  for ( ; s != end; from = *s++)
    {
      if (*s == '-')
	{
	  if (++s == end)
	    {
	      /* trailing dash */
	      obstack_1grow (obs, '-');
	      break;
	    }
	  to = *s;
	  if (from <= to)
	    {
	      while (from++ < to)
		obstack_1grow (obs, from);
	    }
	  else
	    {
	      while (--from >= to)
		obstack_1grow (obs, from);
	    }
	}
      else
	obstack_1grow (obs, *s);
    }
  *len = obstack_object_size (obs);
  return (char *) obstack_finish (obs);
}

/*----------------------------------------------------------------------.
| The macro "translit" translates all characters in the first argument, |
| which are present in the second argument, into the corresponding      |
| character from the third argument.  If the third argument is shorter  |
| than the second, the extra characters in the second argument, are     |
| deleted from the first (pueh).				        |
`----------------------------------------------------------------------*/

static void
m4_translit (struct obstack *obs, int argc, macro_arguments *argv)
{
  const char *data;
  const char *from;
  const char *to;
  size_t from_len;
  size_t to_len;
  char map[UCHAR_MAX + 1];
  char found[UCHAR_MAX + 1];
  unsigned char ch;

  enum { ASIS, REPLACE, DELETE };

  if (bad_argc (arg_info (argv), argc, 2, 3) || arg_empty (argv, 1)
      || arg_empty (argv, 2))
    {
      /* builtin(`translit') is blank, but translit(`abc') is abc.  */
      if (argc >= 2)
	push_arg (obs, argv, 1);
      return;
    }

  from = ARG (2);
  from_len = ARG_LEN (2);

  to = ARG (3);
  to_len = ARG_LEN (3);
  if (memchr (to, '-', to_len) != NULL)
    to = expand_ranges (to, &to_len, arg_scratch ());

  /* If there are only one or two bytes to replace, it is faster to
     use memchr2.  Using expand_ranges does nothing unless there are
     at least three bytes.  */
  if (from_len <= 2)
    {
      const char *p;
      size_t len = ARG_LEN (1);
      int second = from[from_len / 2];
      data = ARG (1);
      while ((p = (char *) memchr2 (data, from[0], second, len)))
	{
	  obstack_grow (obs, data, p - data);
	  len -= p - data + 1;
	  data = p + 1;
	  if (*p == from[0] && to_len)
	    obstack_1grow (obs, to[0]);
	  else if (*p == second && 1 < to_len)
	    obstack_1grow (obs, to[1]);
	}
      obstack_grow (obs, data, len);
      return;
    }

  if (memchr (from, '-', from_len) != NULL)
    from = expand_ranges (from, &from_len, arg_scratch ());

  /* Calling strchr(from) for each character in data is quadratic,
     since both strings can be arbitrarily long.  Instead, create a
     from-to mapping in one pass of from, then use that map in one
     pass of data, for linear behavior.  Traditional behavior is that
     only the first instance of a character in from is consulted,
     hence the found map.  */
  memset (map, 0, sizeof map);
  memset (found, 0, sizeof found);
  while (from_len--)
    {
      ch = *from++;
      if (found[ch] == ASIS)
	{
	  if (to_len)
	    {
	      found[ch] = REPLACE;
	      map[ch] = *to;
	    }
	  else
	    found[ch] = DELETE;
	}
      if (to_len)
	{
	  to++;
	  to_len--;
	}
    }

  data = ARG (1);
  from_len = ARG_LEN (1);
  while (from_len--)
    {
      ch = *data++;
      switch (found[ch])
	{
	case ASIS:
	  obstack_1grow (obs, ch);
	  break;
	case REPLACE:
	  obstack_1grow (obs, map[ch]);
	  break;
	case DELETE:
	  break;
	default:
	  assert (!"m4_translit");
	  abort ();
	}
    }
}

/*--------------------------------------------------------------.
| Frontend for *printf like formatting.  The function format () |
| lives in the file format.c.                                   |
`--------------------------------------------------------------*/

static void
m4_format (struct obstack *obs, int argc, macro_arguments *argv)
{
  if (bad_argc (arg_info (argv), argc, 1, -1))
    return;
  expand_format (obs, argc, argv);
}

/*-------------------------------------------------------------------------.
| Function to perform substitution by regular expressions.  Used by the	   |
| builtins regexp and patsubst.  The changed text is placed on the	   |
| obstack.  The substitution is REPL, with \& substituted by this part of  |
| VICTIM matched by the last whole regular expression, taken from REGS[0], |
| and \N substituted by the text matched by the Nth parenthesized	   |
| sub-expression, taken from REGS[N].					   |
`-------------------------------------------------------------------------*/

static int substitute_warned = 0;

static void
substitute (struct obstack *obs, const call_info *me, const char *victim,
	    const char *repl, size_t repl_len, struct re_registers *regs)
{
  int ch;
  while (1)
    {
      const char *backslash = (char *) memchr (repl, '\\', repl_len);
      if (!backslash)
	{
	  obstack_grow (obs, repl, repl_len);
	  return;
	}
      obstack_grow (obs, repl, backslash - repl);
      repl_len -= backslash - repl + 1;
      if (!repl_len)
	{
	  m4_warn (0, me, _("trailing \\ ignored in replacement"));
	  return;
	}
      repl = backslash + 1;
      repl_len--;
      ch = *repl++;
      switch (ch)
	{
	case '0':
	  if (!substitute_warned)
	    {
	      m4_warn (0, me, _("\
\\0 will disappear, use \\& instead in replacements"));
	      substitute_warned = 1;
	    }
	  /* Fall through.  */

	case '&':
	  if (regs)
	    obstack_grow (obs, victim + regs->start[0],
			  regs->end[0] - regs->start[0]);
	  break;

	case '1': case '2': case '3': case '4': case '5': case '6':
	case '7': case '8': case '9':
	  ch -= '0';
	  if (!regs || regs->num_regs - 1 <= ch)
	    m4_warn (0, me, _("sub-expression %d not present"), ch);
	  else if (regs->end[ch] > 0)
	    obstack_grow (obs, victim + regs->start[ch],
			  regs->end[ch] - regs->start[ch]);
	  break;

	default:
	  obstack_1grow (obs, ch);
	  break;
	}
    }
}

/*------------------------------------------.
| Initialize regular expression variables.  |
`------------------------------------------*/

void
init_pattern_buffer (struct re_pattern_buffer *buf, struct re_registers *regs)
{
  buf->translate = NULL;
  buf->fastmap = NULL;
  buf->buffer = NULL;
  buf->allocated = 0;
  if (regs)
    {
      regs->start = NULL;
      regs->end = NULL;
    }
}

/*------------------------------------------------------------------.
| Regular expression version of index.  Given two arguments, expand |
| to the index of the first match of the second argument (a regexp) |
| in the first.  Expand to -1 if there is no match.  Given a third  |
| argument, a match is substituted according to this argument.      |
`------------------------------------------------------------------*/

static void
m4_regexp (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  const char *victim;		/* first argument */
  const char *regexp;		/* regular expression */
  const char *repl;		/* replacement string */

  struct re_pattern_buffer *buf;/* compiled regular expression */
  struct re_registers *regs;	/* for subexpression matches */
  const char *msg;		/* error message from re_compile_pattern */
  int startpos;			/* start position of match */
  int length;			/* length of first argument */

  if (bad_argc (me, argc, 2, 3))
    {
      /* builtin(`regexp') is blank, but regexp(`abc') is 0.  */
      if (argc == 2)
	shipout_int (obs, 0);
      return;
    }

  victim = ARG (1);
  regexp = ARG (2);
  repl = ARG (3);

  if (arg_empty (argv, 2))
    {
      /* The empty regex matches everything!  */
      if (argc == 3)
	shipout_int (obs, 0);
      else
	substitute (obs, me, victim, repl, ARG_LEN (3), NULL);
      return;
    }

#ifdef DEBUG_REGEX
  if (trace_file)
    {
      fputs ("r:{", trace_file);
      fwrite (regexp, 1, ARG_LEN (2), trace_file);
      if (argc > 3)
	{
	  fputs ("}:{", trace_file);
	  fwrite (repl, 1, ARG_LEN (3), trace_file);
	}
      fputs ("}\n", trace_file);
    }
#endif /* DEBUG_REGEX */

  msg = compile_pattern (regexp, ARG_LEN (2), &buf, &regs);
  if (msg != NULL)
    {
      m4_warn (0, me, _("bad regular expression %s: %s"),
	       quotearg_style_mem (locale_quoting_style, regexp, ARG_LEN (2)),
	       msg);
      return;
    }

  length = ARG_LEN (1);
  /* Avoid overhead of allocating regs if we won't use it.  */
  startpos = re_search (buf, victim, length, 0, length,
			argc == 3 ? NULL : regs);

  if (startpos == -2)
    m4_warn (0, me, _("problem matching regular expression %s"),
	     quotearg_style_mem (locale_quoting_style, regexp, ARG_LEN (2)));
  else if (argc == 3)
    shipout_int (obs, startpos);
  else if (startpos >= 0)
    substitute (obs, me, victim, repl, ARG_LEN (3), regs);
}

/*------------------------------------------------------------------.
| Substitute all matches of a regexp occurring in a string.  Each   |
| match of the second argument (a regexp) in the first argument is  |
| changed to the third argument, with \& substituted by the matched |
| text, and \N substituted by the text matched by the Nth           |
| parenthesized sub-expression.                                     |
`------------------------------------------------------------------*/

static void
m4_patsubst (struct obstack *obs, int argc, macro_arguments *argv)
{
  const call_info *me = arg_info (argv);
  const char *victim;		/* First argument.  */
  const char *regexp;		/* Regular expression.  */
  const char *repl;		/* Replacement text.  */

  struct re_pattern_buffer *buf;/* Compiled regular expression.  */
  struct re_registers *regs;	/* For subexpression matches.  */
  const char *msg;		/* Error message from re_compile_pattern.  */
  int matchpos;			/* Start position of match.  */
  int offset;			/* Current match offset.  */
  int length;			/* Length of first argument.  */
  size_t repl_len;		/* Length of replacement.  */

  if (bad_argc (me, argc, 2, 3))
    {
      /* builtin(`patsubst') is blank, but patsubst(`abc') is abc.  */
      if (argc == 2)
	push_arg (obs, argv, 1);
      return;
    }

  /* The empty regex matches everywhere, but if there is no
     replacement, we need not waste time with it.  */
  if (arg_empty (argv, 2) && arg_empty (argv, 3))
    {
      push_arg (obs, argv, 1);
      return;
    }

  victim = ARG (1);
  regexp = ARG (2);
  repl = ARG (3);
  repl_len = ARG_LEN (3);

#ifdef DEBUG_REGEX
  if (trace_file)
    {
      fputs ("p:{", trace_file);
      fwrite (regexp, 1, ARG_LEN (2), trace_file);
      fputs ("}:{", trace_file);
      fwrite (repl, 1, repl_len, trace_file);
      fputs ("}\n", trace_file);
    }
#endif /* DEBUG_REGEX */

  msg = compile_pattern (regexp, ARG_LEN (2), &buf, &regs);
  if (msg != NULL)
    {
      m4_warn (0, me, _("bad regular expression %s: %s"),
	       quotearg_style_mem (locale_quoting_style, regexp, ARG_LEN (2)),
	       msg);
      return;
    }

  length = ARG_LEN (1);

  offset = 0;
  matchpos = 0;
  while (offset <= length)
    {
      matchpos = re_search (buf, victim, length,
			    offset, length - offset, regs);
      if (matchpos < 0)
	{

	  /* Match failed -- either error or there is no match in the
	     rest of the string, in which case the rest of the string is
	     copied verbatim.  */

	  if (matchpos == -2)
	    m4_warn (0, me, _("problem matching regular expression %s"),
		     quotearg_style_mem (locale_quoting_style, regexp,
					 ARG_LEN (2)));
	  else if (offset < length)
	    obstack_grow (obs, victim + offset, length - offset);
	  break;
	}

      /* Copy the part of the string that was skipped by re_search ().  */

      if (matchpos > offset)
	obstack_grow (obs, victim + offset, matchpos - offset);

      /* Handle the part of the string that was covered by the match.  */

      substitute (obs, me, victim, repl, repl_len, regs);

      /* Update the offset to the end of the match.  If the regexp
	 matched a null string, advance offset one more, to avoid
	 infinite loops.  */

      offset = regs->end[0];
      if (regs->start[0] == regs->end[0])
	{
	  if (offset < length)
	    obstack_1grow (obs, victim[offset]);
	  offset++;
	}
    }
}

/* Finally, a placeholder builtin.  This builtin is not installed by
   default, but when reading back frozen files, this is associated
   with any builtin we don't recognize (for example, if the frozen
   file was created with a changeword capable m4, but is then loaded
   by a different m4 that does not support changeword).  This way, we
   can keep 'm4 -R' quiet in the common case that the user did not
   know or care about the builtin when the frozen file was created,
   while still flagging it as a potential error if an attempt is made
   to actually use the builtin.  */

/*--------------------------------------------------------------------.
| Issue a warning that this macro is a placeholder for an unsupported |
| builtin that was requested while reloading a frozen file.           |
`--------------------------------------------------------------------*/

void
m4_placeholder (struct obstack *obs, int argc, macro_arguments *argv)
{
  m4_warn (0, NULL, _("builtin %s requested by frozen file not found"),
	   quotearg_style_mem (locale_quoting_style, ARG (0), ARG_LEN (0)));
}

/*-------------------------------------------------------------------------.
| This function handles all expansion of user defined and predefined	   |
| macros.  It is called with an obstack OBS, where the macros expansion	   |
| will be placed, as an unfinished object.  SYM points to the macro	   |
| definition, giving the expansion text.  ARGC and ARGV are the arguments, |
| as usual.								   |
`-------------------------------------------------------------------------*/

void
expand_user_macro (struct obstack *obs, symbol *sym,
		   int argc, macro_arguments *argv)
{
  const char *text = SYMBOL_TEXT (sym);
  size_t len = SYMBOL_TEXT_LEN (sym);
  int i;
  while (1)
    {
      const char *dollar = (char *) memchr (text, '$', len);
      if (!dollar)
	{
	  obstack_grow (obs, text, len);
	  return;
	}
      obstack_grow (obs, text, dollar - text);
      len -= dollar - text;
      text = dollar;
      if (len == 1)
	{
	  obstack_1grow (obs, '$');
	  return;
	}
      len--;
      switch (*++text)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  if (no_gnu_extensions)
	    {
	      i = *text++ - '0';
	      len--;
	    }
	  else
	    {
	      for (i = 0; len && isdigit (to_uchar (*text)); text++, len--)
		i = i * 10 + (*text - '0');
	    }
	  push_arg (obs, argv, i);
	  break;

	case '#':		/* number of arguments */
	  shipout_int (obs, argc - 1);
	  text++;
	  len--;
	  break;

	case '*':		/* all arguments */
	case '@':		/* ... same, but quoted */
	  push_args (obs, argv, false, *text == '@');
	  text++;
	  len--;
	  break;

	default:
	  obstack_1grow (obs, '$');
	  break;
	}
    }
}

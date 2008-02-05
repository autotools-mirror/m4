/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2004, 2005, 2006,
   2007, 2008 Free Software Foundation, Inc.

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

#include "m4.h"

#include <getopt.h>
#include <signal.h>
#include <stdarg.h>

#include "version-etc.h"

#define AUTHORS "Rene' Seindal", "Eric Blake"

static void usage (int);

/* Enable sync output for /lib/cpp (-s).  */
int sync_output = 0;

/* Debug (-d[flags]).  */
int debug_level = 0;

/* Hash table size (should be a prime) (-Hsize).  */
size_t hash_table_size = HASHMAX;

/* Disable GNU extensions (-G).  */
int no_gnu_extensions = 0;

/* Prefix all builtin functions by `m4_'.  */
int prefix_all_builtins = 0;

/* Max length of arguments in trace output (-lsize).  */
size_t max_debug_argument_length = SIZE_MAX;

/* Suppress warnings about missing arguments.  */
int suppress_warnings = 0;

/* If true, then warnings affect exit status.  */
static bool fatal_warnings = false;

/* If not zero, then value of exit status for warning diagnostics.  */
int warning_status = 0;

/* Artificial limit for expansion_level in macro.c.  */
int nesting_limit = 1024;

#ifdef ENABLE_CHANGEWORD
/* User provided regexp for describing m4 words.  */
const char *user_word_regexp = "";
#endif

/* The name this program was run with. */
const char *program_name;

/* Global catchall for any errors that should affect final error status, but
   where we try to continue execution in the meantime.  */
int retcode;

struct macro_definition
{
  struct macro_definition *next;
  int code;			/* D, U, s, t, or '\1' */
  const char *arg;
};
typedef struct macro_definition macro_definition;

/* Error handling functions.  */

/*------------------------------------------------------------------.
| Helper for all the error reporting, as a wrapper around	    |
| error_at_line.  Report error message based on FORMAT and ARGS, on |
| behalf of CALLER (if any), otherwise at the global current	    |
| location.  If ERRNUM, decode the errno value that caused the      |
| error.  If STATUS, exit immediately with that status.  If WARN,   |
| prepend 'Warning: '.						    |
`------------------------------------------------------------------*/

static void
m4_verror_at_line (bool warn, int status, int errnum, const call_info *caller,
		   const char *format, va_list args)
{
  char *full = NULL;
  char *safe_macro = NULL;
  const char *macro = caller ? caller->name : NULL;
  size_t len = caller ? caller->name_len : 0;
  const char *file = caller ? caller->file : current_file;
  int line = caller ? caller->line : current_line;

  /* Sanitize MACRO, since we are turning around and using it in a
     format string.  The allocation is overly conservative, but
     problematic macro names only occur via indir or changeword.  */
  if (macro && memchr (macro, '%', len))
    {
      char *p = safe_macro = xcharalloc (2 * len);
      const char *end = macro + len;
      while (macro != end)
	{
	  if (*macro == '%')
	    {
	      *p++ = '%';
	      len++;
	    }
	  *p++ = *macro++;
	}
    }
  if (macro)
    /* Use slot 1, so that the rest of the code can use the simpler
       quotearg interface in slot 0.  */
    macro = quotearg_n_mem (1, safe_macro ? safe_macro : macro, len);
  /* Prepend warning and the macro name, as needed.  But if that fails
     for non-memory reasons (unlikely), then still use the original
     format.  */
  if (warn && macro)
    full = xasprintf (_("Warning: %s: %s"), macro, format);
  else if (warn)
    full = xasprintf (_("Warning: %s"), format);
  else if (macro)
    full = xasprintf (_("%s: %s"), macro, format);
  verror_at_line (status, errnum, line ? file : NULL, line,
		  full ? full : format, args);
  free (full);
  free (safe_macro);
  if ((!warn || fatal_warnings) && !retcode)
    retcode = EXIT_FAILURE;
}

/*------------------------------------------------------------------.
| Wrapper around error.  Report error message based on FORMAT and   |
| subsequent args, on behalf of CALLER (if any), and the current    |
| input line (if any).  If ERRNUM, decode the errno value that      |
| caused the error.  If STATUS, exit immediately with that status.  |
`------------------------------------------------------------------*/

void
m4_error (int status, int errnum, const call_info *caller,
	  const char *format, ...)
{
  va_list args;
  va_start (args, format);
  if (status == EXIT_SUCCESS && warning_status)
    status = EXIT_FAILURE;
  m4_verror_at_line (false, status, errnum, caller, format, args);
  va_end (args);
}

/*------------------------------------------------------------------.
| Wrapper around error.  Report warning message based on FORMAT and |
| subsequent args, on behalf of CALLER (if any), and the current    |
| input line (if any).  If ERRNUM, decode the errno value that      |
| caused the warning.						    |
`------------------------------------------------------------------*/

void
m4_warn (int errnum, const call_info *caller, const char *format, ...)
{
  va_list args;
  if (!suppress_warnings)
    {
      va_start (args, format);
      m4_verror_at_line (true, warning_status, errnum, caller, format, args);
      va_end (args);
    }
}

#ifdef USE_STACKOVF

/*---------------------------------------.
| Tell user stack overflowed and abort.	 |
`---------------------------------------*/

static void
stackovf_handler (void)
{
  m4_error (EXIT_FAILURE, 0, NULL,
	    _("ERROR: stack overflow.  (Infinite define recursion?)"));
}

#endif /* USE_STACKOV */


/*---------------------------------------------.
| Print a usage message and exit with STATUS.  |
`---------------------------------------------*/

static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    xfprintf (stderr, _("Try `%s --help' for more information.\n"),
	      program_name);
  else
    {
      xprintf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
      fputs (_("\
Process macros in FILEs.  If no FILE or if FILE is `-', standard input\n\
is read.\n\
"), stdout);
      puts ("");
      fputs (_("\
Mandatory or optional arguments to long options are mandatory or optional\n\
for short options too.\n\
\n\
Operation modes:\n\
      --help                   display this help and exit\n\
      --version                output version information and exit\n\
"), stdout);
      xprintf (_("\
  -E, --fatal-warnings         once: warnings become errors, twice: stop\n\
                               execution at first error\n\
  -i, --interactive            unbuffer output, ignore interrupts\n\
  -P, --prefix-builtins        force a `m4_' prefix to all builtins\n\
  -Q, --quiet, --silent        suppress some warnings for builtins\n\
      --warn-macro-sequence[=REGEXP]\n\
                               warn if macro definition matches REGEXP,\n\
                               default %s\n\
"), DEFAULT_MACRO_SEQUENCE);
#ifdef ENABLE_CHANGEWORD
      fputs (_("\
  -W, --word-regexp=REGEXP     use REGEXP for macro name syntax\n\
"), stdout);
#endif
      puts ("");
      fputs (_("\
Preprocessor features:\n\
  -D, --define=NAME[=VALUE]    define NAME as having VALUE, or empty\n\
  -I, --include=DIRECTORY      append DIRECTORY to include path\n\
  -s, --synclines              generate `#line NUM \"FILE\"' lines\n\
  -U, --undefine=NAME          undefine NAME\n\
"), stdout);
      puts ("");
      fputs (_("\
Limits control:\n\
  -G, --traditional            suppress all GNU extensions\n\
  -H, --hashsize=PRIME         set symbol lookup hash table size [509]\n\
  -L, --nesting-limit=NUMBER   change artificial nesting limit [1024]\n\
"), stdout);
      puts ("");
      fputs (_("\
Frozen state files:\n\
  -F, --freeze-state=FILE      produce a frozen state on FILE at end\n\
  -R, --reload-state=FILE      reload a frozen state from FILE at start\n\
"), stdout);
      puts ("");
      fputs (_("\
Debugging:\n\
  -d, --debug[=FLAGS]          set debug level (no FLAGS implies `aeq')\n\
      --debugfile=FILE         redirect debug and trace output\n\
  -l, --arglength=NUM          restrict macro tracing size\n\
  -t, --trace=NAME             trace NAME when it is defined\n\
"), stdout);
      puts ("");
      fputs (_("\
FLAGS is any of:\n\
  a   show actual arguments in trace\n\
  c   show collection line in trace\n\
  e   show expansion in trace\n\
  f   include current input file name in trace and debug\n\
  i   show changes in input files in debug\n\
  l   include current input line number in trace and debug\n\
"), stdout);
      fputs (_("\
  p   show results of path searches in debug\n\
  q   quote values in dumpdef and trace, useful with a or e\n\
  t   trace all macro calls, regardless of per-macro traceon state\n\
  x   include unique macro call id in trace, useful with c\n\
  V   shorthand for all of the above flags\n\
"), stdout);
      puts ("");
      fputs (_("\
If defined, the environment variable `M4PATH' is a colon-separated list\n\
of directories included after any specified by `-I'.\n\
"), stdout);
      puts ("");
      fputs (_("\
Exit status is 0 for success, 1 for failure, 63 for frozen file version\n\
mismatch, or whatever value was passed to the m4exit macro.\n\
"), stdout);
      puts ("");
      /* TRANSLATORS: the placeholder indicates the bug-reporting
	 address for this application.  Please add _another line_
	 saying "Report translation bugs to <...>\n" with the address
	 for translation bugs (typically your translation team's web
	 or email address).  */
      xprintf (_("Report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }
  exit (status);
}

/*--------------------------------------.
| Decode options and launch execution.  |
`--------------------------------------*/

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  DEBUGFILE_OPTION = CHAR_MAX + 1,	/* no short opt */
  DIVERSIONS_OPTION,			/* not quite -N, because of message */
  WARN_MACRO_SEQUENCE_OPTION,		/* no short opt */

  HELP_OPTION,				/* no short opt */
  VERSION_OPTION			/* no short opt */
};

static const struct option long_options[] =
{
  {"arglength", required_argument, NULL, 'l'},
  {"debug", optional_argument, NULL, 'd'},
  {"define", required_argument, NULL, 'D'},
  {"error-output", required_argument, NULL, 'o'}, /* FIXME: deprecate in 2.0 */
  {"fatal-warnings", no_argument, NULL, 'E'},
  {"freeze-state", required_argument, NULL, 'F'},
  {"hashsize", required_argument, NULL, 'H'},
  {"include", required_argument, NULL, 'I'},
  {"interactive", no_argument, NULL, 'i'},
  {"nesting-limit", required_argument, NULL, 'L'},
  {"prefix-builtins", no_argument, NULL, 'P'},
  {"quiet", no_argument, NULL, 'Q'},
  {"reload-state", required_argument, NULL, 'R'},
  {"silent", no_argument, NULL, 'Q'},
  {"synclines", no_argument, NULL, 's'},
  {"trace", required_argument, NULL, 't'},
  {"traditional", no_argument, NULL, 'G'},
  {"undefine", required_argument, NULL, 'U'},
  {"word-regexp", required_argument, NULL, 'W'},

  {"debugfile", required_argument, NULL, DEBUGFILE_OPTION},
  {"diversions", required_argument, NULL, DIVERSIONS_OPTION},
  {"warn-macro-sequence", optional_argument, NULL, WARN_MACRO_SEQUENCE_OPTION},

  {"help", no_argument, NULL, HELP_OPTION},
  {"version", no_argument, NULL, VERSION_OPTION},

  { NULL, 0, NULL, 0 },
};

/* Process a command line file NAME, and return true only if it was
   stdin.  */
static void
process_file (const char *name)
{
  if (strcmp (name, "-") == 0)
    {
      /* If stdin is a terminal, we want to allow 'm4 - file -'
	 to read input from stdin twice, like GNU cat.  Besides,
	 there is no point closing stdin before wrapped text, to
	 minimize bugs in syscmd called from wrapped text.  */
      push_file (stdin, "stdin", false);
    }
  else
    {
      char *full_name;
      FILE *fp = m4_path_search (name, &full_name);
      if (fp == NULL)
	{
	  error (0, errno, "cannot open %s",
		 quotearg_style (locale_quoting_style, name));
	  /* Set the status to EXIT_FAILURE, even though we
	     continue to process files after a missing file.  */
	  retcode = EXIT_FAILURE;
	  return;
	}
      push_file (fp, full_name, true);
      free (full_name);
    }
  expand_input ();
}

/* POSIX requires only -D, -U, and -s; and says that the first two
   must be recognized when interspersed with file names.  Traditional
   behavior also handles -s between files.  Starting OPTSTRING with
   '-' forces getopt_long to hand back file names as arguments to opt
   '\1', rather than reordering the command line.  */
#ifdef ENABLE_CHANGEWORD
#define OPTSTRING "-B:D:EF:GH:I:L:N:PQR:S:T:U:W:d::eil:o:st:"
#else
#define OPTSTRING "-B:D:EF:GH:I:L:N:PQR:S:T:U:d::eil:o:st:"
#endif

#ifdef DEBUG_REGEX
FILE *trace_file;
#endif /* DEBUG_REGEX */

int
main (int argc, char *const *argv, char *const *envp)
{
  macro_definition *head;	/* head of deferred argument list */
  macro_definition *tail;
  macro_definition *defn;
  int optchar;			/* option character */

  macro_definition *defines;
  bool interactive = false;
  bool seen_file = false;
  const char *debugfile = NULL;
  const char *frozen_file_to_read = NULL;
  const char *frozen_file_to_write = NULL;
  const char *macro_sequence = "";

  program_name = argv[0];
  retcode = EXIT_SUCCESS;
  atexit (close_stdin);

#ifdef DEBUG_REGEX
  {
    const char *name = getenv ("M4_TRACE_FILE");
    if (name)
      trace_file = fopen (name, "a");
    if (trace_file)
      fputs ("m4:\n", trace_file);
  }
#endif /* DEBUG_REGEX */

  include_init ();
  debug_init ();
  set_quoting_style (NULL, escape_quoting_style);
  set_char_quoting (NULL, ':', 1);
#ifdef USE_STACKOVF
  setup_stackovf_trap (argv, envp, stackovf_handler);
#endif

  /* First, we decode the arguments, to size up tables and stuff.  */

  head = tail = NULL;

  while ((optchar = getopt_long (argc, (char **) argv, OPTSTRING,
				 long_options, NULL)) != -1)
    switch (optchar)
      {
      default:
	usage (EXIT_FAILURE);

      case 'B':
      case 'S':
      case 'T':
	/* Compatibility junk: options that other implementations
	   support, but which we ignore as no-ops and don't list in
	   --help.  */
	error (0, 0, "Warning: `m4 -%c' may be removed in a future release",
	       optchar);
	break;

      case 'N':
      case DIVERSIONS_OPTION:
	/* -N became an obsolete no-op in 1.4.x.  */
	error (0, 0, "Warning: `m4 %s' is deprecated",
	       optchar == 'N' ? "-N" : "--diversions");

      case 'D':
      case 'U':
      case 's':
      case 't':
      case '\1':
	/* Arguments that cannot be handled until later are accumulated.  */

	defn = (macro_definition *) xmalloc (sizeof (macro_definition));
	defn->code = optchar;
	defn->arg = optarg;
	defn->next = NULL;

	if (head == NULL)
	  head = defn;
	else
	  tail->next = defn;
	tail = defn;

	break;

      case 'E':
	if (!fatal_warnings)
	  fatal_warnings = true;
	else
	  warning_status = EXIT_FAILURE;
	break;

      case 'F':
	frozen_file_to_write = optarg;
	break;

      case 'G':
	no_gnu_extensions = 1;
	break;

      case 'H':
	hash_table_size = atol (optarg);
	if (hash_table_size == 0)
	  hash_table_size = HASHMAX;
	break;

      case 'I':
	add_include_directory (optarg);
	break;

      case 'L':
	nesting_limit = atoi (optarg);
	break;

      case 'P':
	prefix_all_builtins = 1;
	break;

      case 'Q':
	suppress_warnings = 1;
	break;

      case 'R':
	frozen_file_to_read = optarg;
	break;

#ifdef ENABLE_CHANGEWORD
      case 'W':
	user_word_regexp = optarg;
	break;
#endif

      case 'd':
	debug_level = debug_decode (optarg, SIZE_MAX);
	if (debug_level < 0)
	  {
	    error (0, 0, "bad debug flags: %s",
		   quotearg_style (locale_quoting_style, optarg));
	    debug_level = 0;
	  }
	break;

      case 'e':
	error (0, 0, "Warning: `m4 -e' is deprecated, use `-i' instead");
	/* fall through */
      case 'i':
	interactive = true;
	break;

      case 'l':
	{
	  long tmp = strtol (optarg, NULL, 10);
	  max_debug_argument_length = tmp <= 0 ? SIZE_MAX : (size_t) tmp;
	}
	break;

      case 'o':
	/* -o/--error-output are deprecated synonyms of --debugfile,
	   but don't issue a deprecation warning until autoconf 2.61
	   or later is more widely established, as such a warning
	   would interfere with all earlier versions of autoconf.  */
      case DEBUGFILE_OPTION:
	/* Don't call debug_set_output here, as it has side effects.  */
	debugfile = optarg;
	break;

      case WARN_MACRO_SEQUENCE_OPTION:
	 /* Don't call set_macro_sequence here, as it can exit.
	    --warn-macro-sequence sets optarg to NULL (which uses the
	    default regexp); --warn-macro-sequence= sets optarg to ""
	    (which disables these warnings).  */
	macro_sequence = optarg;
	break;

      case VERSION_OPTION:
	version_etc (stdout, PACKAGE, PACKAGE_NAME, VERSION, AUTHORS, NULL);
	exit (EXIT_SUCCESS);
	break;

      case HELP_OPTION:
	usage (EXIT_SUCCESS);
	break;
      }

  defines = head;

  /* Do the basic initializations.  */
  if (debugfile && !debug_set_output (NULL, debugfile))
    m4_error (0, errno, NULL, _("cannot set debug file %s"),
	      quotearg_style (locale_quoting_style, debugfile));

  input_init ();
  output_init ();
  symtab_init ();
  set_macro_sequence (macro_sequence);
  include_env_init ();

  if (frozen_file_to_read)
    reload_frozen_state (frozen_file_to_read);
  else
    builtin_init ();

  /* Interactive mode means unbuffered output, and interrupts ignored.  */

  if (interactive)
    {
      signal (SIGINT, SIG_IGN);
      setbuf (stdout, NULL);
    }

  /* Handle deferred command line macro definitions.  Must come after
     initialization of the symbol table.  */

  while (defines != NULL)
    {
      macro_definition *next;
      symbol *sym;

      switch (defines->code)
	{
	case 'D':
	  {
	    const char *value = strchr (defines->arg, '=');
	    size_t len = value ? value - defines->arg : strlen (defines->arg);
	    define_user_macro (defines->arg, len, value ? value + 1 : "",
			       value ? SIZE_MAX : 0, SYMBOL_INSERT);
	  }
	  break;

	case 'U':
	  lookup_symbol (defines->arg, strlen (defines->arg), SYMBOL_DELETE);
	  break;

	case 't':
	  sym = lookup_symbol (defines->arg, strlen (defines->arg),
			       SYMBOL_INSERT);
	  SYMBOL_TRACED (sym) = true;
	  break;

	case 's':
	  sync_output = 1;
	  break;

	case '\1':
	  seen_file = true;
	  process_file (defines->arg);
	  break;

	default:
	  assert (!"main");
	  abort ();
	}

      next = defines->next;
      free (defines);
      defines = next;
    }

  /* Handle remaining input files.  Each file is pushed on the input,
     and the input read.  Wrapup text is handled separately later.  */

  if (optind == argc && !seen_file)
    process_file ("-");
  else
    for (; optind < argc; optind++)
      process_file (argv[optind]);

  /* Now handle wrapup text.  */

  while (pop_wrapup ())
    expand_input ();

  /* Change debug stream back to stderr, to force flushing the debug
     stream and detect any errors it might have encountered.  The
     three standard streams are closed by close_stdin.  */
  debug_set_output (NULL, NULL);

  if (frozen_file_to_write)
    produce_frozen_state (frozen_file_to_write);
  else
    {
      make_diversion (0);
      undivert_all ();
    }
  output_exit ();
  free_regex ();
  quotearg_free ();
#ifdef DEBUG_REGEX
  if (trace_file)
    fclose (trace_file);
#endif /* DEBUG_REGEX */
  exit (retcode);
}

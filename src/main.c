/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1999, 2000, 2003,
   2004, 2005, 2006 Free Software Foundation, Inc.

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

#include "m4.h"
#include "m4private.h"

#include "closeout.h"
#include "configmake.h"
#include "getopt.h"
#include "version-etc.h"
#include "xstrtol.h"

#include <limits.h>

#define AUTHORS _("Rene' Seindal"), "Gary V. Vaughan", "Eric Blake"

typedef struct macro_definition
{
  struct macro_definition *next;
  int code;			/* deferred optchar */
  const char *macro;
} macro_definition;


/* Error handling functions.  */

#ifdef USE_STACKOVF

/* Tell user stack overflowed and abort.  */
static void
stackovf_handler (void)
{
  /* FIXME - calling gettext and error inside a signal handler is dangerous,
     since these functions invoke functions that are not signal-safe.  We
     are sort of justified by the fact that we will exit and never return,
     but this should really be fixed.  */
  error (EXIT_FAILURE, 0, _("stack overflow (infinite define recursion?)"));
}

#endif /* USE_STACKOV */



/* Print a usage message and exit with STATUS.  */
static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     m4_get_program_name ());
  else
    {
      printf (_("Usage: %s [OPTION]... [FILE]...\n"), m4_get_program_name ());
      fputs (_("\
Process macros in FILEs.\n\
If no FILE or if FILE is `-', standard input is read.  With no FILE and both\n\
standard input and standard output are terminals, -e is implied.\n\
"), stdout);
      fputs (_("\
\n\
Mandatory or optional arguments to long options are mandatory or optional\n\
for short options too.\n\
\n\
Operation modes:\n\
      --help                   display this help and exit\n\
      --version                output version information and exit\n\
"), stdout);
      fputs (_("\
  -b, --batch                  buffer output, process interrupts\n\
  -c, --discard-comments       do not copy comments to the output\n\
  -E, --fatal-warnings         stop execution after first warning\n\
  -i, --interactive            unbuffer output, ignore interrupts\n\
  -P, --prefix-builtins        force a `m4_' prefix to all builtins\n\
  -Q, --quiet, --silent        suppress some warnings for builtins\n\
  -r, --regexp-syntax=SPEC     change the default regexp syntax\n\
      --safer                  disable potentially unsafe builtins\n\
"), stdout);
      fputs (_("\
\n\
SPEC is any one of:\n\
  AWK, BASIC, BSD_M4, ED, EMACS, EXTENDED, GNU_AWK, GNU_EGREP, GNU_M4,\n\
  GREP, POSIX_AWK, POSIX_EGREP, MINIMAL, MINIMAL_BASIC, SED.\n\
"), stdout);
      printf (_("\
\n\
Dynamic loading features:\n\
  -M, --module-directory=DIR   add DIR to module search path before\n\
                               `%s'\n\
  -m, --load-module=MODULE     load dynamic MODULE\n\
"), PKGLIBEXECDIR);
      fputs (_("\
\n\
Preprocessor features:\n\
      --import-environment     import all environment variables as macros\n\
  -B, --prepend-include=DIR    add DIR to include path before `.'\n\
  -D, --define=NAME[=VALUE]    define NAME as having VALUE, or empty\n\
  -I, --include=DIR            add DIR to include path after `.'\n\
  -s, --synclines              generate `#line NUM \"FILE\"' lines\n\
  -U, --undefine=NAME          undefine NAME\n\
"), stdout);
      fputs (_("\
\n\
Limits control:\n\
  -G, --traditional            suppress all GNU extensions\n\
  -L, --nesting-limit=NUMBER   change artificial nesting limit [1024]\n\
"), stdout);
      fputs (_("\
\n\
Frozen state files:\n\
  -F, --freeze-state=FILE      produce a frozen state on FILE at end\n\
  -R, --reload-state=FILE      reload a frozen state from FILE at start\n\
"), stdout);
      fputs (_("\
\n\
Debugging:\n\
  -d, --debug[=FLAGS], --debugmode[=FLAGS]\n\
                               set debug level (no FLAGS implies `aeq')\n\
      --debugfile=FILE         redirect debug and trace output\n\
  -l, --debuglen=NUM           restrict macro tracing size\n\
  -t, --trace=NAME             trace NAME when it is defined\n\
"), stdout);
      fputs (_("\
\n\
FLAGS is any of:\n\
  a   show actual arguments in trace\n\
  c   show definition line in trace\n\
  e   show expansion in trace\n\
  f   include current input file name in trace and debug\n\
  i   show changes in input files in debug\n\
  l   include current input line number in trace and debug\n\
"), stdout);
      fputs (_("\
  m   show module information in trace, debug, and dumpdef\n\
  p   show results of path searches in debug\n\
  q   quote values as necessary in dumpdef and trace, useful with a or e\n\
  s   show full stack of pushdef values in dumpdef\n\
  t   trace all macro calls, regardless of named traceon state\n\
  x   include unique macro call id in trace, useful with c\n\
  V   shorthand for all of the above flags\n\
"), stdout);
      fputs (_("\
\n\
If defined, the environment variable `M4PATH' is a colon-separated list\n\
of directories included after any specified by `-I', and the variable\n\
`M4MODPATH' is a colon-separated list of directories searched before any\n\
specified by `-M'.\n\
"), stdout);
      fputs (_("\
\n\
Exit status is 0 for success, 1 for failure, 63 for frozen file version\n\
mismatch, or whatever value was passed to the m4exit macro.\n\
"), stdout);
      printf (_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }
  exit (status);
}

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  ARGLENGTH_OPTION = CHAR_MAX + 1,	/* not quite -l, because of message */
  DEBUGFILE_OPTION,			/* no short opt */
  DIVERSIONS_OPTION,			/* not quite -N, because of message */
  ERROR_OUTPUT_OPTION,			/* not quite -o, because of message */
  HASHSIZE_OPTION,			/* not quite -H, because of message */
  IMPORT_ENVIRONMENT_OPTION,		/* no short opt */
  PREPEND_INCLUDE_OPTION,		/* not quite -B, because of message */
  SAFER_OPTION,				/* -S still has old no-op semantics */

  HELP_OPTION,				/* no short opt */
  VERSION_OPTION			/* no short opt */
};

/* Decode options and launch execution.  */
static const struct option long_options[] =
{
  {"batch", no_argument, NULL, 'b'},
  {"debug", optional_argument, NULL, 'd'},
  {"debuglen", required_argument, NULL, 'l'},
  {"debugmode", optional_argument, NULL, 'd'},
  {"define", required_argument, NULL, 'D'},
  {"discard-comments", no_argument, NULL, 'c'},
  {"fatal-warnings", no_argument, NULL, 'E'},
  {"freeze-state", required_argument, NULL, 'F'},
  {"include", required_argument, NULL, 'I'},
  {"interactive", no_argument, NULL, 'i'},
  {"load-module", required_argument, NULL, 'm'},
  {"module-directory", required_argument, NULL, 'M'},
  {"nesting-limit", required_argument, NULL, 'L'},
  {"prefix-builtins", no_argument, NULL, 'P'},
  {"quiet", no_argument, NULL, 'Q'},
  {"regexp-syntax", required_argument, NULL, 'r'},
  {"reload-state", required_argument, NULL, 'R'},
  {"silent", no_argument, NULL, 'Q'},
  {"synclines", no_argument, NULL, 's'},
  {"trace", required_argument, NULL, 't'},
  {"traditional", no_argument, NULL, 'G'},
  {"undefine", required_argument, NULL, 'U'},
  {"word-regexp", required_argument, NULL, 'W'},

  {"arglength", required_argument, NULL, ARGLENGTH_OPTION},
  {"debugfile", required_argument, NULL, DEBUGFILE_OPTION},
  {"diversions", required_argument, NULL, DIVERSIONS_OPTION},
  {"hashsize", required_argument, NULL, HASHSIZE_OPTION},
  {"error-output", required_argument, NULL, ERROR_OUTPUT_OPTION},
  {"import-environment", no_argument, NULL, IMPORT_ENVIRONMENT_OPTION},
  {"prepend-include", required_argument, NULL, PREPEND_INCLUDE_OPTION},
  {"safer", no_argument, NULL, SAFER_OPTION},

  {"help", no_argument, NULL, HELP_OPTION},
  {"version", no_argument, NULL, VERSION_OPTION},

  { NULL, 0, NULL, 0 },
};

/* POSIX requires only -D, -U, and -s; and says that the first two
   must be recognized when interspersed with file names.  Traditional
   behavior also handles -s between files.  Starting OPTSTRING with
   '-' forces getopt_long to hand back file names as arguments to opt
   '\1', rather than reordering the command line.  */
#define OPTSTRING "-B:D:EF:GH:I:L:M:N:PQR:S:T:U:bcd::eil:m:o:r:st:"

/* For determining whether to be interactive.  */
enum interactive_choice
{
  INTERACTIVE_UNKNOWN,	/* Still processing arguments, no -b or -i yet */
  INTERACTIVE_YES,	/* -i specified last */
  INTERACTIVE_NO	/* -b specified last */
};

/* Convert OPT to size_t, reporting an error using MSGID if it does
   not fit.  */
static size_t
size_opt (char const *opt, char const *msgid)
{
  unsigned long int size;
  strtol_error status = xstrtoul (opt, NULL, 10, &size, "kKmMgGtTPEZY0");
  if (SIZE_MAX < size && status == LONGINT_OK)
    status = LONGINT_OVERFLOW;
  if (status != LONGINT_OK)
    STRTOL_FATAL_ERROR (opt, _(msgid), status);
  return size;
}

/* Process a command line file NAME, and return true only if it was
   stdin.  */
static bool
process_file (m4 *context, const char *name)
{
  bool result = false;
  if (strcmp (name, "-") == 0)
    {
      m4_push_file (context, stdin, "stdin", false);
      result = true;
    }
  else
    {
      char *full_name;
      FILE *fp = m4_path_search (context, name, &full_name);
      if (fp == NULL)
	{
	  m4_error (context, 0, errno, _("cannot open file `%s'"), name);
	  return false;
	}
      m4_push_file (context, fp, full_name, true);
      free (full_name);
    }
  m4_macro_expand_input (context);
  return result;
}


/* Main entry point.  Parse arguments, load modules, then parse input.  */
int
main (int argc, char *const *argv, char *const *envp)
{
  macro_definition *head = NULL;	/* head of deferred argument list */
  macro_definition *tail = NULL;
  macro_definition *defn;
  int optchar;			/* option character */
  size_t size;			/* for parsing numeric option arguments */

  macro_definition *defines;
  bool read_stdin = false;	/* true iff we have read from stdin */
  bool import_environment = false; /* true to import environment */
  bool seen_file = false;
  const char *debugfile = NULL;
  const char *frozen_file_to_read = NULL;
  const char *frozen_file_to_write = NULL;
  enum interactive_choice interactive = INTERACTIVE_UNKNOWN;

  m4 *context;

  int exit_status;

  /* Initialize gnulib error module.  */
  m4_set_program_name (argv[0]);
  atexit (close_stdout);

  setlocale (LC_ALL, "");
#ifdef ENABLE_NLS
  textdomain (PACKAGE);
#endif

  LTDL_SET_PRELOADED_SYMBOLS ();

  context = m4_create ();

  m4__module_init (context);

#ifdef USE_STACKOVF
  setup_stackovf_trap (argv, envp, stackovf_handler);
#endif

  if (getenv ("POSIXLY_CORRECT"))
    m4_set_posixly_correct_opt (context, true);

  /* First, we decode the arguments, to size up tables and stuff.
     Avoid lasting side effects; for example 'm4 --debugfile=oops
     --help' must not create the file `oops'.  */
  while ((optchar = getopt_long (argc, (char **) argv, OPTSTRING,
				 long_options, NULL)) != -1)
    switch (optchar)
      {
      default:
	usage (EXIT_FAILURE);

      case 'H':
      case HASHSIZE_OPTION:
	/* -H was supported in 1.4.x, but is a no-op now.  FIXME -
	    remove support for -H after 2.0.  */
	error (0, 0, _("Warning: `%s' is deprecated"),
	       optchar == 'H' ? "-H" : "--hashsize");
	break;

      case 'N':
      case DIVERSIONS_OPTION:
	/* -N became an obsolete no-op in 1.4.x.  FIXME - remove
	   support for -N after 2.0.  */
	error (0, 0, _("Warning: `%s' is deprecated"),
	       optchar == 'N' ? "-N" : "--diversions");
	break;

      case 'S':
      case 'T':
	/* Compatibility junk: options that other implementations
	   support, but which we ignore as no-ops and don't list in
	   --help.  */
	error (0, 0, _("Warning: `-%c' is deprecated"),
	       optchar);
	break;

      case 'D':
      case 'U':
      case 'm':
      case 'r':
      case 's':
      case 't':
      case '\1':
	/* Arguments that cannot be handled until later are accumulated.  */

	defn = xmalloc (sizeof *defn);
	defn->code = optchar;
	defn->macro = optarg;
	defn->next = NULL;

	if (head == NULL)
	  head = defn;
	else
	  tail->next = defn;
	tail = defn;

	break;

      case 'B':
	/* In 1.4.x, -B<num> was a no-op option for compatibility with
	   Solaris m4.  Warn if optarg is all numeric.  FIXME -
	   silence this warning after 2.0.  */
	if (isdigit ((unsigned char) *optarg))
	  {
	    char *end;
	    errno = 0;
	    strtol (optarg, &end, 10);
	    if (*end == '\0' && errno == 0)
	      error (0, 0, _("Warning: recommend using `-B ./%s' instead"),
		     optarg);
	  }
	/* fall through */
      case PREPEND_INCLUDE_OPTION:
	m4_add_include_directory (context, optarg, true);
	break;

      case 'E':
	m4_set_fatal_warnings_opt (context, true);
	break;

      case 'F':
	frozen_file_to_write = optarg;
	break;

      case 'G':
	m4_set_no_gnu_extensions_opt (context, true);
	m4_set_posixly_correct_opt (context, true);
	break;

      case 'I':
	m4_add_include_directory (context, optarg, false);
	break;

      case 'L':
	size = size_opt (optarg, N_("nesting limit"));
	m4_set_nesting_limit_opt (context, size);
	break;

      case 'M':
	if (lt_dlinsertsearchdir (lt_dlgetsearchpath (), optarg) != 0)
	  {
	    const char *dlerr = lt_dlerror ();
	    if (dlerr == NULL)
	      m4_error (context, EXIT_FAILURE, 0,
			_("failed to add search directory `%s'"),
			optarg);
	    else
	      m4_error (context, EXIT_FAILURE, 0,
			_("failed to add search directory `%s': %s"),
			optarg, dlerr);
	  }
	break;

      case 'P':
	m4_set_prefix_builtins_opt (context, true);
	break;

      case 'Q':
	m4_set_suppress_warnings_opt (context, true);
	break;

      case 'R':
	frozen_file_to_read = optarg;
	break;

      case 'b':
	interactive = INTERACTIVE_NO;
	break;

      case 'c':
	m4_set_discard_comments_opt (context, true);
	break;

      case 'd':
	{
	  int old = m4_get_debug_level_opt (context);
	  m4_set_debug_level_opt (context, m4_debug_decode (context, old,
							    optarg));
	}
	if (m4_get_debug_level_opt (context) < 0)
	  {
	    error (0, 0, _("bad debug flags: `%s'"), optarg);
	    m4_set_debug_level_opt (context, 0);
	  }
	break;

      case 'e':
	error (0, 0, _("Warning: `%s' is deprecated, use `%s' instead"),
	       "-e", "-i");
	/* fall through */
      case 'i':
	interactive = INTERACTIVE_YES;
	break;

      case ARGLENGTH_OPTION:
	error (0, 0, _("Warning: `%s' is deprecated, use `%s' instead"),
	       "--arglength", "--debuglen");
	/* fall through */
      case 'l':
	size = size_opt (optarg,
			 N_("debug argument length"));
	m4_set_max_debug_arg_length_opt (context, size);
	break;

      case 'o':
      case ERROR_OUTPUT_OPTION:
	/* FIXME: -o is inconsistent with other tools' use of
	   -o/--output for creating an output file instead of using
	   stdout, and --error-output is misnamed since it does not
	   affect error messages to stderr.  Change the meaning of -o
	   after 2.1.  */
	error (0, 0, _("Warning: `%s' is deprecated, use `%s' instead"),
	       optchar == 'o' ? "-o" : "--error-output", "--debugfile");
	/* fall through */
      case DEBUGFILE_OPTION:
	/* Don't call m4_debug_set_output here, as it has side effects.  */
	debugfile = optarg;
	break;

      case IMPORT_ENVIRONMENT_OPTION:
	import_environment = true;
	break;

      case SAFER_OPTION:
	m4_set_safer_opt (context, true);
	break;

      case VERSION_OPTION:
	version_etc (stdout, PACKAGE, PACKAGE_NAME TIMESTAMP,
		     VERSION, AUTHORS, NULL);
	exit (EXIT_SUCCESS);
	break;

      case HELP_OPTION:
	usage (EXIT_SUCCESS);
	break;
      }

  /* Interactive if specified, or if no input files and stdin and
     stderr are terminals, to match sh behavior.  Interactive mode
     means unbuffered output, and interrupts ignored.  */

  m4_set_interactive_opt (context, (interactive == INTERACTIVE_YES
				    || (interactive == INTERACTIVE_UNKNOWN
					&& optind == argc
					&& isatty (STDIN_FILENO)
					&& isatty (STDERR_FILENO))));
  if (m4_get_interactive_opt (context))
    {
      signal (SIGINT, SIG_IGN);
      setbuf (stdout, NULL);
    }


  /* Do the basic initializations.  */
  if (debugfile && !m4_debug_set_output (context, debugfile))
    m4_error (context, 0, errno, _("cannot set debug file `%s'"), debugfile);
  m4_input_init (context);
  m4_output_init (context);
  m4_include_env_init (context);

  if (frozen_file_to_read)
    reload_frozen_state (context, frozen_file_to_read);
  else
    {
      m4_module_load (context, "m4", 0);
      if (m4_get_no_gnu_extensions_opt (context))
	m4_module_load (context, "traditional", 0);
      else
	m4_module_load (context, "gnu", 0);
    }

  /* Import environment variables as macros.  The definition are
     prepended to the macro definition list, so -U can override
     environment variables. */

  if (import_environment)
    {
      char *const *env;

      for (env = envp; *env != NULL; env++)
	{
	  defn = xmalloc (sizeof *defn);
	  defn->code = 'D';
	  defn->macro = *env;
	  defn->next = head;
	  head = defn;
	}
    }

  /* Handle deferred command line macro definitions.  Must come after
     initialization of the symbol table.  */
  {
    defines = head;

    while (defines != NULL)
      {
	macro_definition *next;
	const char *arg = defines->macro;

	switch (defines->code)
	  {
	  case 'D':
	    {
	      m4_symbol_value *value = m4_symbol_value_create ();

	      /* defines->arg is read-only, so we need a copy.	*/
	      char *macro_name = xstrdup (arg);
	      char *macro_value = strchr (macro_name, '=');

	      if (macro_value != NULL)
		*macro_value++ = '\0';
	      m4_set_symbol_value_text (value, xstrdup (macro_value
							? macro_value : ""));

	      m4_symbol_define (M4SYMTAB, macro_name, value);
	      free (macro_name);
	    }
	    break;

	  case 'U':
	    m4_symbol_delete (M4SYMTAB, arg);
	    break;

	  case 'm':
	    m4_module_load (context, arg, 0);
	    break;

	  case 'r':
	    m4_set_regexp_syntax_opt (context,
				      m4_regexp_syntax_encode (arg));
	    if (m4_get_regexp_syntax_opt (context) < 0)
	      {
		m4_error (context, EXIT_FAILURE, 0,
			  _("bad regexp syntax option: `%s'"), arg);
	      }
	    break;

	  case 's':
	    m4_set_sync_output_opt (context, true);
	    break;

	  case 't':
	    m4_set_symbol_name_traced (M4SYMTAB, arg, true);
	    break;

	  case '\1':
	    seen_file = true;
	    read_stdin |= process_file (context, arg);
	    break;

	  default:
	    assert (!"INTERNAL ERROR: bad code in deferred arguments");
	    abort ();
	  }

	next = defines->next;
	free (defines);
	defines = next;
      }
  }

  /* Handle remaining input files.  Each file is pushed on the input,
     and the input read.  */

  if (optind == argc && !seen_file)
    read_stdin = process_file (context, "-");
  else
    for (; optind < argc; optind++)
      read_stdin |= process_file (context, argv[optind]);

  /* Now handle wrapup text.
     FIXME - when -F is in effect, should wrapped text be frozen?  */
  while (m4_pop_wrapup (context))
    m4_macro_expand_input (context);

  if (frozen_file_to_write)
    produce_frozen_state (context, frozen_file_to_write);
  else
    {
      m4_make_diversion (context, 0);
      m4_undivert_all (context);
    }

  /* The remaining cleanup functions systematically free all of the
     memory we still have pointers to.  By definition, if there is
     anything left when we're done: it was caused by a memory leak.
     Strictly, we don't need to do this, but it makes leak detection
     a whole lot easier!  */

  m4__module_exit (context);
  m4_output_exit ();
  m4_input_exit ();

  /* Change debug stream back to stderr, to force flushing the debug
     stream and detect any errors it might have encountered.  Close
     stdin if we read from it, to detect any errors.  */
  m4_debug_set_output (context, NULL);
  if (read_stdin && fclose (stdin) == EOF)
    m4_error (context, 0, errno, _("error closing stdin"));

  exit_status = m4_get_exit_status (context);
  m4_delete (context);

  m4_hash_exit ();

#ifdef USE_STACKOVF
  stackovf_exit ();
#endif

  exit (exit_status);
}

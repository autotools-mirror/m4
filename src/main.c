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
#include "getopt.h"
#include "version-etc.h"
#include "gnu/progname.h"
#include "pathconf.h"

#include <limits.h>

#define AUTHORS _("Rene' Seindal"), "Gary V. Vaughan"

typedef struct macro_definition
{
  struct macro_definition *next;
  int code;			/* D, U or t */
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
	     program_name);
  else
    {
      printf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
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
  -e, --interactive            unbuffer output, ignore interrupts\n\
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
"), MODULE_PATH);
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
  -d, --debug[=FLAGS]          set debug level (no FLAGS implies `aeq')\n\
      --debugfile=FILE         redirect debug and trace output\n\
  -l, --arglength=NUM          restrict macro tracing size\n\
  -t, --trace=NAME             trace NAME when it is defined\n\
"), stdout);
      fputs (_("\
\n\
FLAGS is any of:\n\
  a   show actual arguments\n\
  c   show before collect, after collect and after call\n\
  e   show expansion\n\
  f   say current input file name\n\
  i   show changes in input files\n\
  l   say current input line number\n\
"), stdout);
      fputs (_("\
  m   show actions related to modules\n\
  p   show results of path searches\n\
  q   quote values as necessary, with a or e flag\n\
  t   trace for all macro calls, not only traceon'ed\n\
  x   add a unique macro call id, useful with c flag\n\
  V   shorthand for all of the above flags\n\
"), stdout);
      fputs (_("\
\n\
If defined, the environment variable `M4PATH' is a colon-separated list\n\
of directories included after any specified by `-I', and the variable\n\
`M4MODPATH' is a colon-separated list of directories searched after any\n\
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
  DEBUGFILE_OPTION = CHAR_MAX + 1,	/* no short opt */
  DIVERSIONS_OPTION,			/* not quite -N, because of message */
  ERROR_OUTPUT_OPTION,			/* not quite -o, because of message */
  IMPORT_ENVIRONMENT_OPTION,		/* no short opt */
  PREPEND_INCLUDE_OPTION,		/* not quite -B, because of message */
  SAFER_OPTION,				/* -S still has old no-op semantics */

  HELP_OPTION,				/* no short opt */
  VERSION_OPTION			/* no short opt */
};

/* Decode options and launch execution.  */
static const struct option long_options[] =
{
  {"arglength", required_argument, NULL, 'l'},
  {"batch", no_argument, NULL, 'b'},
  {"debug", optional_argument, NULL, 'd'},
  {"define", required_argument, NULL, 'D'},
  {"discard-comments", no_argument, NULL, 'c'},
  {"fatal-warnings", no_argument, NULL, 'E'},
  {"freeze-state", required_argument, NULL, 'F'},
  {"hashsize", required_argument, NULL, 'H'},
  {"include", required_argument, NULL, 'I'},
  {"interactive", no_argument, NULL, 'e'},
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

  {"debugfile", required_argument, NULL, DEBUGFILE_OPTION},
  {"diversions", required_argument, NULL, DIVERSIONS_OPTION},
  {"error-output", required_argument, NULL, ERROR_OUTPUT_OPTION},
  {"import-environment", no_argument, NULL, IMPORT_ENVIRONMENT_OPTION},
  {"prepend-include", required_argument, NULL, PREPEND_INCLUDE_OPTION},
  {"safer", no_argument, NULL, SAFER_OPTION},

  {"help", no_argument, NULL, HELP_OPTION},
  {"version", no_argument, NULL, VERSION_OPTION},

  { NULL, 0, NULL, 0 },
};

#define OPTSTRING "B:D:EF:GH:I:L:M:N:PQR:S:T:U:bcd::el:m:o:r:st:"

/* For determining whether to be interactive.  */
enum interactive_choice
{
  INTERACTIVE_UNKNOWN,	/* Still processing arguments, no -b or -e yet */
  INTERACTIVE_YES,	/* -e specified last */
  INTERACTIVE_NO	/* -b specified last */
};

int
main (int argc, char *const *argv, char *const *envp)
{
  macro_definition *head;	/* head of deferred argument list */
  macro_definition *tail;
  macro_definition *defn;
  int optchar;			/* option character */

  macro_definition *defines;
  FILE *fp;
  char *filename;
  bool read_stdin = false;	/* true iff we have read from stdin */
  bool import_environment = false; /* true to import environment */
  const char *frozen_file_to_read = NULL;
  const char *frozen_file_to_write = NULL;
  enum interactive_choice interactive = INTERACTIVE_UNKNOWN;

  m4 *context;

  int exit_status;

  /* Initialize gnulib error module.  */
  set_program_name (argv[0]);

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

  /* First, we decode the arguments, to size up tables and stuff.  */

  head = tail = NULL;

  while ((optchar = getopt_long (argc, (char **) argv, OPTSTRING,
				 long_options, NULL)) != -1)
    switch (optchar)
      {
      default:
	usage (EXIT_FAILURE);

      case 'H':
	/* -H was supported in 1.4.x.  FIXME - make obsolete after
	   2.0, and remove after 2.1.  For now, keep it silent.  */
	break;

      case 'N':
      case DIVERSIONS_OPTION:
	/* -N became an obsolete no-op in 1.4.x.  FIXME - remove
	   support for -N after 2.0.  */
	error (0, 0, _("Warning: `m4 %s' is deprecated"),
	       optchar == 'N' ? "-N" : "--diversions");
	break;

      case 'S':
      case 'T':
	/* Compatibility junk: options that other implementations
	   support, but which we ignore as no-ops and don't list in
	   --help.  */
	error (0, 0, _("Warning: `m4 -%c' may be removed in a future release"),
	       optchar);
	break;

      case 'D':
      case 'U':
      case 't':
      case 'm':
      case 'r':
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
	      error (0, 0, _("Warning: recommend using `m4 -B ./%s' instead"),
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
	m4_set_nesting_limit_opt (context, atoi (optarg));
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
	interactive = INTERACTIVE_YES;
	break;

      case 'l':
	m4_set_max_debug_arg_length_opt (context, atoi (optarg));
	if (m4_get_max_debug_arg_length_opt (context) <= 0)
	  m4_set_max_debug_arg_length_opt (context, 0);
	break;

      case 'o':
      case ERROR_OUTPUT_OPTION:
	/* FIXME: -o is inconsistent with other tools' use of
	   -o/--output for creating an output file instead of using
	   stdout, and --error-output is misnamed since it does not
	   affect error messages to stderr.  Change the meaning of -o
	   after 2.1.  */
	error (0, 0, _("Warning: %s is deprecated, use --debugfile instead"),
	       optchar == 'o' ? "-o" : "--error-output");
	/* fall through */
      case DEBUGFILE_OPTION:
	if (!m4_debug_set_output (context, optarg))
	  error (0, errno, "%s", optarg);
	break;

      case 's':
	m4_set_sync_output_opt (context, true);
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
     stderr are terminals, to match sh behavior.  */

  m4_set_interactive_opt (context, (interactive == INTERACTIVE_YES
				    || (interactive == INTERACTIVE_UNKNOWN
					&& optind == argc
					&& isatty (STDIN_FILENO)
					&& isatty (STDERR_FILENO))));

  /* Do the basic initializations.  */

  m4_input_init (context);
  m4_output_init ();
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
	char *macro_value;
	const char *arg = defines->macro;

	switch (defines->code)
	  {
	  case 'D':
	    {
	      m4_symbol_value *value = m4_symbol_value_create ();

	      macro_value = strchr (arg, '=');
	      if (macro_value == NULL)
		macro_value = "";
	      else
		*macro_value++ = '\0';
	      m4_set_symbol_value_text (value, xstrdup (macro_value));

	      m4_symbol_pushdef (M4SYMTAB, arg, value);
	    }
	    break;

	  case 'U':
	    m4_symbol_delete (M4SYMTAB, arg);
	    break;

	  case 't':
	    m4_set_symbol_name_traced (M4SYMTAB, arg, true);
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

	  default:
	    assert (!"INTERNAL ERROR: bad code in deferred arguments");
	    abort ();
	  }

	next = defines->next;
	free (defines);
	defines = next;
      }
  }

  /* Interactive mode means unbuffered output, and interrupts ignored.  */

  if (m4_get_interactive_opt (context))
    {
      signal (SIGINT, SIG_IGN);
      setbuf (stdout, (char *) NULL);
    }

  /* Handle the various input files.  Each file is pushed on the input,
     and the input read.  Wrapup text is handled separately later.  */

  exit_status = EXIT_SUCCESS;
  if (optind == argc)
    {
      m4_push_file (context, stdin, "stdin", false);
      read_stdin = true;
      m4_macro_expand_input (context);
    }
  else
    for (; optind < argc; optind++)
      {
	if (strcmp (argv[optind], "-") == 0)
	  {
	    m4_push_file (context, stdin, "stdin", false);
	    read_stdin = true;
	  }
	else
	  {
	    fp = m4_path_search (context, argv[optind], &filename);
	    if (fp == NULL)
	      {
		error (0, errno, "%s", argv[optind]);
		exit_status = EXIT_FAILURE;
		continue;
	      }
	    else
	      {
		m4_push_file (context, fp, filename, true);
		free (filename);
	      }
	  }
	m4_macro_expand_input (context);
      }

  /* Now handle wrapup text.  */
  while (m4_pop_wrapup ())
    m4_macro_expand_input (context);

  /* Change debug stream back to stderr, to force flushing the debug
     stream and detect any errors it might have encountered.  Close
     stdin if we read from it, to detect any errors.  */
  m4_debug_set_output (context, NULL);
  if (read_stdin && fclose (stdin) == EOF)
    m4_error (context, 0, errno, _("error closing stdin"));

  if (frozen_file_to_write)
    produce_frozen_state (context, frozen_file_to_write);
  else
    {
      m4_make_diversion (0);
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

  if (exit_status == EXIT_SUCCESS)
    exit_status = m4_get_exit_status (context);
  m4_delete (context);

  m4_hash_exit ();

#ifdef USE_STACKOVF
  stackovf_exit ();
#endif

  exit (exit_status);
}

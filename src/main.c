/* GNU m4 -- A simple macro processor

   Copyright (C) 1989-1994, 1999, 2000, 2003, 2004, 2005, 2006
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

#include "m4.h"
#include "m4private.h"
#include "getopt.h"
#include "version-etc.h"
#include "gnu/progname.h"

#define AUTHORS _("Rene' Seindal"), "Gary V. Vaughan"


/* Name of frozen file to digest after initialization.  */
const char *frozen_file_to_read = NULL;

/* Name of frozen file to produce near completion.  */
const char *frozen_file_to_write = NULL;

/* If nonzero, display usage information and exit.  */
static int show_help = 0;

/* If nonzero, print the version on standard output and exit.  */
static int show_version = 0;

/* If nonzero, import the environment as macros.  */
static int import_environment = 0;

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
If no FILE or if FILE is `-', standard input is read.\n\
"), stdout);
      fputs (_("\
\n\
Mandatory or optional arguments to long options are mandatory or optional\n\
for short options too.\n\
\n\
Operation modes:\n\
      --help                   display this help and exit\n\
      --version                output version information and exit\n\
  -b, --batch                  buffer output, process interrupts\n\
  -c, --discard-comments       do not copy comments to the output\n\
  -E, --fatal-warnings         stop execution after first warning\n\
  -e, --interactive            unbuffer output, ignore interrupts\n\
  -P, --prefix-builtins        force a `m4_' prefix to all builtins\n\
  -Q, --quiet, --silent        suppress some warnings for builtins\n\
  -r, --regexp-syntax=SPEC     change the default regexp syntax\n\
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
  -M, --module-directory=DIRECTORY  add DIRECTORY to the module search path\n\
  -m, --load-module=MODULE          load dynamic MODULE from %s\n\
"), USER_MODULE_PATH_ENV);
      fputs (_("\
\n\
Preprocessor features:\n\
      --import-environment     import all environment variables as macros\n\
  -D, --define=NAME[=VALUE]    define NAME has having VALUE, or empty\n\
  -I, --include=DIRECTORY      append DIRECTORY to include path\n\
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
  -l, --arglength=NUM          restrict macro tracing size\n\
  -o, --error-output=FILE      redirect debug and trace output\n\
  -t, --trace=NAME             trace NAME when it will be defined\n\
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
  p   show results of path searches\n\
  q   quote values as necessary, with a or e flag\n\
  t   trace for all macro calls, not only traceon'ed\n\
  x   add a unique macro call id, useful with c flag\n\
  V   shorthand for all of the above flags\n\
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

/* Decode options and launch execution.  */
static const struct option long_options[] =
{
  {"arglength", required_argument, NULL, 'l'},
  {"batch", no_argument, NULL, 'b'},
  {"debug", optional_argument, NULL, 'd'},
  {"discard-comments", no_argument, NULL, 'c'},
  {"diversions", required_argument, NULL, 'N'},
  {"error-output", required_argument, NULL, 'o'},
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
  {"traditional", no_argument, NULL, 'G'},
  {"word-regexp", required_argument, NULL, 'W'},

  {"import-environment", no_argument, &import_environment, 1},

  {"help", no_argument, &show_help, 1},
  {"version", no_argument, &show_version, 1},

  /* These are somewhat troublesome.  */
  { "define", required_argument, NULL, 'D' },
  { "undefine", required_argument, NULL, 'U' },
  { "trace", required_argument, NULL, 't' },

  { 0, 0, 0, 0 },
};

#define OPTSTRING "B:D:EF:GH:I:L:M:N:PQR:S:T:U:bcd::el:m:o:r:st:"

int
main (int argc, char *const *argv, char *const *envp)
{
  macro_definition *head;	/* head of deferred argument list */
  macro_definition *tail;
  macro_definition *new;
  int optchar;			/* option character */

  macro_definition *defines;
  FILE *fp;
  char *filename;

  m4 *context;

  int exit_status;

  /* Initialise gnulib error module.  */
  set_program_name (argv[0]);

  setlocale (LC_ALL, "");
#ifdef ENABLE_NLS
  textdomain(PACKAGE);
#endif

  LTDL_SET_PRELOADED_SYMBOLS();

  context = m4_create ();

  m4__module_init (context);

#ifdef USE_STACKOVF
  setup_stackovf_trap (argv, envp, stackovf_handler);
#endif

  if (isatty (STDIN_FILENO))
    m4_set_interactive_opt (context, true);

  if (getenv ("POSIXLY_CORRECT"))
    m4_set_posixly_correct_opt (context, true);

  /* First, we decode the arguments, to size up tables and stuff.  */

  head = tail = NULL;

  while (optchar = getopt_long (argc, (char **) argv, OPTSTRING,
				long_options, NULL),
	 optchar != EOF)
    switch (optchar)
      {
      default:
	usage (EXIT_FAILURE);

      case 0:
	break;

      case 'B':			/* compatibility junk */
      case 'H':
      case 'N':
      case 'S':
      case 'T':
	break;

      case 'D':
      case 'U':
      case 't':
      case 'm':
      case 'r':
	/* Arguments that cannot be handled until later are accumulated.  */

	new = xmalloc (sizeof *new);
	new->code = optchar;
	new->macro = optarg;
	new->next = NULL;

	if (head == NULL)
	  head = new;
	else
	  tail->next = new;
	tail = new;

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
	m4_add_include_directory (context, optarg);
	break;

      case 'L':
	m4_set_nesting_limit_opt (context, atoi (optarg));
	break;
      case 'M':
	if (lt_dlinsertsearchdir (lt_dlgetsearchpath(), optarg) != 0)
	  {
	    const char *dlerr = lt_dlerror();
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
	m4_set_interactive_opt (context, false);
	break;

      case 'c':
	m4_set_discard_comments_opt (context, true);
	break;

      case 'd':
	m4_set_debug_level_opt (context, m4_debug_decode (context, optarg));
	if (m4_get_debug_level_opt (context) < 0)
	  {
	    error (0, 0, _("bad debug flags: `%s'"), optarg);
	    m4_set_debug_level_opt (context, 0);
	  }
	break;

      case 'e':
	m4_set_interactive_opt (context, true);
	break;

      case 'l':
	m4_set_max_debug_arg_length_opt (context, atoi (optarg));
	if (m4_get_max_debug_arg_length_opt (context) <= 0)
	  m4_set_max_debug_arg_length_opt (context, 0);
	break;

      case 'o':
	if (!m4_debug_set_output (context, optarg))
	  error (0, errno, "%s", optarg);
	break;

      case 's':
	m4_set_sync_output_opt (context, true);
	break;
      }

  if (show_version)
    {
      version_etc (stdout, PACKAGE, PACKAGE_NAME TIMESTAMP,
		   VERSION, AUTHORS, NULL);
      exit (EXIT_SUCCESS);
    }

  if (show_help)
    usage (EXIT_SUCCESS);

  /* Do the basic initialisations.  */

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
     preprended to the macro definition list, so -U can override
     environment variables. */

  if (import_environment)
    {
      char *const *env;

      for (env = envp; *env != NULL; env++)
	{
	  new = xmalloc (sizeof *new);
	  new->code = 'D';
	  new->macro = *env;
	  new->next = head;
	  head = new;
	}
    }

  /* Handle deferred command line macro definitions.  Must come after
     initialisation of the symbol table.  */
  {
    defines = head;

    while (defines != NULL)
      {
	macro_definition *next;
	char *macro_value;
	const char *optarg = defines->macro;

	switch (defines->code)
	  {
	  case 'D':
	    {
	      m4_symbol_value *value = m4_symbol_value_create ();

	      macro_value = strchr (optarg, '=');
	      if (macro_value == NULL)
		macro_value = "";
	      else
		*macro_value++ = '\0';
	      m4_set_symbol_value_text (value, xstrdup (macro_value));

	      m4_symbol_pushdef (M4SYMTAB, optarg, value);
	    }
	    break;

	  case 'U':
	    m4_symbol_delete (M4SYMTAB, optarg);
	    break;

	  case 't':
	    m4_set_symbol_name_traced (M4SYMTAB, optarg);
	    break;

	  case 'm':
	    m4_module_load (context, optarg, 0);
	    break;

	  case 'r':
	    m4_set_regexp_syntax_opt (context,
				      m4_regexp_syntax_encode (optarg));
	    if (m4_get_regexp_syntax_opt (context) < 0)
	      {
		m4_error (context, EXIT_FAILURE, 0,
			  _("bad regexp syntax option: `%s'"), optarg);
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
      m4_push_file (context, stdin, "stdin");
      m4_macro_expand_input (context);
    }
  else
    for (; optind < argc; optind++)
      {
	if (strcmp (argv[optind], "-") == 0)
	  m4_push_file (context, stdin, "stdin");
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
		m4_push_file (context, fp, filename);
		free (filename);
	      }
	  }
	m4_macro_expand_input (context);
      }

  /* Now handle wrapup text.  */

  while (m4_pop_wrapup ())
    m4_macro_expand_input (context);

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

/* GNU m4 -- A simple macro processor

   Copyright (C) 1989-1994, 2004-2014, 2016-2017, 2020-2025 Free
   Software Foundation, Inc.

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "m4.h"

#include <getopt.h>
#include <signal.h>
#include <stdarg.h>

#include "c-stack.h"
#include "configmake.h"
#include "ignore-value.h"
#include "progname.h"
#include "propername.h"
#include "version-etc.h"

/* TRANSLATORS: This is a non-ASCII name: The first name is (with
   Unicode escapes) "Ren\u00e9" or (with HTML entities) "Ren&eacute;".  */
#define AUTHORS                                                 \
  proper_name_utf8 ("Rene' Seindal", "Ren\xC3\xA9 Seindal"),    \
  proper_name ("Eric Blake")

/* Enable sync output for /lib/cpp (-s).  */
int sync_output = 0;

/* Debug (-d[flags]).  */
int debug_level = 0;

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

/* Pathname of dependency file being made (--makedep=PATH). */
static const char *makedep_path = NULL;

/* Target for dependency rule being made (--makedep-target=TARGET). */
static const char *makedep_target = NULL;

/* Bitmask of places that will assume non-existent files are actually
   generated, and so a dependency should be listed regardless
   (--makedep-gen-missing-*). */
int makedep_gen_missing = REF_NONE;

/* Bitmask of which places files are referenced from that will trigger
   phony rules to be generated (--makedep-phony-*). */
static int makedep_phony = REF_NONE;

/* Global catchall for any errors that should affect final error status, but
   where we try to continue execution in the meantime.  */
int retcode;

struct macro_definition
{
  struct macro_definition *next;
  int code;                     /* See label `defer'.  */
  const char *arg;
};
typedef struct macro_definition macro_definition;

/* Error handling functions.  */

/* Helper for all the error reporting, as a wrapper around
   error_at_line.  Report error message based on FORMAT and ARGS, on
   behalf of CALLER (if any), otherwise at the global current
   location.  If ERRNUM, decode the errno value that caused the error.
   If STATUS, exit immediately with that status.  If WARN, prepend
   'warning: '.  */
static void
ATTRIBUTE_FORMAT ((__printf__, 5, 0))
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
     problematic macro names only occur via indir.  */
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
    full = xasprintf (_("warning: %s: %s"), macro, format);
  else if (warn)
    full = xasprintf (_("warning: %s"), format);
  else if (macro)
    full = xasprintf (_("%s: %s"), macro, format);
  verror_at_line (status, errnum, line ? file : NULL, line,
                  full ? full : format, args);
  free (full);
  free (safe_macro);
  if ((!warn || fatal_warnings) && !retcode)
    retcode = EXIT_FAILURE;
  va_end (args);
}

/* Wrapper around error.  Report error message based on FORMAT and
   subsequent args, on behalf of CALLER (if any), and the current
   input line (if any).  If ERRNUM, decode the errno value that caused
   the error.  If STATUS, exit immediately with that status.  */
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

/* Wrapper around error.  Report warning message based on FORMAT and
   subsequent args, on behalf of CALLER (if any), and the current
   input line (if any).  If ERRNUM, decode the errno value that caused
   the warning.  */
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

#ifndef SIGBUS
# define SIGBUS SIGILL
#endif

#ifndef NSIG
# ifndef MAX
#  define MAX(a,b) ((a) < (b) ? (b) : (a))
# endif
# define NSIG (MAX (SIGABRT, MAX (SIGILL, MAX (SIGFPE,  \
                                               MAX (SIGSEGV, SIGBUS)))) + 1)
#endif

/* Pre-translated messages for program errors.  Do not translate in
   the signal handler, since gettext and strsignal are not
   async-signal-safe.  */
static const char *volatile program_error_message;
static const char *volatile signal_message[NSIG];

/* Print a nicer message about any programmer errors, then exit.  This
   must be aysnc-signal safe, since it is executed as a signal
   handler.  If SIGNO is zero, this represents a stack overflow; in
   that case, we return to allow c_stack_action to handle things.  */
static void
fault_handler (int signo)
{
  if (signo)
    {
      /* POSIX states that reading static memory is, in general, not
         async-safe.  However, the static variables that we read are
         never modified once this handler is installed, so this
         particular usage is safe.  And it seems an oversight that
         POSIX claims strlen is not async-safe.  Ignore write
         failures, since we will exit with non-zero status anyway.  */
#define WRITE(f, b, l) ignore_value (write (f, b, l))
      WRITE (STDERR_FILENO, program_name, strlen (program_name));
      WRITE (STDERR_FILENO, ": ", 2);
      WRITE (STDERR_FILENO, program_error_message,
             strlen (program_error_message));
      if (signal_message[signo])
        {
          WRITE (STDERR_FILENO, ": ", 2);
          WRITE (STDERR_FILENO, signal_message[signo],
                 strlen (signal_message[signo]));
        }
      WRITE (STDERR_FILENO, "\n", 1);
#undef WRITE
      _exit (EXIT_INTERNAL_ERROR);
    }
}


/* Print a usage message and exit with STATUS.  */
static _Noreturn void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    {
      xfprintf (stderr, _("Try `%s --help' for more information."),
                program_name);
      fputs ("\n", stderr);
    }
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
"), stdout);
      puts ("");
      fputs (_("\
Operation modes:\n\
      --help                   display this help and exit\n\
      --version                output version information and exit\n\
"), stdout);
      fputs (_("\
  -E, --fatal-warnings         once: warnings become errors, twice: stop\n\
                                 execution at first error\n\
  -i, --interactive            unbuffer output, ignore interrupts\n\
  -P, --prefix-builtins        force a `m4_' prefix to all builtins\n\
  -Q, --quiet, --silent        suppress some warnings for builtins\n\
"), stdout);
      xprintf (_("\
      --warn-macro-sequence[=REGEXP]\n\
                               warn if macro definition matches REGEXP,\n\
                                 default %s\n\
"), DEFAULT_MACRO_SEQUENCE);
      puts ("");
      fputs (_("\
Preprocessor features:\n\
  -D, --define=NAME[=VALUE]    define NAME as having VALUE, or empty\n\
  -I, --include=DIRECTORY      append DIRECTORY to include path\n\
  -s, --synclines              generate `#line NUM \"FILE\"' lines\n\
  -U, --undefine=NAME          undefine NAME\n\
"), stdout);
      puts ("");
      xprintf (_("\
Limits control:\n\
  -g, --gnu                    override -G to re-enable GNU extensions\n\
  -G, --traditional            suppress all GNU extensions\n\
  -H, --hashsize=PRIME         set symbol lookup hash table size [%d]\n\
  -L, --nesting-limit=NUMBER   change nesting limit, 0 for unlimited [%d]\n\
"), HASHMAX, nesting_limit);
      puts ("");
      fputs (_("\
Frozen state files:\n\
  -F, --freeze-state=FILE      produce a frozen state on FILE at end\n\
  -R, --reload-state=FILE      reload a frozen state from FILE at start\n\
"), stdout);
      puts ("");
      fputs (_("\
Make dependency generation:\n\
      --makedep=FILE           write make dependency rule(s) into FILE\n\
      --makedep-target=TARGET  specify target of generated dependency rule\n\
                                 The --makedep and --makedep-target options\n\
                                 must be used together, either both present,\n\
                                 or neither present.\n\
      --makedep-gen-missing-argfiles\n\
      --makedep-gen-missing-include\n\
      --makedep-gen-missing-sinclude\n\
                               files that do not exist (on command line,\n\
                                 via include(), or via sinclude(),\n\
                                 respectively) are assumed to be generated\n\
                                 files and become dependencies regardless.\n\
      --makedep-gen-missing-all\n\
                               equivalent to --makedep-gen-missing-argfiles\n\
                                 --makedep-gen-missing-include\n\
                                 --makedep-gen-missing-sinclude\n\
      --makedep-phony-argfiles\n\
      --makedep-phony-include\n\
      --makedep-phony-sinclude\n\
                               generate a \"phony\" target for each file\n\
                                 that is specified on the command line, the\n\
                                 subject of an include() macro, or the\n\
                                 subject of an sinclude() macro,\n\
                                 respectively.\n\
      --makedep-phony-all      equivalent to --makedep-phony-argfiles\n\
                                 --makedep-phony-include\n\
                                 --makedep-phony-sinclude\n\
"), stdout);
      puts ("");
      fputs (_("\
Debugging:\n\
  -d, --debug[=[-|+]FLAGS], --debugmode[=[-|+]FLAGS]\n\
                               set debug level (no FLAGS implies `+adeq')\n\
      --debugfile[=FILE]       redirect debug and trace output to FILE\n\
                                 (default stderr, discard if empty string)\n\
  -l, --arglength=NUM          restrict macro tracing size\n\
  -t, --trace=NAME             trace NAME when it is defined\n\
"), stdout);
      puts ("");
      fputs (_("\
FLAGS is any of:\n\
  a   show actual arguments in trace\n\
  c   show collection line in trace\n\
  d   warn when dereferencing undefined macros\n\
  e   show expansion in trace\n\
  f   include current input file name in trace and debug\n\
  i   show changes in input files in debug\n\
  l   include current input line number in trace and debug\n\
"), stdout);
      fputs (_("\
  o   output dumpdef to stderr rather than debug file\n\
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
      emit_bug_reporting_address ();
    }
  exit (status);
}

/* Decode options and launch execution.  */

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  DEBUGFILE_OPTION = CHAR_MAX + 1,      /* no short opt */
  MAKEDEP_OPTION,               /* no short opt */
  MAKEDEP_TARGET_OPTION,        /* no short opt */
  MAKEDEP_GEN_MISSING_ARGFILES_OPTION,  /* no short opt */
  MAKEDEP_GEN_MISSING_INCLUDE_OPTION,   /* no short opt */
  MAKEDEP_GEN_MISSING_SINCLUDE_OPTION,  /* no short opt */
  MAKEDEP_GEN_MISSING_ALL_OPTION,       /* no short opt */
  MAKEDEP_PHONY_ARGFILES_OPTION,        /* no short opt */
  MAKEDEP_PHONY_INCLUDE_OPTION, /* no short opt */
  MAKEDEP_PHONY_SINCLUDE_OPTION,        /* no short opt */
  MAKEDEP_PHONY_ALL_OPTION,     /* no short opt */
  WARN_MACRO_SEQUENCE_OPTION,   /* no short opt */

  HELP_OPTION,                  /* no short opt */
  VERSION_OPTION                /* no short opt */
};

static const struct option long_options[] = {
  {"arglength", required_argument, NULL, 'l'},
  {"debug", optional_argument, NULL, 'd'},
  {"debugmode", optional_argument, NULL, 'd'},
  {"define", required_argument, NULL, 'D'},
  /* FIXME: deprecate in 2.0 */
  {"error-output", required_argument, NULL, 'o'},
  {"fatal-warnings", no_argument, NULL, 'E'},
  {"freeze-state", required_argument, NULL, 'F'},
  {"gnu", no_argument, NULL, 'g'},
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

  {"debugfile", optional_argument, NULL, DEBUGFILE_OPTION},
  {"warn-macro-sequence", optional_argument, NULL,
   WARN_MACRO_SEQUENCE_OPTION},

  {"makedep", required_argument, NULL, MAKEDEP_OPTION},
  {"makedep-target", required_argument, NULL, MAKEDEP_TARGET_OPTION},
  {"makedep-gen-missing-argfiles", no_argument, NULL,
   MAKEDEP_GEN_MISSING_ARGFILES_OPTION},
  {"makedep-gen-missing-include", no_argument, NULL,
   MAKEDEP_GEN_MISSING_INCLUDE_OPTION},
  {"makedep-gen-missing-sinclude", no_argument, NULL,
   MAKEDEP_GEN_MISSING_SINCLUDE_OPTION},
  {"makedep-gen-missing-all", no_argument, NULL,
   MAKEDEP_GEN_MISSING_ALL_OPTION},
  {"makedep-phony-argfiles", no_argument, NULL,
   MAKEDEP_PHONY_ARGFILES_OPTION},
  {"makedep-phony-include", no_argument, NULL, MAKEDEP_PHONY_INCLUDE_OPTION},
  {"makedep-phony-sinclude", no_argument, NULL,
   MAKEDEP_PHONY_SINCLUDE_OPTION},
  {"makedep-phony-all", no_argument, NULL, MAKEDEP_PHONY_ALL_OPTION},

  {"help", no_argument, NULL, HELP_OPTION},
  {"version", no_argument, NULL, VERSION_OPTION},

  {NULL, 0, NULL, 0},
};

/* Process a command line file NAME, and return true only if it was
   stdin.  */
static void
process_file (const char *name)
{
  if (STREQ (name, "-"))
    {
      /* If stdin is a terminal, we want to allow 'm4 - file -'
         to read input from stdin twice, like GNU cat.  Besides,
         there is no point closing stdin before wrapped text, to
         minimize bugs in syscmd called from wrapped text.  */
      /* TRANSLATORS: This is a short name for `standard input', used
         when a command line file was given as `-'.  */
      push_file (stdin, _("stdin"), false);
    }
  else
    {
      char *full_name;
      FILE *fp = m4_path_search (name, false, &full_name);
      if (fp == NULL)
        {
          error (0, errno, _("cannot open %s"),
                 quotearg_style (locale_quoting_style, name));
          if (makedep_gen_missing & REF_CMD_LINE)
            record_dependency (name, REF_CMD_LINE);
          else
            /* Set the status to EXIT_FAILURE, even though we
               continue to process files after a missing file.  */
            retcode = EXIT_FAILURE;
          return;
        }
      record_dependency (full_name, REF_CMD_LINE);
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
#define OPTSTRING "-B:D:EF:GH:I:L:PQR:S:T:U:d::egil:o:st:"

#ifdef DEBUG_REGEX
FILE *trace_file;
#endif /* DEBUG_REGEX */

int
main (int argc, char *const *argv, char *const *envp MAYBE_UNUSED)
{
  struct sigaction act;
  macro_definition *head;       /* head of deferred argument list */
  macro_definition *tail;
  macro_definition *defn;
  int optchar;                  /* option character */

  macro_definition *defines;
  bool interactive = false;
  bool seen_file = false;
  const char *debugfile = NULL;
  const char *frozen_file_to_read = NULL;
  const char *frozen_file_to_write = NULL;
  const char *macro_sequence = "";
  /* Hash table size (should be a prime) (-Hsize).  */
  size_t hash_table_size = HASHMAX;

  set_program_name (argv[0]);
  retcode = EXIT_SUCCESS;
  setlocale (LC_ALL, "");
  /* m4 1.4.x does not want locale-aware decimal separators in the
     format builtin; easiest is to override the user's choice of
     LC_NUMERIC. */
  setlocale (LC_NUMERIC, "C");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
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

  /* Stack overflow and program error handling.  Ignore failure to
     install a handler, since this is merely for improved output on
     crash, and we should never crash ;).  We install SIGBUS and
     SIGSEGV handlers prior to using the c-stack module; depending on
     the platform, c-stack will then override none, SIGSEGV, or both
     handlers.  */
  program_error_message
    = xasprintf (_("internal error detected; please report this bug to <%s>"),
                 PACKAGE_BUGREPORT);
  signal_message[SIGSEGV] = xstrdup (strsignal (SIGSEGV));
  signal_message[SIGABRT] = xstrdup (strsignal (SIGABRT));
  signal_message[SIGILL] = xstrdup (strsignal (SIGILL));
  signal_message[SIGFPE] = xstrdup (strsignal (SIGFPE));
  if (SIGBUS != SIGILL && SIGBUS != SIGSEGV)
    signal_message[SIGBUS] = xstrdup (strsignal (SIGBUS));
  sigemptyset (&act.sa_mask);
  /* One-shot - if we fault while handling a fault, we want to revert
     to default signal behavior.  */
  act.sa_flags = SA_NODEFER | SA_RESETHAND;
  act.sa_handler = fault_handler;
  sigaction (SIGSEGV, &act, NULL);
  sigaction (SIGABRT, &act, NULL);
  sigaction (SIGILL, &act, NULL);
  sigaction (SIGFPE, &act, NULL);
  sigaction (SIGBUS, &act, NULL);
  if (c_stack_action (fault_handler) == 0)
    nesting_limit = 0;

#ifdef DEBUG_STKOVF
  /* Make it easier to test our fault handlers.  Exporting M4_CRASH=0
     attempts a SIGSEGV, exporting it as 1 attempts an assertion
     failure with a fallback to abort.  */
  {
    char *crash = getenv ("M4_CRASH");
    if (crash)
      {
        if (!strtol (crash, NULL, 10))
          ++ * (int *) 8;
        assert (false);
        abort ();
      }
  }
#endif /* DEBUG_STKOVF */

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
        error (0, 0,
               _("warning: `m4 -%c' may be removed in a future release"),
               optchar);
        break;

      case '\1':
        seen_file = true;
        FALLTHROUGH;
      case 'D':
      case 'U':
      case 's':
      case 't':
      case DEBUGFILE_OPTION:
      defer:
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
        debug_decode ("-d", SIZE_MAX);
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
        {
          long tmp = strtol (optarg, NULL, 10);
          hash_table_size = tmp;
        }
        if (hash_table_size == 0)
          hash_table_size = HASHMAX;
        break;

      case 'I':
        add_include_directory (optarg);
        break;

      case 'L':
        {
          long tmp = strtol (optarg, NULL, 10);
          nesting_limit = tmp;
        }
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

      case 'd':
        if (seen_file)
          goto defer;
        if (debug_decode (optarg, SIZE_MAX) < 0)
          error (0, 0, _("bad debug flags: %s"),
                 quotearg_style (locale_quoting_style, optarg));
        break;

      case 'e':
        error (0, 0, _("warning: `%s' is deprecated, use `%s' instead"),
               "-e", "-i");
        FALLTHROUGH;
      case 'i':
        interactive = true;
        break;

      case 'g':
        no_gnu_extensions = 0;
        break;

      case 'l':
        {
          long tmp = strtol (optarg, NULL, 10);
          max_debug_argument_length = tmp <= 0 ? SIZE_MAX : (size_t) tmp;
        }
        break;

      case 'o':
        /* -o/--error-output are deprecated synonyms of --debugfile,
           so issue a warning.  Don't call debug_set_output here, as
           it has side effects.  */
        error (0, 0, _("warning: `%s' is deprecated, use `%s' instead"),
               optchar == 'o' ? "-o" : "--error-output", "--debugfile");
        debugfile = optarg;
        break;

      case WARN_MACRO_SEQUENCE_OPTION:
        /* Don't call set_macro_sequence here, as it can exit.
           --warn-macro-sequence sets optarg to NULL (which uses the
           default regexp); --warn-macro-sequence= sets optarg to ""
           (which disables these warnings).  */
        macro_sequence = optarg;
        break;

      case MAKEDEP_OPTION:
        if (makedep_path != NULL)
          usage (EXIT_FAILURE);
        makedep_path = optarg;
        break;

      case MAKEDEP_TARGET_OPTION:
        if (makedep_target != NULL)
          usage (EXIT_FAILURE);
        makedep_target = optarg;
        break;

      case MAKEDEP_GEN_MISSING_ARGFILES_OPTION:
        makedep_gen_missing |= REF_CMD_LINE;
        break;

      case MAKEDEP_GEN_MISSING_INCLUDE_OPTION:
        makedep_gen_missing |= REF_INCLUDE;
        break;

      case MAKEDEP_GEN_MISSING_SINCLUDE_OPTION:
        makedep_gen_missing |= REF_SINCLUDE;
        break;

      case MAKEDEP_GEN_MISSING_ALL_OPTION:
        makedep_gen_missing |= REF_ALL;
        break;

      case MAKEDEP_PHONY_ARGFILES_OPTION:
        makedep_phony |= REF_CMD_LINE;
        break;

      case MAKEDEP_PHONY_INCLUDE_OPTION:
        makedep_phony |= REF_INCLUDE;
        break;

      case MAKEDEP_PHONY_SINCLUDE_OPTION:
        makedep_phony |= REF_SINCLUDE;
        break;

      case MAKEDEP_PHONY_ALL_OPTION:
        makedep_phony |= REF_ALL;
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

  /* Verify mutual consistency of makedep options. */
  if (!makedep_path && !makedep_target)
    {
      /* Makedep mode is NOT active. */
      if (makedep_gen_missing)
        m4_error (0, 0, NULL,
                  _("--makedep-gen-missing-* requires --makedep and "
                    "--makedep-target"));
      if (makedep_phony)
        m4_error (0, 0, NULL,
                  _("--makedep-phony-* requires --makedep and "
                    "--makedep-target"));
      if ((makedep_gen_missing | makedep_phony) != 0)
        exit (EXIT_FAILURE);
    }
  else if (makedep_path && makedep_target)
    {
      /* Makedep mode is active. */
    }
  else
    {
      m4_error (0, 0, NULL,
                _("--makedep must be used with --makedep-target"));
      exit (EXIT_FAILURE);
    }

  input_init ();
  output_init ();
  symtab_init (hash_table_size);
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
      const char *arg = defines->arg;

      switch (defines->code)
        {
        case 'D':
          assert (arg);
          {
            const char *value = strchr (arg, '=');
            size_t len = value ? (size_t) (value - arg) : strlen (arg);
            define_user_macro (arg, len, value ? value + 1 : "",
                               value ? SIZE_MAX : 0, SYMBOL_INSERT);
          }
          break;

        case 'U':
          assert (arg);
          lookup_symbol (arg, strlen (arg), SYMBOL_DELETE);
          break;

        case 'd':
          if (debug_decode (arg, SIZE_MAX) < 0)
            error (0, 0, _("bad debug flags: %s"),
                   quotearg_style (locale_quoting_style, optarg));
          break;

        case 't':
          assert (arg);
          sym = lookup_symbol (arg, strlen (arg), SYMBOL_INSERT);
          SYMBOL_TRACED (sym) = true;
          break;

        case 's':
          sync_output = 1;
          break;

        case '\1':
          assert (arg);
          process_file (arg);
          break;

        case DEBUGFILE_OPTION:
          if (!debug_set_output (NULL, arg))
            m4_error (0, errno, NULL, _("cannot set debug file %s"),
                      quotearg_style (locale_quoting_style,
                                      arg ? arg : _("stderr")));
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
  if (makedep_path)
    generate_make_dependencies (makedep_path, makedep_target, makedep_phony);
#ifndef NDEBUG
  /* Only spend time freeing memory to help isolate leaks; if
     assertions are disabled, save the time and exit now.  */
  free_regex ();
  quotearg_free ();
  symtab_free ();
#endif /* NDEBUG */
#ifdef DEBUG_REGEX
  if (trace_file)
    fclose (trace_file);
#endif /* DEBUG_REGEX */
  exit (retcode);
}

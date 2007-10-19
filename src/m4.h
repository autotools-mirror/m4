/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2004, 2005, 2006, 2007
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

/* We use <config.h> instead of "config.h" so that a compilation
   using -I. -I$srcdir will use ./config.h rather than $srcdir/config.h
   (which it would do because it found this file in $srcdir).  */

#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "binary-io.h"
#include "clean-temp.h"
#include "cloexec.h"
#include "close-stream.h"
#include "closein.h"
#include "error.h"
#include "exitfail.h"
#include "obstack.h"
#include "stdio--.h"
#include "stdlib--.h"
#include "unistd--.h"
#include "verror.h"
#include "xalloc.h"
#include "xprintf.h"
#include "xvasprintf.h"

/* Canonicalize UNIX recognition macros.  */
#if defined unix || defined __unix || defined __unix__ \
  || defined _POSIX_VERSION || defined _POSIX2_VERSION \
  || defined __NetBSD__ || defined __OpenBSD__ \
  || defined __APPLE__ || defined __APPLE_CC__
# define UNIX 1
#endif

/* Canonicalize Windows recognition macros.  */
#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
# define W32_NATIVE 1
#endif

/* Canonicalize OS/2 recognition macro.  */
#ifdef __EMX__
# define OS2 1
#endif

/* Used for version mismatch, when -R detects a frozen file it can't parse.  */
#define EXIT_MISMATCH 63

/* M4 1.4.x is not yet internationalized.  But when it is, this can be
   redefined as gettext().  */
#define _(STRING) STRING

/* Various declarations.  */

struct string
  {
    char *string;		/* characters of the string */
    size_t length;		/* length of the string */
  };
typedef struct string STRING;

/* Memory allocation.  */
#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	free

/* Those must come first.  */
typedef struct token_data token_data;
typedef struct macro_arguments macro_arguments;
typedef void builtin_func (struct obstack *, int, macro_arguments *);

/* Gnulib's stdbool doesn't work with bool bitfields.  For nicer
   debugging, use bool when we know it works, but use the more
   portable unsigned int elsewhere.  */
#if __GNUC__ > 2
typedef bool bool_bitfield;
#else
typedef unsigned int bool_bitfield;
#endif /* !__GNUC__ */

/* Take advantage of GNU C compiler source level optimization hints,
   using portable macros.  */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 6)
#  define M4_GNUC_ATTRIBUTE(args)	__attribute__ (args)
#else
#  define M4_GNUC_ATTRIBUTE(args)
#endif  /* __GNUC__ */

#define M4_GNUC_UNUSED		M4_GNUC_ATTRIBUTE ((__unused__))
#define M4_GNUC_PRINTF(fmt, arg)			\
  M4_GNUC_ATTRIBUTE ((__format__ (__printf__, fmt, arg)))

/* File: m4.c  --- global definitions.  */

/* Option flags.  */
extern int sync_output;			/* -s */
extern int debug_level;			/* -d */
extern size_t hash_table_size;		/* -H */
extern int no_gnu_extensions;		/* -G */
extern int prefix_all_builtins;		/* -P */
extern int max_debug_argument_length;	/* -l */
extern int suppress_warnings;		/* -Q */
extern int warning_status;		/* -E */
extern int nesting_limit;		/* -L */
#ifdef ENABLE_CHANGEWORD
extern const char *user_word_regexp;	/* -W */
#endif

/* Error handling.  */
extern int retcode;
extern const char *program_name;

void m4_error (int, int, const char *, const char *, ...)
  M4_GNUC_PRINTF (4, 5);
void m4_error_at_line (int, int, const char *, int, const char *,
		       const char *, ...) M4_GNUC_PRINTF (6, 7);
void m4_warn (int, const char *, const char *, ...) M4_GNUC_PRINTF (3, 4);
void m4_warn_at_line (int, const char *, int, const char *,
		      const char *, ...) M4_GNUC_PRINTF (5, 6);

#ifdef USE_STACKOVF
void setup_stackovf_trap (char *const *, char *const *,
			  void (*handler) (void));
#endif

/* File: debug.c  --- debugging and tracing function.  */

extern FILE *debug;

/* The value of debug_level is a bitmask of the following.  */

/* a: show arglist in trace output */
#define DEBUG_TRACE_ARGS 1
/* e: show expansion in trace output */
#define DEBUG_TRACE_EXPANSION 2
/* q: quote args and expansion in trace output */
#define DEBUG_TRACE_QUOTE 4
/* t: trace all macros -- overrides trace{on,off} */
#define DEBUG_TRACE_ALL 8
/* l: add line numbers to trace output */
#define DEBUG_TRACE_LINE 16
/* f: add file name to trace output */
#define DEBUG_TRACE_FILE 32
/* p: trace path search of include files */
#define DEBUG_TRACE_PATH 64
/* c: show macro call before args collection */
#define DEBUG_TRACE_CALL 128
/* i: trace changes of input files */
#define DEBUG_TRACE_INPUT 256
/* x: add call id to trace output */
#define DEBUG_TRACE_CALLID 512

/* V: very verbose --  print everything */
#define DEBUG_TRACE_VERBOSE 1023
/* default flags -- equiv: aeq */
#define DEBUG_TRACE_DEFAULT 7

#define DEBUG_PRINT1(Fmt, Arg1)                                 \
  do								\
    {								\
      if (debug != NULL)					\
	xfprintf (debug, Fmt, Arg1);				\
    }								\
  while (0)

#define DEBUG_PRINT3(Fmt, Arg1, Arg2, Arg3)                     \
  do								\
    {								\
      if (debug != NULL)					\
	xfprintf (debug, Fmt, Arg1, Arg2, Arg3);                \
    }								\
  while (0)

#define DEBUG_MESSAGE(Fmt)                                      \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  xfprintf (debug, Fmt);                                \
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

#define DEBUG_MESSAGE1(Fmt, Arg1)                               \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  xfprintf (debug, Fmt, Arg1);				\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

#define DEBUG_MESSAGE2(Fmt, Arg1, Arg2)                         \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  xfprintf (debug, Fmt, Arg1, Arg2);			\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

void debug_init (void);
int debug_decode (const char *);
void debug_flush_files (void);
bool debug_set_output (const char *, const char *);
void debug_message_prefix (void);

void trace_prepre (const char *, int);
void trace_pre (const char *, int, int, macro_arguments *);
void trace_post (const char *, int, int, macro_arguments *, const char *);

/* File: input.c  --- lexical definitions.  */

typedef struct token_chain token_chain;

/* Various different token types.  Avoid overlap with token_data_type,
   since the shared prefix of the enumerators is a bit confusing.  */
enum token_type
{
  TOKEN_EOF = 4,/* End of file, TOKEN_VOID.  */
  TOKEN_STRING,	/* Quoted string or comment, TOKEN_TEXT or TOKEN_COMP.  */
  TOKEN_WORD,	/* An identifier, TOKEN_TEXT.  */
  TOKEN_OPEN,	/* Active character `(', TOKEN_TEXT.  */
  TOKEN_COMMA,	/* Active character `,', TOKEN_TEXT.  */
  TOKEN_CLOSE,	/* Active character `)', TOKEN_TEXT.  */
  TOKEN_SIMPLE,	/* Any other single character, TOKEN_TEXT.  */
  TOKEN_MACDEF	/* A macro's definition (see "defn"), TOKEN_FUNC.  */
};

/* The data for a token, a macro argument, and a macro definition.  */
enum token_data_type
{
  TOKEN_VOID,	/* Token still being constructed, u is invalid.  */
  TOKEN_TEXT,	/* Straight text, u.u_t is valid.  */
  TOKEN_FUNC,	/* Builtin function definition, u.func is valid.  */
  TOKEN_COMP	/* Composite argument, u.chain is valid.  */
};

/* Composite tokens are built of a linked list of chains.  */
struct token_chain
{
  token_chain *next;	/* Pointer to next link of chain.  */
  char *str;		/* NUL-terminated string if text, else NULL.  */
  macro_arguments *argv;/* Reference to earlier $@.  */
  unsigned int index;	/* Argument index within argv.  */
};

/* The content of a token or macro argument.  */
struct token_data
{
  enum token_data_type type;
  union
    {
      struct
	{
	  /* We don't support NUL in text, yet.  So len is just a
	     cache for now.  But it will be essential if we ever DO
	     support NUL.  */
	  size_t len;
	  char *text;
#ifdef ENABLE_CHANGEWORD
	  char *original_text;
#endif
	}
      u_t;
      builtin_func *func;

      /* Composite text: a linked list of straight text and $@
	 placeholders.  */
      token_chain *chain;
    }
  u;
};

// TODO - make this struct opaque, and move definition to macro.c
/* Opaque structure describing all arguments to a macro, including the
   macro name at index 0.  */
struct macro_arguments
{
  /* Number of arguments owned by this object, may be larger than
     arraylen since the array can refer to multiple arguments via a
     single $@ reference.  */
  unsigned int argc;
  /* False unless the macro expansion refers to $@, determines whether
     this object can be freed at end of macro expansion or must wait
     until next byte read from file.  */
  bool inuse;
  const char *argv0; /* The macro name being expanded.  */
  size_t argv0_len; /* Length of argv0.  */
  size_t arraylen; /* True length of allocated elements in array.  */
  /* Used as a variable-length array, storing information about each
     argument.  */
  token_data *array[FLEXIBLE_ARRAY_MEMBER];
};

#define TOKEN_DATA_TYPE(Td)		((Td)->type)
#define TOKEN_DATA_LEN(Td)		((Td)->u.u_t.len)
#define TOKEN_DATA_TEXT(Td)		((Td)->u.u_t.text)
#ifdef ENABLE_CHANGEWORD
# define TOKEN_DATA_ORIG_TEXT(Td)	((Td)->u.u_t.original_text)
#endif
#define TOKEN_DATA_FUNC(Td)		((Td)->u.func)

typedef enum token_type token_type;
typedef enum token_data_type token_data_type;

void input_init (void);
token_type peek_token (void);
token_type next_token (token_data *, int *, const char *);
void skip_line (const char *);

/* push back input */
void push_file (FILE *, const char *, bool);
void push_macro (builtin_func *);
struct obstack *push_string_init (void);
const char *push_string_finish (void);
void push_wrapup (const char *);
bool pop_wrapup (void);

/* current input file, and line */
extern const char *current_file;
extern int current_line;

/* left and right quote, begin and end comment */
extern STRING bcomm, ecomm;
extern STRING lquote, rquote;

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

void set_quotes (const char *, const char *);
void set_comment (const char *, const char *);
#ifdef ENABLE_CHANGEWORD
void set_word_regexp (const char *, const char *);
#endif

/* File: output.c --- output functions.  */
extern int current_diversion;
extern int output_current_line;

void output_init (void);
void output_exit (void);
void output_text (const char *, int);
void shipout_text (struct obstack *, const char *, int, int);
void make_diversion (int);
void insert_diversion (int);
void insert_file (FILE *);
void freeze_diversions (FILE *);

/* File symtab.c  --- symbol table definitions.  */

/* Operation modes for lookup_symbol ().  */
enum symbol_lookup
{
  SYMBOL_LOOKUP,
  SYMBOL_INSERT,
  SYMBOL_DELETE,
  SYMBOL_PUSHDEF,
  SYMBOL_POPDEF
};

/* Symbol table entry.  */
struct symbol
{
  struct symbol *next;
  bool_bitfield traced : 1;
  bool_bitfield shadowed : 1;
  bool_bitfield macro_args : 1;
  bool_bitfield blind_no_args : 1;
  bool_bitfield deleted : 1;
  int pending_expansions;

  char *name;
  token_data data;  /* Type should be only TOKEN_TEXT or TOKEN_FUNC.  */
};

#define SYMBOL_NEXT(S)		((S)->next)
#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_SHADOWED(S)	((S)->shadowed)
#define SYMBOL_MACRO_ARGS(S)	((S)->macro_args)
#define SYMBOL_BLIND_NO_ARGS(S)	((S)->blind_no_args)
#define SYMBOL_DELETED(S)	((S)->deleted)
#define SYMBOL_PENDING_EXPANSIONS(S) ((S)->pending_expansions)
#define SYMBOL_NAME(S)		((S)->name)
#define SYMBOL_TYPE(S)		(TOKEN_DATA_TYPE (&(S)->data))
#define SYMBOL_TEXT(S)		(TOKEN_DATA_TEXT (&(S)->data))
#define SYMBOL_FUNC(S)		(TOKEN_DATA_FUNC (&(S)->data))

typedef enum symbol_lookup symbol_lookup;
typedef struct symbol symbol;
typedef void hack_symbol (symbol *, void *);

#define HASHMAX 509		/* default, overridden by -Hsize */

extern symbol **symtab;

void free_symbol (symbol *sym);
void symtab_init (void);
symbol *lookup_symbol (const char *, symbol_lookup);
void hack_all_symbols (hack_symbol *, void *);

/* File: macro.c  --- macro expansion.  */

extern int expansion_level;

void expand_input (void);
void call_macro (symbol *, int, macro_arguments *, struct obstack *);

unsigned int arg_argc (macro_arguments *);
token_data_type arg_type (macro_arguments *, unsigned int);
const char *arg_text (macro_arguments *, unsigned int);
size_t arg_len (macro_arguments *, unsigned int);
builtin_func *arg_func (macro_arguments *, unsigned int);


/* File: builtin.c  --- builtins.  */

struct builtin
{
  const char *name;
  bool_bitfield gnu_extension : 1;
  bool_bitfield groks_macro_args : 1;
  bool_bitfield blind_if_no_args : 1;
  builtin_func *func;
};

struct predefined
{
  const char *unix_name;
  const char *gnu_name;
  const char *func;
};

typedef struct builtin builtin;
typedef struct predefined predefined;
struct re_pattern_buffer;
struct re_registers;

/* The default sequence detects multi-digit parameters (obsolete after
   1.4.x), and any use of extended arguments with the default ${}
   syntax (new in 2.0).  */
#define DEFAULT_MACRO_SEQUENCE "\\$\\({[^}]*}\\|[0-9][0-9]+\\)"

void builtin_init (void);
void define_builtin (const char *, const builtin *, symbol_lookup);
void set_macro_sequence (const char *);
void free_regex (void);
void define_user_macro (const char *, size_t, const char *, symbol_lookup);
void undivert_all (void);
void expand_user_macro (struct obstack *, symbol *, int, macro_arguments *);
void m4_placeholder (struct obstack *, int, macro_arguments *);
void init_pattern_buffer (struct re_pattern_buffer *, struct re_registers *);
const char *ntoa (int32_t, int);

const builtin *find_builtin_by_addr (builtin_func *);
const builtin *find_builtin_by_name (const char *);

/* File: path.c  --- path search for include files.  */

void include_init (void);
void include_env_init (void);
void add_include_directory (const char *);
FILE *m4_path_search (const char *, char **);

/* File: eval.c  --- expression evaluation.  */

bool evaluate (const char *, const char *, int32_t *);

/* File: format.c  --- printf like formatting.  */

void format (struct obstack *, int, macro_arguments *);

/* File: freeze.c --- frozen state files.  */

void produce_frozen_state (const char *);
void reload_frozen_state (const char *);

/* Debugging the memory allocator.  */

#ifdef WITH_DMALLOC
# define DMALLOC_FUNC_CHECK
# include <dmalloc.h>
#endif

/* Other debug stuff.  */

#ifdef DEBUG
# define DEBUG_INCL   1
# define DEBUG_INPUT  1
# define DEBUG_MACRO  1
# define DEBUG_OUTPUT 1
# define DEBUG_REGEX  1
# define DEBUG_STKOVF 1
# define DEBUG_SYM    1
#endif

/* Convert a possibly-signed character to an unsigned character.  This is
   a bit safer than casting to unsigned char, since it catches some type
   errors that the cast doesn't.  */
#if HAVE_INLINE
static inline unsigned char to_uchar (char ch) { return ch; }
#else
# define to_uchar(C) ((unsigned char) (C))
#endif

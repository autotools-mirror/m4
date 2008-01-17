/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2004, 2005, 2006, 2007,
   2008 Free Software Foundation, Inc.

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
#include <limits.h>
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
#include "intprops.h"
#include "obstack.h"
#include "quotearg.h"
#include "stdio--.h"
#include "stdlib--.h"
#include "unistd--.h"
#include "vasnprintf.h"
#include "verror.h"
#include "xalloc.h"
#include "xmemdup0.h"
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

/* Describes a pair of strings, such as begin and end quotes.  */
struct string_pair
  {
    char *str1;
    size_t len1;
    char *str2;
    size_t len2;
  };
typedef struct string_pair string_pair;

/* Memory allocation.  */
#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	free

/* These must come first.  */
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
extern size_t max_debug_argument_length;/* -l */
extern int suppress_warnings;		/* -Q */
extern int warning_status;		/* -E */
extern int nesting_limit;		/* -L */
#ifdef ENABLE_CHANGEWORD
extern const char *user_word_regexp;	/* -W */
#endif

/* Error handling.  */

/* A structure containing context that was valid when a macro call
   started collecting arguments; used for tracing and error messages
   even when the global context changes in the meantime.  */
struct call_info
{
  const char *file;	/* The file containing the macro invocation.  */
  int line;		/* The line the macro was called on.  */
  int call_id;		/* The unique sequence call id of the macro.  */
  int trace : 1;	/* True to trace this macro.  */
  int debug_level : 31;	/* The debug level for tracing the macro call.  */
  const char *name;	/* The macro name.  */
  size_t name_len;	/* The length of name.  */
};
typedef struct call_info call_info;

extern int retcode;
extern const char *program_name;

void m4_error (int, int, const call_info *, const char *, ...)
  M4_GNUC_PRINTF (4, 5);
void m4_warn (int, const call_info *, const char *, ...) M4_GNUC_PRINTF (3, 4);

#ifdef USE_STACKOVF
void setup_stackovf_trap (char *const *, char *const *,
			  void (*handler) (void));
#endif

/* File: debug.c  --- debugging and tracing function.  */

extern FILE *debug;

/* The value of debug_level is a bitmask of the following.  */

/* a: show arglist in trace output */
#define DEBUG_TRACE_ARGS 0x001
/* e: show expansion in trace output */
#define DEBUG_TRACE_EXPANSION 0x002
/* q: quote args and expansion in trace output */
#define DEBUG_TRACE_QUOTE 0x004
/* t: trace all macros -- overrides trace{on,off} */
#define DEBUG_TRACE_ALL 0x008
/* l: add line numbers to trace output */
#define DEBUG_TRACE_LINE 0x010
/* f: add file name to trace output */
#define DEBUG_TRACE_FILE 0x020
/* p: trace path search of include files */
#define DEBUG_TRACE_PATH 0x040
/* c: show macro call before args collection */
#define DEBUG_TRACE_CALL 0x080
/* i: trace changes of input files */
#define DEBUG_TRACE_INPUT 0x100
/* x: add call id to trace output */
#define DEBUG_TRACE_CALLID 0x200

/* V: very verbose --  print everything */
#define DEBUG_TRACE_VERBOSE 0x3FF
/* default flags -- equiv: aeq */
#define DEBUG_TRACE_DEFAULT 0x007

void debug_init (void);
int debug_decode (const char *);
void debug_flush_files (void);
bool debug_set_output (const call_info *, const char *);
void debug_message (const char *, ...) M4_GNUC_PRINTF (1, 2);

void trace_prepre (const call_info *);
unsigned int trace_pre (macro_arguments *);
void trace_post (unsigned int, const call_info *);


/* File: input.c  --- lexical definitions.  */

typedef struct token_chain token_chain;

/* Various different token types.  Avoid overlap with token_data_type,
   since the shared prefix of the enumerators is a bit confusing.  */
enum token_type
{
  TOKEN_EOF = 4,/* End of file, TOKEN_VOID.  */
  TOKEN_STRING,	/* Quoted string, TOKEN_TEXT or TOKEN_COMP.  */
  TOKEN_COMMENT,/* Comment, TOKEN_TEXT or TOKEN_COMP.  */
  TOKEN_WORD,	/* An identifier, TOKEN_TEXT.  */
  TOKEN_OPEN,	/* Active character `(', TOKEN_TEXT.  */
  TOKEN_COMMA,	/* Active character `,', TOKEN_TEXT.  */
  TOKEN_CLOSE,	/* Active character `)', TOKEN_TEXT.  */
  TOKEN_SIMPLE,	/* Any other single character, TOKEN_TEXT.  */
  TOKEN_MACDEF,	/* A builtin macro, TOKEN_FUNC or TOKEN_COMP.  */
  TOKEN_ARGV	/* A series of parameters, TOKEN_COMP.  */
};

/* The data for a token, a macro argument, and a macro definition.  */
enum token_data_type
{
  TOKEN_VOID,	/* Token still being constructed, u is invalid.  */
  TOKEN_TEXT,	/* Straight text, u.u_t is valid.  */
  TOKEN_FUNC,	/* Builtin function definition, u.func is valid.  */
  TOKEN_COMP	/* Composite argument, u.u_c is valid.  */
};

/* A link in a chain of token data.  */
enum token_chain_type
{
  CHAIN_STR,	/* Link contains a string, u.u_s is valid.  */
  CHAIN_FUNC,	/* Builtin function definition, u.func is valid.  */
  CHAIN_ARGV,	/* Link contains a $@ reference, u.u_a is valid.  */
  CHAIN_LOC	/* Link contains location of m4wrap, u.u_l is valid.  */
};

/* Composite tokens are built of a linked list of chains.  Each link
   of the chain is either a single text reference (ie. $1), or an argv
   reference (ie. $@).  */
struct token_chain
{
  token_chain *next;		/* Pointer to next link of chain.  */
  enum token_chain_type type;	/* Type of this link.  */
  unsigned int quote_age;	/* Quote_age of this link of chain, or 0.  */
  union
    {
      struct
	{
	  const char *str;	/* Pointer to text.  */
	  size_t len;		/* Remaining length of str.  */
	  int level;		/* Expansion level of link content, or -1.  */
	}
      u_s;
      builtin_func *func;		/* Builtin token from defn.  */
      struct
	{
	  macro_arguments *argv;	/* Reference to earlier $@.  */
	  unsigned int index;		/* Argument index within argv.  */
	  bool_bitfield flatten : 1;	/* True to treat builtins as text.  */
	  bool_bitfield comma : 1;	/* True when `,' is next input.  */
	  bool_bitfield skip_last : 1;	/* True if last argument omitted.  */
	  bool_bitfield has_func : 1;	/* True if argv includes func.  */
	  const string_pair *quotes;	/* NULL for $*, quotes for $@.  */
	}
      u_a;
      struct
	{
	  const char *file;	/* File where subsequent links originate.  */
	  int line;		/* Line where subsequent links originate.  */
	}
      u_l;
    }
  u;
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
	  char *text; /* The contents of the token.  */
	  /* The value of quote_age when this token was scanned.  If
	     this token is later encountered in the context of
	     scanning a quoted string, and quote_age has not changed,
	     then rescanning this string is provably unnecessary.  If
	     zero, then this string potentially contains content that
	     might change the parse on rescan.  Ignored for 0 len.  */
	  unsigned int quote_age;
#ifdef ENABLE_CHANGEWORD
	  /* If changeword is in effect, and contains a () group, then
	     this contains the entire token, while text contains the
	     portion that matched the () group to form a macro name.
	     Otherwise, this field is unused.  */
	  const char *original_text;
	  size_t original_len; /* Length of original_text.  */
#endif
	}
      u_t;
      builtin_func *func;

      /* Composite text: a linked list of straight text and $@
	 placeholders.  */
      struct
	{
	  token_chain *chain;		/* First link of the chain.  */
	  token_chain *end;		/* Last link of the chain.  */
	  bool_bitfield wrapper : 1;	/* True if this is a $@ ref.  */
	  bool_bitfield has_func : 1;	/* True if chain includes func.  */
	}
      u_c;
    }
  u;
};

#define TOKEN_DATA_TYPE(Td)		((Td)->type)
#define TOKEN_DATA_LEN(Td)		((Td)->u.u_t.len)
#define TOKEN_DATA_TEXT(Td)		((Td)->u.u_t.text)
#define TOKEN_DATA_QUOTE_AGE(Td)	((Td)->u.u_t.quote_age)
#ifdef ENABLE_CHANGEWORD
# define TOKEN_DATA_ORIG_TEXT(Td)	((Td)->u.u_t.original_text)
# define TOKEN_DATA_ORIG_LEN(Td)	((Td)->u.u_t.original_len)
#endif
#define TOKEN_DATA_FUNC(Td)		((Td)->u.func)

typedef enum token_type token_type;
typedef enum token_data_type token_data_type;

void input_init (void);
token_type peek_token (void);
token_type next_token (token_data *, int *, struct obstack *, bool,
		       const call_info *);
void skip_line (const call_info *);

/* push back input */
void make_text_link (struct obstack *, token_chain **, token_chain **);
void push_file (FILE *, const char *, bool);
void append_macro (struct obstack *, builtin_func *, token_chain **,
		   token_chain **);
void push_macro (struct obstack *, builtin_func *);
struct obstack *push_string_init (const char *, int);
bool push_token (token_data *, int, bool);
void push_string_finish (void);
struct obstack *push_wrapup_init (const call_info *, token_chain ***);
void push_wrapup_finish (void);
bool pop_wrapup (void);
void input_print (struct obstack *);

/* current input file, and line */
extern const char *current_file;
extern int current_line;

/* left and right quote, begin and end comment */
extern string_pair curr_comm;
extern string_pair curr_quote;

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

void set_quotes (const char *, size_t, const char *, size_t);
void set_comment (const char *, size_t, const char *, size_t);
#ifdef ENABLE_CHANGEWORD
void set_word_regexp (const call_info *, const char *, size_t);
#endif
unsigned int quote_age (void);
bool safe_quotes (void);
const string_pair *quote_cache (struct obstack *, unsigned int,
				const string_pair *);

/* File: output.c --- output functions.  */
extern int current_diversion;
extern int output_current_line;

void output_init (void);
void output_exit (void);
void output_text (const char *, int);
void divert_text (struct obstack *, const char *, int, int);
bool shipout_string_trunc (struct obstack *, const char *, size_t, size_t *);
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
  size_t len;
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
#define SYMBOL_NAME_LEN(S)	((S)->len)
#define SYMBOL_TYPE(S)		(TOKEN_DATA_TYPE (&(S)->data))
#define SYMBOL_TEXT(S)		(TOKEN_DATA_TEXT (&(S)->data))
#define SYMBOL_TEXT_LEN(S)	(TOKEN_DATA_LEN (&(S)->data))
#define SYMBOL_FUNC(S)		(TOKEN_DATA_FUNC (&(S)->data))

typedef enum symbol_lookup symbol_lookup;
typedef struct symbol symbol;
typedef void hack_symbol (symbol *, void *);

#define HASHMAX 509		/* default, overridden by -Hsize */

extern symbol **symtab;

void free_symbol (symbol *sym);
void symtab_init (void);
symbol *lookup_symbol (const char *, size_t, symbol_lookup);
void hack_all_symbols (hack_symbol *, void *);

/* File: macro.c  --- macro expansion.  */

extern int expansion_level;

void expand_input (void);
void call_macro (symbol *, macro_arguments *, struct obstack *);
size_t adjust_refcount (int, bool);

bool arg_adjust_refcount (macro_arguments *, bool);
unsigned int arg_argc (macro_arguments *);
const call_info *arg_info (macro_arguments *);
token_data_type arg_type (macro_arguments *, unsigned int);
const char *arg_text (macro_arguments *, unsigned int, bool);
bool arg_equal (macro_arguments *, unsigned int, unsigned int);
bool arg_empty (macro_arguments *, unsigned int);
size_t arg_len (macro_arguments *, unsigned int, bool);
builtin_func *arg_func (macro_arguments *, unsigned int);
struct obstack *arg_scratch (void);
bool arg_print (struct obstack *, macro_arguments *, unsigned int,
		const string_pair *, bool, token_chain **, const char *,
		size_t *, bool);
macro_arguments *make_argv_ref (macro_arguments *, const char *, size_t,
				bool, bool);
void push_arg (struct obstack *, macro_arguments *, unsigned int);
void push_arg_quote (struct obstack *, macro_arguments *, unsigned int,
		     const string_pair *);
void push_args (struct obstack *, macro_arguments *, bool, bool);
void wrap_args (macro_arguments *);

/* Grab the text at argv index I.  Assumes macro_argument *argv is in
   scope, and aborts if the argument is not text.  */
#define ARG(i) arg_text (argv, i, false)

/* Grab the text length at argv index I.  Assumes macro_argument *argv
   is in scope, and aborts if the argument is not text.  */
#define ARG_LEN(i) arg_len (argv, i, false)


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
bool bad_argc (const call_info *, int, unsigned int, unsigned int);
void define_builtin (const char *, size_t, const builtin *, symbol_lookup);
void set_macro_sequence (const char *);
void free_regex (void);
void define_user_macro (const char *, size_t, const char *, size_t,
			symbol_lookup);
void undivert_all (void);
void expand_user_macro (struct obstack *, symbol *, int, macro_arguments *);
void m4_placeholder (struct obstack *, int, macro_arguments *);
void init_pattern_buffer (struct re_pattern_buffer *, struct re_registers *);

const builtin *find_builtin_by_addr (builtin_func *);
const builtin *find_builtin_by_name (const char *);
void func_print (struct obstack *, const builtin *, bool, token_chain **,
		 const string_pair *);

/* File: path.c  --- path search for include files.  */

void include_init (void);
void include_env_init (void);
void add_include_directory (const char *);
FILE *m4_path_search (const char *, char **);

/* File: eval.c  --- expression evaluation.  */

bool evaluate (const call_info *, const char *, size_t, int32_t *);

/* File: format.c  --- printf like formatting.  */

void expand_format (struct obstack *, int, macro_arguments *);

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

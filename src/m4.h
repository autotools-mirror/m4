/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef M4_H
#define M4_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#ifdef __STDC__
# define voidstar void *
#else
# define voidstar char *
#endif

#include <stdio.h>
#include <ctype.h>

#include "obstack.h"

/* An ANSI string.h and pre-ANSI memory.h might conflict.  */

#if defined (HAVE_STRING_H) || defined (STDC_HEADERS)
# include <string.h>
# if !defined (STDC_HEADERS) && defined (HAVE_MEMORY_H)
#  include <memory.h>
# endif
#else
# include <strings.h>
# ifndef memcpy
#  define memcpy(D, S, N) bcopy((S), (D), (N))
# endif
# ifndef strchr
#  define strchr(S, C) index ((S), (C))
# endif
# ifndef strrchr
#  define strrchr(S, C) rindex ((S), (C))
# endif
# ifndef bcopy
void bcopy ();
# endif
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>
#else /* not STDC_HEADERS */

voidstar malloc ();
voidstar realloc ();
char *getenv ();
double atof ();
long strtol ();

#endif /* STDC_HEADERS */

/* Some systems do not define EXIT_*, even with STDC_HEADERS.  */
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

/* If FALSE is defined, we presume TRUE is defined too.  In this case,
   merely typedef boolean as being int.  Or else, define these all.  */
#ifndef FALSE
/* Do not use `enum boolean': this tag is used in SVR4 <sys/types.h>.  */
typedef enum { FALSE = 0, TRUE = 1 } boolean;
#else
typedef int boolean;
#endif

char *mktemp ();

#ifndef __P
# ifdef PROTOTYPES
#  define __P(Args) Args
# else
#  define __P(Args) ()
# endif
#endif

#if HAVE_LOCALE_H
# include <locale.h>
#else
# define setlocale(Category, Locale)
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(Text) gettext ((Text))
#else
#define _(Text) (Text)
#endif


/* Various declarations.  */

struct string
  {
    unsigned char *string;	/* characters of the string */
    size_t length;		/* length of the string */
  };
typedef struct string STRING;

/* Memory allocation.  */
voidstar xmalloc __P ((unsigned int));
voidstar xrealloc __P ((voidstar , unsigned int));
void xfree __P ((voidstar));
char *xstrdup __P ((const char *));
#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree

/* Other library routines.  */
void error __P ((int, int, const char *, ...));

/* Those must come first.  */
typedef void builtin_func ();
typedef struct token_data token_data;


/* File: m4.c  --- global definitions.  */

/* Option flags.  */
extern int interactive;			/* -e */
extern int sync_output;			/* -s */
extern int debug_level;			/* -d */
extern int hash_table_size;		/* -H */
extern int no_gnu_extensions;		/* -G */
extern int prefix_all_builtins;		/* -P */
extern int max_debug_argument_length;	/* -l */
extern int suppress_warnings;		/* -Q */
extern int warning_status;		/* -E */
extern int nesting_limit;		/* -L */
extern int discard_comments;		/* -c */
#ifdef ENABLE_CHANGEWORD
extern const char *user_word_regexp;	/* -W */
#endif

/* Error handling.  */
#include "error.h"
#define M4ERROR(Arglist) (error Arglist)

#ifdef USE_STACKOVF
void setup_stackovf_trap __P ((char *const *, char *const *,
			       void (*handler) (void)));
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

#define DEBUG_PRINT1(Fmt, Arg1) \
  do								\
    {								\
      if (debug != NULL)					\
	fprintf (debug, Fmt, Arg1);				\
    }								\
  while (0)

#define DEBUG_PRINT3(Fmt, Arg1, Arg2, Arg3) \
  do								\
    {								\
      if (debug != NULL)					\
	fprintf (debug, Fmt, Arg1, Arg2, Arg3);			\
    }								\
  while (0)

#define DEBUG_MESSAGE(Fmt) \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  fprintf (debug, Fmt);					\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

#define DEBUG_MESSAGE1(Fmt, Arg1) \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  fprintf (debug, Fmt, Arg1);				\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

#define DEBUG_MESSAGE2(Fmt, Arg1, Arg2) \
  do								\
    {								\
      if (debug != NULL)					\
	{							\
	  debug_message_prefix ();				\
	  fprintf (debug, Fmt, Arg1, Arg2);			\
	  putc ('\n', debug);					\
	}							\
    }								\
  while (0)

void debug_init __P ((void));
int debug_decode __P ((const char *));
void debug_flush_files __P ((void));
boolean debug_set_output __P ((const char *));
void debug_message_prefix __P ((void));

void trace_prepre __P ((const char *, int));
void trace_pre __P ((const char *, int, int, token_data **));
void trace_post __P ((const char *, int, int, token_data **, const char *));


/* File: input.c  --- lexical definitions.  */

/* Various different token types.  */
enum token_type
{
  TOKEN_EOF,			/* end of file */
  TOKEN_NONE,			/* discardable token */
  TOKEN_STRING,			/* a quoted string */
  TOKEN_SPACE,			/* whitespace */
  TOKEN_WORD,			/* an identifier */
  TOKEN_SIMPLE,			/* a single character */
  TOKEN_MACDEF			/* a macros definition (see "defn") */
};

/* The data for a token, a macro argument, and a macro definition.  */
enum token_data_type
{
  TOKEN_VOID,
  TOKEN_TEXT,
  TOKEN_FUNC
};

struct token_data
{
  enum token_data_type type;
  union
    {
      struct
	{
	  char *text;
#ifdef ENABLE_CHANGEWORD
	  char *original_text;
#endif
	}
      u_t;
      struct
	{
	  builtin_func *func;
	  boolean traced;
	}
      u_f;
    }
  u;
};

#define TOKEN_DATA_TYPE(Td)		((Td)->type)
#define TOKEN_DATA_TEXT(Td)		((Td)->u.u_t.text)
#ifdef ENABLE_CHANGEWORD
# define TOKEN_DATA_ORIG_TEXT(Td)	((Td)->u.u_t.original_text)
#endif
#define TOKEN_DATA_FUNC(Td)		((Td)->u.u_f.func)
#define TOKEN_DATA_FUNC_TRACED(Td) 	((Td)->u.u_f.traced)

typedef enum token_type token_type;
typedef enum token_data_type token_data_type;

void input_init __P ((void));
int peek_input __P ((void));
token_type next_token __P ((token_data *));
void skip_line __P ((void));

/* push back input */
void push_file __P ((FILE *, const char *));
void push_macro __P ((builtin_func *, boolean));
struct obstack *push_string_init __P ((void));
const char *push_string_finish __P ((void));
void push_wrapup __P ((const char *));
boolean pop_wrapup __P ((void));

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

/* Syntax table definitions. */
/* Please read the comment at the top of input.c for details */
extern unsigned short syntax_table[256];

/* These are simple values, not bit masks.  There is no overlap. */
#define SYNTAX_OTHER	(0x0000)

#define SYNTAX_IGNORE	(0x0001)
#define SYNTAX_SPACE	(0x0002)
#define SYNTAX_OPEN	(0x0003)
#define SYNTAX_CLOSE	(0x0004)
#define SYNTAX_COMMA	(0x0005)
#define SYNTAX_DOLLAR	(0x0006) /* not used yet */
#define SYNTAX_ACTIVE	(0x0007)
#define SYNTAX_ESCAPE	(0x0008)

/* These are values to be assigned to syntax table entries, but they are
   used as bit masks with IS_ALNUM.*/
#define SYNTAX_ALPHA	(0x0010)
#define SYNTAX_NUM	(0x0020)
#define SYNTAX_ALNUM	(SYNTAX_ALPHA|SYNTAX_NUM)

/* These are bit masks to AND with other categories.  
   See input.c for details. */
#define SYNTAX_LQUOTE	(0x0100)
#define SYNTAX_RQUOTE	(0x0200)
#define SYNTAX_BCOMM	(0x0400)
#define SYNTAX_ECOMM	(0x0800)

/* These bits define the syntax code of a character */
#define SYNTAX_VALUE	(0x00FF|SYNTAX_LQUOTE|SYNTAX_BCOMM)
#define SYNTAX_MASKS	(0xFF00)

#define IS_OTHER(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_OTHER)
#define IS_IGNORE(ch) ((syntax_table[(int)(ch)]) == SYNTAX_IGNORE)
#define IS_SPACE(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_SPACE)

#define IS_OPEN(ch)   ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_OPEN)
#define IS_CLOSE(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_CLOSE)
#define IS_COMMA(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_COMMA)
#define IS_DOLLAR(ch) ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_DOLLAR)
#define IS_ACTIVE(ch) ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_ACTIVE)

#define IS_ESCAPE(ch) ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_ESCAPE)
#define IS_ALPHA(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_ALPHA)
#define IS_NUM(ch)    ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_NUM)
#define IS_ALNUM(ch)  (((syntax_table[(int)(ch)]) & SYNTAX_ALNUM) != 0)

#define IS_LQUOTE(ch) (syntax_table[(int)(ch)] & SYNTAX_LQUOTE)
#define IS_RQUOTE(ch) (syntax_table[(int)(ch)] & SYNTAX_RQUOTE)
#define IS_BCOMM(ch)  (syntax_table[(int)(ch)] & SYNTAX_BCOMM)
#define IS_ECOMM(ch)  (syntax_table[(int)(ch)] & SYNTAX_ECOMM)


void set_quotes __P ((const char *, const char *));
void set_comment __P ((const char *, const char *));
void set_syntax __P ((int, const char *));
#ifdef ENABLE_CHANGEWORD
void set_word_regexp __P ((const char *));
#endif


/* File: output.c --- output functions.  */
extern int current_diversion;
extern int output_current_line;

void output_init __P ((void));
void shipout_text __P ((struct obstack *, const char *, int));
void make_diversion __P ((int));
void insert_diversion __P ((int));
void insert_file __P ((FILE *));
void freeze_diversions __P ((FILE *));


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
  boolean traced;
  boolean shadowed;
  boolean macro_args;
  boolean blind_no_args;

  char *name;
  token_data data;
};

#define SYMBOL_NEXT(S)		((S)->next)
#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_SHADOWED(S)	((S)->shadowed)
#define SYMBOL_MACRO_ARGS(S)	((S)->macro_args)
#define SYMBOL_BLIND_NO_ARGS(S)	((S)->blind_no_args)
#define SYMBOL_NAME(S)		((S)->name)
#define SYMBOL_TYPE(S)		(TOKEN_DATA_TYPE (&(S)->data))
#define SYMBOL_TEXT(S)		(TOKEN_DATA_TEXT (&(S)->data))
#define SYMBOL_FUNC(S)		(TOKEN_DATA_FUNC (&(S)->data))

typedef enum symbol_lookup symbol_lookup;
typedef struct symbol symbol;
typedef void hack_symbol ();

#define HASHMAX 509		/* default, overridden by -Hsize */

extern symbol **symtab;

void symtab_init __P ((void));
symbol *lookup_symbol __P ((const char *, symbol_lookup));
void hack_all_symbols __P ((hack_symbol *, const char *));


/* File: macro.c  --- macro expansion.  */

void expand_input __P ((void));
void call_macro __P ((symbol *, int, token_data **, struct obstack *));


/* File: builtin.c  --- builtins.  */

struct builtin
{
  const char *name;
  boolean gnu_extension;
  boolean groks_macro_args;
  boolean blind_if_no_args;
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

void builtin_init __P ((void));
void define_builtin __P ((const char *, const builtin *, symbol_lookup,
			  boolean));
void define_user_macro __P ((const char *, const char *, symbol_lookup));
void undivert_all __P ((void));
void expand_user_macro __P ((struct obstack *, symbol *, int, token_data **));

void install_builtin_table (builtin *table);
const builtin *find_builtin_by_addr __P ((builtin_func *));
const builtin *find_builtin_by_name __P ((const char *));


/* File: path.c  --- path search for include files.  */

void include_init __P ((void));
void include_env_init __P ((void));
void add_include_directory __P ((const char *));
FILE *path_search __P ((const char *, char **));

/* These are for other search paths */

struct search_path
{
  struct search_path *next;	/* next directory to search */
  const char *dir;		/* directory */
  int len;
};

typedef struct search_path search_path;

struct search_path_info
{
  search_path *list;		/* the list of path directories */
  search_path *list_end;	/* the end of same */
  int max_length;		/* length of longest directory name */
};

struct search_path_info *search_path_info_new __P((void));
void search_path_env_init __P ((struct search_path_info *, char *, boolean));
void search_path_add __P ((struct search_path_info *, const char *));



/* File: eval.c  --- expression evaluation.  */

boolean evaluate __P ((struct obstack *obs,
		       const char *, const int radix, int min));

#ifdef WITH_GMP
boolean mp_evaluate __P ((struct obstack *obs,
			  const char *, const int radix, int min));
#endif /* WITH_GMP */


/* File: format.c  --- printf like formatting.  */

void format __P ((struct obstack *, int, token_data **));


/* File: freeze.c --- frozen state files.  */

void produce_frozen_state __P ((const char *));
void reload_frozen_state __P ((const char *));



/* File: module.c --- dynamic modules */

#ifdef WITH_MODULES

typedef void module_init_t (struct obstack *obs);
typedef void module_finish_t (void);

typedef voidstar module_func (const char *);

void module_load (const char *modname, struct obstack *obs);
void module_unload_all (void);

#endif /* WITH_MODULES */


/* Debugging the memory allocator.  */

#ifdef WITH_DMALLOC
# define DMALLOC_FUNC_CHECK
# include <dmalloc.h>
#endif

/* Other debug stuff.  */

#ifdef DEBUG
# define DEBUG_INPUT
# define DEBUG_MACRO
# define DEBUG_SYM
# define DEBUG_INCL
# define DEBUG_MODULE
#endif

#endif /* M4_H */

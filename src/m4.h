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
#include <stdio.h>
#include <ctype.h>

#include "m4module.h"

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

char *mktemp ();

#if HAVE_LOCALE_H
# include <locale.h>
#else
# define setlocale(Category, Locale)
#endif

/* Error handling.  */
#ifdef USE_STACKOVF
void setup_stackovf_trap M4_PARAMS((char *const *, char *const *,
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

void debug_init M4_PARAMS((void));
int debug_decode M4_PARAMS((const char *));
void debug_flush_files M4_PARAMS((void));
boolean debug_set_output M4_PARAMS((const char *));
void debug_message_prefix M4_PARAMS((void));

void trace_prepre M4_PARAMS((const char *, int));
void trace_pre M4_PARAMS((const char *, int, int, token_data **));
void trace_post M4_PARAMS((const char *, int, int, token_data **,
			   const char *));


/* File: input.c  --- lexical definitions.  */

void input_init M4_PARAMS((void));
int peek_input M4_PARAMS((void));
token_type next_token M4_PARAMS((token_data *));
void skip_line M4_PARAMS((void));

/* push back input */
void push_file M4_PARAMS((FILE *, const char *));
void push_macro M4_PARAMS((builtin_func *, boolean));
struct obstack *push_string_init M4_PARAMS((void));
const char *push_string_finish M4_PARAMS((void));
void push_wrapup M4_PARAMS((const char *));
boolean pop_wrapup M4_PARAMS((void));

/* current input file, and line */
extern const char *current_file;
extern int current_line;

void set_quotes M4_PARAMS((const char *, const char *));
void set_comment M4_PARAMS((const char *, const char *));
void set_syntax M4_PARAMS((int, const char *));
#ifdef ENABLE_CHANGEWORD
void set_word_regexp M4_PARAMS((const char *));
#endif


/* File: output.c --- output functions.  */
extern int current_diversion;
extern int output_current_line;

void output_init M4_PARAMS((void));
void shipout_text M4_PARAMS((struct obstack *, const char *, int));
void make_diversion M4_PARAMS((int));
void insert_diversion M4_PARAMS((int));
void insert_file M4_PARAMS((FILE *));
void freeze_diversions M4_PARAMS((FILE *));


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
#ifndef COMPILING_M4
typedef voidstar symbol;
#else
typedef struct symbol symbol;
#endif

typedef enum symbol_lookup symbol_lookup;
typedef void hack_symbol ();

extern symbol **symtab;

void symtab_init M4_PARAMS((void));
symbol *lookup_symbol M4_PARAMS((const char *, symbol_lookup));
void hack_all_symbols M4_PARAMS((hack_symbol *, const char *));


/* File: macro.c  --- macro expansion.  */

void expand_input M4_PARAMS((void));
void call_macro M4_PARAMS((symbol *, int, token_data **, struct obstack *));


/* File: builtin.c  --- builtins.  */

struct predefined
{
  const char *unix_name;
  const char *gnu_name;
  const char *func;
};

typedef struct predefined predefined;

void builtin_init M4_PARAMS((void));
void define_builtin M4_PARAMS((const char *, const builtin *, symbol_lookup,
			  boolean));
void define_user_macro M4_PARAMS((const char *, const char *, symbol_lookup));
void undivert_all M4_PARAMS((void));
void expand_user_macro M4_PARAMS((struct obstack *, symbol *, int,
				  token_data **));

void install_builtin_table M4_PARAMS((builtin *table));
int remove_builtin_table M4_PARAMS((builtin *table));
const builtin *find_builtin_by_addr M4_PARAMS((builtin_func *));
const builtin *find_builtin_by_name M4_PARAMS((const char *));


/* File: path.c  --- path search for include files.  */

void include_init M4_PARAMS((void));
void include_env_init M4_PARAMS((void));
void add_include_directory M4_PARAMS((const char *));
FILE *path_search M4_PARAMS((const char *, char **));

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

struct search_path_info *search_path_info_new M4_PARAMS((void));
void search_path_env_init M4_PARAMS((struct search_path_info *, char *,
				     boolean));
void search_path_add M4_PARAMS((struct search_path_info *, const char *));



/* File: eval.c  --- expression evaluation.  */

boolean evaluate M4_PARAMS((struct obstack *obs,
			    const char *, const int radix, int min));

#ifdef WITH_GMP
boolean mp_evaluate M4_PARAMS((struct obstack *obs,
			       const char *, const int radix, int min));
#endif /* WITH_GMP */


/* File: format.c  --- printf like formatting.  */

void format M4_PARAMS((struct obstack *, int, token_data **));


/* File: freeze.c --- frozen state files.  */

void produce_frozen_state M4_PARAMS((const char *));
void reload_frozen_state M4_PARAMS((const char *));



/* File: module.c --- dynamic modules */

#ifdef WITH_MODULES

typedef voidstar module_func M4_PARAMS((const char *));

void module_init M4_PARAMS((void));
void module_load M4_PARAMS((const char *modname, struct obstack *obs));
void module_unload M4_PARAMS((const char *modname, struct obstack *obs));
void module_unload_all M4_PARAMS((void));

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

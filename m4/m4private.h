/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1998, 1999, 2004, 2005,
   2006, 2007 Free Software Foundation, Inc.

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

#ifndef M4PRIVATE_H
#define M4PRIVATE_H 1

#include <config.h>

#include <assert.h>
#include <errno.h>

#include "m4module.h"

#include "cloexec.h"
#include "stdio--.h"
#include "stdlib--.h"
#include "unistd--.h"

typedef struct m4__search_path_info m4__search_path_info;

typedef enum {
  M4_SYMBOL_VOID, /* Traced but undefined.  */
  M4_SYMBOL_TEXT, /* Plain text.  */
  M4_SYMBOL_FUNC, /* Builtin function.  */
  M4_SYMBOL_PLACEHOLDER, /* Placeholder for unknown builtin during -R.  */
} m4__symbol_type;

#define BIT_TEST(flags, bit)	(((flags) & (bit)) == (bit))
#define BIT_SET(flags, bit)	((flags) |= (bit))
#define BIT_RESET(flags, bit)	((flags) &= ~(bit))


/* --- CONTEXT MANAGEMENT --- */

struct m4 {
  m4_symbol_table *	symtab;
  m4_syntax_table *	syntax;

  const char *		current_file;	/* Current input file.  */
  int			current_line;	/* Current input line.  */
  int			output_line;	/* Current output line.  */

  FILE *	debug_file;		/* File for debugging output.  */
  m4_obstack	trace_messages;
  int		exit_status;		/* Cumulative exit status.  */
  int		current_diversion;	/* Current output diversion.  */

  /* Option flags  (set in src/main.c).  */
  bool		no_gnu_extensions;		/* -G */
  size_t	nesting_limit;			/* -L */
  int		debug_level;			/* -d */
  size_t	max_debug_arg_length;		/* -l */
  int		regexp_syntax;			/* -r */
  int		opt_flags;

  /* __PRIVATE__: */
  m4__search_path_info	*search_path;	/* The list of path directories. */
};

#define M4_OPT_PREFIX_BUILTINS_BIT	(1 << 0) /* -P */
#define M4_OPT_SUPPRESS_WARN_BIT	(1 << 1) /* -Q */
#define M4_OPT_DISCARD_COMMENTS_BIT	(1 << 2) /* -c */
#define M4_OPT_INTERACTIVE_BIT		(1 << 3) /* -e */
#define M4_OPT_SYNCOUTPUT_BIT		(1 << 4) /* -s */
#define M4_OPT_POSIXLY_CORRECT_BIT	(1 << 5) /* POSIXLY_CORRECT */
#define M4_OPT_FATAL_WARN_BIT		(1 << 6) /* -E */
#define M4_OPT_SAFER_BIT		(1 << 7) /* --safer */

/* Fast macro versions of accessor functions for public fields of m4,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
#  define m4_get_symbol_table(C)		((C)->symtab)
#  define m4_set_symbol_table(C, V)		((C)->symtab = (V))
#  define m4_get_syntax_table(C)		((C)->syntax)
#  define m4_set_syntax_table(C, V)		((C)->syntax = (V))
#  define m4_get_current_file(C)		((C)->current_file)
#  define m4_set_current_file(C, V)		((C)->current_file = (V))
#  define m4_get_current_line(C)		((C)->current_line)
#  define m4_set_current_line(C, V)		((C)->current_line = (V))
#  define m4_get_output_line(C)			((C)->output_line)
#  define m4_set_output_line(C, V)		((C)->output_line = (V))
#  define m4_get_debug_file(C)			((C)->debug_file)
#  define m4_set_debug_file(C, V)		((C)->debug_file = (V))
#  define m4_get_trace_messages(C)		((C)->trace_messages)
#  define m4_set_trace_messages(C, V)		((C)->trace_messages = (V))
#  define m4_get_exit_status(C)			((C)->exit_status)
#  define m4_set_exit_status(C, V)		((C)->exit_status = (V))
#  define m4_get_current_diversion(C)		((C)->current_diversion)
#  define m4_set_current_diversion(C, V)	((C)->current_diversion = (V))
#  define m4_get_no_gnu_extensions_opt(C)	((C)->no_gnu_extensions)
#  define m4_set_no_gnu_extensions_opt(C, V)	((C)->no_gnu_extensions = (V))
#  define m4_get_nesting_limit_opt(C)		((C)->nesting_limit)
#  define m4_set_nesting_limit_opt(C, V)	((C)->nesting_limit = (V))
#  define m4_get_debug_level_opt(C)		((C)->debug_level)
#  define m4_set_debug_level_opt(C, V)		((C)->debug_level = (V))
#  define m4_get_max_debug_arg_length_opt(C)	((C)->max_debug_arg_length)
#  define m4_set_max_debug_arg_length_opt(C, V)	((C)->max_debug_arg_length=(V))
#  define m4_get_regexp_syntax_opt(C)		((C)->regexp_syntax)
#  define m4_set_regexp_syntax_opt(C, V)	((C)->regexp_syntax = (V))

#  define m4_get_prefix_builtins_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_PREFIX_BUILTINS_BIT))
#  define m4_get_suppress_warnings_opt(C)				\
		(BIT_TEST((C)->opt_flags, M4_OPT_SUPPRESS_WARN_BIT))
#  define m4_get_discard_comments_opt(C)				\
		(BIT_TEST((C)->opt_flags, M4_OPT_DISCARD_COMMENTS_BIT))
#  define m4_get_interactive_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_INTERACTIVE_BIT))
#  define m4_get_syncoutput_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_SYNCOUTPUT_BIT))
#  define m4_get_posixly_correct_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_POSIXLY_CORRECT_BIT))
#  define m4_get_fatal_warnings_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_FATAL_WARN_BIT))
#  define m4_get_safer_opt(C)						\
		(BIT_TEST((C)->opt_flags, M4_OPT_SAFER_BIT))

/* No fast opt bit set macros, as they would need to evaluate their
   arguments more than once, which would subtly change their semantics.  */
#endif

/* Accessors for private fields of m4, which have no function version
   exported in m4module.h.  */
#define m4__get_search_path(C)			((C)->search_path)



/* --- MODULE MANAGEMENT --- */

#define USER_MODULE_PATH_ENV	"M4MODPATH"
#define BUILTIN_SYMBOL		"m4_builtin_table"
#define MACRO_SYMBOL		"m4_macro_table"
#define INIT_SYMBOL		"m4_init_module"
#define FINISH_SYMBOL		"m4_finish_module"

extern void	    m4__module_init (m4 *context);
extern lt_dlhandle  m4__module_open (m4 *context, const char *name,
				     m4_obstack *obs);
extern void	    m4__module_exit (m4 *context);
extern lt_dlhandle  m4__module_next (lt_dlhandle);
extern lt_dlhandle  m4__module_find (const char *name);



/* --- SYMBOL TABLE MANAGEMENT --- */

struct m4_symbol
{
  bool		traced;
  m4_symbol_value *	value;
};

struct m4_symbol_value {
  m4_symbol_value *	next;
  lt_dlhandle		handle;
  int			flags;

  m4_hash *		arg_signature;
  unsigned int		min_args;
  unsigned int		max_args;
  size_t		pending_expansions;

  m4__symbol_type	type;
  union {
    const char *	text; /* Valid when type is TEXT, PLACEHOLDER.  */
    const m4_builtin *	builtin; /* Valid when type is FUNC.  */
  } u;
};

#define VALUE_NEXT(T)		((T)->next)
#define VALUE_HANDLE(T)		((T)->handle)
#define VALUE_FLAGS(T)		((T)->flags)
#define VALUE_ARG_SIGNATURE(T)	((T)->arg_signature)
#define VALUE_MIN_ARGS(T)	((T)->min_args)
#define VALUE_MAX_ARGS(T)	((T)->max_args)
#define VALUE_PENDING(T)	((T)->pending_expansions)

#define SYMBOL_NEXT(S)		(VALUE_NEXT	     ((S)->value))
#define SYMBOL_HANDLE(S)	(VALUE_HANDLE	     ((S)->value))
#define SYMBOL_FLAGS(S)		(VALUE_FLAGS	     ((S)->value))
#define SYMBOL_ARG_SIGNATURE(S)	(VALUE_ARG_SIGNATURE ((S)->value))
#define SYMBOL_MIN_ARGS(S)	(VALUE_MIN_ARGS      ((S)->value))
#define SYMBOL_MAX_ARGS(S)	(VALUE_MAX_ARGS      ((S)->value))
#define SYMBOL_PENDING(S)	(VALUE_PENDING       ((S)->value))

/* Fast macro versions of symbol table accessor functions,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
#  define m4_get_symbol_traced(S)	((S)->traced)
#  define m4_get_symbol_value(S)	((S)->value)
#  define m4_set_symbol_value(S, V)	((S)->value = (V))

#  define m4_symbol_value_create()	xzalloc (sizeof (m4_symbol_value))
/* m4_symbol_value_delete is too complex for a simple macro.  */

#  define m4_is_symbol_value_text(V)	((V)->type == M4_SYMBOL_TEXT)
#  define m4_is_symbol_value_func(V)	((V)->type == M4_SYMBOL_FUNC)
#  define m4_is_symbol_value_void(V)	((V)->type == M4_SYMBOL_VOID)
#  define m4_is_symbol_value_placeholder(V)				\
					((V)->type == M4_SYMBOL_PLACEHOLDER)
#  define m4_get_symbol_value_text(V)	((V)->u.text)
#  define m4_get_symbol_value_func(V)	((V)->u.builtin->func)
#  define m4_get_symbol_value_builtin(V) ((V)->u.builtin)
#  define m4_get_symbol_value_placeholder(V)				\
					((V)->u.text)
#  define m4_symbol_value_groks_macro(V) (BIT_TEST ((V)->flags,		\
						    VALUE_MACRO_ARGS_BIT))

#  define m4_set_symbol_value_text(V, T)				\
	((V)->type = M4_SYMBOL_TEXT, (V)->u.text = (T))
#  define m4_set_symbol_value_builtin(V, B)				\
	((V)->type = M4_SYMBOL_FUNC, (V)->u.builtin = (B))
#  define m4_set_symbol_value_placeholder(V, T)				\
	((V)->type = M4_SYMBOL_PLACEHOLDER, (V)->u.text = (T))
#endif



/* m4_symbol_value.flags bit masks.  Be sure these are a consistent
   superset of the M4_BUILTIN_* bit masks, so we can copy
   m4_builtin.flags to m4_symbol_arg.flags.  We can use additional
   bits for private use.  */

#define VALUE_MACRO_ARGS_BIT		(1 << 0)
#define VALUE_BLIND_ARGS_BIT		(1 << 1)
#define VALUE_SIDE_EFFECT_ARGS_BIT	(1 << 2)
#define VALUE_DELETED_BIT		(1 << 3)


struct m4_symbol_arg {
  int		index;
  int		flags;
  char *	default_val;
};

#define SYMBOL_ARG_INDEX(A)	((A)->index)
#define SYMBOL_ARG_FLAGS(A)	((A)->flags)
#define SYMBOL_ARG_DEFAULT(A)	((A)->default_val)

/* m4_symbol_arg.flags bit masks:  */

#define SYMBOL_ARG_REST_BIT	(1 << 0)
#define SYMBOL_ARG_KEY_BIT	(1 << 1)

extern void m4__symtab_remove_module_references (m4_symbol_table*, lt_dlhandle);




/* --- SYNTAX TABLE MANAGEMENT --- */

/* CHAR_RETRY must be last, because we size the syntax table to hold
   all other characters and sentinels. */
#define CHAR_EOF	256	/* character return on EOF */
#define CHAR_BUILTIN	257	/* character return for BUILTIN token */
#define CHAR_RETRY	258	/* character return for end of input block */

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

typedef struct {
  char *string;		/* characters of the string */
  size_t length;	/* length of the string */
} m4_string;

struct m4_syntax_table {
  /* Please read the comment at the top of input.c for details.  table
     holds the current syntax, and orig holds the default syntax.  */
  unsigned short table[CHAR_RETRY];
  unsigned short orig[CHAR_RETRY];

  m4_string lquote;
  m4_string rquote;
  m4_string bcomm;
  m4_string ecomm;

  /* True iff strlen(lquote) == strlen(rquote) == 1 and lquote is not
     interfering with macro names.  */
  bool is_single_quotes;

  /* True iff strlen(bcomm) == strlen(ecomm) == 1 and bcomm is not
     interfering with macros or quotes.  */
  bool is_single_comments;

  /* True iff some character has M4_SYNTAX_ESCAPE.  */
  bool is_macro_escaped;
};

/* Fast macro versions of syntax table accessor functions,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
#  define m4_get_syntax_lquote(S)		((S)->lquote.string)
#  define m4_get_syntax_rquote(S)		((S)->rquote.string)
#  define m4_get_syntax_bcomm(S)		((S)->bcomm.string)
#  define m4_get_syntax_ecomm(S)		((S)->ecomm.string)

#  define m4_is_syntax_single_quotes(S)		((S)->is_single_quotes)
#  define m4_is_syntax_single_comments(S)	((S)->is_single_comments)
#  define m4_is_syntax_macro_escaped(S)		((S)->is_macro_escaped)
#endif


/* Various different token types.  */
typedef enum {
  M4_TOKEN_EOF,			/* end of file */
  M4_TOKEN_NONE,		/* discardable token */
  M4_TOKEN_STRING,		/* a quoted string */
  M4_TOKEN_SPACE,		/* whitespace */
  M4_TOKEN_WORD,		/* an identifier */
  M4_TOKEN_OPEN,		/* argument list start */
  M4_TOKEN_COMMA,		/* argument separator */
  M4_TOKEN_CLOSE,		/* argument list end */
  M4_TOKEN_SIMPLE,		/* a single character */
  M4_TOKEN_MACDEF		/* a macro's definition (see "defn") */
} m4__token_type;

extern	m4__token_type	m4__next_token (m4 *, m4_symbol_value *);
extern	bool		m4__next_token_is_open (m4 *);



/* --- PATH MANAGEMENT --- */

typedef struct m4__search_path m4__search_path;

struct m4__search_path {
  m4__search_path *next;	/* next directory to search */
  const char *dir;		/* directory */
  int len;
};

struct m4__search_path_info {
  m4__search_path *list;	/* the list of path directories */
  m4__search_path *list_end;	/* the end of same */
  int max_length;		/* length of longest directory name */
};

extern void m4__include_init (m4 *);


/* Debugging the memory allocator.  */

#if WITH_DMALLOC
#  define DMALLOC_FUNC_CHECK
#  include <dmalloc.h>
#endif /* WITH_DMALLOC */



/* Convenience macro to zero a variable after freeing it.  */
#define DELETE(Expr)	((Expr) = (free ((void *) Expr), (void *) 0))


#if DEBUG
# define DEBUG_INCL
# define DEBUG_INPUT
# define DEBUG_MODULES
# define DEBUG_OUTPUT
# define DEBUG_STKOVF
# define DEBUG_SYM
# define DEBUG_SYNTAX
#endif

#endif /* m4private.h */

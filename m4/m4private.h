/* GNU m4 -- A simple macro processor
   Copyright 1989-1994, 1998-1999 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307  USA
*/

#ifndef M4PRIVATE_H
#define M4PRIVATE_H 1

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <assert.h>

#include "m4module.h"

typedef enum {
  M4_SYMBOL_VOID,
  M4_SYMBOL_TEXT,
  M4_SYMBOL_FUNC
} m4__symbol_type;

#define BIT_TEST(flags, bit)	(((flags) & (bit)) == (bit))
#define BIT_SET(flags, bit)	((flags) |= (bit))
#define BIT_RESET(flags, bit)	((flags) &= ~(bit))


/* --- CONTEXT MANAGEMENT --- */

struct m4 {
  m4_symbol_table *symtab;
  m4_syntax_table *syntax;

  FILE *	 debug_file;		/* File for debugging output.  */
  m4_obstack trace_messages;

  /* Option flags  (set in src/main.c).  */
  int		warning_status;			/* -E */
  boolean	no_gnu_extensions;		/* -G */
  int		nesting_limit;			/* -L */
  int		debug_level;			/* -d */
  int		max_debug_arg_length;		/* -l */
  int		opt_flags;
};

#define M4_OPT_PREFIX_BUILTINS_BIT	(1 << 0) /* -P */
#define M4_OPT_SUPPRESS_WARN_BIT	(1 << 1) /* -Q */
#define M4_OPT_DISCARD_COMMENTS_BIT	(1 << 2) /* -c */
#define M4_OPT_INTERACTIVE_BIT		(1 << 3) /* -e */
#define M4_OPT_SYNC_OUTPUT_BIT		(1 << 4) /* -s */
#define M4_OPT_POSIXLY_CORRECT_BIT	(1 << 5) /* POSIXLY_CORRECT */

#ifdef NDEBUG
#  define m4_get_symbol_table(C)		((C)->symtab)
#  define m4_get_syntax_table(C)		((C)->syntax)
#  define m4_get_debug_file(C)			((C)->debug_file)
#  define m4_get_trace_messages(C)		((C)->trace_messages)
#  define m4_get_warning_status_opt(C)		((C)->warning_status)
#  define m4_get_no_gnu_extensions_opt(C)	((C)->no_gnu_extensions)
#  define m4_get_nesting_limit_opt(C)		((C)->nesting_limit)
#  define m4_get_debug_level_opt(C)		((C)->debug_level)
#  define m4_get_max_debug_arg_length_opt(C)	((C)->max_debug_arg_length)

#  define m4_get_prefix_builtins_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_PREFIX_BUILTINS_BIT))
#  define m4_get_suppress_warnings_opt(C)				\
		(BIT_TEST((C)->opt_flags, M4_OPT_SUPPRESS_WARN_BIT))
#  define m4_get_discard_comments_opt(C)				\
		(BIT_TEST((C)->opt_flags, M4_OPT_DISCARD_COMMENTS_BIT))
#  define m4_get_interactive_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_INTERACTIVE_BIT))
#  define m4_get_sync_output_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_SYNC_OUTPUT_BIT))
#  define m4_get_posixly_correct_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_POSIXLY_CORRECT_BIT))
#endif



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



/* --- SYMBOL TABLE MANAGEMENT --- */

struct m4_symbol
{
  boolean		traced;
  m4_symbol_value *	value;
};

struct m4_symbol_value {
  m4_symbol_value *	next;
  lt_dlhandle		handle;
  int			flags;

  m4_hash *		arg_signature;
  int			min_args, max_args;

  m4__symbol_type	type;
  union {
    char *		text;
    m4_builtin_func *	func;
  } u;
};

#define VALUE_NEXT(T)		((T)->next)
#define VALUE_HANDLE(T) 	((T)->handle)
#define VALUE_FLAGS(T)		((T)->flags)
#define VALUE_ARG_SIGNATURE(T) 	((T)->arg_signature)
#define VALUE_MIN_ARGS(T)	((T)->min_args)
#define VALUE_MAX_ARGS(T)	((T)->max_args)

#define SYMBOL_NEXT(S)		(VALUE_NEXT          ((S)->value))
#define SYMBOL_HANDLE(S)	(VALUE_HANDLE        ((S)->value))
#define SYMBOL_FLAGS(S)		(VALUE_FLAGS         ((S)->value))
#define SYMBOL_ARG_SIGNATURE(S)	(VALUE_ARG_SIGNATURE ((S)->value))
#define SYMBOL_MIN_ARGS(S)	(VALUE_MIN_ARGS      ((S)->value))
#define SYMBOL_MAX_ARGS(S)	(VALUE_MAX_ARGS      ((S)->value))

#ifdef NDEBUG
#  define m4_get_symbol_traced(S)	((S)->traced)
#  define m4_set_symbol_traced(S, V)	((S)->traced = (V))

#  define m4_symbol_value_create()	(XCALLOC (m4_symbol_value, 1))
#  define m4_symbol_value_delete(V)	(XFREE (V))

#  define m4_is_symbol_value_text(V)	((V)->type == M4_SYMBOL_TEXT)
#  define m4_is_symbol_value_func(V)	((V)->type == M4_SYMBOL_FUNC)
#  define m4_is_symbol_value_void(V)	((V)->type == M4_SYMBOL_VOID)
#  define m4_get_symbol_value_text(V)	((V)->u.text)
#  define m4_get_symbol_value_func(V)	((V)->u.func)

#  define m4_set_symbol_value_text(V, T)				\
	((V)->type = M4_SYMBOL_TEXT, (V)->u.text = (T))
#  define m4_set_symbol_value_func(V, F)				\
	((V)->type = M4_SYMBOL_FUNC, (V)->u.func = (F))
#endif



/* m4_symbol_value.flags bit masks:  */

#define VALUE_MACRO_ARGS_BIT	(1 << 0)
#define VALUE_BLIND_ARGS_BIT	(1 << 1)


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

extern void	m4__symtab_remove_module_references (m4_symbol_table*, lt_dlhandle);




/* --- SYNTAX TABLE MANAGEMENT --- */

#define CHAR_EOF	256	/* character return on EOF */
#define CHAR_BUILTIN	257	/* character return for BUILTIN token */
#define CHAR_RETRY	258	/* character return for end of input block */

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

struct m4_syntax_table {
  /* Please read the comment at the top of input.c for details */
  unsigned short table[256];

  m4_string lquote, rquote;
  m4_string bcomm, ecomm;

  /* TRUE iff strlen(rquote) == strlen(lquote) == 1 */
  boolean is_single_quotes;

  /* TRUE iff strlen(bcomm) == strlen(ecomm) == 1 */
  boolean is_single_comments;

  /* TRUE iff some character has M4_SYNTAX_ESCAPE */
  boolean is_macro_escaped;
};

#ifdef NDEBUG
#  define m4_get_syntax_lquote(S)	((S)->lquote.string)
#  define m4_get_syntax_rquote(S)	((S)->rquote.string)
#  define m4_get_syntax_bcomm(S)	((S)->bcomm.string)
#  define m4_get_syntax_ecomm(S)	((S)->ecomm.string)

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
  M4_TOKEN_SIMPLE,		/* a single character */
  M4_TOKEN_MACDEF		/* a macros definition (see "defn") */
} m4__token_type;

extern	m4__token_type m4__next_token (m4 *context, m4_symbol_value *);



/* Debugging the memory allocator.  */

#if WITH_DMALLOC
#  define DMALLOC_FUNC_CHECK
#  include <dmalloc.h>

/* Dmalloc expects us to use a void returning xfree.  */
#  undef XFREE
#  define XFREE(p)	if (p) xfree (p)

#endif /* WITH_DMALLOC */

/* Other debug stuff.  */

#if DEBUG
# define DEBUG_INPUT
# define DEBUG_MACRO
# define DEBUG_SYM
# define DEBUG_INCL
# define DEBUG_MODULE
#endif

#endif /* m4private.h */

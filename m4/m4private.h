/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1998, 1999, 2004, 2005,
   2006, 2007 Free Software Foundation, Inc.

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

#ifndef M4PRIVATE_H
#define M4PRIVATE_H 1

#include <m4/m4module.h>
#include <ltdl.h>

#include "cloexec.h"

typedef struct m4__search_path_info m4__search_path_info;

typedef enum {
  M4_SYMBOL_VOID,		/* Traced but undefined, u is invalid.  */
  M4_SYMBOL_TEXT,		/* Plain text, u.u_t is valid.  */
  M4_SYMBOL_FUNC,		/* Builtin function, u.func is valid.  */
  M4_SYMBOL_PLACEHOLDER,	/* Placeholder for unknown builtin from -R.  */
  M4_SYMBOL_COMP		/* Composite symbol, u.chain is valid.  */
} m4__symbol_type;

#define BIT_TEST(flags, bit)	(((flags) & (bit)) == (bit))
#define BIT_SET(flags, bit)	((flags) |= (bit))
#define BIT_RESET(flags, bit)	((flags) &= ~(bit))

/* Gnulib's stdbool doesn't work with bool bitfields.  For nicer
   debugging, use bool when we know it works, but use the more
   portable unsigned int elsewhere.  */
#if __GNUC__ > 2
typedef bool bool_bitfield;
#else
typedef unsigned int bool_bitfield;
#endif /* !__GNUC__ */


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
#define M4_OPT_POSIXLY_CORRECT_BIT	(1 << 5) /* -G/POSIXLY_CORRECT */
#define M4_OPT_FATAL_WARN_BIT		(1 << 6) /* -E once */
#define M4_OPT_WARN_EXIT_BIT		(1 << 7) /* -E twice */
#define M4_OPT_SAFER_BIT		(1 << 8) /* --safer */

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
#  define m4_get_warnings_exit_opt(C)					\
		(BIT_TEST((C)->opt_flags, M4_OPT_WARN_EXIT_BIT))
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

struct m4_module
{
  lt_dlhandle	handle;		/* ltdl module information.  */
  int		refcount;	/* count of loads not matched by unload.  */
  /* TODO: add struct members, such as copy of builtins (so that we
     can store additional information about builtins, and so that the
     list isn't changed by the module behind our backs once we have
     initialized it) or cached pointers (to reduce the number of calls
     needed to lt_dlsym).  */
};

extern void	    m4__module_init (m4 *context);
extern m4_module *  m4__module_open (m4 *context, const char *name,
				     m4_obstack *obs);
extern void	    m4__module_exit (m4 *context);
extern m4_module *  m4__module_next (m4_module *);
extern m4_module *  m4__module_find (const char *name);

/* Fast macro versions of symbol table accessor functions, that also
   have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
# define m4_module_refcount(M)	((M)->refcount)
#endif


/* --- SYMBOL TABLE MANAGEMENT --- */

typedef struct m4_symbol_chain m4_symbol_chain;

struct m4_symbol
{
  bool traced;			/* True if this symbol is traced.  */
  m4_symbol_value *value;	/* Linked list of pushdef'd values.  */
};

/* Composite symbols are built of a linked list of chain objects.  */
struct m4_symbol_chain
{
  m4_symbol_chain *next;/* Pointer to next link of chain.  */
  char *str;		/* NUL-terminated string if text, or NULL.  */
  size_t len;		/* Length of str, or 0.  */
  m4_macro_args *argv;	/* Reference to earlier $@.  */
  unsigned int index;	/* Argument index within argv.  */
  bool flatten;		/* True to treat builtins as text.  */
};

/* A symbol value is used both for values associated with a macro
   name, and for arguments to a macro invocation.  */
struct m4_symbol_value
{
  m4_symbol_value *	next;
  m4_module *		module;
  unsigned int		flags;

  m4_hash *		arg_signature;
  unsigned int		min_args;
  unsigned int		max_args;
  size_t		pending_expansions;

  m4__symbol_type	type;
  union
  {
    struct
    {
      size_t		len;	/* Length of string.  */
      const char *	text;	/* String contents.  */
    } u_t;			/* Valid when type is TEXT, PLACEHOLDER.  */
    const m4_builtin *	builtin;/* Valid when type is FUNC.  */
    m4_symbol_chain *	chain;	/* Valid when type is COMP.  */
  } u;
};

/* Structure describing all arguments to a macro, including the macro
   name at index 0.  */
struct m4_macro_args
{
  /* One more than the highest actual argument.  May be larger than
     arraylen since the array can refer to multiple arguments via a
     single $@ reference.  */
  unsigned int argc;
  /* False unless the macro expansion refers to $@; determines whether
     this object can be freed at end of macro expansion or must wait
     until all references have been rescanned.  */
  bool_bitfield inuse : 1;
  /* False if all arguments are just text or func, true if this argv
     refers to another one.  */
  bool_bitfield has_ref : 1;
  const char *argv0; /* The macro name being expanded.  */
  size_t argv0_len; /* Length of argv0.  */
  size_t arraylen; /* True length of allocated elements in array.  */
  /* Used as a variable-length array, storing information about each
     argument.  */
  m4_symbol_value *array[FLEXIBLE_ARRAY_MEMBER];
};

#define VALUE_NEXT(T)		((T)->next)
#define VALUE_MODULE(T)		((T)->module)
#define VALUE_FLAGS(T)		((T)->flags)
#define VALUE_ARG_SIGNATURE(T)	((T)->arg_signature)
#define VALUE_MIN_ARGS(T)	((T)->min_args)
#define VALUE_MAX_ARGS(T)	((T)->max_args)
#define VALUE_PENDING(T)	((T)->pending_expansions)

#define SYMBOL_NEXT(S)		(VALUE_NEXT		((S)->value))
#define SYMBOL_MODULE(S)	(VALUE_MODULE		((S)->value))
#define SYMBOL_FLAGS(S)		(VALUE_FLAGS		((S)->value))
#define SYMBOL_ARG_SIGNATURE(S)	(VALUE_ARG_SIGNATURE	((S)->value))
#define SYMBOL_MIN_ARGS(S)	(VALUE_MIN_ARGS		((S)->value))
#define SYMBOL_MAX_ARGS(S)	(VALUE_MAX_ARGS		((S)->value))
#define SYMBOL_PENDING(S)	(VALUE_PENDING		((S)->value))

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
#  define m4_get_symbol_value_text(V)	((V)->u.u_t.text)
#  define m4_get_symbol_value_len(V)	((V)->u.u_t.len)
#  define m4_get_symbol_value_func(V)	((V)->u.builtin->func)
#  define m4_get_symbol_value_builtin(V) ((V)->u.builtin)
#  define m4_get_symbol_value_placeholder(V)				\
					((V)->u.u_t.text)
#  define m4_symbol_value_groks_macro(V) (BIT_TEST ((V)->flags,		\
						    VALUE_MACRO_ARGS_BIT))

#  define m4_set_symbol_value_text(V, T, L)				\
  ((V)->type = M4_SYMBOL_TEXT, (V)->u.u_t.text = (T), (V)->u.u_t.len = (L))
#  define m4_set_symbol_value_builtin(V, B)				\
  ((V)->type = M4_SYMBOL_FUNC, (V)->u.builtin = (B))
#  define m4_set_symbol_value_placeholder(V, T)				\
  ((V)->type = M4_SYMBOL_PLACEHOLDER, (V)->u.u_t.text = (T))
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

extern void m4__symtab_remove_module_references (m4_symbol_table*,
						 m4_module *);




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


/* --- MACRO MANAGEMENT --- */

/* Various different token types.  */
typedef enum {
  M4_TOKEN_EOF,		/* End of file, M4_SYMBOL_VOID.  */
  M4_TOKEN_NONE,	/* Discardable token, M4_SYMBOL_VOID.  */
  M4_TOKEN_STRING,	/* Quoted string or comment, M4_SYMBOL_TEXT or
			   M4_SYMBOL_COMP.  */
  M4_TOKEN_SPACE,	/* Whitespace, M4_SYMBOL_TEXT.  */
  M4_TOKEN_WORD,	/* An identifier, M4_SYMBOL_TEXT.  */
  M4_TOKEN_OPEN,	/* Argument list start, M4_SYMBOL_TEXT.  */
  M4_TOKEN_COMMA,	/* Argument separator, M4_SYMBOL_TEXT.  */
  M4_TOKEN_CLOSE,	/* Argument list end, M4_SYMBOL_TEXT.  */
  M4_TOKEN_SIMPLE,	/* Single character, M4_SYMBOL_TEXT.  */
  M4_TOKEN_MACDEF	/* Macro's definition (see "defn"), M4_SYMBOL_FUNC.  */
} m4__token_type;

extern	m4__token_type	m4__next_token (m4 *, m4_symbol_value *, int *,
					const char *);
extern	bool		m4__next_token_is_open (m4 *);

/* Fast macro versions of macro argv accessor functions,
   that also have an identically named function exported in m4module.h.  */
#ifdef NDEBUG
# define m4_arg_argc(A)		(A)->argc
#endif /* NDEBUG */


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

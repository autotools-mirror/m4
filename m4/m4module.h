/* GNU m4 -- A simple macro processor

   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1999, 2000, 2003,
   2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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

#ifndef M4MODULE_H
#define M4MODULE_H 1

#include <m4/hash.h>
#include <m4/system.h>

BEGIN_C_DECLS



/* --- MODULE AUTHOR DECLARATIONS --- */

typedef struct m4		m4;
typedef struct m4_builtin	m4_builtin;
typedef struct m4_macro		m4_macro;
typedef struct m4_symbol_value	m4_symbol_value;
typedef struct m4_input_block	m4_input_block;
typedef struct m4_module	m4_module;
typedef struct m4_macro_args	m4_macro_args;

typedef struct obstack		m4_obstack;

typedef void   m4_builtin_func  (m4 *, m4_obstack *, unsigned int,
				 m4_macro_args *);

/* The value of m4_builtin flags is built from these:  */
enum {
  /* Set if macro can handle non-text tokens, such as builtin macro
     tokens; if clear, non-text tokens are flattened to the empty
     string before invoking the builtin.  */
  M4_BUILTIN_GROKS_MACRO	= (1 << 0),
  /* Set if macro should only be recognized with arguments; may only
     be set if min_args is nonzero.  */
  M4_BUILTIN_BLIND		= (1 << 1),
  /* Set if macro has side effects even when there are too few
     arguments; may only be set if min_args is nonzero.  */
  M4_BUILTIN_SIDE_EFFECT	= (1 << 2),

  /* Mask of valid flag bits.  Any other bits must be set to 0.  */
  M4_BUILTIN_FLAGS_MASK		= (1 << 3) - 1
};

struct m4_builtin
{
  m4_builtin_func * func;	/* implementation of the builtin */
  const char *	    name;	/* name found by builtin, printed by dumpdef */
  int		    flags;	/* bitwise OR of M4_BUILTIN_* bits */
  unsigned int	    min_args;	/* 0-based minimum number of arguments */
  /* max arguments, UINT_MAX if unlimited; must be >= min_args */
  unsigned int	    max_args;
};

struct m4_macro
{
  const char *name;
  const char *value;
};

#define M4BUILTIN(name)							\
  static void CONC (builtin_, name)					\
   (m4 *context, m4_obstack *obs, unsigned int argc, m4_macro_args *argv);

#define M4BUILTIN_HANDLER(name)						\
  static void CONC (builtin_, name)					\
   (m4 *context, m4_obstack *obs, unsigned int argc, m4_macro_args *argv)

#define M4INIT_HANDLER(name)						\
  void CONC (name, CONC (_LTX_, m4_init_module))			\
       (m4 *context, m4_module *module, m4_obstack *obs);		\
  void CONC (name, CONC (_LTX_, m4_init_module))			\
	(m4 *context, m4_module *module, m4_obstack *obs)

#define M4FINISH_HANDLER(name)						\
  void CONC (name, CONC (_LTX_, m4_finish_module))			\
       (m4 *context, m4_module *module, m4_obstack *obs);		\
  void CONC (name, CONC (_LTX_, m4_finish_module))			\
	(m4 *context, m4_module *module, m4_obstack *obs)

#define M4_MODULE_IMPORT(M, S)						\
  CONC (S, _func) *S = (CONC (S, _func) *)				\
	m4_module_import (context, STR (M), STR (S), obs)

/* Grab the text contents of argument I, or abort if the argument is
   not text.  Assumes that `m4_macro_args *argv' is in scope.  */
#define M4ARG(i) m4_arg_text (argv, i)

extern bool	m4_bad_argc	   (m4 *, int, const char *,
				    unsigned int, unsigned int, bool);
extern bool	m4_numeric_arg	   (m4 *, const char *, const char *, int *);
extern void	m4_dump_args	   (m4 *, m4_obstack *, unsigned int,
				    m4_macro_args *, const char *, bool);
extern bool	m4_parse_truth_arg (m4 *, const char *, const char *, bool);

/* Error handling.  */
extern void m4_error (m4 *, int, int, const char *, const char *, ...)
  M4_GNUC_PRINTF (5, 6);
extern void m4_error_at_line (m4 *, int, int, const char *, int,
			      const char *, const char *, ...)
  M4_GNUC_PRINTF (7, 8);
extern void m4_warn  (m4 *, int, const char *, const char *, ...)
  M4_GNUC_PRINTF (4, 5);
extern void m4_warn_at_line  (m4 *, int, const char *, int, const char *,
			      const char *, ...)
  M4_GNUC_PRINTF (6, 7);

extern const char *	m4_get_program_name (void);
extern void		m4_set_program_name (const char *);
extern void		m4_set_exit_failure (int);


/* --- CONTEXT MANAGEMENT --- */

typedef struct m4_syntax_table	m4_syntax_table;
typedef struct m4_symbol_table	m4_symbol_table;
typedef struct m4_symbol	m4_symbol;

extern m4 *		m4_create	(void);
extern void		m4_delete	(m4 *);

#define m4_context_field_table						\
	M4FIELD(m4_symbol_table *, symbol_table,   symtab)		\
	M4FIELD(m4_syntax_table *, syntax_table,   syntax)		\
	M4FIELD(const char *,	   current_file,   current_file)	\
	M4FIELD(int,		   current_line,   current_line)	\
	M4FIELD(int,		   output_line,	   output_line)		\
	M4FIELD(FILE *,		   debug_file,	   debug_file)		\
	M4FIELD(m4_obstack,	   trace_messages, trace_messages)	\
	M4FIELD(int,		   exit_status,	   exit_status)		\
	M4FIELD(int,	current_diversion,	   current_diversion)	\
	M4FIELD(size_t,	nesting_limit_opt,	   nesting_limit)	\
	M4FIELD(int,	debug_level_opt,	   debug_level)		\
	M4FIELD(size_t,	max_debug_arg_length_opt,  max_debug_arg_length)\
	M4FIELD(int,	regexp_syntax_opt,	   regexp_syntax)	\


#define m4_context_opt_bit_table					\
	M4OPT_BIT(M4_OPT_PREFIX_BUILTINS_BIT,	prefix_builtins_opt)	\
	M4OPT_BIT(M4_OPT_SUPPRESS_WARN_BIT,	suppress_warnings_opt)	\
	M4OPT_BIT(M4_OPT_DISCARD_COMMENTS_BIT,	discard_comments_opt)	\
	M4OPT_BIT(M4_OPT_INTERACTIVE_BIT,	interactive_opt)	\
	M4OPT_BIT(M4_OPT_SYNCOUTPUT_BIT,	syncoutput_opt)		\
	M4OPT_BIT(M4_OPT_POSIXLY_CORRECT_BIT,	posixly_correct_opt)	\
	M4OPT_BIT(M4_OPT_FATAL_WARN_BIT,	fatal_warnings_opt)	\
	M4OPT_BIT(M4_OPT_WARN_EXIT_BIT,		warnings_exit_opt)	\
	M4OPT_BIT(M4_OPT_SAFER_BIT,		safer_opt)		\


#define M4FIELD(type, base, field)					\
	extern type CONC (m4_get_, base) (m4 *context);			\
	extern type CONC (m4_set_, base) (m4 *context, type value);
m4_context_field_table
#undef M4FIELD

#define M4OPT_BIT(bit, base)						\
  extern bool CONC (m4_get_, base) (m4 *context);			\
  extern bool CONC (m4_set_, base) (m4 *context, bool value);
m4_context_opt_bit_table
#undef M4OPT_BIT

#define M4SYMTAB	(m4_get_symbol_table (context))
#define M4SYNTAX	(m4_get_syntax_table (context))



/* --- MODULE MANAGEMENT --- */

typedef void m4_module_init_func   (m4 *, m4_module *, m4_obstack *);
typedef void m4_module_finish_func (m4 *, m4_module *, m4_obstack *);

extern m4_module *  m4_module_load     (m4 *, const char*, m4_obstack*);
extern const char * m4_module_makeresident (m4_module *);
extern int	    m4_module_refcount (const m4_module *);
extern void	    m4_module_unload   (m4 *, const char*, m4_obstack*);
extern void *	    m4_module_import   (m4 *, const char*, const char*,
					m4_obstack*);

extern const char * m4_get_module_name (const m4_module *);
extern void	    m4__module_exit    (m4 *);



/* --- SYMBOL TABLE MANAGEMENT --- */


typedef void *m4_symtab_apply_func (m4_symbol_table *, const char *,
				    m4_symbol *, void *);

extern m4_symbol_table *m4_symtab_create  (size_t);
extern void	  m4_symtab_delete  (m4_symbol_table*);
extern void *	  m4_symtab_apply   (m4_symbol_table*, bool,
				     m4_symtab_apply_func*, void*);

extern m4_symbol *m4_symbol_lookup  (m4_symbol_table*, const char *);
extern m4_symbol *m4_symbol_pushdef (m4_symbol_table*,
				     const char *, m4_symbol_value *);
extern m4_symbol *m4_symbol_define  (m4_symbol_table*,
				     const char *, m4_symbol_value *);
extern void       m4_symbol_popdef  (m4_symbol_table*, const char *);
extern m4_symbol *m4_symbol_rename  (m4_symbol_table*, const char *,
				     const char *);

extern void       m4_symbol_delete  (m4_symbol_table*, const char *);

#define m4_symbol_delete(symtab, name)			M4_STMT_START {	\
	while (m4_symbol_lookup ((symtab), (name)))			\
	    m4_symbol_popdef ((symtab), (name));	} M4_STMT_END

extern m4_symbol_value *m4_get_symbol_value	  (m4_symbol*);
extern bool		m4_get_symbol_traced	  (m4_symbol*);
extern bool		m4_set_symbol_name_traced (m4_symbol_table*,
						   const char *, bool);
extern void	m4_symbol_value_print	(m4_symbol_value *, m4_obstack *, bool,
					 const char *, const char *, size_t,
					 bool);
extern void	m4_symbol_print		(m4_symbol *, m4_obstack *, bool,
					 const char *, const char *, bool,
					 size_t, bool);
extern bool	m4_symbol_value_groks_macro	(m4_symbol_value *);

#define m4_is_symbol_void(symbol)					\
	(m4_is_symbol_value_void (m4_get_symbol_value (symbol)))
#define m4_is_symbol_text(symbol)					\
	(m4_is_symbol_value_text (m4_get_symbol_value (symbol)))
#define m4_is_symbol_func(symbol)					\
	(m4_is_symbol_value_func (m4_get_symbol_value (symbol)))
#define m4_is_symbol_placeholder(symbol)				\
	(m4_is_symbol_value_placeholder (m4_get_symbol_value (symbol)))
#define m4_get_symbol_text(symbol)					\
	(m4_get_symbol_value_text (m4_get_symbol_value (symbol)))
#define m4_get_symbol_len(symbol)					\
	(m4_get_symbol_value_len (m4_get_symbol_value (symbol)))
#define m4_get_symbol_func(symbol)					\
	(m4_get_symbol_value_func (m4_get_symbol_value (symbol)))
#define m4_get_symbol_builtin(symbol)					\
	(m4_get_symbol_value_builtin (m4_get_symbol_value (symbol)))
#define m4_get_symbol_placeholder(symbol)				\
	(m4_get_symbol_value_placeholder (m4_get_symbol_value (symbol)))
#define m4_symbol_groks_macro(symbol)					\
	(m4_symbol_value_groks_macro (m4_get_symbol_value (symbol)))

extern m4_symbol_value *m4_symbol_value_create	  (void);
extern void		m4_symbol_value_delete	  (m4_symbol_value *);
extern void		m4_symbol_value_copy	  (m4_symbol_value *,
						   m4_symbol_value *);
extern bool		m4_is_symbol_value_text   (m4_symbol_value *);
extern bool		m4_is_symbol_value_func   (m4_symbol_value *);
extern bool		m4_is_symbol_value_placeholder  (m4_symbol_value *);
extern bool		m4_is_symbol_value_void	  (m4_symbol_value *);

extern const char *	m4_get_symbol_value_text  (m4_symbol_value *);
extern size_t		m4_get_symbol_value_len   (m4_symbol_value *);
extern unsigned int	m4_get_symbol_value_quote_age	(m4_symbol_value *);

extern m4_builtin_func *m4_get_symbol_value_func  (m4_symbol_value *);
extern const m4_builtin *m4_get_symbol_value_builtin	(m4_symbol_value *);
extern const char *	m4_get_symbol_value_placeholder	(m4_symbol_value *);

extern void		m4_set_symbol_value_text  (m4_symbol_value *,
						   const char *, size_t,
						   unsigned int);
extern void		m4_set_symbol_value_builtin	(m4_symbol_value *,
							 const m4_builtin *);
extern void		m4_set_symbol_value_placeholder	(m4_symbol_value *,
							 const char *);



/* --- BUILTIN MANAGEMENT --- */

extern m4_symbol_value	*m4_builtin_find_by_name (m4_module *, const char *);
extern const m4_builtin	*m4_builtin_find_by_func (m4_module *,
						  m4_builtin_func *);



/* --- MACRO MANAGEMENT --- */

extern void	m4_macro_expand_input	(m4 *);
extern void	m4_macro_call		(m4 *, m4_symbol_value *,
					 m4_obstack *, unsigned int,
					 m4_macro_args *);
extern unsigned int m4_arg_argc		(m4_macro_args *);
extern m4_symbol_value *m4_arg_symbol	(m4_macro_args *, unsigned int);
extern bool	m4_is_arg_text		(m4_macro_args *, unsigned int);
extern bool	m4_is_arg_func		(m4_macro_args *, unsigned int);
extern const char *m4_arg_text		(m4_macro_args *, unsigned int);
extern bool	m4_arg_equal		(m4_macro_args *, unsigned int,
					 unsigned int);
extern bool	m4_arg_empty		(m4_macro_args *, unsigned int);
extern size_t	m4_arg_len		(m4_macro_args *, unsigned int);
extern m4_builtin_func *m4_arg_func	(m4_macro_args *, unsigned int);
extern m4_macro_args *m4_make_argv_ref	(m4 *, m4_macro_args *, const char *,
                                         size_t, bool, bool);
extern void	m4_push_arg		(m4 *, m4_obstack *, m4_macro_args *,
					 unsigned int);
extern void	m4_push_args		(m4 *, m4_obstack *, m4_macro_args *,
					 bool, bool);


/* --- RUNTIME DEBUGGING --- */

/* The value of debug_level is a bitmask of the following:  */
enum {
  /* a: show arglist in trace output */
  M4_DEBUG_TRACE_ARGS		= (1 << 0),
  /* e: show expansion in trace output */
  M4_DEBUG_TRACE_EXPANSION	= (1 << 1),
  /* q: quote args and expansion in trace output */
  M4_DEBUG_TRACE_QUOTE		= (1 << 2),
  /* t: trace all macros -- overrides trace{on,off} */
  M4_DEBUG_TRACE_ALL		= (1 << 3),
  /* l: add line numbers to trace output */
  M4_DEBUG_TRACE_LINE		= (1 << 4),
  /* f: add file name to trace output */
  M4_DEBUG_TRACE_FILE		= (1 << 5),
  /* p: trace path search of include files */
  M4_DEBUG_TRACE_PATH		= (1 << 6),
  /* c: show macro call before args collection */
  M4_DEBUG_TRACE_CALL		= (1 << 7),
  /* i: trace changes of input files */
  M4_DEBUG_TRACE_INPUT		= (1 << 8),
  /* x: add call id to trace output */
  M4_DEBUG_TRACE_CALLID		= (1 << 9),
  /* m: trace module actions */
  M4_DEBUG_TRACE_MODULE		= (1 << 10),
  /* s: trace pushdef stacks */
  M4_DEBUG_TRACE_STACK		= (1 << 11),

  /* V: very verbose --  print everything */
  M4_DEBUG_TRACE_VERBOSE	= ((1 << 12) - 1)
};

/* default flags -- equiv: aeq */
#define M4_DEBUG_TRACE_DEFAULT		\
	(M4_DEBUG_TRACE_ARGS | M4_DEBUG_TRACE_EXPANSION | M4_DEBUG_TRACE_QUOTE)

#define m4_is_debug_bit(C,B)	((m4_get_debug_level_opt (C) & (B)) != 0)

extern int	m4_debug_decode		(m4 *, int, const char *);
extern bool	m4_debug_set_output	(m4 *, const char *, const char *);
extern void	m4_debug_message_prefix (m4 *);
extern void	m4_debug_message	(m4 *, int, const char *, ...)
  M4_GNUC_PRINTF (3, 4);



/* --- REGEXP SYNTAX --- */

extern const char *	m4_regexp_syntax_decode	(int);
extern int		m4_regexp_syntax_encode	(const char *);



/* --- SYNTAX TABLE DEFINITIONS --- */

extern	m4_syntax_table	*m4_syntax_create	(void);
extern	void		 m4_syntax_delete	(m4_syntax_table *syntax);
extern	int		 m4_syntax_code		(char ch);

extern	const char *	 m4_get_syntax_lquote	(m4_syntax_table *syntax);
extern	const char *	 m4_get_syntax_rquote	(m4_syntax_table *syntax);
extern	const char *	 m4_get_syntax_bcomm	(m4_syntax_table *syntax);
extern	const char *	 m4_get_syntax_ecomm	(m4_syntax_table *syntax);

extern	bool		 m4_is_syntax_single_quotes	(m4_syntax_table *);
extern	bool		 m4_is_syntax_single_comments	(m4_syntax_table *);
extern	bool		 m4_is_syntax_macro_escaped	(m4_syntax_table *);

/* These are values to be assigned to syntax table entries.  Although
   they are bit masks for fast categorization in m4__next_token(),
   only one value per syntax table entry is allowed.  The enumeration
   is currently sorted in order of parsing precedence.  */
enum {
  M4_SYNTAX_IGNORE		= 0,
  M4_SYNTAX_ESCAPE		= 1 << 0,
  M4_SYNTAX_ALPHA		= 1 << 1,
  M4_SYNTAX_LQUOTE		= 1 << 2,
  M4_SYNTAX_BCOMM		= 1 << 3,
  M4_SYNTAX_OTHER		= 1 << 4,
  M4_SYNTAX_NUM			= 1 << 5,
  M4_SYNTAX_DOLLAR		= 1 << 6,
  M4_SYNTAX_LBRACE		= 1 << 7,
  M4_SYNTAX_RBRACE		= 1 << 8,
  M4_SYNTAX_SPACE		= 1 << 9,
  M4_SYNTAX_ACTIVE		= 1 << 10,
  M4_SYNTAX_OPEN		= 1 << 11,
  M4_SYNTAX_CLOSE		= 1 << 12,
  M4_SYNTAX_COMMA		= 1 << 13,

  /* These values are bit masks to OR with categories above, a syntax entry
     may have any number of these in addition to a maximum of one of the
     values above.  */
  M4_SYNTAX_RQUOTE		= 1 << 14,
  M4_SYNTAX_ECOMM		= 1 << 15
};

#define M4_SYNTAX_MASKS		(M4_SYNTAX_RQUOTE | M4_SYNTAX_ECOMM)
#define M4_SYNTAX_VALUE		(~(M4_SYNTAX_RQUOTE | M4_SYNTAX_ECOMM))

#define m4_syntab(S, C)		((S)->table[(C)])
/* Determine if character C matches any of the bitwise-or'd syntax
   categories T for the given syntax table S.  C can be either an
   unsigned int (including special values such as CHAR_BUILTIN) or a
   char which will be interpreted as an unsigned char.  */
#define m4_has_syntax(S, C, T)						\
  ((m4_syntab ((S), sizeof (C) == 1 ? to_uchar (C) : (C)) & (T)) > 0)

extern void	m4_set_quotes	(m4_syntax_table*, const char*, const char*);
extern void	m4_set_comment	(m4_syntax_table*, const char*, const char*);
extern int	m4_set_syntax	(m4_syntax_table*, char, char, const char*);



/* --- INPUT TOKENIZATION --- */

extern	void	m4_input_init	(m4 *context);
extern	void	m4_input_exit	(void);
extern	void	m4_skip_line	(m4 *context, const char *);

/* push back input */

extern	void	m4_push_file	(m4 *, FILE *, const char *, bool);
extern	void	m4_push_builtin	(m4 *, m4_symbol_value *);
extern	m4_obstack	*m4_push_string_init	(m4 *);
extern	m4_input_block	*m4_push_string_finish	(void);
extern	void	m4_push_wrapup	(m4 *, const char *);
extern	bool	m4_pop_wrapup	(m4 *);
extern	void	m4_input_print	(m4 *, m4_obstack *, m4_input_block *);



/* --- OUTPUT MANAGEMENT --- */

extern void	m4_output_init		(m4 *);
extern void	m4_output_exit		(void);
extern void	m4_output_text		(m4 *, const char *, size_t);
extern void	m4_divert_text		(m4 *, m4_obstack *, const char *,
					 size_t, int);
extern void	m4_shipout_int		(m4_obstack *, int);
extern void	m4_shipout_string	(m4 *, m4_obstack *, const char *,
					 size_t, bool);
extern bool	m4_shipout_string_trunc	(m4 *, m4_obstack *, const char *,
					 size_t, bool, size_t *);

extern void	m4_make_diversion    (m4 *, int);
extern void	m4_insert_diversion  (m4 *, int);
extern void	m4_insert_file	     (m4 *, FILE *);
extern void	m4_freeze_diversions (m4 *, FILE *);
extern void	m4_undivert_all	     (m4 *);



/* --- PATH MANAGEMENT --- */

extern void	m4_include_env_init	 (m4 *);
extern void	m4_add_include_directory (m4 *, const char *, bool);
extern FILE *   m4_path_search		 (m4 *, const char *, char **);



#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	free


/* Convert a possibly-signed character to an unsigned character.  This is
   a bit safer than casting to unsigned char, since it catches some type
   errors that the cast doesn't.  */
#if HAVE_INLINE
static inline unsigned char to_uchar (char ch) { return ch; }
#else
# define to_uchar(C) ((unsigned char) (C))
#endif

END_C_DECLS

#endif /* !M4MODULE_H */

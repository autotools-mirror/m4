/* GNU m4 -- A simple macro processor
   Copyright 1989-1994, 1999, 2000, 2003 Free Software Foundation, Inc.

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

#ifndef M4MODULE_H
#define M4MODULE_H 1

#include <m4/system.h>
#include <m4/error.h>
#include <m4/ltdl.h>
#include <m4/hash.h>

BEGIN_C_DECLS

#define M4_DEFAULT_NESTING_LIMIT	250



/* Various declarations.  */

typedef struct m4		m4;
typedef struct obstack		m4_obstack;
typedef struct m4_symbol_value	m4_symbol_value;
typedef void   m4_builtin_func  (m4 *, m4_obstack *, int, m4_symbol_value **);

typedef struct {
  const char *	    name;
  m4_builtin_func * func;
  boolean	    groks_macro_args, blind_if_no_args;
  int		    min_args, max_args;
} m4_builtin;

typedef struct {
  const char *name;
  const char *value;
} m4_macro;

typedef struct {
    unsigned char *string;	/* characters of the string */
    size_t length;		/* length of the string */
} m4_string;


#define M4BUILTIN(name) 					\
  static void CONC(builtin_, name) 				\
   (m4 *context, m4_obstack *obs, int argc, m4_symbol_value **argv);

#define M4BUILTIN_HANDLER(name) 				\
  static void CONC(builtin_, name)				\
   (m4 *context, m4_obstack *obs, int argc, m4_symbol_value **argv)

#define M4INIT_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_init_module)) 			\
	(m4 *context, lt_dlhandle handle, m4_obstack *obs);	\
  void CONC(name, CONC(_LTX_, m4_init_module)) 			\
	(m4 *context, lt_dlhandle handle, m4_obstack *obs)

#define M4FINISH_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_finish_module)) 		\
	(m4 *context, lt_dlhandle handle, m4_obstack *obs);	\
  void CONC(name, CONC(_LTX_, m4_finish_module)) 		\
	(m4 *context, lt_dlhandle handle, m4_obstack *obs)

#define M4ARG(i)	(argc > (i) ? m4_get_symbol_value_text (argv[i]) : "")

extern boolean 	    m4_bad_argc	      (m4 *, int, m4_symbol_value **,
				       int, int);
extern const char * m4_skip_space     (m4 *, const char *);
extern boolean	    m4_numeric_arg    (m4 *, int, m4_symbol_value **,
				       int, int *);
extern void	    m4_dump_args      (m4 *, m4_obstack *, int,
				       m4_symbol_value **, const char *,
				       boolean);
extern const char * m4_expand_ranges  (const char *, m4_obstack *);

/* Error handling.  */
#define M4ERROR(Arglist) (error Arglist)
#define M4WARN(Arglist) 				M4_STMT_START {	\
	if (!m4_get_suppress_warnings_opt (context)) M4ERROR (Arglist);	\
							} M4_STMT_END



/* --- CONTEXT MANAGEMENT --- */

typedef struct m4_syntax_table	m4_syntax_table;
typedef struct m4_symbol_table	m4_symbol_table;
typedef struct m4_symbol	m4_symbol;

extern m4 *		m4_create	(void);
extern void		m4_delete	(m4 *);

#define m4_context_field_table 						\
	M4FIELD(m4_symbol_table *, symbol_table,   symtab)		\
	M4FIELD(m4_syntax_table *, syntax_table,   syntax)		\
	M4FIELD(FILE *,		   debug_file,	   debug_file)		\
	M4FIELD(m4_obstack,	   trace_messages, trace_messages)	\
	M4FIELD(int,	 warning_status_opt,	   warning_status)	\
	M4FIELD(boolean, no_gnu_extensions_opt,    no_gnu_extensions)	\
	M4FIELD(int,	 nesting_limit_opt,	   nesting_limit)	\
	M4FIELD(int,	 debug_level_opt,	   debug_level)		\
	M4FIELD(int,	 max_debug_arg_length_opt, max_debug_arg_length)\


#define m4_context_opt_bit_table					\
	M4OPT_BIT(M4_OPT_PREFIX_BUILTINS_BIT,	prefix_builtins_opt)	\
	M4OPT_BIT(M4_OPT_SUPPRESS_WARN_BIT,	suppress_warnings_opt)	\
	M4OPT_BIT(M4_OPT_DISCARD_COMMENTS_BIT,	discard_comments_opt)	\
	M4OPT_BIT(M4_OPT_INTERACTIVE_BIT,	interactive_opt)	\
	M4OPT_BIT(M4_OPT_SYNC_OUTPUT_BIT,	sync_output_opt)	\
	M4OPT_BIT(M4_OPT_POSIXLY_CORRECT_BIT,	posixly_correct_opt)	\


#define M4FIELD(type, base, field)					\
	extern type CONC(m4_get_, base) (m4 *context);			\
	extern type CONC(m4_set_, base) (m4 *context, type value);
m4_context_field_table
#undef M4FIELD

#define M4OPT_BIT(bit, base) 						\
	extern boolean CONC(m4_get_, base) (m4 *context);		\
	extern boolean CONC(m4_set_, base) (m4 *context, boolean value);
m4_context_opt_bit_table
#undef M4OPT_BIT

#define M4SYMTAB	(m4_get_symbol_table (context))
#define M4SYNTAX	(m4_get_syntax_table (context))



/* --- MODULE MANAGEMENT --- */

typedef void m4_module_init_func   (m4 *, lt_dlhandle, m4_obstack*);
typedef void m4_module_finish_func (m4 *, lt_dlhandle, m4_obstack*);

extern lt_dlhandle  m4_module_load   (m4 *, const char*, m4_obstack*);
extern void	    m4_module_unload (m4 *, const char*, m4_obstack*);

extern const char  *m4_get_module_name		(lt_dlhandle);
extern m4_builtin  *m4_get_module_builtin_table	(lt_dlhandle);
extern m4_macro	   *m4_get_module_macro_table	(lt_dlhandle);



/* --- SYMBOL TABLE MANAGEMENT --- */


typedef void *m4_symtab_apply_func (m4_symbol_table *symtab, const char *key,
				    m4_symbol *symbol, void *userdata);

extern m4_symbol_table *m4_symtab_create  (size_t, boolean *);
extern void	  m4_symtab_delete  (m4_symbol_table*);
extern void *	  m4_symtab_apply   (m4_symbol_table*, m4_symtab_apply_func*,
				     void*);

extern m4_symbol *m4_symbol_lookup  (m4_symbol_table*, const char *);
extern m4_symbol *m4_symbol_pushdef (m4_symbol_table*,
				     const char *, m4_symbol_value *);
extern m4_symbol *m4_symbol_define  (m4_symbol_table*,
				     const char *, m4_symbol_value *);
extern void       m4_symbol_popdef  (m4_symbol_table*, const char *);
extern void       m4_symbol_delete  (m4_symbol_table*, const char *);

#define m4_symbol_delete(symtab, name)			M4_STMT_START {	\
	while (m4_symbol_lookup ((symtab), (name)))			\
 	    m4_symbol_popdef ((symtab), (name));	} M4_STMT_END

extern m4_symbol_value *m4_get_symbol_value	  (m4_symbol*);
extern boolean		m4_get_symbol_traced	  (m4_symbol*);
extern boolean		m4_set_symbol_traced	  (m4_symbol*, boolean);
extern boolean		m4_set_symbol_name_traced (m4_symbol_table*,
						   const char *);

#define m4_is_symbol_text(symbol)					\
	(m4_is_symbol_value_text (m4_get_symbol_value (symbol)))
#define m4_is_symbol_func(symbol)					\
	(m4_is_symbol_value_func (m4_get_symbol_value (symbol)))
#define m4_get_symbol_text(symbol)					\
	(m4_get_symbol_value_text (m4_get_symbol_value (symbol)))
#define m4_get_symbol_func(symbol)					\
	(m4_get_symbol_value_func (m4_get_symbol_value (symbol)))

extern m4_symbol_value *m4_symbol_value_create	  (void);
extern void		m4_symbol_value_delete	  (m4_symbol_value *);
extern void		m4_symbol_value_copy	  (m4_symbol_value *,
						   m4_symbol_value *);
extern boolean		m4_is_symbol_value_text   (m4_symbol_value *);
extern boolean		m4_is_symbol_value_func   (m4_symbol_value *);
extern char	       *m4_get_symbol_value_text  (m4_symbol_value *);
extern m4_builtin_func *m4_get_symbol_value_func  (m4_symbol_value *);
extern void		m4_set_symbol_value_text  (m4_symbol_value *, char *);
extern void		m4_set_symbol_value_func  (m4_symbol_value *,
						   m4_builtin_func *);



/* --- BUILTIN MANAGEMENT --- */

extern const m4_builtin *m4_builtin_find_by_name (
				const m4_builtin *, const char *);
extern const m4_builtin *m4_builtin_find_by_func (
				const m4_builtin *, m4_builtin_func *);



/* --- MACRO MANAGEMENT --- */

extern void 	   m4_macro_expand_input (m4 *);
extern void	   m4_macro_call	 (m4 *, m4_symbol *, m4_obstack *,
					  int, m4_symbol_value **);



/* --- RUNTIME DEBUGGING --- */

/* The value of debug_level is a bitmask of the following:  */
enum {
  /* a: show arglist in trace output */
  M4_DEBUG_TRACE_ARGS 		= (1 << 0),
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

  /* V: very verbose --  print everything */
  M4_DEBUG_TRACE_VERBOSE	= (~0)
};

/* default flags -- equiv: aeq */
#define M4_DEBUG_TRACE_DEFAULT		\
	(M4_DEBUG_TRACE_ARGS|M4_DEBUG_TRACE_EXPANSION|M4_DEBUG_TRACE_QUOTE)

#define m4_is_debug_bit(C,B)	(BIT_TEST (m4_get_debug_level_opt (C), (B)))

extern int	m4_debug_decode		(m4 *, const char *);
extern void	m4_debug_flush_files	(m4 *);
extern boolean	m4_debug_set_output	(m4 *, const char *);
extern void	m4_debug_message_prefix (m4 *);

#define M4_DEBUG_PRINT1(C, Fmt, Arg1) 			M4_STMT_START {	\
      if (m4_get_debug_file (C) != NULL)				\
	fprintf (m4_get_debug_file (C), Fmt, Arg1);	} M4_STMT_END

#define M4_DEBUG_PRINT2(Fmt, Arg1, Arg2)		M4_STMT_START {	\
      if (m4_get_debug_file (C) != NULL)				\
	fprintf (m4_get_debug_file (C), Fmt, Arg1, Arg2);} M4_STMT_END

#define M4_DEBUG_PRINT3(Fmt, Arg1, Arg2, Arg3)		M4_STMT_START {	\
	if (m4_get_debug_file (C) != NULL)				\
	fprintf (m4_get_debug_file (C), Fmt, Arg1, Arg2, Arg3);	} M4_STMT_END

#define M4_DEBUG_MESSAGE(C, Fmt)			M4_STMT_START {	\
      if (m4_get_debug_file (C) != NULL)				\
	{								\
	  m4_debug_message_prefix (C);					\
	  fprintf (m4_get_debug_file (C), Fmt);				\
	  putc ('\n', m4_get_debug_file (C));				\
	}						} M4_STMT_END

#define M4_DEBUG_MESSAGE1(C, Fmt, Arg1)			M4_STMT_START {	\
      if (m4_get_debug_file (C) != NULL)				\
	{								\
	  m4_debug_message_prefix (C);					\
	  fprintf (m4_get_debug_file (C), Fmt, Arg1);			\
	  putc ('\n', m4_get_debug_file (C));				\
	}						} M4_STMT_END

#define M4_DEBUG_MESSAGE2(C, Fmt, Arg1, Arg2)		M4_STMT_START {	\
      if (m4_get_debug_file (C) != NULL)				\
	{								\
	  m4_debug_message_prefix (C);					\
	  fprintf (m4_get_debug_file (C), Fmt, Arg1, Arg2);		\
	  putc ('\n', m4_get_debug_file (C));				\
	}						} M4_STMT_END



/* --- SYNTAX TABLE DEFINITIONS --- */

extern	m4_syntax_table	*m4_syntax_create	(void);
extern	void		 m4_syntax_delete	(m4_syntax_table *syntax);
extern	int		 m4_syntax_code		(char ch);

extern	const char *	 m4_get_syntax_lquote	(m4_syntax_table *syntax);
extern	const char *	 m4_get_syntax_rquote	(m4_syntax_table *syntax);
extern	const char *	 m4_get_syntax_bcomm	(m4_syntax_table *syntax);
extern	const char *	 m4_get_syntax_ecomm	(m4_syntax_table *syntax);

extern	boolean		 m4_is_syntax_single_quotes	(m4_syntax_table *);
extern	boolean		 m4_is_syntax_single_comments	(m4_syntax_table *);
extern	boolean		 m4_is_syntax_macro_escaped	(m4_syntax_table *);

/* These are values to be assigned to syntax table entries, although they
   are bit masks for fast categorisation in m4__next_token(), only one
   value per syntax table entry is allowed.  */
enum {
  M4_SYNTAX_OTHER		= (1 << 0),
  M4_SYNTAX_IGNORE		= (1 << 1),
  M4_SYNTAX_SPACE		= (1 << 2),
  M4_SYNTAX_OPEN		= (1 << 3),
  M4_SYNTAX_CLOSE		= (1 << 4),
  M4_SYNTAX_COMMA		= (1 << 5),
  M4_SYNTAX_DOLLAR		= (1 << 6),
  M4_SYNTAX_ACTIVE		= (1 << 7),
  M4_SYNTAX_ESCAPE		= (1 << 8),
  M4_SYNTAX_ASSIGN		= (1 << 9),
  M4_SYNTAX_ALPHA		= (1 << 10),
  M4_SYNTAX_NUM			= (1 << 11),

  /* These values ARE bit masks to AND with categories above, a syntax entry
     may have any number of these in addition to a maximum of one of the
     values above.  */
  M4_SYNTAX_LQUOTE		= (1 << 12),
  M4_SYNTAX_RQUOTE		= (1 << 13),
  M4_SYNTAX_BCOMM		= (1 << 14),
  M4_SYNTAX_ECOMM		= (1 << 15),
};

#define M4_SYNTAX_MASKS		(M4_SYNTAX_LQUOTE|M4_SYNTAX_RQUOTE|M4_SYNTAX_BCOMM|M4_SYNTAX_ECOMM)
#define M4_SYNTAX_VALUE		(~(M4_SYNTAX_RQUOTE|M4_SYNTAX_ECOMM))

#define m4_syntab(S,C)		((S)->table[(int)(C)])
#define m4_has_syntax(S,C,T)	((m4_syntab((S),(C)) & (T)) > 0)
#define m4_is_syntax(S,C,T)	((m4_syntab((S),(C)) & M4_SYNTAX_VALUE) == (T))

extern void	m4_set_quotes	(m4_syntax_table*, const char*, const char*);
extern void	m4_set_comment	(m4_syntax_table*, const char*, const char*);
extern int	m4_set_syntax	(m4_syntax_table*, char, const unsigned char*);



/* --- INPUT TOKENISATION --- */

/* current input file, and line */
extern const char *m4_current_file;
extern int m4_current_line;

extern	void	m4_input_init	(void);
extern	void	m4_input_exit	(void);
extern	int	m4_peek_input	(m4 *context);
extern	void	m4_skip_line	(m4 *context);

/* push back input */

extern	void	m4_push_file	(m4 *context, FILE *, const char *);
extern	void	m4_push_single	(int ch);
extern	void	m4_push_builtin	(m4_symbol_value *);
extern	m4_obstack *m4_push_string_init (m4 *context);
extern	const char *m4_push_string_finish (void);
extern	void	m4_push_wrapup	(const char *);
extern	boolean	m4_pop_wrapup	(void);



/* --- OUTPUT MANAGEMENT --- */

extern int m4_current_diversion;
extern int m4_output_current_line;

extern void	m4_output_init	  (void);
extern void	m4_output_exit	  (void);
extern void	m4_shipout_text	  (m4 *, m4_obstack *, const char *, int);
extern void	m4_shipout_int    (m4_obstack *, int);
extern void	m4_shipout_string (m4 *, m4_obstack *, const char *,
				   int, boolean);

extern void	m4_make_diversion    (int);
extern void	m4_insert_diversion  (int);
extern void	m4_insert_file	     (FILE *);
extern void	m4_freeze_diversions (FILE *);
extern void	m4_undivert_all	     (void);

extern void	m4_include_init          (void);
extern void	m4_include_env_init      (m4 *);
extern void	m4_add_include_directory (m4 *, const char *);
extern FILE *   m4_path_search           (m4 *, const char *, char **);

/* These are for other search paths */

struct m4_search_path
{
  struct m4_search_path *next;	/* next directory to search */
  const char *dir;		/* directory */
  int len;
};

typedef struct m4_search_path m4_search_path;

struct m4_search_path_info
{
  m4_search_path *list;		/* the list of path directories */
  m4_search_path *list_end;	/* the end of same */
  int max_length;		/* length of longest directory name */
};

extern struct m4_search_path_info *m4_search_path_info_new (void);
extern	void	m4_search_path_env_init (struct m4_search_path_info *, char *, boolean);
extern	void	m4_search_path_add (struct m4_search_path_info *, const char *);


/* The structure dump_symbol_data is used to pass the information needed
   from call to call to dump_symbol.  */

struct m4_dump_symbol_data
{
  m4_obstack *obs;		/* obstack for table */
  const char **base;		/* base of table */
  int size;			/* size of table */
};

extern void *m4_dump_symbol_CB (m4_symbol_table*, const char*, m4_symbol *, void *);
extern void m4_dump_symbols (m4 *context, struct m4_dump_symbol_data *data, int argc, m4_symbol_value **argv, boolean complain);



#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree

END_C_DECLS

#endif /* !M4MODULE_H */

/* GNU m4 -- A simple macro processor
   Copyright 1989-1994, 1999, 2000 Free Software Foundation, Inc.

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

#if HAVE_OBSTACK
#  include <obstack.h>
#else
#  include <m4/obstack.h>
#endif

#include <m4/error.h>
#include <m4/ltdl.h>
#include <m4/system.h>

BEGIN_C_DECLS


/* Various declarations.  */

typedef struct m4_symbol m4_symbol;
typedef struct m4_module_data m4_module_data;
typedef struct m4_token_data m4_token_data;

typedef void m4_builtin_func M4_PARAMS((struct obstack *, int,
					struct m4_token_data **));
typedef VOID *m4_module_func M4_PARAMS((const char *));
typedef void m4_hack_symbol ();


typedef struct {
    unsigned char *string;	/* characters of the string */
    size_t length;		/* length of the string */
} m4_string;

/* Operation modes for m4_lookup_symbol ().  */
typedef enum
{
  M4_SYMBOL_LOOKUP,
  M4_SYMBOL_INSERT,
  M4_SYMBOL_DELETE,
  M4_SYMBOL_PUSHDEF,
  M4_SYMBOL_POPDEF,
  M4_SYMBOL_IGNORE
} m4_symbol_lookup;

typedef struct {
  const char *name;
  const char *value;
} m4_macro;

typedef struct {
  const char	   *name;
  m4_builtin_func  *func;
  boolean	    groks_macro_args;
  boolean	    blind_if_no_args;
} m4_builtin;


extern void	    m4_module_init   M4_PARAMS((void));
extern lt_dlhandle  m4_module_load   M4_PARAMS((const char*, struct obstack*));
extern void	    m4_module_unload M4_PARAMS((const char*, struct obstack*));
extern lt_dlhandle  m4_module_open   M4_PARAMS((const char*, struct obstack*));
extern void	    m4_module_close  M4_PARAMS((lt_dlhandle, struct obstack*));
extern void	    m4_module_close_all M4_PARAMS((struct obstack*));

extern const char  *m4_module_name     M4_PARAMS((lt_dlhandle));
extern m4_builtin  *m4_module_builtins M4_PARAMS((lt_dlhandle));
extern m4_macro	   *m4_module_macros   M4_PARAMS((lt_dlhandle));

extern lt_dlhandle  m4_module_find_by_builtin M4_PARAMS((const m4_builtin*));


extern void m4_macro_define		M4_PARAMS((const lt_dlhandle,
				const char *, const char *, m4_symbol_lookup));
extern void m4_macro_table_install	M4_PARAMS((
				const lt_dlhandle, const m4_macro *));

extern void m4_builtin_define		M4_PARAMS((const lt_dlhandle,
				const char *, const m4_builtin *,
				m4_symbol_lookup, boolean));
extern void m4_builtin_table_install	M4_PARAMS((
				const lt_dlhandle, const m4_builtin *));

extern const m4_builtin *m4_builtin_find_by_name M4_PARAMS((
				const m4_builtin *, const char *));
extern const m4_builtin *m4_builtin_find_by_func M4_PARAMS((
				const m4_builtin *, m4_builtin_func *));

extern m4_symbol **m4_symtab;

extern void	m4_symtab_init		M4_PARAMS((void));
extern m4_symbol *m4_lookup_symbol	M4_PARAMS((const char *,
						   m4_symbol_lookup));
extern void	m4_hack_all_symbols	M4_PARAMS((m4_hack_symbol *,
						   const char *));
extern void	m4_remove_table_reference_symbols M4_PARAMS((
						m4_builtin *, m4_macro *));


/* Various different token types.  */
typedef enum {
  M4_TOKEN_EOF,			/* end of file */
  M4_TOKEN_NONE,		/* discardable token */
  M4_TOKEN_STRING,		/* a quoted string */
  M4_TOKEN_SPACE,		/* whitespace */
  M4_TOKEN_WORD,		/* an identifier */
  M4_TOKEN_SIMPLE,		/* a single character */
  M4_TOKEN_MACDEF		/* a macros definition (see "defn") */
} m4_token_t;

/* The data for a token, a macro argument, and a macro definition.  */
typedef enum {
  M4_TOKEN_VOID,
  M4_TOKEN_TEXT,
  M4_TOKEN_FUNC
} m4_token_data_t;

typedef void m4_module_init_func   M4_PARAMS((lt_dlhandle, struct obstack*));
typedef void m4_module_finish_func M4_PARAMS((lt_dlhandle, struct obstack*));

extern m4_token_data_t  m4_token_data_type	  M4_PARAMS((m4_token_data*));
extern char	       *m4_token_data_text	  M4_PARAMS((m4_token_data*));
extern char	       *m4_token_data_orig_text	  M4_PARAMS((m4_token_data*));
extern m4_builtin_func *m4_token_data_func	  M4_PARAMS((m4_token_data*));
extern boolean		m4_token_data_func_traced M4_PARAMS((m4_token_data*));


#define M4ARG(i)	(argc > (i) ? m4_token_data_text (argv[i]) : "")

#define M4BUILTIN(name) 					\
  static void CONC(builtin_, name) 				\
  M4_PARAMS((struct obstack *, int , m4_token_data **));

#define M4BUILTIN_HANDLER(name) 				\
  static void CONC(builtin_, name) (obs, argc, argv)		\
	struct obstack *obs; int argc; m4_token_data **argv;

#define M4INIT_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_init_module)) (handle, obs)	\
	lt_dlhandle handle; struct obstack *obs;

#define M4FINISH_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_finish_module)) (handle, obs)	\
	lt_dlhandle handle; struct obstack *obs;

/* Error handling.  */
#define M4ERROR(Arglist) (error Arglist)

#define HASHMAX 509		/* default, overridden by -Hsize */

/* The name this program was run with. */
M4_SCOPE const char *program_name;

/* Option flags  (defined in utility.c; set in m4.c).  */
M4_SCOPE int interactive;		/* -e */
M4_SCOPE int sync_output;		/* -s */
M4_SCOPE int debug_level;		/* -d */
M4_SCOPE int hash_table_size;		/* -H */
M4_SCOPE int no_gnu_extensions;		/* -G */
M4_SCOPE int prefix_all_builtins;	/* -P */
M4_SCOPE int max_debug_argument_length;	/* -l */
M4_SCOPE int suppress_warnings;		/* -Q */
M4_SCOPE int warning_status;		/* -E */
M4_SCOPE int nesting_limit;		/* -L */
M4_SCOPE int discard_comments;		/* -c */
M4_SCOPE const char *user_word_regexp;	/* -W */

/* left and right quote, begin and end comment */
M4_SCOPE m4_string lquote;
M4_SCOPE m4_string rquote;

M4_SCOPE m4_string bcomm;
M4_SCOPE m4_string ecomm;

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

boolean m4_bad_argc M4_PARAMS((m4_token_data *, int, int, int));
const char *m4_skip_space M4_PARAMS((const char *));
boolean m4_numeric_arg M4_PARAMS((m4_token_data *, const char *, int *));
void m4_shipout_int M4_PARAMS((struct obstack *, int));
void m4_shipout_string M4_PARAMS((struct obstack*, const char*, int, boolean));
void m4_dump_args M4_PARAMS((struct obstack *obs, int argc, m4_token_data **argv, const char *sep, boolean quoted));


M4_SCOPE FILE *m4_debug;

/* The value of debug_level is a bitmask of the following.  */

/* a: show arglist in trace output */
#define M4_DEBUG_TRACE_ARGS 1
/* e: show expansion in trace output */
#define M4_DEBUG_TRACE_EXPANSION 2
/* q: quote args and expansion in trace output */
#define M4_DEBUG_TRACE_QUOTE 4
/* t: trace all macros -- overrides trace{on,off} */
#define M4_DEBUG_TRACE_ALL 8
/* l: add line numbers to trace output */
#define M4_DEBUG_TRACE_LINE 16
/* f: add file name to trace output */
#define M4_DEBUG_TRACE_FILE 32
/* p: trace path search of include files */
#define M4_DEBUG_TRACE_PATH 64
/* c: show macro call before args collection */
#define M4_DEBUG_TRACE_CALL 128
/* i: trace changes of input files */
#define M4_DEBUG_TRACE_INPUT 256
/* x: add call id to trace output */
#define M4_DEBUG_TRACE_CALLID 512

/* V: very verbose --  print everything */
#define M4_DEBUG_TRACE_VERBOSE 1023
/* default flags -- equiv: aeq */
#define M4_DEBUG_TRACE_DEFAULT 7

#define M4_DEBUG_PRINT1(Fmt, Arg1) \
  do								\
    {								\
      if (m4_debug != NULL)					\
	fprintf (m4_debug, Fmt, Arg1);				\
    }								\
  while (0)

#define M4_DEBUG_PRINT2(Fmt, Arg1, Arg2) \
  do								\
    {								\
      if (m4_debug != NULL)					\
	fprintf (m4_debug, Fmt, Arg1, Arg2);			\
    }								\
  while (0)

#define M4_DEBUG_PRINT3(Fmt, Arg1, Arg2, Arg3) \
  do								\
    {								\
      if (m4_debug != NULL)					\
	fprintf (m4_debug, Fmt, Arg1, Arg2, Arg3);		\
    }								\
  while (0)

#define M4_DEBUG_MESSAGE(Fmt) \
  do								\
    {								\
      if (m4_debug != NULL)					\
	{							\
	  m4_debug_message_prefix ();				\
	  fprintf (m4_debug, Fmt);				\
	  putc ('\n', m4_debug);				\
	}							\
    }								\
  while (0)

#define M4_DEBUG_MESSAGE1(Fmt, Arg1) \
  do								\
    {								\
      if (m4_debug != NULL)					\
	{							\
	  m4_debug_message_prefix ();				\
	  fprintf (m4_debug, Fmt, Arg1);			\
	  putc ('\n', m4_debug);				\
	}							\
    }								\
  while (0)

#define M4_DEBUG_MESSAGE2(Fmt, Arg1, Arg2) \
  do								\
    {								\
      if (m4_debug != NULL)					\
	{							\
	  m4_debug_message_prefix ();				\
	  fprintf (m4_debug, Fmt, Arg1, Arg2);			\
	  putc ('\n', m4_debug);				\
	}							\
    }								\
  while (0)

void m4_debug_init M4_PARAMS((void));
int m4_debug_decode M4_PARAMS((const char *));
void m4_debug_flush_files M4_PARAMS((void));
boolean m4_debug_set_output M4_PARAMS((const char *));
void m4_debug_message_prefix M4_PARAMS((void));

void m4_trace_prepre M4_PARAMS((const char *, int));
void m4_trace_pre M4_PARAMS((const char *, int, int, m4_token_data **));
void m4_trace_post M4_PARAMS((const char *, int, int, m4_token_data **,
			   const char *));

/* Exit code from last "syscmd" command.  */
M4_SCOPE int m4_sysval;
M4_SCOPE int m4_expansion_level;

const char *m4_expand_ranges M4_PARAMS((const char *s, struct obstack *obs));
void m4_expand_input M4_PARAMS((void));
void m4_call_macro M4_PARAMS((m4_symbol *, int, m4_token_data **, struct obstack *));
void m4_process_macro M4_PARAMS((struct obstack *obs, m4_symbol *symbol, int argc, m4_token_data **argv));



/* --- SYNTAX TABLE DEFINITIONS --- */

/* These are simple values, not bit masks.  There is no overlap. */
#define M4_SYNTAX_OTHER		(0x0000)

#define M4_SYNTAX_IGNORE	(0x0001)
#define M4_SYNTAX_SPACE		(0x0002)
#define M4_SYNTAX_OPEN		(0x0003)
#define M4_SYNTAX_CLOSE		(0x0004)
#define M4_SYNTAX_COMMA		(0x0005)
#define M4_SYNTAX_DOLLAR	(0x0006) /* not used yet */
#define M4_SYNTAX_ACTIVE	(0x0007)
#define M4_SYNTAX_ESCAPE	(0x0008)

/* These are values to be assigned to syntax table entries, but they are
   used as bit masks with M4_IS_ALNUM.*/
#define M4_SYNTAX_ALPHA		(0x0010)
#define M4_SYNTAX_NUM		(0x0020)
#define M4_SYNTAX_ALNUM		(M4_SYNTAX_ALPHA|M4_SYNTAX_NUM)

/* These are bit masks to AND with other categories.
   See input.c for details. */
#define M4_SYNTAX_LQUOTE	(0x0100)
#define M4_SYNTAX_RQUOTE	(0x0200)
#define M4_SYNTAX_BCOMM		(0x0400)
#define M4_SYNTAX_ECOMM		(0x0800)

/* These bits define the syntax code of a character */
#define M4_SYNTAX_VALUE		(0x00FF|M4_SYNTAX_LQUOTE|M4_SYNTAX_BCOMM)
#define M4_SYNTAX_MASKS		(0xFF00)


#define m4_syntax(ch)	m4_syntax_table[(int)(ch)]

#define M4_IS_OTHER(ch)  ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_OTHER)
#define M4_IS_IGNORE(ch) ((m4_syntax(ch)) == M4_SYNTAX_IGNORE)
#define M4_IS_SPACE(ch)  ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_SPACE)

#define M4_IS_OPEN(ch)   ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_OPEN)
#define M4_IS_CLOSE(ch)  ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_CLOSE)
#define M4_IS_COMMA(ch)  ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_COMMA)
#define M4_IS_DOLLAR(ch) ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_DOLLAR)
#define M4_IS_ACTIVE(ch) ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_ACTIVE)

#define M4_IS_ESCAPE(ch) ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_ESCAPE)
#define M4_IS_ALPHA(ch)  ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_ALPHA)
#define M4_IS_NUM(ch)    ((m4_syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_NUM)
#define M4_IS_ALNUM(ch)  (((m4_syntax(ch)) & M4_SYNTAX_ALNUM) != 0)

#define M4_IS_LQUOTE(ch) (m4_syntax(ch) & M4_SYNTAX_LQUOTE)
#define M4_IS_RQUOTE(ch) (m4_syntax(ch) & M4_SYNTAX_RQUOTE)
#define M4_IS_BCOMM(ch)  (m4_syntax(ch) & M4_SYNTAX_BCOMM)
#define M4_IS_ECOMM(ch)  (m4_syntax(ch) & M4_SYNTAX_ECOMM)

/* Please read the comment at the top of input.c for details */
M4_SCOPE unsigned short m4_syntax_table[256];

/* current input file, and line */
M4_SCOPE const char *m4_current_file;
M4_SCOPE int m4_current_line;

extern	int	m4_syntax_code	M4_PARAMS((char ch));
extern	void	m4_input_init	M4_PARAMS((void));
extern	void	m4_syntax_init	M4_PARAMS((void));
extern	int	m4_peek_input	M4_PARAMS((void));
extern	m4_token_t m4_next_token M4_PARAMS((m4_token_data *));
extern	void	m4_skip_line	M4_PARAMS((void));

/* push back input */
extern	void	m4_push_file	M4_PARAMS((FILE *, const char *));
extern	void	m4_push_single	M4_PARAMS((int ch));
extern	void	m4_push_macro	M4_PARAMS((m4_builtin_func *, lt_dlhandle,
					   boolean));
extern	struct obstack *m4_push_string_init M4_PARAMS((void));
extern	const char *m4_push_string_finish M4_PARAMS((void));
extern	void	m4_push_wrapup	M4_PARAMS((const char *));
extern	boolean	m4_pop_wrapup	M4_PARAMS((void));

extern	void	m4_set_quotes	M4_PARAMS((const char *, const char *));
extern	void	m4_set_comment	M4_PARAMS((const char *, const char *));
extern	void	m4_set_syntax	M4_PARAMS((char, const unsigned char *));
#ifdef ENABLE_CHANGEWORD
extern	void	m4_set_word_regexp M4_PARAMS((const char *));
#endif

M4_SCOPE int m4_current_diversion;
M4_SCOPE int m4_output_current_line;

extern	void	m4_output_init	M4_PARAMS((void));
extern	void	m4_shipout_text	M4_PARAMS((struct obstack *, const char *, int));
extern	void	m4_make_diversion M4_PARAMS((int));
extern	void	m4_insert_diversion M4_PARAMS((int));
extern	void	m4_insert_file	M4_PARAMS((FILE *));
extern	void	m4_freeze_diversions M4_PARAMS((FILE *));
extern	void	m4_undivert_all	M4_PARAMS((void));

extern	void	m4_include_init M4_PARAMS((void));
extern	void	m4_include_env_init M4_PARAMS((void));
extern	void	m4_add_include_directory M4_PARAMS((const char *));
extern	FILE   *m4_path_search M4_PARAMS((const char *, char **));

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

extern struct m4_search_path_info *m4_search_path_info_new M4_PARAMS((void));
extern	void	m4_search_path_env_init M4_PARAMS((struct m4_search_path_info *, char *, boolean));
extern	void	m4_search_path_add M4_PARAMS((struct m4_search_path_info *, const char *));


/* The structure dump_symbol_data is used to pass the information needed
   from call to call to dump_symbol.  */

struct m4_dump_symbol_data
{
  struct obstack *obs;		/* obstack for table */
  m4_symbol **base;		/* base of table */
  int size;			/* size of table */
};

extern void m4_dump_symbol M4_PARAMS((m4_symbol *symbol, struct m4_dump_symbol_data *data));
extern void m4_dump_symbols M4_PARAMS((struct m4_dump_symbol_data *data, int argc, m4_token_data **argv, boolean complain));




/* --- EXPRESSION EVALUATION --- */

typedef boolean (*m4_eval_func) M4_PARAMS((struct obstack *obs,
				const char *expr, const int radix, int min));

extern boolean m4_evaluate M4_PARAMS((struct obstack *obs,
				      const char *, const int radix, int min));
extern void m4_do_eval M4_PARAMS((struct obstack *obs, int argc, m4_token_data **argv, m4_eval_func func));

#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree

END_C_DECLS

#endif /* !M4MODULE_H */

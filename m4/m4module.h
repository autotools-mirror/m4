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

#include <m4/system.h>
#include <m4/error.h>
#include <m4/ltdl.h>
#include <m4/hash.h>

BEGIN_C_DECLS


/* Various declarations.  */

typedef struct m4_symbol m4_symbol;
typedef struct m4_module_data m4_module_data;
typedef struct m4_token_data m4_token_data;

typedef void m4_builtin_func (struct obstack *, int, struct m4_token_data **);
typedef void *m4_module_func (const char *);
typedef int m4_symtab_apply_func (const char *name, m4_symbol *symbol, void *data);

typedef struct {
    unsigned char *string;	/* characters of the string */
    size_t length;		/* length of the string */
} m4_string;

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


extern void	    m4_module_init   (void);
extern lt_dlhandle  m4_module_load   (const char*, struct obstack*);
extern void	    m4_module_unload (const char*, struct obstack*);
extern lt_dlhandle  m4_module_open   (const char*, struct obstack*);
extern void	    m4_module_close  (lt_dlhandle, struct obstack*);
extern void	    m4_module_close_all (struct obstack*);

extern const char  *m4_module_name     (lt_dlhandle);
extern m4_builtin  *m4_module_builtins (lt_dlhandle);
extern m4_macro	   *m4_module_macros   (lt_dlhandle);

extern lt_dlhandle  m4_module_find_by_builtin (const m4_builtin*);


extern m4_symbol *m4_macro_pushdef	(const char *name, lt_dlhandle handle,
					 const char *text);
extern m4_symbol *m4_macro_define	(const char *name, lt_dlhandle handle,
					 const char *text);
extern void	  m4_macro_table_install (lt_dlhandle handle,
					  const m4_macro *table);

extern m4_symbol *m4_builtin_pushdef	(const char *name, lt_dlhandle handle,
					 const m4_builtin *bp);
extern m4_symbol *m4_builtin_define	(const char *name, lt_dlhandle handle,
					 const m4_builtin *bp);
extern void	  m4_builtin_table_install (lt_dlhandle handle,
					    const m4_builtin *table);

extern const m4_builtin *m4_builtin_find_by_name (
				const m4_builtin *, const char *);
extern const m4_builtin *m4_builtin_find_by_func (
				const m4_builtin *, m4_builtin_func *);

extern m4_hash *m4_symtab;

extern void	m4_symtab_init		(void);
extern int	m4_symtab_apply		(m4_symtab_apply_func *, void *);
extern void	m4_symtab_remove_module_references (lt_dlhandle);

extern m4_symbol *m4_symbol_lookup	(const char *);
extern m4_symbol *m4_symbol_pushdef	(const char *);
extern m4_symbol *m4_symbol_define	(const char *);
extern void       m4_symbol_popdef	(const char *);
extern void       m4_symbol_delete	(const char *);
extern void	  m4_symbol_builtin	(m4_symbol *symbol, lt_dlhandle handle,
					 const m4_builtin *bp);
extern void	  m4_symbol_macro	(m4_symbol *symbol, lt_dlhandle handle,
					 const char *text);


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

typedef void m4_module_init_func   (lt_dlhandle, struct obstack*);
typedef void m4_module_finish_func (lt_dlhandle, struct obstack*);

extern m4_token_data_t  m4_token_data_type	  (m4_token_data*);
extern char	       *m4_token_data_text	  (m4_token_data*);
extern m4_builtin_func *m4_token_data_func	  (m4_token_data*);


#define M4ARG(i)	(argc > (i) ? m4_token_data_text (argv[i]) : "")

#define M4BUILTIN(name) 					\
  static void CONC(builtin_, name) 				\
  	(struct obstack *, int , m4_token_data **);

#define M4BUILTIN_HANDLER(name) 				\
  static void CONC(builtin_, name) (obs, argc, argv)		\
	struct obstack *obs; int argc; m4_token_data **argv;

#define M4INIT_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_init_module)) 			\
	(lt_dlhandle handle, struct obstack *obs);		\
  void CONC(name, CONC(_LTX_, m4_init_module)) 			\
	(lt_dlhandle handle, struct obstack *obs)

#define M4FINISH_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_finish_module)) 		\
	(lt_dlhandle handle, struct obstack *obs);		\
  void CONC(name, CONC(_LTX_, m4_finish_module)) 		\
	(lt_dlhandle handle, struct obstack *obs)

/* Error handling.  */
#define M4ERROR(Arglist) (error Arglist)
#define M4WARN(Arglist) \
  do								\
    {								\
       if (!suppress_warnings)                                  \
         M4ERROR (Arglist);					\
    }								\
  while (0)

/* The name this program was run with. */
const char *program_name;

/* Option flags  (defined in utility.c; set in m4.c).  */
int interactive;		/* -e */
int sync_output;		/* -s */
int debug_level;		/* -d */
int hash_table_size;		/* -H */
int no_gnu_extensions;		/* -G */
int prefix_all_builtins;	/* -P */
int max_debug_argument_length;	/* -l */
int suppress_warnings;		/* -Q */
int warning_status;		/* -E */
int nesting_limit;		/* -L */
int discard_comments;		/* -c */

/* left and right quote, begin and end comment */
m4_string lquote;
m4_string rquote;

m4_string bcomm;
m4_string ecomm;

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

boolean m4_bad_argc (m4_token_data *, int, int, int);
const char *m4_skip_space (const char *);
boolean m4_numeric_arg (m4_token_data *, const char *, int *);
void m4_shipout_int (struct obstack *, int);
void m4_shipout_string (struct obstack*, const char*, int, boolean);
void m4_dump_args (struct obstack *obs, int argc, m4_token_data **argv, const char *sep, boolean quoted);


FILE *m4_debug;

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

void m4_debug_init (void);
int m4_debug_decode (const char *);
void m4_debug_flush_files (void);
boolean m4_debug_set_output (const char *);
void m4_debug_message_prefix (void);

void m4_trace_prepre (const char *, int);
void m4_trace_pre (const char *, int, int, m4_token_data **);
void m4_trace_post (const char *, int, int, m4_token_data **,
			   const char *);

/* Exit code from last "syscmd" command.  */
int m4_sysval;
int m4_expansion_level;

const char *m4_expand_ranges (const char *s, struct obstack *obs);
void m4_expand_input (void);
void m4_call_macro (m4_symbol *, int, m4_token_data **, struct obstack *);
void m4_process_macro (struct obstack *obs, m4_symbol *symbol, int argc, m4_token_data **argv);



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
unsigned short m4_syntax_table[256];

/* current input file, and line */
const char *m4_current_file;
int m4_current_line;

extern	int	m4_syntax_code	(char ch);
extern	void	m4_input_init	(void);
extern	void	m4_syntax_init	(void);
extern	int	m4_peek_input	(void);
extern	m4_token_t m4_next_token (m4_token_data *);
extern	void	m4_skip_line	(void);

/* push back input */

extern	void	m4_push_file	(FILE *, const char *);
extern	void	m4_push_single	(int ch);
extern	void	m4_push_macro	(m4_builtin_func *, lt_dlhandle,
					   boolean);
extern	struct obstack *m4_push_string_init (void);
extern	const char *m4_push_string_finish (void);
extern	void	m4_push_wrapup	(const char *);
extern	boolean	m4_pop_wrapup	(void);

extern	void	m4_set_quotes	(const char *, const char *);
extern	void	m4_set_comment	(const char *, const char *);
extern	void	m4_set_syntax	(char, const unsigned char *);

int m4_current_diversion;
int m4_output_current_line;

extern	void	m4_output_init	(void);
extern	void	m4_shipout_text	(struct obstack *, const char *, int);
extern	void	m4_make_diversion (int);
extern	void	m4_insert_diversion (int);
extern	void	m4_insert_file	(FILE *);
extern	void	m4_freeze_diversions (FILE *);
extern	void	m4_undivert_all	(void);

extern	void	m4_include_init (void);
extern	void	m4_include_env_init (void);
extern	void	m4_add_include_directory (const char *);
extern	FILE   *m4_path_search (const char *, char **);

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
  struct obstack *obs;		/* obstack for table */
  const char **base;		/* base of table */
  int size;			/* size of table */
};

extern int m4_dump_symbol (const char *name, m4_symbol *symbol, void *data);
extern void m4_dump_symbols (struct m4_dump_symbol_data *data, int argc, m4_token_data **argv, boolean complain);




/* --- EXPRESSION EVALUATION --- */

typedef boolean (*m4_eval_func) (struct obstack *obs,
				const char *expr, const int radix, int min);

extern boolean m4_evaluate (struct obstack *obs,
				      const char *, const int radix, int min);
extern void m4_do_eval (struct obstack *obs, int argc, m4_token_data **argv, m4_eval_func func);

#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree

END_C_DECLS

#endif /* !M4MODULE_H */

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


/* Various declarations.  */

typedef struct m4		m4;
typedef struct m4_symbol	m4_symbol;
typedef struct m4_symbol_value		m4_symbol_value;
typedef struct m4_hash		m4_symtab;


typedef void m4_builtin_func (m4 *, struct obstack *, int, m4_symbol_value **);
typedef void *m4_module_func (const char *);

typedef struct {
    unsigned char *string;	/* characters of the string */
    size_t length;		/* length of the string */
} m4_string;

typedef struct {
  const char *name;
  const char *value;
} m4_macro;

typedef struct {
  const char *	    name;
  m4_builtin_func * func;
  boolean	    groks_macro_args, blind_if_no_args;
  int		    min_args, max_args;
} m4_builtin;



/* --- CONTEXT MANAGEMENT --- */

extern m4 *	  m4_create	(void);
extern void	  m4_delete	(m4 *);
extern m4_symtab *m4_get_symtab	(m4 *);

#define M4SYMTAB	(m4_get_symtab (context))



/* --- MODULE MANAGEMENT --- */

typedef void m4_module_init_func   (m4 *, lt_dlhandle, struct obstack*);
typedef void m4_module_finish_func (m4 *, lt_dlhandle, struct obstack*);

extern lt_dlhandle  m4_module_load   (m4 *, const char*, struct obstack*);
extern void	    m4_module_unload (m4 *, const char*, struct obstack*);

extern const char  *m4_get_module_name		(lt_dlhandle);
extern m4_builtin  *m4_get_module_builtin_table	(lt_dlhandle);
extern m4_macro	   *m4_get_module_macro_table	(lt_dlhandle);

extern void	m4_set_module_macro_table   (m4 *context, lt_dlhandle handle,
					     const m4_macro *table);
extern void	m4_set_module_builtin_table (m4 *context, lt_dlhandle handle,
					     const m4_builtin *table);


/* --- SYMBOL TABLE MANAGEMENT --- */


typedef int m4_symtab_apply_func (m4_symtab *symtab, const void *key,
				  void *value, void *data);

extern m4_symtab *m4_symtab_create  (size_t);
extern void	  m4_symtab_delete  (m4_symtab*);
extern void *	  m4_symtab_apply   (m4_symtab*, m4_symtab_apply_func*, void*);

#define m4_symtab_apply(symtab, func, userdata)				\
 (m4_hash_apply ((m4_hash*)(symtab), (m4_hash_apply_func*)(func), (userdata)))


extern m4_symbol *m4_symbol_lookup  (m4_symtab*, const char *);
extern m4_symbol *m4_symbol_pushdef (m4_symtab*, const char *, m4_symbol_value *);
extern m4_symbol *m4_symbol_define  (m4_symtab*, const char *, m4_symbol_value *);
extern void       m4_symbol_popdef  (m4_symtab*, const char *);
extern void       m4_symbol_delete  (m4_symtab*, const char *);

#define m4_symbol_delete(symtab, name)			M4_STMT_START {	\
	while (m4_symbol_lookup ((symtab), (name)))			\
 	    m4_symbol_popdef ((symtab), (name));	} M4_STMT_END

extern void	  m4_set_symbol_traced (m4_symtab*, const char *);



/* The data for a token, a macro argument, and a macro definition.  */
typedef enum {
  M4_SYMBOL_VOID,
  M4_SYMBOL_TEXT,
  M4_SYMBOL_FUNC
} m4_symbol_type;




/* --- MACRO (and builtin) MANAGEMENT --- */

extern const m4_builtin *m4_builtin_find_by_name (
				const m4_builtin *, const char *);
extern const m4_builtin *m4_builtin_find_by_func (
				const m4_builtin *, m4_builtin_func *);

extern m4_symbol_type	m4_get_symbol_value_type (m4_symbol_value *);
extern char	       *m4_get_symbol_value_text (m4_symbol_value *);
extern m4_builtin_func *m4_get_symbol_value_func (m4_symbol_value *);

#define M4ARG(i)	(argc > (i) ? m4_get_symbol_value_text (argv[i]) : "")

#define M4BUILTIN(name) 					\
  static void CONC(builtin_, name) 				\
   (m4 *context, struct obstack *obs, int argc, m4_symbol_value **argv);

#define M4BUILTIN_HANDLER(name) 				\
  static void CONC(builtin_, name)				\
   (m4 *context, struct obstack *obs, int argc, m4_symbol_value **argv)

#define M4INIT_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_init_module)) 			\
	(m4 *context, lt_dlhandle handle, struct obstack *obs);	\
  void CONC(name, CONC(_LTX_, m4_init_module)) 			\
	(m4 *context, lt_dlhandle handle, struct obstack *obs)

#define M4FINISH_HANDLER(name)					\
  void CONC(name, CONC(_LTX_, m4_finish_module)) 		\
	(m4 *context, lt_dlhandle handle, struct obstack *obs);	\
  void CONC(name, CONC(_LTX_, m4_finish_module)) 		\
	(m4 *context, lt_dlhandle handle, struct obstack *obs)

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
#ifdef _LIBC
/* In the GNU C library, there is a predefined variable for this.  */
# define program_name program_invocation_name
#endif
extern const char *program_name;

/* Option flags  (defined in utility.c; set in m4.c).  */
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

/* left and right quote, begin and end comment */
extern m4_string lquote;
extern m4_string rquote;

extern m4_string bcomm;
extern m4_string ecomm;

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

extern boolean m4_bad_argc (int, m4_symbol_value **, int, int);
extern const char *m4_skip_space (const char *);
extern boolean m4_numeric_arg (int, m4_symbol_value **, int, int *);
extern void m4_shipout_int (struct obstack *, int);
extern void m4_shipout_string (struct obstack*, const char*, int, boolean);
extern void m4_dump_args (struct obstack *obs, int argc, m4_symbol_value **argv, const char *sep, boolean quoted);



/* --- RUNTIME DEBUGGING --- */

extern FILE *m4_debug;

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

extern void m4_debug_init (void);
extern void m4_debug_exit (void);
extern int m4_debug_decode (const char *);
extern void m4_debug_flush_files (void);
extern boolean m4_debug_set_output (const char *);
extern void m4_debug_message_prefix (void);

extern void m4_trace_prepre (const char *, int);
extern void m4_trace_pre (const char *, int, int, m4_symbol_value **);
extern void m4_trace_post (const char *, int, int, m4_symbol_value **,
			   const char *);

/* Exit code from last "syscmd" command.  */
extern int m4_sysval;
extern int m4_expansion_level;

extern const char *m4_expand_ranges (const char *s, struct obstack *obs);
extern void 	   m4_expand_input  (m4 *context);
extern void	   m4_call_macro    (m4_symbol *symbol, m4 *context,
				     struct obstack *obs, int argc,
				     m4_symbol_value **argv);
extern void	   m4_process_macro (m4_symbol *symbol, m4 *context,
				     struct obstack *obs, int argc,
				     m4_symbol_value **argv);



/* --- SYNTAX TABLE DEFINITIONS --- */

/* Please read the comment at the top of input.c for details */
extern unsigned short m4_syntax_table[256];

extern	void	m4_syntax_init	(void);
extern	void	m4_syntax_exit	(void);
extern	int	m4_syntax_code	(char ch);

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
#define M4_SYNTAX_ASSIGN	(0x0009)

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

#define m4__syntax(ch)	m4_syntax_table[(int)(ch)]

#define M4_IS_OTHER(ch)  ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_OTHER)
#define M4_IS_IGNORE(ch) ((m4__syntax(ch)) == M4_SYNTAX_IGNORE)
#define M4_IS_SPACE(ch)  ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_SPACE)

#define M4_IS_OPEN(ch)   ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_OPEN)
#define M4_IS_CLOSE(ch)  ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_CLOSE)
#define M4_IS_COMMA(ch)  ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_COMMA)
#define M4_IS_DOLLAR(ch) ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_DOLLAR)
#define M4_IS_ACTIVE(ch) ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_ACTIVE)
#define M4_IS_ESCAPE(ch) ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_ESCAPE)
#define M4_IS_ASSIGN(ch) ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_ASSIGN)

#define M4_IS_ALPHA(ch)  ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_ALPHA)
#define M4_IS_NUM(ch)    ((m4__syntax(ch)&M4_SYNTAX_VALUE) == M4_SYNTAX_NUM)
#define M4_IS_ALNUM(ch)  (((m4__syntax(ch)) & M4_SYNTAX_ALNUM) != 0)

#define M4_IS_LQUOTE(ch) (m4__syntax(ch) & M4_SYNTAX_LQUOTE)
#define M4_IS_RQUOTE(ch) (m4__syntax(ch) & M4_SYNTAX_RQUOTE)
#define M4_IS_BCOMM(ch)  (m4__syntax(ch) & M4_SYNTAX_BCOMM)
#define M4_IS_ECOMM(ch)  (m4__syntax(ch) & M4_SYNTAX_ECOMM)

#define M4_IS_IDENT(ch)	 (M4_IS_OTHER(ch) || M4_IS_ALNUM(ch))


/* --- TOKENISATION AND INPUT --- */

/* current input file, and line */
extern const char *m4_current_file;
extern int m4_current_line;

extern	void	m4_input_init	(void);
extern	void	m4_input_exit	(void);
extern	int	m4_peek_input	(void);
extern	void	m4_symbol_value_copy	(m4_symbol_value *dest, m4_symbol_value *src);
extern	void	m4_skip_line	(void);

/* push back input */

extern	void	m4_push_file	(FILE *, const char *);
extern	void	m4_push_single	(int ch);
extern	void	m4_push_builtin	(m4_symbol_value *);
extern	struct obstack *m4_push_string_init (void);
extern	const char *m4_push_string_finish (void);
extern	void	m4_push_wrapup	(const char *);
extern	boolean	m4_pop_wrapup	(void);

extern	void	m4_set_quotes	(const char *, const char *);
extern	void	m4_set_comment	(const char *, const char *);
extern	void	m4_set_syntax	(char, const unsigned char *);

extern int m4_current_diversion;
extern int m4_output_current_line;

extern	void	m4_output_init	(void);
extern	void	m4_output_exit	(void);
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

extern int m4_dump_symbol (const void *name, void *symbol, void *data);
extern void m4_dump_symbols (m4 *context, struct m4_dump_symbol_data *data, int argc, m4_symbol_value **argv, boolean complain);



#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree

END_C_DECLS

#endif /* !M4MODULE_H */

/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1999-2000 Free Software Foundation, Inc.

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

#ifndef M4MODULE_H
#define M4MODULE_H

#include <sys/types.h>

/* This is okay in an installed file, because it will not change the
   behaviour of the including program whether ENABLE_NLS is defined
   or not.  */
#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(Text) gettext ((Text))
#  else
#    define _(Text) (Text)
#  endif
#endif

#include <m4/system.h>
#include <m4/error.h>
#include <m4/list.h>
#include <m4/obstack.h>


BEGIN_C_DECLS


/* Syntax table definitions. */
/* Please read the comment at the top of input.c for details */
M4_SCOPE unsigned short m4_syntax_table[256];

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


/* Various declarations.  */

typedef struct {
    unsigned char *string;	/* characters of the string */
    size_t length;		/* length of the string */
} m4_string;

/* Memory allocation.  */
#define XCALLOC(type, num)	((type *) xcalloc ((num), sizeof(type)))
#define XMALLOC(type, num)	((type *) xmalloc ((num) * sizeof(type)))
#define XREALLOC(type, p, num)	((type *) xrealloc ((p), (num) * sizeof(type)))
#define XFREE(stale)				M4_STMT_START {		\
  	if (stale) { free ((VOID *) stale);  stale = 0; }		\
						} M4_STMT_END

extern VOID *xcalloc  M4_PARAMS((size_t n, size_t s));
extern VOID *xmalloc  M4_PARAMS((size_t n));
extern VOID *xrealloc M4_PARAMS((VOID *p, size_t n));
extern void  xfree    M4_PARAMS((VOID *stale));

extern char *xstrdup  M4_PARAMS((const char *string));

#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree



typedef void m4_builtin_func ();

typedef struct {
  const char *name;
  m4_builtin_func *func;
  boolean groks_macro_args;
  boolean blind_if_no_args;
} m4_builtin;

typedef struct {
  const char *name;
  const char *value;
} m4_macro;

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

typedef void m4_module_init_t M4_PARAMS((struct obstack *));
typedef void m4_module_finish_t M4_PARAMS((void));

#ifdef COMPILING_M4
typedef struct m4_token_data m4_token_data;
#else
typedef VOID *m4_token_data;
#endif

m4_token_data_t m4_token_data_type M4_PARAMS((m4_token_data *));
char *m4_token_data_text M4_PARAMS((m4_token_data *));
char *m4_token_data_orig_text M4_PARAMS((m4_token_data *));
m4_builtin_func *m4_token_data_func M4_PARAMS((m4_token_data *));
boolean m4_token_data_func_traced M4_PARAMS((m4_token_data *));

#define M4ARG(i)	(argc > (i) ? m4_token_data_text (argv[i]) : "")

#define M4BUILTIN(name) 					\
  static void CONC(builtin_, name) 				\
  M4_PARAMS((struct obstack *, int , m4_token_data **));
					     
#define M4BUILTIN_HANDLER(name) 				\
  static void CONC(builtin_, name) (obs, argc, argv)		\
	struct obstack *obs; int argc; m4_token_data **argv;

/* Error handling.  */
#define M4ERROR(Arglist) (error Arglist)

#define HASHMAX 509		/* default, overridden by -Hsize */

/* The name this program was run with. */
M4_SCOPE const char *program_name;

/* Option flags  (defined in m4module.c; set in m4.c).  */
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
M4_SCOPE m4_string m4_lquote;
M4_SCOPE m4_string m4_rquote;

M4_SCOPE m4_string m4_bcomm;
M4_SCOPE m4_string m4_ecomm;

#define M4_DEF_LQUOTE "`"
#define M4_DEF_RQUOTE "\'"
#define M4_DEF_BCOMM "#"
#define M4_DEF_ECOMM "\n"

boolean m4_bad_argc M4_PARAMS((m4_token_data *, int, int, int));
const char *m4_skip_space M4_PARAMS((const char *));
boolean m4_numeric_arg M4_PARAMS((m4_token_data *, const char *, int *));
void m4_shipout_int M4_PARAMS((struct obstack *, int));
void m4_shipout_string M4_PARAMS((struct obstack*, const char*, int, boolean));

END_C_DECLS

#endif /* !M4MODULE_H */

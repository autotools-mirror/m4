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

#ifndef M4MODULE_H
#define M4MODULE_H

#include <sys/types.h>
#include <m4error.h>
#include <m4obstack.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(Text) gettext ((Text))
#else
#define _(Text) (Text)
#endif

#ifndef M4_PARAMS
# ifdef __STDC__
#  define M4_PARAMS(Args) Args
# else
#  define M4_PARAMS(Args) ()
# endif
#endif

/* DLL building support on win32 hosts;  mostly to workaround their
   ridiculous implementation of data symbol exporting. */
#ifndef M4_SCOPE
#  ifdef _WIN32
  /* Incase we are linking a dll with this library, the
     LIBM4_DLL_IMPORT takes precedence over a generic DLL_EXPORT
     when defining the SCOPE variable for M4.  */
#    ifdef LIBM4_DLL_IMPORT	/* define if linking with this dll */
#      define M4_SCOPE	extern __declspec(dllimport)
#    else
#      ifdef DLL_EXPORT		/* defined by libtool (if required) */
#        define M4_SCOPE	__declspec(dllexport)
#      endif /* DLL_EXPORT */
#    endif /* LIBM4_DLL_IMPORT */
#  endif /* M4_SCOPE */
#  ifndef M4_SCOPE		/* static linking or !_WIN32 */
#    define M4_SCOPE	extern
#  endif
#endif

#if __STDC__
# define voidstar void *
#else
# define voidstar char *
#endif

/* If FALSE is defined, we presume TRUE is defined too.  In this case,
   merely typedef boolean as being int.  Or else, define these all.  */
#ifndef FALSE
/* Do not use `enum boolean': this tag is used in SVR4 <sys/types.h>.  */
typedef enum { FALSE = 0, TRUE = 1 } boolean;
#else
typedef int boolean;
#endif


/* Syntax table definitions. */
/* Please read the comment at the top of input.c for details */
M4_SCOPE unsigned short syntax_table[256];

/* These are simple values, not bit masks.  There is no overlap. */
#define SYNTAX_OTHER	(0x0000)

#define SYNTAX_IGNORE	(0x0001)
#define SYNTAX_SPACE	(0x0002)
#define SYNTAX_OPEN	(0x0003)
#define SYNTAX_CLOSE	(0x0004)
#define SYNTAX_COMMA	(0x0005)
#define SYNTAX_DOLLAR	(0x0006) /* not used yet */
#define SYNTAX_ACTIVE	(0x0007)
#define SYNTAX_ESCAPE	(0x0008)

/* These are values to be assigned to syntax table entries, but they are
   used as bit masks with IS_ALNUM.*/
#define SYNTAX_ALPHA	(0x0010)
#define SYNTAX_NUM	(0x0020)
#define SYNTAX_ALNUM	(SYNTAX_ALPHA|SYNTAX_NUM)

/* These are bit masks to AND with other categories.  
   See input.c for details. */
#define SYNTAX_LQUOTE	(0x0100)
#define SYNTAX_RQUOTE	(0x0200)
#define SYNTAX_BCOMM	(0x0400)
#define SYNTAX_ECOMM	(0x0800)

/* These bits define the syntax code of a character */
#define SYNTAX_VALUE	(0x00FF|SYNTAX_LQUOTE|SYNTAX_BCOMM)
#define SYNTAX_MASKS	(0xFF00)

#define IS_OTHER(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_OTHER)
#define IS_IGNORE(ch) ((syntax_table[(int)(ch)]) == SYNTAX_IGNORE)
#define IS_SPACE(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_SPACE)

#define IS_OPEN(ch)   ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_OPEN)
#define IS_CLOSE(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_CLOSE)
#define IS_COMMA(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_COMMA)
#define IS_DOLLAR(ch) ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_DOLLAR)
#define IS_ACTIVE(ch) ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_ACTIVE)

#define IS_ESCAPE(ch) ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_ESCAPE)
#define IS_ALPHA(ch)  ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_ALPHA)
#define IS_NUM(ch)    ((syntax_table[(int)(ch)]&SYNTAX_VALUE) == SYNTAX_NUM)
#define IS_ALNUM(ch)  (((syntax_table[(int)(ch)]) & SYNTAX_ALNUM) != 0)

#define IS_LQUOTE(ch) (syntax_table[(int)(ch)] & SYNTAX_LQUOTE)
#define IS_RQUOTE(ch) (syntax_table[(int)(ch)] & SYNTAX_RQUOTE)
#define IS_BCOMM(ch)  (syntax_table[(int)(ch)] & SYNTAX_BCOMM)
#define IS_ECOMM(ch)  (syntax_table[(int)(ch)] & SYNTAX_ECOMM)


/* Various declarations.  */

struct string
  {
    unsigned char *string;	/* characters of the string */
    size_t length;		/* length of the string */
  };
typedef struct string STRING;

/* Memory allocation.  */
voidstar xmalloc M4_PARAMS((unsigned int));
voidstar xrealloc M4_PARAMS((voidstar, unsigned int));
void xfree M4_PARAMS((voidstar));
char *xstrdup M4_PARAMS((const char *));
#define obstack_chunk_alloc	xmalloc
#define obstack_chunk_free	xfree

/* Other library routines.  */
void error M4_PARAMS((int , int, const char *, ...));


typedef void builtin_func ();

typedef struct {
  const char *name;
  boolean gnu_extension;
  boolean groks_macro_args;
  boolean blind_if_no_args;
  builtin_func *func;
} builtin;

/* Various different token types.  */
typedef enum {
  TOKEN_EOF,			/* end of file */
  TOKEN_NONE,			/* discardable token */
  TOKEN_STRING,			/* a quoted string */
  TOKEN_SPACE,			/* whitespace */
  TOKEN_WORD,			/* an identifier */
  TOKEN_SIMPLE,			/* a single character */
  TOKEN_MACDEF			/* a macros definition (see "defn") */
} token_type;

/* The data for a token, a macro argument, and a macro definition.  */
typedef enum {
  TOKEN_VOID,
  TOKEN_TEXT,
  TOKEN_FUNC
} token_data_type;

typedef void module_init_t M4_PARAMS((struct obstack *));
typedef void module_finish_t M4_PARAMS((void));

#ifdef COMPILING_M4
typedef struct token_data token_data;
#else
typedef voidstar token_data;
#endif

token_data_type	m4_token_data_type M4_PARAMS((token_data *));
char *m4_token_data_text M4_PARAMS((token_data *));
char *m4_token_data_orig_text M4_PARAMS((token_data *));
builtin_func *m4_token_data_func M4_PARAMS((token_data *));
boolean m4_token_data_func_traced M4_PARAMS((token_data *));

#define M4ARG(i)	(argc > (i) ? m4_token_data_text (argv[i]) : "")

#define M4BUILTIN(name) \
  static void name M4_PARAMS((struct obstack *, int, token_data **))

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
M4_SCOPE STRING lquote;
M4_SCOPE STRING rquote;

M4_SCOPE STRING bcomm;
M4_SCOPE STRING ecomm;

#define DEF_LQUOTE "`"
#define DEF_RQUOTE "\'"
#define DEF_BCOMM "#"
#define DEF_ECOMM "\n"

boolean m4_bad_argc M4_PARAMS((token_data *, int, int, int));
const char *m4_skip_space M4_PARAMS((const char *));
boolean m4_numeric_arg M4_PARAMS((token_data *, const char *, int *));
void m4_shipout_int M4_PARAMS((struct obstack *, int));
void m4_shipout_string M4_PARAMS((struct obstack*, const char*, int, boolean));

#endif /* M4MODULE_H */

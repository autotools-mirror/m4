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
#include <m4module.h>



/* --- CONTEXT MANAGEMENT --- */

struct m4 {
  m4_symtab *symtab;
};

#define M4_SYMTAB(context) ((context)->symtab)

#define m4_get_symtab(context)	((context)->symtab)



/* --- MODULE MANAGEMENT --- */

#define USER_MODULE_PATH_ENV	"M4MODPATH"
#define BUILTIN_SYMBOL		"m4_builtin_table"
#define MACRO_SYMBOL		"m4_macro_table"
#define INIT_SYMBOL		"m4_init_module"
#define FINISH_SYMBOL		"m4_finish_module"

extern void	    m4__module_init (void);
extern lt_dlhandle  m4__module_open (m4 *context, const char *name,
				     struct obstack *obs);
extern void	    m4__module_exit (m4 *context);



/* --- SYMBOL TABLE MANAGEMENT --- */

extern void	m4__symtab_remove_module_references (m4_symtab*, lt_dlhandle);


/* TRUE iff strlen(rquote) == strlen(lquote) == 1 */
extern boolean m4__single_quotes;

/* TRUE iff strlen(bcomm) == strlen(ecomm) == 1 */
extern boolean m4__single_comments;

/* TRUE iff some character has M4_SYNTAX_ESCAPE */
extern boolean m4__use_macro_escape;

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

struct m4_symbol_value {
  m4_symbol_value *	next;
  lt_dlhandle		handle;
  int			flags;

  m4_hash *		arg_signature;
  int			min_args, max_args;

  m4_symbol_type	type;
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
#define VALUE_TYPE(T)		((T)->type)
#define VALUE_TEXT(T)		((T)->u.text)
#define VALUE_FUNC(T)		((T)->u.func)

/* m4_symbol_value.flags bit masks:  */

#define VALUE_MACRO_ARGS_BIT	(1 << 0)
#define VALUE_BLIND_ARGS_BIT	(1 << 1)

#define BIT_TEST(flags, bit)	(((flags) & (bit)) == (bit))
#define BIT_SET(flags, bit)	((flags) |= (bit))
#define BIT_RESET(flags, bit)	((flags) &= ~(bit))


/* Redefine the exported function to this faster
   macro based version for internal use by the m4 code. */
#undef M4ARG
#define M4ARG(i)	(argc > (i) ? VALUE_TEXT (argv[i]) : "")


struct m4_symbol
{
  boolean		traced;
  m4_symbol_value *	value;
};

#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_VALUE(S)		((S)->value)

#define SYMBOL_NEXT(S)		(VALUE_NEXT          (SYMBOL_VALUE (S)))
#define SYMBOL_HANDLE(S)	(VALUE_HANDLE        (SYMBOL_VALUE (S)))
#define SYMBOL_FLAGS(S)		(VALUE_FLAGS         (SYMBOL_VALUE (S)))
#define SYMBOL_ARG_SIGNATURE(S)	(VALUE_ARG_SIGNATURE (SYMBOL_VALUE (S)))
#define SYMBOL_MIN_ARGS(S)	(VALUE_MIN_ARGS      (SYMBOL_VALUE (S)))
#define SYMBOL_MAX_ARGS(S)	(VALUE_MAX_ARGS      (SYMBOL_VALUE (S)))
#define SYMBOL_TYPE(S)		(VALUE_TYPE          (SYMBOL_VALUE (S)))
#define SYMBOL_TEXT(S)		(VALUE_TEXT          (SYMBOL_VALUE (S)))
#define SYMBOL_FUNC(S)		(VALUE_FUNC          (SYMBOL_VALUE (S)))


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

extern	m4__token_type m4__next_token (m4_symbol_value *);



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

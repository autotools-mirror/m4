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

/* TRUE iff strlen(rquote) == strlen(lquote) == 1 */
extern boolean m4__single_quotes;

/* TRUE iff strlen(bcomm) == strlen(ecomm) == 1 */
extern boolean m4__single_comments;

/* TRUE iff some character has M4_SYNTAX_ESCAPE */
extern boolean m4__use_macro_escape;

struct m4_module_data {
  m4_builtin	    *bp;	/* `m4_builtin_table' address */
  m4_macro	    *mp;	/* `m4_macro_table' address */
};


/* m4_token.flags bit masks:  */

#define TOKEN_MACRO_ARGS_BIT		(1 << 0)
#define TOKEN_BLIND_ARGS_BIT		(1 << 1)


struct m4_token {
  m4_token *	next;
  lt_dlhandle		handle;
  int			flags;
  int			min_args, max_args;

  m4_data_t		type;
  union {
    char *		text;
    m4_builtin_func *	func;
  } u;
};

#define TOKEN_NEXT(T)		((T)->next)
#define TOKEN_HANDLE(T) 	((T)->handle)
#define TOKEN_FLAGS(T)		((T)->flags)
#define TOKEN_MIN_ARGS(T)	((T)->min_args)
#define TOKEN_MAX_ARGS(T)	((T)->max_args)
#define TOKEN_TYPE(T)		((T)->type)
#define TOKEN_TEXT(T)		((T)->u.text)
#define TOKEN_FUNC(T)		((T)->u.func)

#define BIT_TEST(flags, bit)	(((flags) & (bit)) == (bit))
#define BIT_SET(flags, bit)	((flags) |= (bit))
#define BIT_RESET(flags, bit)	((flags) &= ~(bit))


/* Redefine the exported function to this faster
   macro based version for internal use by the m4 code. */
#undef M4ARG
#define M4ARG(i)	(argc > (i) ? TOKEN_TEXT (argv[i]) : "")


struct m4_symbol
{
  boolean	traced;
  m4_token *	token;
};

#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_TOKEN(S)		((S)->token)

#define SYMBOL_NEXT(S)		(TOKEN_NEXT     (SYMBOL_TOKEN (S)))
#define SYMBOL_HANDLE(S)	(TOKEN_HANDLE   (SYMBOL_TOKEN (S)))
#define SYMBOL_FLAGS(S)		(TOKEN_FLAGS    (SYMBOL_TOKEN (S)))
#define SYMBOL_MIN_ARGS(S)	(TOKEN_MIN_ARGS (SYMBOL_TOKEN (S)))
#define SYMBOL_MAX_ARGS(S)	(TOKEN_MAX_ARGS (SYMBOL_TOKEN (S)))
#define SYMBOL_TYPE(S)		(TOKEN_TYPE     (SYMBOL_TOKEN (S)))
#define SYMBOL_TEXT(S)		(TOKEN_TEXT     (SYMBOL_TOKEN (S)))
#define SYMBOL_FUNC(S)		(TOKEN_FUNC     (SYMBOL_TOKEN (S)))



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

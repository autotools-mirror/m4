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

struct m4_module_data {
  m4_builtin	    *bp;	/* `m4_builtin_table' address */
  m4_macro	    *mp;	/* `m4_macro_table' address */
};


struct m4_symbol {
  m4_symbol	*	next;
  m4_token_data_t	type;
  boolean		traced;
  boolean		macro_args;
  boolean		blind_no_args;
  lt_dlhandle		handle;
  union {
    char *		text;
    m4_builtin_func *	func;
  } u;
};

#define M4_SYMBOL_NEXT(Symbol)		((Symbol)->next)
#define M4_SYMBOL_TYPE(Symbol)		((Symbol)->type)
#define M4_SYMBOL_TRACED(Symbol)	((Symbol)->traced)
#define M4_SYMBOL_MACRO_ARGS(Symbol)	((Symbol)->macro_args)
#define M4_SYMBOL_BLIND_NO_ARGS(Symbol)	((Symbol)->blind_no_args)
#define M4_SYMBOL_HANDLE(Symbol)	((Symbol)->handle)
#define M4_SYMBOL_TEXT(Symbol)		((Symbol)->u.text)
#define M4_SYMBOL_FUNC(Symbol)		((Symbol)->u.func)

/* Redefine the exported function to this faster
   macro based version for internal use by the m4 code. */
#undef M4ARG
#define M4ARG(i)	(argc > (i) ? M4_SYMBOL_TEXT (argv[i]) : "")



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

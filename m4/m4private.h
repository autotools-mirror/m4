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

#include <m4module.h>

struct m4_module {
  struct m4_module  *next;	/* previously loaded module */
  char		    *modname;	/* name of this module */
  lt_dlhandle	     handle;	/* libltdl module handle */
  m4_builtin	    *bp;	/* `m4_builtin_table' address */
  m4_macro	    *mp;	/* `m4_macro_table' address */
  unsigned int	     ref_count;	/* number of times module_load was called */
};


struct m4_token_data {
  m4_token_data_t type;
  union {
    struct {
	char *text;
#ifdef ENABLE_CHANGEWORD
	char *original_text;
#endif
    } u_t;
    struct {
	m4_builtin_func *func;
	boolean traced;
    } u_f;
  } u;
};

#define M4_TOKEN_DATA_TYPE(Td)		((Td)->type)
#define M4_TOKEN_DATA_TEXT(Td)		((Td)->u.u_t.text)
#ifdef ENABLE_CHANGEWORD
#  define M4_TOKEN_DATA_ORIG_TEXT(Td)	((Td)->u.u_t.original_text)
#endif
#define M4_TOKEN_DATA_FUNC(Td)		((Td)->u.u_f.func)
#define M4_TOKEN_DATA_FUNC_TRACED(Td) 	((Td)->u.u_f.traced)

/* Redefine the exported function using macro to this faster
   macro based version for internal use by the m4 code. */
#undef M4ARG
#define M4ARG(i)	(argc > (i) ? M4_TOKEN_DATA_TEXT (argv[i]) : "")

struct m4_symbol
{
  struct m4_symbol *next;
  boolean traced;
  boolean shadowed;
  boolean macro_args;
  boolean blind_no_args;

  char *name;
  m4_token_data data;
  const m4_module *module;
};

#define SYMBOL_NEXT(S)		((S)->next)
#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_SHADOWED(S)	((S)->shadowed)
#define SYMBOL_MACRO_ARGS(S)	((S)->macro_args)
#define SYMBOL_BLIND_NO_ARGS(S)	((S)->blind_no_args)
#define SYMBOL_MODULE(S)	((S)->module)
#define SYMBOL_NAME(S)		((S)->name)
#define SYMBOL_TYPE(S)		(M4_TOKEN_DATA_TYPE (&(S)->data))
#define SYMBOL_TEXT(S)		(M4_TOKEN_DATA_TEXT (&(S)->data))
#define SYMBOL_FUNC(S)		(M4_TOKEN_DATA_FUNC (&(S)->data))


/* Debugging the memory allocator.  */

#if WITH_DMALLOC
# define DMALLOC_FUNC_CHECK
# include <dmalloc.h>
#endif

/* Other debug stuff.  */

#if DEBUG
# define DEBUG_INPUT
# define DEBUG_MACRO
/* # define DEBUG_SYM */
/* # define DEBUG_INCL */
# define DEBUG_MODULE
#endif

#endif /* m4private.h */

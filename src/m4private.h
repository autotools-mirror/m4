/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1998-1999 Free Software Foundation, Inc.
  
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

#ifndef M4PRIVATE_H
#define M4PRIVATE_H 1

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#define COMPILING_M4
#include "m4.h"


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

struct symbol
{
  struct symbol *next;
  boolean traced;
  boolean shadowed;
  boolean macro_args;
  boolean blind_no_args;

  char *name;
  m4_token_data data;
};

#define SYMBOL_NEXT(S)		((S)->next)
#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_SHADOWED(S)	((S)->shadowed)
#define SYMBOL_MACRO_ARGS(S)	((S)->macro_args)
#define SYMBOL_BLIND_NO_ARGS(S)	((S)->blind_no_args)
#define SYMBOL_NAME(S)		((S)->name)
#define SYMBOL_TYPE(S)		(M4_TOKEN_DATA_TYPE (&(S)->data))
#define SYMBOL_TEXT(S)		(M4_TOKEN_DATA_TEXT (&(S)->data))
#define SYMBOL_FUNC(S)		(M4_TOKEN_DATA_FUNC (&(S)->data))


/* This is the internal version of the `m4_builtin' typedef from m4module.h,
 * with the addition of fields that shouldn't be visible to module writers. */
struct m4_builtin {
  const char *name;
  m4_builtin_func *func;
  boolean groks_macro_args;
  boolean blind_if_no_args;
  boolean gnu_extension;
};

/* and similarly for `m4_macro' */
struct m4_macro
{
  const char *name;
  const char *value;
#ifdef WITH_MODULES
  m4_module *module;
#endif
};

#endif /* m4private.h */

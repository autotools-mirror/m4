/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 90, 91, 92, 93, 94, 98 Free Software Foundation, Inc.
  
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

struct token_data {
  token_data_type type;
  union {
    struct {
	char *text;
#ifdef ENABLE_CHANGEWORD
	char *original_text;
#endif	
    } u_t;
    struct {
	builtin_func *func;
	boolean traced;
    } u_f;
  } u;
};

#define TOKEN_DATA_TYPE(Td)		((Td)->type)
#define TOKEN_DATA_TEXT(Td)		((Td)->u.u_t.text)
#ifdef ENABLE_CHANGEWORD
#  define TOKEN_DATA_ORIG_TEXT(Td)	((Td)->u.u_t.original_text)
#endif	
#define TOKEN_DATA_FUNC(Td)		((Td)->u.u_f.func)
#define TOKEN_DATA_FUNC_TRACED(Td) 	((Td)->u.u_f.traced)

/* Redefine the exported function using macro to this faster
   macro based version for internal use by the m4 code. */
#undef M4ARG
#define M4ARG(i)	(argc > (i) ? TOKEN_DATA_TEXT (argv[i]) : "")

struct symbol
{
  struct symbol *next;
  boolean traced;
  boolean shadowed;
  boolean macro_args;
  boolean blind_no_args;

  char *name;
  token_data data;
};

#define SYMBOL_NEXT(S)		((S)->next)
#define SYMBOL_TRACED(S)	((S)->traced)
#define SYMBOL_SHADOWED(S)	((S)->shadowed)
#define SYMBOL_MACRO_ARGS(S)	((S)->macro_args)
#define SYMBOL_BLIND_NO_ARGS(S)	((S)->blind_no_args)
#define SYMBOL_NAME(S)		((S)->name)
#define SYMBOL_TYPE(S)		(TOKEN_DATA_TYPE (&(S)->data))
#define SYMBOL_TEXT(S)		(TOKEN_DATA_TEXT (&(S)->data))
#define SYMBOL_FUNC(S)		(TOKEN_DATA_FUNC (&(S)->data))

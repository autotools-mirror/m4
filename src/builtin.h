/* GNU m4 -- A simple macro processor
   Copyright (C) 1998 Free Software Foundation, Inc.
  
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

/* Declarations for builtin macros.  */ 

#ifndef BUILTIN_H
#define BUILTIN_H 1

#include <m4.h>

#define ARG(i)	(argc > (i) ? TOKEN_DATA_TEXT (argv[i]) : "")

#define DECLARE(name) \
  static void name __P ((struct obstack *, int, token_data **))



boolean bad_argc __P ((token_data *name, int argc, int min, int max));
const char *skip_space __P ((const char *arg));

boolean numeric_arg __P ((token_data *macro, const char *arg, int *valuep));
void shipout_int __P ((struct obstack *obs, int val));
void shipout_string __P ((struct obstack *obs, const char *s, int len, boolean quoted));

#endif /* BUILTIN_H */

/* GNU m4 -- A simple macro processor
   Copyright (C) 2003, 2006-2008, 2010, 2013 Free Software Foundation,
   Inc.

   This file is part of GNU M4.

   GNU M4 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU M4 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MODULES_M4_H
#define MODULES_M4_H 1

#include <m4/m4module.h>

BEGIN_C_DECLS

/* This structure is used to pass the information needed
   from call to call in m4_dump_symbols.  */
typedef struct
{
  m4_obstack *obs;              /* obstack for table */
  const m4_string *base;        /* base of table */
  int size;                     /* size of table */
} m4_dump_symbol_data;


/* Types used to cast imported symbols to, so we get type checking
   across the interface boundary.  */
typedef void m4_sysval_flush_func (m4 *context, bool report);
typedef void m4_set_sysval_func (int value);
typedef void m4_dump_symbols_func (m4 *context, m4_dump_symbol_data *data,
                                   size_t argc, m4_macro_args *argv,
                                   bool complain);
typedef const char *m4_expand_ranges_func (const char *s, size_t *len,
                                           m4_obstack *obs);
typedef void m4_make_temp_func (m4 *context, m4_obstack *obs,
                                const m4_call_info *macro, const char *name,
                                size_t len, bool dir);

END_C_DECLS

#endif /* !MODULES_M4_H */

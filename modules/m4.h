/* GNU m4 -- A simple macro processor
   Copyright 2003 Free Software Foundation, Inc.

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

#ifndef MODULES_M4_H
#define MODULES_M4_H 1

#include <m4module.h>

BEGIN_C_DECLS

/* This structure is used to pass the information needed
   from call to call in m4_dump_symbols.  */
typedef struct
{
  m4_obstack *obs;		/* obstack for table */
  const char **base;		/* base of table */
  int size;			/* size of table */
} m4_dump_symbol_data;


/* Types used to cast imported symbols to, so we get type checking
   across the interface boundary.  */
typedef void m4_sysval_flush_func (m4 *context);
typedef void m4_set_sysval_func (int value);
typedef void m4_dump_symbols_func (m4 *context, m4_dump_symbol_data *data,
		int argc, m4_symbol_value **argv, boolean complain);
typedef const char *m4_expand_ranges_func  (const char *s, m4_obstack *obs);

END_C_DECLS

#endif /* !MODULES_M4_H */

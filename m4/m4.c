/* GNU m4 -- A simple macro processor
   Copyright 1989, 1990, 1991, 1992, 1993, 1994, 2001
   Free Software Foundation, Inc.

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

#include "m4private.h"

m4 *
m4_create (void)
{
  m4 *context = XMALLOC (m4, 1);

  context->symtab = m4_symtab_create (0);

  return context;
}

void
m4_delete (m4 *context)
{
  assert (context);

  if (context->symtab)
    m4_symtab_delete (context->symtab);

  xfree (context);
}

#undef m4_get_symtab
m4_symtab *
m4_get_symtab (m4 *context)
{
  assert (context);
  return context->symtab;
}

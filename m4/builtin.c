/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1999, 2000 Free Software Foundation, Inc.

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

/* Code for all builtin macros, initialisation of symbol table, and
   expansion of user defined macros.  */

#include "m4.h"
#include "m4private.h"

/* Find the builtin which has NAME.  If HANDLE argument is supplied
   then search only in HANDLE's builtin table.  */
const m4_builtin *
m4_builtin_find_by_name (lt_dlhandle handle, const char *name)
{
  lt_dlhandle cur = handle ? handle : lt_dlhandle_next (0);

  do
    {
      const m4_builtin *builtin =
	(const m4_builtin *) lt_dlsym (cur, BUILTIN_SYMBOL);

      if (builtin)
	{
	  for (; builtin->name != NULL; builtin++)
	    if (!strcmp (builtin->name, name))
	      return builtin;
	}
    }
  while (!handle && (cur = lt_dlhandle_next (cur)));

  return 0;
}

/* Find the builtin which has FUNC.  If HANDLE argument is supplied
   then search only in HANDLE's builtin table.  */
const m4_builtin *
m4_builtin_find_by_func (lt_dlhandle handle, m4_builtin_func *func)
{
  lt_dlhandle cur = handle ? handle : lt_dlhandle_next (0);

  do
    {
      const m4_builtin *builtin =
	(const m4_builtin *) lt_dlsym (cur, BUILTIN_SYMBOL);

      if (builtin)
	{
	  for (; builtin->name != NULL; builtin++)
	    if (builtin->func == func)
	      return builtin;
	}
    }
  while (!handle && (cur = lt_dlhandle_next (cur)));

  return 0;
}

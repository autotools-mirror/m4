/* GNU m4 -- A simple macro processor
   Copyright (C) 2000, 2005, 2006, 2007, 2008, 2010 Free Software
   Foundation, Inc.

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

#include <config.h>

/* This module needs private symbols, and may not compile correctly if
   m4private.h isn't included.  */
#include "m4private.h"


/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	load_LTX_m4_builtin_table
#define m4_macro_table		load_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions					\
  BUILTIN (load,	false,	true,	false,	1,	1  )	\
  BUILTIN (m4modules,	false,	false,	false,	0,	0  )	\
  BUILTIN (refcount,	false,	true,	false,	1,	1  )	\
  BUILTIN (unload,	false,	true,	false,	1,	1  )	\


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind, side, min, max) M4BUILTIN (handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
const m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)			\
  M4BUILTIN_ENTRY (handler, #handler, macros, blind, side, min, max)

  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};


/* A table for mapping m4 symbol names to simple expansion text. */
const m4_macro m4_macro_table[] =
{
  /* name		text	min	max */
  { "__load__",		"",	0,	0 },
  { NULL,		NULL,	0,	0 },
};


/* This module cannot be safely unloaded from memory, incase the unload
   is triggered by the unload builtin, and the module is removed while
   unload is in progress.  */
M4INIT_HANDLER (load)
{
  const char *err = m4_module_makeresident (module);
  if (err)
    m4_error (context, 0, 0, NULL, _("cannot make module `%s' resident: %s"),
	      m4_get_module_name (module), err);
}



/* Loading an external module at runtime.
   The following functions provide the implementation for each
   of the m4 builtins declared in `m4_builtin_table[]' above.  */

/**
 * m4modules()
 **/
M4BUILTIN_HANDLER (m4modules)
{
  /* The expansion of this builtin is a comma separated list of
     loaded modules.  */
  m4_module *module = m4__module_next (NULL);

  if (module)
    do
      {
	m4_shipout_string (context, obs, m4_get_module_name (module), SIZE_MAX,
			   true);

	if ((module = m4__module_next (module)))
	  obstack_1grow (obs, ',');
      }
    while (module);
}

/**
 * refcount(module)
 **/
M4BUILTIN_HANDLER (refcount)
{
  m4_module *module = m4__module_find (M4ARG (1));
  m4_shipout_int (obs, module ? m4_module_refcount (module) : 0);
}

/**
 * load(MODULE-NAME)
 **/
M4BUILTIN_HANDLER (load)
{
  /* Load the named module and install the builtins and macros
     exported by that module.  */
  m4_module_load (context, M4ARG(1), obs);
}

/**
 * unload(MODULE-NAME)
 **/
M4BUILTIN_HANDLER (unload)
{
  /* Remove all builtins and macros installed by the named module,
     and then unload the module from memory entirely.  */
  m4_module_unload (context, M4ARG(1), obs);
}

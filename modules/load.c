/* GNU m4 -- A simple macro processor
   Copyright 2000 Free Software Foundation, Inc.

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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <m4module.h>
#include "m4private.h"

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	load_LTX_m4_builtin_table
#define m4_macro_table		load_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind minargs maxargs */
#define builtin_functions			\
	BUILTIN(modules,	FALSE,	FALSE,	1,	1  )	\
	BUILTIN(load,		FALSE,	TRUE,	2,	2  )	\
	BUILTIN(unload,		FALSE,	TRUE,	2,	2  )	\


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind, min, max) M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, min, max)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind, min, max },
  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE, 0, 0 },
};


/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  /* name			text */
  { "__modules__",		"" },
  { 0, 0 },
};


/* This module cannot be safely unloaded from memory, incase the unload
   is triggered by the unload builtin, and the module is removed while
   unload is in progress.  */
M4INIT_HANDLER (load)
{
  if (handle)
    if (lt_dlmakeresident (handle) != 0)
      {
	M4ERROR ((m4_get_warning_status_opt (context), 0,
		  _("Warning: cannot make module `%s' resident: %s"),
		  m4_get_module_name (handle), lt_dlerror ()));
      }
}



/* Loading an external module at runtime.
   The following functions provide the implementation for each
   of the m4 builtins declared in `m4_builtin_table[]' above.  */

/**
 * modules()
 **/
M4BUILTIN_HANDLER (modules)
{
  /* The expansion of this builtin is a comma separated list of
     loaded modules.  */
  lt_dlhandle handle = lt_dlhandle_next (NULL);

  if (handle)
    do
      {
	m4_shipout_string (obs, m4_get_module_name (handle), 0, TRUE);

	if ((handle = lt_dlhandle_next (handle)))
	  obstack_1grow (obs, ',');
      }
    while (handle);
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

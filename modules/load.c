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

/* Rename exported symbols for dlpreload()ing.  */
#define m4_builtin_table	load_LTX_m4_builtin_table
#define m4_macro_table		load_LTX_m4_macro_table


/* Maintain each of the builtins implemented in this modules along
   with their details in a single table for easy maintenance.

		function	macros	blind */
#define builtin_functions			\
	BUILTIN(modules,	FALSE,	FALSE )	\
	BUILTIN(load,		FALSE,	TRUE  )	\
	BUILTIN(unload,		FALSE,	TRUE  )


/* Generate prototypes for each builtin handler function. */
#define BUILTIN(handler, macros,  blind)	M4BUILTIN(handler)
  builtin_functions
#undef BUILTIN


/* Generate a table for mapping m4 symbol names to handler functions. */
m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind },
  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE },
};


/* A table for mapping m4 symbol names to simple expansion text. */
m4_macro m4_macro_table[] =
{
  { "__modules__",		"" },
  { 0, 0 },
};


/*------------------------------------------------------------.
| Loading external module at runtime.                         |
| The following functions provide the implementation for each |
| of the m4 builtins declared in `m4_builtin_table[]' above.  |
`------------------------------------------------------------*/

/* The expansion of this builtin is a comma separated list of
   loaded modules.  */
M4BUILTIN_HANDLER (modules)
{
  List *p = m4_modules;
  
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  while (p)
    {
      m4_shipout_string (obs, m4_module_name ((m4_module *) p), 0, TRUE);
      p = LIST_NEXT (p);

      if (p)
	obstack_1grow (obs, ',');
    }
}

/* Load the named module and install the builtins and macros
   exported by that module.  */
M4BUILTIN_HANDLER (load)
{
  const m4_module *module;
  
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  module = m4_module_load (M4ARG(1), obs);

  if (module)
    {
      const m4_builtin *bp	= m4_module_builtins (module);
      const m4_macro *mp	= m4_module_macros (module);

      /* Install the builtin functions.  */
      if (bp)
	m4_builtin_table_install (module, bp);

      /* Install the user macros. */
      if (mp)
	m4_macro_table_install (module, mp);
    }
}

/* Remove all builtins and macros installed by the named module,
   and then unload the module from memory entirely.  */
M4BUILTIN_HANDLER (unload)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  m4_module_unload (M4ARG(1), obs);
}

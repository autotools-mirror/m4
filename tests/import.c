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

#include <config.h>

/* Build using only the exported interfaces, unless NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

/*         function     macros  blind   side    minargs maxargs */
#define builtin_functions                                       \
  BUILTIN (import,      false,  false,  false,  0,      1)      \
  BUILTIN (symbol_fail, false,  false,  false,  0,      1)      \
  BUILTIN (module_fail, false,  false,  false,  0,      1)      \

#define BUILTIN(handler, macros, blind, side, min, max) M4BUILTIN (handler)
  builtin_functions
#undef BUILTIN

static const m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)                 \
  M4BUILTIN_ENTRY (handler, #handler, macros, blind, side, min, max)

  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};


void
include_import (m4 *context, m4_module *module, m4_obstack *obs)
{
  m4_install_builtins (context, module, m4_builtin_table);
}



typedef bool export_test_func (const char *);
typedef bool no_such_func (const char *);

/**
 * import()
 **/
M4BUILTIN_HANDLER (import)
{
  M4_MODULE_IMPORT (modtest, export_test);

  const char *s = "`import'::`import' called.";

  assert (obs != 0);
  obstack_grow (obs, s, strlen(s));

  if (export_test && export_test (M4ARG (1)))
    fputs ("TRUE\n", stderr);
}

/**
 * symbol_fail()
 **/
M4BUILTIN_HANDLER (symbol_fail)
{
  M4_MODULE_IMPORT (modtest, no_such);

  const char *s = "`import'::`symbol_fail' called.";

  assert (obs != 0);
  obstack_grow (obs, s, strlen(s));

  if (no_such && no_such (M4ARG (1)))
    fputs ("TRUE\n", stderr);
}

/**
 * module_fail()
 **/
M4BUILTIN_HANDLER (module_fail)
{
  M4_MODULE_IMPORT (no_such, no_such);

  const char *s = "`import'::`module_fail' called.";

  assert (obs != 0);
  obstack_grow (obs, s, strlen(s));

  if (no_such && no_such (M4ARG (1)))
    fputs ("TRUE\n", stderr);
}

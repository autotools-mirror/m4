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

#include "m4.h"

#ifdef WITH_MODULES

#define DEBUG_MODULES
#undef DEBUG_MODULES

/* 
 * This file implements dynamic modules in GNU m4.  A module is a
 * compiled shared object, that can be linked into GNU m4 at run
 * time. Information about creating modules is in ../modules/README. 
 *
 * The current implementation uses dlopen(3).  To enable this
 * experimental feature give configure the `--with-modules' switch.
 * This implementation is only tested on Linux.
 *
 * A m4 module need only define one external symbol, called
 * `m4_macro_table'.  This symbol should point to a table of `struct
 * builtin' exactly as the one in builtin.c.  This table is pushed on a
 * list of builtin tables and each definition therein is added to the
 * symbol table.
 *
 * The code implementing loadable modules is modest.  It is divided
 * between the files path.c (search in module path), builtin.c (user
 * interface and support for multiple builtin tables) and this file (OS
 * dependant routines).
 *
 * To load a module, use `loadmodule(modulename.so)', where .so is the
 * normal extention for shared object files.  The function
 * `m4_loadmodule' calls module_load() in this file, which uses
 * module_search() in path.c to find the module in the module search
 * path.  This path is initialised from the environment variable
 * M4MODPATH, and cannot be modified in any way.  Module_search()
 * constructs absolute file names and calls module_try_load() in this
 * file.  This function returns NULL on failure and a non-NULL void* on
 * success.  If succesful module_search() returns the value of this
 * void*, which is a handle for the vm segment mapped.  Module_load()
 * checks to see if the module is alreay loaded, and if not, retrives
 * the symbol `m4_macro_table' and returns it's value to
 * m4_loadmodule().  This pointer should be a builtin*, which is
 * installed using install_builtin_table().
 *
 * When a module is loaded, the function "void m4_init_module(struct
 * obstack *obs)" is called, if defined.  Any non NULL return value of
 * this function will be the expansion of "loadmodule".  Before program
 * exit, all modules are unloaded and the function "void
 * m4_finish_module(void)" is called, if defined.
 *
 * There is no way to unload a module unless at program exit.  It is
 * safe to load the same module several times, it has no effect.
 **/

#if defined (HAVE_SHL_LOAD)

#include <dl.h>

voidstar
dynamic_load (const char *modname)
{
  return shl_load (modname, BIND_IMMEDIATE, 0L);
}

void
dynamic_unload (voidstar handle)
{
  shl_unload ((shl_t)handle);
}

voidstar
dynamic_find_data (voidstar handle, const char *symbol)
{
  voidstar addr;

  /* XXX TYPE_PROCEDURE is probably wrong here, as it is a data pointer */
  shl_findsym ((shl_t)handle, symbol, TYPE_PROCEDURE, (voidstar)&addr);
  return addr;
}

voidstar
dynamic_find_func (voidstar handle, const char *symbol)
{
  voidstar addr;

  shl_findsym ((shl_t)handle, symbol, TYPE_PROCEDURE, (voidstar)&addr);
  return addr;
}

#else  /* HAVE_DLOPEN */

#include <dlfcn.h>

voidstar
dynamic_load (const char *modname)
{
  return dlopen (modname, RTLD_NOW);
}

void
dynamic_unload(voidstar handle)
{
  dlclose (handle);
}

voidstar
dynamic_find_data (voidstar handle, const char *symbol)
{
  return dlsym (handle, symbol);
}

voidstar
dynamic_find_func (voidstar handle, const char *symbol)
{
  return dlsym (handle, symbol);
}

#endif /* HAVE_DLOPEN */




/* 
 * The rest of the code should be common for all interfaces. 
 */


/* This list is used to check for repeated loading of the same modules.  */

struct module_list {
  struct module_list *next;
  char *modname;
  void *handle;
};

typedef struct module_list module_list;

static module_list *modules;

/* 
 * Initialisation.  Currently the module search path in path.c is
 * initialised from M4MODPATH.  Only absolute path names are accepted to
 * prevent the path search of the dlopen library from finding wrong
 * files.
 */
void
module_init (void)
{
  module_env_init ();
}

/* 
 * Attempt to load a module with a absolute file name.  It is used as a
 * callback from module_search() in path.c.
 */
voidstar
module_try_load (const char *modname)
{
  return dynamic_load (modname);
}


void
module_unload(voidstar handle)
{
  dynamic_unload (handle);
}

/* 
 * Load a module.  Modname can be a absolute file name or, if relative,
 * is is searched in the module path.  Return value is the value of the
 * symbol `m4_macro_table' in the module.  The module is unloaded in
 * case of error.  The builtins from the module are installed in the
 * symbol table by the caller, m4_loadmodule() in builtin.c.
 */

struct builtin *
module_load (const char *modname, struct obstack *obs)
{
  void *handle;
  struct module_list *list;

  builtin *btab;
  module_init_t *init_func;

  handle = module_search(modname, module_try_load);
  if (handle == NULL)
    {
      M4ERROR ((EXIT_FAILURE, 0, 
		_("ERROR: cannot find module `%s'"), modname));
    }

  for (list = modules; list != NULL; list = list->next)
    if (list->handle == handle)
      {
#ifdef DEBUG_MODULES
	DEBUG_MESSAGE1("module %s handle already seen", modname);
#endif /* DEBUG_MODULES */
	module_unload(handle);
	return NULL;
      }

  btab = (builtin *) dynamic_find_data (handle, "m4_macro_table");

  if (btab == NULL) {
#ifdef DEBUG_MODULES
    DEBUG_MESSAGE1("module %s no symbol m4_macro_table", modname);
#endif /* DEBUG_MODULES */
    module_unload(handle);
    return NULL;
  }

  list = xmalloc (sizeof (struct module_list));
  list->next = modules;
  list->modname = xstrdup(modname);
  list->handle = handle;
  modules = list;

#ifdef DEBUG_MODULES
  DEBUG_MESSAGE1("module %s loaded ok", modname);
#endif /* DEBUG_MODULES */


  init_func = (module_init_t *)
    dynamic_find_func (handle, "m4_init_module");

  if (init_func != NULL)
    {
      (*init_func)(obs);

#ifdef DEBUG_MODULES
      DEBUG_MESSAGE1("module %s init hook called", modname);
#endif /* DEBUG_MODULES */
    }

  return btab;
}

void
module_unload_all(void)
{
  struct module_list *next;
  module_finish_t *finish_func;

  while (modules != NULL)
    {
      finish_func = (module_finish_t *)
	dynamic_find_func (modules->handle, "m4_finish_module");

      if (finish_func != NULL)
	{
	  (*finish_func)();

#ifdef DEBUG_MODULES
	  DEBUG_MESSAGE1("module %s finish hook called", modules->modname);
#endif /* DEBUG_MODULES */
	}

      module_unload (modules->handle);

#ifdef DEBUG_MODULES
      DEBUG_MESSAGE1("module %s unloaded", modules->modname);
#endif /* DEBUG_MODULES */

      next = modules->next;
      xfree(modules);
      modules = next;
    }
}



#endif /* WITH_MODULES */


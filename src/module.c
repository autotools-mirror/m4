/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1998, 99 Free Software Foundation, Inc.
  
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

#define COMPILING_M4
#include "m4.h"

#ifdef WITH_MODULES

#define MODULES_UNINITIALISED -4444

#include "pathconf.h"
#include "ltdl.h"

#define DEBUG_MODULES  /* Define this to see runtime debug info. */
#undef DEBUG_MODULES

/* 
 * This file implements dynamic modules in GNU m4.  A module is a
 * compiled shared object, that can be linked into GNU m4 at run
 * time. Information about creating modules is in ../modules/README. 
 *
 * The current implementation uses libltdl, which is in turn can load
 * modules using either dlopen(3) (exists on GNU/Linux, OSF, Solaris,
 * SunOS), shl_load(3) (exists on HPUX), LoadLibrary(3) (exists on
 * Windows, cygwin, OS/2), load_add_on(3) (exists on BeOS), and can
 * also fall back to dld_link(3) from libdld or lt_dlpreload(3) from
 * libtool if shared libraries are not available on the host machine.  To
 * enable this experimental feature give configure the `--with-modules'
 * switch.
 *
 * A m4 module need only define one external symbol, called
 * `m4_macro_table'.  This symbol should point to a table of `struct
 * builtin' exactly as the one in builtin.c.  This table is pushed on a
 * list of builtin tables and each definition therein is added to the
 * symbol table.
 *
 * The code implementing loadable modules is modest.  It is divided
 * between the files builtin.c (user interface and support for multiple
 * builtin tables) and this file (OS dependant routines).
 *
 * To load a module, use `loadmodule(modulename)'.  The function
 * `m4_loadmodule' calls module_load() in this file, which uses
 * libltdl to find the module in the module search path.  This
 * path is initialised from the environment variable M4MODPATH, or if
 * not set, initalised to a configuration time default.  This
 * function reads the libtool .la file to get the real library name
 * (which can be system dependent) and returns NULL on failure and a
 * non-NULL void* on success.  If successful lt_dlopen returns the
 * value of this void*, which is a handle for the vm segment mapped.
 * Module_load() checks to see if the module is already loaded, and if
 * not, retrieves the symbol `m4_macro_table' and returns it's value to
 * m4_loadmodule().  This pointer should be a builtin*, which is
 * installed using install_builtin_table().
 *
 * When a module is loaded, the function "void m4_init_module(struct
 * obstack *obs)" is called, if defined.  Any non NULL return value of
 * this function will be the expansion of "loadmodule".  Before program
 * exit, all modules are unloaded and the function "void
 * m4_finish_module(void)" is called, if defined.  It is
 * safe to load the same module several times.
 *
 * To unload a module, use `unloadmodule(modulename)'.  The function
 * `m4_unloadmodule' calls module_unload() in this file, which uses
 * remove_builtin_table() to remove the builtins defined by the
 * unloaded module from the symbol table.  If the module has been loaded
 * several times with calls to `loadmodule(modulename)', then the module
 * will not be unloaded until the same number of calls to unloadmodule
 * have been made (nor will the builtin table be purged, nor the finish
 * function called).  If the module is successfully unloaded, and it
 * defined a function called `m4_finish_module', that function will be
 * called last of all.
 *
 * Modules loaded from the command line (with the `-m' switch) can be
 * removed with unloadmodule too.
 *
 * When m4 exits, it will unload every remaining loaded module, calling
 * any `m4_finish_module' functions they defined before closing.
 **/

/* This list is used to check for repeated loading of the same modules.  */

typedef struct module_list {
  struct module_list *next;	/* previously loaded module */
  char *modname;		/* name of this module */
  lt_dlhandle handle;		/* libltdl module handle */
  builtin *bp;			/* `m4_macro_table' address */
  unsigned int ref_count;	/* number of times module_load was called */
} module_list;

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
  static int errors = MODULES_UNINITIALISED;

  /* Do this only once! */
  if (errors != MODULES_UNINITIALISED)
    return;

  /* initialise libltdl's memory management. */
  lt_dlmalloc = xmalloc;
  lt_dlfree = xfree;

  errors = lt_dlinit();

  /* If the user set M4MODPATH, then use that as the start of libltdls
   * module search path, else fall back on the default.
   */
  if (errors == 0)
    {
      char *path = getenv("M4MODPATH");

#ifdef DEBUG_MODULES
      DEBUG_MESSAGE("Module system initialised.");
#endif /* DEBUG_MODULES */
 
      if (path != NULL)
	errors = lt_dladdsearchdir(path);
    }
    
  if (errors == 0)
    errors = lt_dladdsearchdir(MODULE_PATH);

  if (errors != 0)
    {
      /* Couldn't initialise the module system; diagnose and exit. */
      const char *dlerror = lt_dlerror();
      M4ERROR ((EXIT_FAILURE, 0,
		_("ERROR: failed to initialise modules: %s"), dlerror));
    }
}

/* 
 * Load a module.  MODNAME can be a absolute file name or, if relative,
 * it is searched for in the module path.  The module is unloaded in
 * case of error.  The builtins from the module are installed in the
 * symbol table.
 */

void
module_load (const char *modname, struct obstack *obs)
{
  module_init_t *init_func;
  lt_dlhandle module;
  module_list *list;
  builtin *bp;

  module_init();

  /* Dynamically load the named module. */
  module = lt_dlopenext(modname);

  if (module != NULL)
    {
      for (list = modules; list != NULL; list = list->next)
	if (list->handle == module)
	  {
	    lt_dlclose(module); /* close the duplicate copy */
	    list->ref_count++;  /* increment the load counter */
#ifdef DEBUG_MODULES
	    DEBUG_MESSAGE2("module %s: now has %d references.",
			     modname, list->ref_count);
#endif /* DEBUG_MODULES */
	    return;
	  }
    }


  /* Find the initialising table in the loaded module. */
  if (module != NULL)
    {
      bp = (builtin*)lt_dlsym(module, "m4_macro_table");
      if (bp == NULL)
	{
#ifdef DEBUG_MODULES
	  DEBUG_MESSAGE1("module %s: no symbol m4_macro_table", modname);
#endif /* DEBUG_MODULES */
	  lt_dlclose(module);
	  module = NULL;
	}
    }


  /* Find and run the initialising function in the loaded module
   * (if any).
   */
  if (module != NULL)
    {
      init_func = (module_init_t*)lt_dlsym(module, "m4_init_module");
      if (init_func != NULL)
	{
	  (*init_func)(obs);
	}
#ifdef DEBUG_MODULES
      else
	DEBUG_MESSAGE1("module %s: no symbol m4_init_module", modname);
#endif /* DEBUG_MODULES */
    }


  /* If the module was correctly loaded and has the necessary
   * symbols, then update our internal tables to remember the
   * new module.
   */
  if (module != NULL)
    {
#ifdef DEBUG_MODULES
      DEBUG_MESSAGE1("module %s: loaded ok", modname);
#endif /* DEBUG_MODULES */

      list = xmalloc (sizeof (struct module_list));
      list->next = modules;
      list->modname = xstrdup(modname);
      list->handle = module;
      list->bp = bp;
      list->ref_count = 1;
      modules = list;

      install_builtin_table(bp);
    }
  else
    {
      /* Couldn't load the module; diagnose and exit. */
      const char *dlerror = lt_dlerror();
      if (dlerror == NULL)
	M4ERROR ((EXIT_FAILURE, 0,
		  _("ERROR: cannot find module: `%s'"), modname));
      else
	M4ERROR ((EXIT_FAILURE, 0,
		  _("ERROR: cannot find module: `%s': %s"),
		  modname, dlerror));
    }
}

/* 
 * Unload a module.  
 */
void
module_unload (const char *modname, struct obstack *obs)
{
  int errors = 0;
  struct module_list *list, *prev = NULL;
  lt_dlhandle module = NULL;
  module_finish_t *finish_func;

  /* Scan the list for the first module with a matching modname field. */
  for (list = modules; list != NULL; list = list->next)
    {
      if (modname != NULL && strcmp (modname, list->modname) == 0)
	{
	  module = list->handle;
	  break;
	}
      prev = list;
    }
  
  if (module == NULL)
    {
      M4ERROR ((warning_status, 0,
		_("ERROR: cannot close module: %s is not loaded."), modname));
      return;
    }

  /* Only do the actual unload when the number of calls to unload this
     module is equal to the number of times it was loaded. */
  if (--list->ref_count > 0)
    {
#ifdef DEBUG_MODULES
      DEBUG_MESSAGE2("module %s: now has %d references.",
		     modname, list->ref_count);
#endif /* DEBUG_MODULES */
      return;
    }
  
  /* Update our internal tables to remove the builtins provided
   * by the unloaded module.
   */
  if (remove_builtin_table(list->bp) < 0)
    {
      /* Couldn't unload the module's builtins; diagnose and exit. */
      M4ERROR ((EXIT_FAILURE, 0,
		_("ERROR: cannot unload module builtins: `%s'"), modname));
    }
#ifdef DEBUG_MODULES
  DEBUG_MESSAGE1("module %s: builtins undefined", modname);
#endif /* DEBUG_MODULES */
  
  /* Run the finishing function for the loaded module. */
  finish_func = (module_finish_t*)
    lt_dlsym(module, "m4_finish_module");

  if (finish_func != NULL)
    {
      (*finish_func)();

#ifdef DEBUG_MODULES
      DEBUG_MESSAGE1("module %s: finish hook called", modname);
#endif /* DEBUG_MODULES */
    }

  errors = lt_dlclose (module);
  if (errors != 0)
    {
      const char *dlerror = lt_dlerror();
      if (dlerror == NULL)
	M4ERROR ((EXIT_FAILURE, 0,
		  _("ERROR: cannot close module: `%s'"), modname));
      else
	M4ERROR ((EXIT_FAILURE, 0,
		  _("ERROR: cannot cannot close module: `%s': %s"),
		  modname, dlerror));
    }
  else
    {
#ifdef DEBUG_MODULES
      DEBUG_MESSAGE1("module %s unloaded", list->modname);
#endif /* DEBUG_MODULES */

      if (prev)
	prev->next = list->next;
      else
	modules = list->next;
      
      xfree(list->modname);
      xfree(list);
    }
}

void
module_unload_all(void)
{
  int errors = 0;
  struct module_list *next;
  module_finish_t *finish_func;

  /* Find and run the finishing function for each loaded module. */
  while (modules != NULL)
    {
      finish_func = (module_finish_t*)
	lt_dlsym(modules->handle, "m4_finish_module");

      if (finish_func != NULL)
	{
	  (*finish_func)();

#ifdef DEBUG_MODULES
	  DEBUG_MESSAGE1("module %s finish hook called", modules->modname);
#endif /* DEBUG_MODULES */
	}

      errors = lt_dlclose (modules->handle);
      if (errors != 0)
	break;

#ifdef DEBUG_MODULES
      DEBUG_MESSAGE1("module %s unloaded", modules->modname);
#endif /* DEBUG_MODULES */

      next = modules->next;
      xfree(modules->modname);
      xfree(modules);
      modules = next;
    }

  if (errors != 0)
    errors = lt_dlexit();

  if (errors != 0)
    {
      const char *dlerror = lt_dlerror();
      if (modules == NULL)
        {
          if (dlerror == NULL)
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close modules")));
	  else
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot cannot close modules: %s"),
		      dlerror));
	}
      else
        {
	  if (dlerror == NULL)
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close module: `%s'"),
		      modules->modname));
	  else
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot cannot close module: `%s': %s"),
		      modules->modname, dlerror));
	}
    }
}

#endif /* WITH_MODULES */

/* GNU m4 -- A simple macro processor
   Copyright 1989-1994, 1998, 99 Free Software Foundation, Inc.

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

#define MODULES_UNINITIALISED -4444

#include "m4.h"
#include "pathconf.h"
#include "ltdl.h"
#include "m4private.h"

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
 * libtool if shared libraries are not available on the host machine.
 *
 * A m4 module need only define one external symbol, called
 * `m4_builtin_table'.  This symbol should point to a table of `m4_builtin'.
 * This table is pushed on a list of builtin tables and each definition
 * therein is added to the symbol table.
 *
 * The code implementing loadable modules is modest.  It is divided
 * between the files builtin.c (user interface and support for multiple
 * builtin tables) and this file (OS dependant routines).
 *
 * To load a module, use `load(modulename)'.  The function
 * `builtin_load' calls m4_module_load() in this file, which uses
 * libltdl to find the module in the module search path.  This
 * path is initialised from the environment variable M4MODPATH, or if
 * not set, initalised to a configuration time default.  This
 * function reads the libtool .la file to get the real library name
 * (which can be system dependent) and returns NULL on failure and a
 * non-NULL void* on success.  If successful lt_dlopen returns the
 * value of this void*, which is a handle for the vm segment mapped.
 * Module_load() checks to see if the module is already loaded, and if
 * not, retrieves the symbol `m4_builtin_table' and returns it's value to
 * m4_loadmodule().  This pointer should be an m4_builtin*, which is
 * installed using install_builtin_table().
 *
 * In addition to builtin functions, you can also define static macro
 * expansions in the `m4_macro_table' symbol.  If you define this symbol
 * in your modules, it should be an array of `m4_macro's, mapping macro
 * names to the expansion text.
 *
 * When a module is loaded, the function "void m4_init_module(struct
 * obstack *obs)" is called, if defined.  Any non NULL return value of
 * this function will be the expansion of "load".  Before program
 * exit, all modules are unloaded and the function "void
 * m4_finish_module(void)" is called, if defined.  It is
 * safe to load the same module several times.
 *
 * To unload a module, use `unload(modulename)'.  The function
 * `m4_unload' calls m4_module_unload() in this file, which uses
 * remove_builtin_table() to remove the builtins defined by the
 * unloaded module from the symbol table.  If the module has been loaded
 * several times with calls to `load(modulename)', then the module
 * will not be unloaded until the same number of calls to unload
 * have been made (nor will the builtin table be purged, nor the finish
 * function called).  If the module is successfully unloaded, and it
 * defined a function called `m4_finish_module', that function will be
 * called last of all.
 *
 * Modules loaded from the command line (with the `-m' switch) can be
 * removed with unload too.
 *
 * When m4 exits, it will unload every remaining loaded module, calling
 * any `m4_finish_module' functions they defined before closing.
 **/

M4_GLOBAL_DATA List *m4_modules;

const char *
m4_module_name (module)
     const m4_module *module;
{
  return module ? module->modname : NULL;
}

const m4_builtin *
m4_module_builtins (module)
     const m4_module *module;
{
  return module ? module->bp : NULL;
}

const m4_macro *
m4_module_macros (module)
     const m4_module *module;
{
  return module ? module->mp : NULL;
}

VOID *
m4_module_find_by_modname (elt, match)
     List *elt;
     VOID *match;
{
  const m4_module *module = (const m4_module *) elt;

  if (strcmp (module->modname, (char *) match) == 0)
    return (VOID *) module;

  return NULL;
}

VOID *
m4_module_find_by_builtin (elt, match)
     List *elt;
     VOID *match;
{
  const m4_module *module = (const m4_module *) elt;

  if (module && module->bp)
    {
      const m4_builtin *bp;

      for (bp = &module->bp[0]; bp->name; ++bp)
	{
	  if (bp == (const m4_builtin *) match)
	    return (VOID *) module;
	}
    }

  return NULL;
}

/*
 * Initialisation.  Currently the module search path in path.c is
 * initialised from M4MODPATH.  Only absolute path names are accepted to
 * prevent the path search of the dlopen library from finding wrong
 * files.
 */
void
m4_module_init ()
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
      M4_DEBUG_MESSAGE("Module system initialised.");
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
 * case of error.
 */

static VOID *
module_handle_find (elt, match)
     List *elt;
     VOID *match;
{
  m4_module *module = (m4_module *) elt;
  if (module->handle == match)
    return (VOID *) module;

  return NULL;
}

const m4_module *
m4_module_load (modname, obs)
     const char *modname;
     struct obstack *obs;
{
  lt_dlhandle	    handle	= NULL;
  m4_module_init_t *init_func	= NULL;
  m4_module	   *module	= NULL;
  m4_builtin	   *bp		= NULL;
  m4_macro	   *mp		= NULL;

  /* Dynamically load the named module. */
  handle = lt_dlopenext (modname);

  if (handle)
    {
      module = (m4_module *) list_find (m4_modules, handle,
					module_handle_find);
      if (module)
	{
	  lt_dlclose (handle); /* close the duplicate copy */

	  /* increment the load counter */
	  module->ref_count++;

#ifdef DEBUG_MODULES
	  M4_DEBUG_MESSAGE2("module %s: now has %d references.",
			 modname, module->ref_count);
#endif /* DEBUG_MODULES */
	  return module;
	}
    }


  if (handle)
    {
      /* Find the builtin table in the loaded module. */
      bp = (m4_builtin *) lt_dlsym (handle, "m4_builtin_table");

      /* Find the macro table in the loaded module. */
      mp = (m4_macro *) lt_dlsym (handle, "m4_macro_table");

      /* Find and run the initialising function in the loaded module
       * (if any).
       */
      init_func = (m4_module_init_t*) lt_dlsym (handle, "m4_init_module");
      if (init_func != NULL)
	(*init_func) (obs);
    }
  else
    {
      /* Couldn't load the module; diagnose and exit. */
      const char *dlerror = lt_dlerror();
      if (dlerror == NULL)
	M4ERROR ((EXIT_FAILURE, 0,
		  _("ERROR: cannot load module: `%s'"), modname));
      else
	M4ERROR ((EXIT_FAILURE, 0,
		  _("ERROR: cannot load module: `%s': %s"),
		  modname, dlerror));
    }

  if (!bp && !mp && !init_func)
    {
      M4ERROR ((EXIT_FAILURE, 0,
		_("ERROR: module `%s' has no entry points"), modname));
    }

  /* If the module was correctly loaded and has the necessary
   * symbols, then update our internal tables to remember the
   * new module.
   */
  if (handle)
    {
      module = XMALLOC (m4_module, 1);

#ifdef DEBUG_MODULES
      M4_DEBUG_MESSAGE1("module %s: loaded ok", modname);
#endif /* DEBUG_MODULES */

      module->modname	= xstrdup (modname);
      module->handle	= handle;
      module->bp	= bp;
      module->mp	= mp;
      module->ref_count = 1;

      m4_modules	= list_cons ((List *) module, m4_modules);
    }

  if (module)
    {
      boolean m4_resident_module
	= (boolean) lt_dlsym (handle, "m4_resident_module");

      if (m4_resident_module)
	{
	  lt_dlmakeresident (handle);
	}
    }

  return module;
}

void
m4_module_install (modname)
     const char *modname;
{
  const m4_module *module = m4_module_load (modname, NULL);

  if (module)
    {
      const m4_builtin *bp	= m4_module_builtins (module);
      const m4_macro *mp	= m4_module_macros (module);

      /* Install the macro functions.  */
      if (bp)
	m4_builtin_table_install (module, bp);

      /* Install the user macros. */
      if (mp)
	m4_macro_table_install (module, mp);
    }
}


/*
 * Unload a module.
 */
void
m4_module_unload (modname, obs)
     const char *modname;
     struct obstack *obs;
{
  m4_module_finish_t *finish_func;
  int		errors	= 0;
  lt_dlhandle	handle	= NULL;
  m4_module    *module	= NULL;
  List		*cur	= NULL;
  List		*prev	= NULL;

  /* Scan the list for the first module with a matching modname field. */
  for (cur = m4_modules; cur != NULL; cur = LIST_NEXT (cur))
    {
      module = (m4_module *) cur;
      if (modname != NULL && strcmp (modname, module->modname) == 0)
	{
	  handle = module->handle;
	  break;
	}
      prev = cur;
    }

  if (handle == NULL)
    {
      M4ERROR ((warning_status, 0,
		_("ERROR: cannot close module: %s is not loaded."), modname));
      return;
    }

  /* Only do the actual unload when the number of calls to unload this
     module is equal to the number of times it was loaded. */
  --module->ref_count;
  if (module->ref_count > 0)
    {
#ifdef DEBUG_MODULES
      M4_DEBUG_MESSAGE2("module %s: now has %d references.",
		     modname, module->ref_count);
#endif /* DEBUG_MODULES */
      return;
    }

  if (module->ref_count == 0)
    {
      /* Remove the table references only when ref_count is *exactly* equal
	 to 0.  If m4_module_unload is called again on a resideent module
	 after the references have already been removed, we needn't try to
	 remove them again!  */
      m4_remove_table_reference_symbols (module->bp, module->mp);

#ifdef DEBUG_MODULES
      M4_DEBUG_MESSAGE1("module %s: builtins undefined", modname);
#endif /* DEBUG_MODULES */
    }

  if (lt_dlisresident (handle))
    {
      /* Fix up reference count for resident modules, for the next
	 call to m4_module_unload(). */
      module->ref_count = 0;
    }
  else
    {
      /* Run the finishing function for the loaded module. */
      finish_func
	= (m4_module_finish_t *) lt_dlsym (handle, "m4_finish_module");

      if (finish_func != NULL)
	(*finish_func)();

      errors = lt_dlclose (handle);
      if (errors != 0)
	{
	  const char *dlerror = lt_dlerror();

	  if (dlerror == NULL)
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close module: `%s'"), modname));
	  else
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close module: `%s': %s"),
		      modname, dlerror));
	}
      else
	{
#ifdef DEBUG_MODULES
	  M4_DEBUG_MESSAGE1("module %s unloaded", module->modname);
#endif /* DEBUG_MODULES */

	  if (prev)
	    prev->next = (List *) module->next;
	  else
	    m4_modules = (List *) module->next;

	  xfree (module->modname);
	  xfree (module);
	}
    }
}

void
m4_module_unload_all ()
{
  int errors = 0;
  m4_module *module;
  m4_module_finish_t *finish_func;

  /* Find and run the finishing function for each loaded module. */
  while (m4_modules != NULL)
    {
      module = (m4_module *) m4_modules;

      finish_func = (m4_module_finish_t*)
	lt_dlsym(module->handle, "m4_finish_module");

      if (finish_func != NULL)
	{
	  (*finish_func)();

#ifdef DEBUG_MODULES
	  M4_DEBUG_MESSAGE1("module %s finish hook called", module->modname);
#endif /* DEBUG_MODULES */
	}

      errors = lt_dlclose (module->handle);
      if (errors != 0)
	break;

#ifdef DEBUG_MODULES
      M4_DEBUG_MESSAGE1("module %s unloaded", module->modname);
#endif /* DEBUG_MODULES */

      m4_modules = LIST_NEXT (m4_modules);
      xfree(module->modname);
      xfree(module);
    }

  if (errors != 0)
    errors = lt_dlexit();

  if (errors != 0)
    {
      const char *dlerror = lt_dlerror();
      if (m4_modules == NULL)
        {
          if (dlerror == NULL)
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close modules")));
	  else
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close modules: %s"),
		      dlerror));
	}
      else
        {
	  if (dlerror == NULL)
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close module: `%s'"),
		      module->modname));
	  else
	    M4ERROR ((EXIT_FAILURE, 0,
		      _("ERROR: cannot close module: `%s': %s"),
		      module->modname, dlerror));
	}
    }
}

/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1998, 1999, 2002,
   2003, 2004, 2005, 2006, 2007, 2008 Free Software Foundation, Inc.

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

#include "configmake.h"
#include "m4private.h"
#include "xvasprintf.h"

/* Define this to see runtime debug info.  Implied by DEBUG.  */
/*#define DEBUG_MODULES */

/*
 * This file implements dynamic modules in GNU M4.  A module is a
 * compiled shared object, that can be loaded into GNU M4 at run
 * time. Information about creating modules is in ../modules/README.
 *
 * This implementation uses libltdl, which is in turn can open modules
 * using either dlopen(3) (exists on GNU/Linux, OSF, Solaris, SunOS and
 * others), shl_load(3) (exists on HPUX), LoadLibrary(3) (exists on
 * Windows, cygwin, OS/2), load_add_on(3) (exists on BeOS), NSAddImage
 * (exists on MacOS) and can also fall back to dld_link(3) from GNU
 * libdld or lt_dlpreload from libtool if shared libraries are not
 * available on the host machine.
 *
 * An M4 module will usually define an external symbol called
 * `m4_builtin_table'.  This symbol points to a table of `m4_builtin'.
 * The table is saved as libltdl caller data and each definition therein
 * is added to the symbol table.
 *
 * To load a module, call m4_module_load(), which uses the libltdl
 * API to find the module in the module search path.  The search
 * path is initialized from the environment variable M4MODPATH, followed
 * by the configuration time default where the modules shipped with M4
 * itself are installed.  Libltdl reads the libtool .la file to
 * get the real library name (which can be system dependent), returning
 * NULL on failure or else a libtool module handle for the newly mapped
 * vm segment containing the module code.  If the module is not already
 * loaded, m4_module_load() retrieves its value for the symbol
 * `m4_builtin_table', which is installed using set_module_builtin_table().
 *
 * In addition to builtin functions, you can also define static macro
 * expansions in the `m4_macro_table' symbol.  If you define this symbol
 * in your modules, it should be an array of `m4_macro's, mapping macro
 * names to the expansion text.  Any macros defined in `m4_macro_table'
 * are installed into the M4 symbol table with set_module_macro_table().
 *
 * Each time a module is loaded, the module function prototyped as
 * "M4INIT_HANDLER (<module name>)" is called, if defined.  Any value
 * stored in OBS by this function becomes the expansion of the macro
 * which called it.  Before M4 exits, all modules are unloaded and the
 * function prototyped as "M4FINISH_HANDLER (<module name>)" is called,
 * if defined.  It is safe to load the same module several times: the
 * init and finish functions will also be called multiple times in this
 * case.
 *
 * To unload a module, use m4_module_unload(). which uses
 * m4__symtab_remove_module_references() to remove the builtins defined by
 * the unloaded module from the symbol table.  If the module has been
 * loaded several times with calls to m4_module_load, then the module will
 * not be unloaded until the same number of calls to m4_module_unload()
 * have been made (nor will the symbol table be purged).
 **/

#define MODULE_SELF_NAME	"!myself!"

static const char*  module_dlerror (void);
static int	    module_remove  (m4 *context, m4_module *module,
				    m4_obstack *obs);

static void	    install_builtin_table (m4*, m4_module *);
static void	    install_macro_table   (m4*, m4_module *);

static int	    m4__module_interface	(lt_dlhandle handle,
						 const char *id_string);

static lt_dlinterface_id iface_id = NULL;

const char *
m4_get_module_name (const m4_module *module)
{
  const lt_dlinfo *info;

  assert (module && module->handle);

  info = lt_dlgetinfo (module->handle);

  return info ? info->name : NULL;
}

void *
m4_module_import (m4 *context, const char *module_name,
		  const char *symbol_name, m4_obstack *obs)
{
  m4_module *	module		= m4__module_find (module_name);
  void *	symbol_address	= NULL;

  /* Try to load the module if it is not yet available (errors are
     diagnosed by m4_module_load).  */
  /* FIXME - should this use m4__module_open instead, to avoid
     polluting the symbol table when importing a function?  */
  if (!module)
    module = m4_module_load (context, module_name, obs);

  if (module)
    {
      symbol_address = lt_dlsym (module->handle, symbol_name);

      if (!symbol_address)
	m4_error (context, 0, 0, NULL,
		  _("cannot load symbol `%s' from module `%s'"),
		  symbol_name, module_name);
    }

  return symbol_address;
}

static void
install_builtin_table (m4 *context, m4_module *module)
{
  size_t i;

  assert (context);
  assert (module);
  for (i = 0; i < module->builtins_len; i++)
    {
      m4_symbol_value *value = m4_symbol_value_create ();
      const char *name = module->builtins[i].builtin.name;

      m4__set_symbol_value_builtin (value, &module->builtins[i]);
      if (m4_get_prefix_builtins_opt (context))
	name = xasprintf ("m4_%s", name);

      m4_symbol_pushdef (M4SYMTAB, name, value);

      if (m4_get_prefix_builtins_opt (context))
	free ((char *) name);
    }
  if (i)
    m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
		      _("module %s: builtins loaded"),
		      m4_get_module_name (module));
}

static void
install_macro_table (m4 *context, m4_module *module)
{
  const m4_macro *mp;

  assert (context);
  assert (module);

  mp = (const m4_macro *) lt_dlsym (module->handle, MACRO_SYMBOL);

  if (mp)
    {
      for (; mp->name != NULL; mp++)
	{
	  m4_symbol_value *value = m4_symbol_value_create ();
	  size_t len = strlen (mp->value);

	  /* Sanity check that builtins meet the required interface.  */
	  assert (mp->min_args <= mp->max_args);

	  m4_set_symbol_value_text (value, xmemdup (mp->value, len + 1),
				    len, 0);
	  VALUE_MODULE (value) = module;
	  VALUE_MIN_ARGS (value) = mp->min_args;
	  VALUE_MAX_ARGS (value) = mp->max_args;

	  m4_symbol_pushdef (M4SYMTAB, mp->name, value);
	}

      m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			_("module %s: macros loaded"),
			m4_get_module_name (module));
    }
}

m4_module *
m4_module_load (m4 *context, const char *name, m4_obstack *obs)
{
  m4_module *module = m4__module_open (context, name, obs);

  if (module && module->refcount == 1)
    {
      install_builtin_table (context, module);
      install_macro_table (context, module);
    }

  return module;
}

/* Make the module MODULE resident.  Return NULL on success, or a
   pre-translated error string on failure.  */
const char *
m4_module_makeresident (m4_module *module)
{
  assert (module);
  return lt_dlmakeresident (module->handle) ? module_dlerror () : NULL;
}

/* Unload a module.  */
void
m4_module_unload (m4 *context, const char *name, m4_obstack *obs)
{
  m4_module *	module  = NULL;
  int		errors	= 0;

  assert (context);

  if (name)
    module = m4__module_find (name);

  if (!module)
    {
      const char *error_msg = _("module not loaded");

      lt_dlseterror (lt_dladderror (error_msg));
      ++errors;
    }
  else
    errors = module_remove (context, module, obs);

  if (errors)
    {
      m4_error (context, EXIT_FAILURE, 0, NULL,
		_("cannot unload module `%s': %s"),
		name ? name : MODULE_SELF_NAME, module_dlerror ());
    }
}



static int
m4__module_interface (lt_dlhandle handle, const char *id_string)
{
  /* Shortcut.  If we've already associated our wrapper with this
     handle, then we've validated the handle in the past, and don't
     need to waste any time on additional lt_dlsym calls.  */
  m4_module *module = (m4_module *) lt_dlcaller_get_data (iface_id, handle);
  if (module)
    return 0;

  /* A valid m4 module must provide at least one of these symbols.  */
  return !(lt_dlsym (handle, INIT_SYMBOL)
	   || lt_dlsym (handle, FINISH_SYMBOL)
	   || lt_dlsym (handle, BUILTIN_SYMBOL)
	   || lt_dlsym (handle, MACRO_SYMBOL));
}


/* Return successive loaded modules that pass the interface test registered
   with the interface id.  */
m4_module *
m4__module_next (m4_module *module)
{
  lt_dlhandle handle = module ? module->handle : NULL;
  assert (iface_id);

  /* Resident modules still show up in the lt_dlhandle_iterate loop
     after they have been unloaded from m4.  */
  do
    {
      handle = lt_dlhandle_iterate (iface_id, handle);
      if (!handle)
	return NULL;
      module = (m4_module *) lt_dlcaller_get_data (iface_id, handle);
    }
  while (!module);
  assert (module->handle == handle);
  return module;
}

/* Return the first loaded module that passes the registered interface test
   and is called NAME.  */
m4_module *
m4__module_find (const char *name)
{
  lt_dlhandle handle;
  m4_module *module;
  assert (iface_id);

  handle = lt_dlhandle_fetch (iface_id, name);
  if (!handle)
    return NULL;
  module = (m4_module *) lt_dlcaller_get_data (iface_id, handle);
  if (module)
    assert (module->handle == handle);
  return module;
}


/* Initialization.  Currently the module search path in path.c is
   initialized from M4MODPATH.  Only absolute path names are accepted to
   prevent the path search of the dlopen library from finding wrong
   files. */
void
m4__module_init (m4 *context)
{
  int errors = 0;

  /* Do this only once!  If we already have an iface_id, then the
     module system has already been initialized.  */
  if (iface_id)
    {
      m4_error (context, 0, 0, NULL,
		_("multiple module loader initializations"));
      return;
    }

  errors      = lt_dlinit ();

  /* Register with libltdl for a key to store client data against
     ltdl module handles.  */
  if (!errors)
    {
      iface_id = lt_dlinterface_register ("m4 libm4", m4__module_interface);

      if (!iface_id)
	{
	  const char *error_msg = _("libltdl client registration failed");

	  lt_dlseterror (lt_dladderror (error_msg));

	  /* No need to check error statuses from the calls above -- If
	     either fails for some reason, a diagnostic will be set for
	     lt_dlerror() anyway.  */
	  ++errors;
	}
    }

  if (!errors)
    errors = lt_dlsetsearchpath (PKGLIBEXECDIR);

  /* If the user set M4MODPATH, then use that as the start of the module
     search path.  */
  if (!errors)
    {
      char *path = getenv (USER_MODULE_PATH_ENV);

      if (path)
	errors = lt_dlinsertsearchdir (lt_dlgetsearchpath (), path);
    }

  /* Couldn't initialize the module system; diagnose and exit.  */
  if (errors)
    m4_error (context, EXIT_FAILURE, 0, NULL,
	      _("failed to initialize module loader: %s"), module_dlerror ());

#ifdef DEBUG_MODULES
  fputs ("Module loader initialized.\n", stderr);
#endif /* DEBUG_MODULES */
}


/* Compare two builtins A and B for sorting, as in qsort.  */
static int
compare_builtin_CB (const void *a, const void *b)
{
  const m4__builtin *builtin_a = (const m4__builtin *) a;
  const m4__builtin *builtin_b = (const m4__builtin *) b;
  int result = strcmp (builtin_a->builtin.name, builtin_b->builtin.name);
  /* A builtin module should never provide two builtins with the same
     name.  */
  assert (result || a == b);
  return result;
}

/* Load a module.  NAME can be a absolute file name or, if relative,
   it is searched for in the module path.  The module is unloaded in
   case of error.  */
m4_module *
m4__module_open (m4 *context, const char *name, m4_obstack *obs)
{
  static const char *	suffixes[]	= { "", ".la", LT_MODULE_EXT, NULL };
  char *		filepath	= NULL;
  lt_dlhandle		handle		= NULL;
  lt_dladvise		advise		= NULL;
  m4_module *		module		= NULL;
  m4_module_init_func *	init_func	= NULL;

  assert (context);
  assert (iface_id);		/* need to have called m4__module_init */

  /* Try opening as a preloaded module initially incase path searching
     has been disabled by POSIXLY_CORRECT... */
  if (!lt_dladvise_init (&advise) && !lt_dladvise_preload (&advise))
    handle = lt_dlopenadvise (name, advise);
  lt_dladvise_destroy (&advise);

  /* ...otherwise resort to a path search anyway.  */
  if (!handle)
    {
      filepath = m4_path_search (context, name, suffixes);
      if (filepath)
        {
          handle = lt_dlopenext (filepath);
          free (filepath);
        }
    }

  if (handle)
    {
      const lt_dlinfo *info = lt_dlgetinfo (handle);

      /* If we have a handle, there must be handle info.  */
      assert (info);

#ifdef DEBUG_MODULES
      if (info->ref_count > 1)
	{
	  xfprintf (stderr, "module %s: now has %d libtool references.",
		    name, info->ref_count);
	}
#endif /* DEBUG_MODULES */

      m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			_("module %s: opening file `%s'"),
			name ? name : MODULE_SELF_NAME, info->filename);

      /* Provide the m4_module corresponding to the lt_dlhandle, if
	 not yet created.  */
      module = (m4_module *) lt_dlcaller_get_data (iface_id, handle);
      if (!module)
	{
	  void *old;
	  const char *err;
	  const m4_builtin *bp;

	  module = (m4_module *) xzalloc (sizeof *module);
	  module->handle = handle;

	  /* TODO - change module interface to return function pointer
	     that supplies both table and length of table, rather than
	     returning data pointer that must have a sentinel
	     entry?  */
	  bp = (m4_builtin *) lt_dlsym (module->handle, BUILTIN_SYMBOL);
	  if (bp)
	    {
	      const m4_builtin *tmp;
	      m4__builtin *builtin;
	      for (tmp = bp; tmp->name; tmp++)
		module->builtins_len++;
	      module->builtins =
		(m4__builtin *) xnmalloc (module->builtins_len,
					  sizeof *module->builtins);
	      for (builtin = module->builtins; bp->name != NULL;
		   bp++, builtin++)
		{
		  /* Sanity check that builtins meet the required
		     interface.  */
		  assert (bp->min_args <= bp->max_args);
		  assert (bp->min_args > 0
			  || (bp->flags & (M4_BUILTIN_BLIND
					   | M4_BUILTIN_SIDE_EFFECT)) == 0);
		  assert (bp->max_args
			  || (bp->flags & M4_BUILTIN_FLATTEN_ARGS) == 0);
		  assert ((bp->flags & ~M4_BUILTIN_FLAGS_MASK) == 0);

		  memcpy (&builtin->builtin, bp, sizeof *bp);
		  builtin->builtin.name = xstrdup (bp->name);
		  builtin->module = module;
		}
	    }
	  qsort (module->builtins, module->builtins_len,
		 sizeof *module->builtins, compare_builtin_CB);

	  /* clear out any stale errors, since we have to use
	     lt_dlerror to distinguish between success and
	     failure.  */
	  lt_dlerror ();
	  old = lt_dlcaller_set_data (iface_id, handle, module);
	  assert (!old);
	  err = lt_dlerror ();
	  if (err)
	    m4_error (context, EXIT_FAILURE, 0, NULL,
		      _("unable to load module `%s': %s"), name, err);
	}

      /* Find and run any initializing function in the opened module,
	 each time the module is opened.  */
      module->refcount++;
      init_func = (m4_module_init_func *) lt_dlsym (handle, INIT_SYMBOL);
      if (init_func)
	{
	  init_func (context, module, obs);

	  m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			    _("module %s: init hook called"), name);
	}
      else if (!lt_dlsym (handle, FINISH_SYMBOL)
	       && !lt_dlsym (handle, BUILTIN_SYMBOL)
	       && !lt_dlsym (handle, MACRO_SYMBOL))
	{
	  m4_error (context, EXIT_FAILURE, 0, NULL,
		    _("module `%s' has no entry points"), name);
	}

      m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			_("module %s: opened"), name);
    }
  else
    {
      /* Couldn't open the module; diagnose and exit. */
      m4_error (context, EXIT_FAILURE, 0, NULL,
		_("cannot open module `%s': %s"), name, module_dlerror ());
    }

  return module;
}

void
m4__module_exit (m4 *context)
{
  m4_module *	module	= m4__module_next (NULL);
  int		errors	= 0;

  while (module && !errors)
    {
      m4_module *      pending	= module;

      /* If we are about to unload the final reference, move on to the
	 next module before we unload the current one.  */
      if (pending->refcount <= 1)
	module = m4__module_next (module);

      errors = module_remove (context, pending, NULL);
    }

  assert (iface_id);		/* need to have called m4__module_init */
  lt_dlinterface_free (iface_id);
  iface_id = NULL;

  if (!errors)
    errors = lt_dlexit ();

  if (errors)
    {
      m4_error (context, EXIT_FAILURE, 0, NULL,
		_("cannot unload all modules: %s"), module_dlerror ());
    }
}



/* FIXME - libtool doesn't expose lt_dlerror strings for translation.  */
static const char *
module_dlerror (void)
{
  const char *dlerror = lt_dlerror ();

  if (!dlerror)
    dlerror = _("unknown error");

  return dlerror;
}

/* Close one reference to the module MODULE, and output to OBS any
   information from the finish hook of the module.  If no references
   to MODULE remain, also remove all symbols and other memory
   associated with the module.  */
static int
module_remove (m4 *context, m4_module *module, m4_obstack *obs)
{
  const lt_dlinfo *		info;
  int				errors	= 0;
  const char *			name;
  lt_dlhandle			handle;
  bool				last_reference = false;
  bool				resident = false;
  m4_module_finish_func *	finish_func;

  assert (module && module->handle);

  /* Be careful when closing myself.  */
  handle = module->handle;
  name = m4_get_module_name (module);
  name = xstrdup (name ? name : MODULE_SELF_NAME);

  info = lt_dlgetinfo (handle);
  resident = info->is_resident;

  /* Only do the actual close when the number of calls to close this
     module is equal to the number of times it was opened. */
#ifdef DEBUG_MODULES
  if (info->ref_count > 1)
    {
      xfprintf (stderr, "module %s: now has %d libtool references.",
		name, info->ref_count - 1);
    }
#endif /* DEBUG_MODULES */

  if (module->refcount-- == 1)
    {
      /* Remove the table references only when ref_count is *exactly*
	 equal to 1.  If module_close is called again on a
	 resident module after the references have already been
	 removed, we needn't try to remove them again!  */
      m4__symtab_remove_module_references (M4SYMTAB, module);

      m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			_("module %s: symbols unloaded"), name);
      last_reference = true;
    }

  finish_func = (m4_module_finish_func *) lt_dlsym (handle, FINISH_SYMBOL);
  if (finish_func)
    {
      finish_func (context, module, obs);

      m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			_("module %s: finish hook called"), name);
    }

  if (last_reference && resident)
    {
      /* Special case when closing last reference to resident module -
	 we need to remove the association of the m4_module wrapper
	 with the dlhandle, because we are about to free the wrapper,
	 but the module will still show up in lt_dlhandle_iterate.
	 Still call lt_dlclose to reduce the ref count, but ignore the
	 failure about not closing a resident module.  */
      void *old = lt_dlcaller_set_data (iface_id, handle, NULL);
      if (!old)
	m4_error (context, EXIT_FAILURE, 0, NULL,
		  _("unable to close module `%s': %s"), name,
		  module_dlerror());
      assert (old == module);
      lt_dlclose (handle);
      m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			_("module %s: resident module not closed"), name);
    }
  else
    {
      errors = lt_dlclose (handle);
      /* Ignore the error expected if the module was resident.  */
      if (resident)
	errors = 0;
      if (!errors)
	{
	  m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
			    _("module %s: closed"), name);
	}
    }

  if (errors)
    m4_error (context, EXIT_FAILURE, 0, NULL,
	      _("cannot close module `%s': %s"), name, module_dlerror ());
  if (last_reference)
    {
      size_t i;
      for (i = 0; i < module->builtins_len; i++)
	free ((char *) module->builtins[i].builtin.name);
      free (module->builtins);
      free (module);
    }

  DELETE (name);

  return errors;
}


/* Below here are the accessor functions behind fast macros.  Declare
   them last, so the rest of the file can use the macros.  */

/* Return the current refcount, or times that module MODULE has been
   opened.  */
#undef m4_module_refcount
int
m4_module_refcount (const m4_module *module)
{
  const lt_dlinfo *info;
  assert (module);
  info = lt_dlgetinfo (module->handle);
  assert (info);
  assert (module->refcount <= info->ref_count);
  return module->refcount;
}

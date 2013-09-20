/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1998-1999, 2002-2008, 2010, 2013 Free
   Software Foundation, Inc.

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
 * An M4 module will usually define an external symbol named after the
 * basename of the loadable module:
 *
 *   void
 *   mymod_LTX_m4_init_module (m4 *context, m4_module *module,
 *                             m4_obstack *obs)
 *
 * The function is only called the first time the module is included
 * and generally uses either `m4_install_builtins' or
 * `m4_install_macros' (or both!) to register whatever builtins and
 * macros are provided by the module.
 *
 * To load a module, call m4_module_load(), which searches for the
 * module in directories from M4PATH. The search path is initialized
 * from the environment variable M4PATH, followed by the configuration
 * time default where the modules shipped with M4 itself are installed.
 * `m4_module_load' returns NULL on failure, or else an opaque module
 * handle for the newly mapped vm segment containing the module code.
 * If the module is not already loaded, m4_module_load() the builtins
 * and macros registered by `mymod_LTX_m4_init_module' are installed
 * into the symbol table using `install_builtin_table' and `install_
 * macro_table' respectively.
 **/

#define MODULE_SELF_NAME        "!myself!"

static const char*  module_dlerror (void);

static void         install_builtin_table (m4*, m4_module *);
static void         install_macro_table   (m4*, m4_module *);

static int          compare_builtin_CB    (const void *a, const void *b);
static int          m4__module_interface  (lt_dlhandle handle,
                                           const char *id_string);

static lt_dlinterface_id iface_id = NULL;

const char *
m4_get_module_name (const m4_module *module)
{
  assert (module);
  return module->name;
}

void *
m4_module_import (m4 *context, const char *module_name,
                  const char *symbol_name, m4_obstack *obs)
{
  m4_module *   module          = m4__module_find (context, module_name);
  void *        symbol_address  = NULL;

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

void
m4_install_builtins (m4 *context, m4_module *module, const m4_builtin *bp)
{
  assert (context);
  assert (module);
  assert (bp);

  const m4_builtin *tmp;
  m4__builtin *builtin;
  for (tmp = bp; tmp->name; tmp++)
    module->builtins_len++;
  module->builtins = (m4__builtin *) xnmalloc (module->builtins_len,
                                               sizeof *module->builtins);
  for (builtin = module->builtins; bp->name != NULL; bp++, builtin++)
    {
      /* Sanity check that builtins meet the required interface. */
      assert (bp->min_args <= bp->max_args);
      assert (bp->min_args > 0 ||
              (bp->flags & (M4_BUILTIN_BLIND|M4_BUILTIN_SIDE_EFFECT)) == 0);
      assert (bp->max_args ||
              (bp->flags & M4_BUILTIN_FLATTEN_ARGS) == 0);
      assert ((bp->flags & ~M4_BUILTIN_FLAGS_MASK) == 0);
      memcpy (&builtin->builtin, bp, sizeof *bp);
      builtin->builtin.name = xstrdup (bp->name);
      builtin->module = module;
    }
  qsort (module->builtins, module->builtins_len,
         sizeof *module->builtins, compare_builtin_CB);
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

      m4_symbol_pushdef (M4SYMTAB, name, strlen (name), value);

      if (m4_get_prefix_builtins_opt (context))
        DELETE (name);
    }
  if (i)
    m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
                      _("module %s: builtins loaded"),
                      m4_get_module_name (module));
}

void
m4_install_macros (m4 *context, m4_module *module, const m4_macro *mp)
{
  assert (context);
  assert (module);
  assert (mp);

  module->macros = (m4_macro *) mp;
}

static void
install_macro_table (m4 *context, m4_module *module)
{
  const m4_macro *mp;

  assert (context);
  assert (module);

  mp = module->macros;

  if (mp)
    {
      for (; mp->name != NULL; mp++)
        {
          m4_symbol_value *value = m4_symbol_value_create ();
          size_t len = strlen (mp->value);

          /* Sanity check that builtins meet the required interface.  */
          assert (mp->min_args <= mp->max_args);

          m4_set_symbol_value_text (value, xmemdup0 (mp->value, len), len, 0);
          VALUE_MODULE (value) = module;
          VALUE_MIN_ARGS (value) = mp->min_args;
          VALUE_MAX_ARGS (value) = mp->max_args;

          m4_symbol_pushdef (M4SYMTAB, mp->name, strlen (mp->name), value);
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

  if (module)
    {
      const lt_dlinfo *info = lt_dlgetinfo (module->handle);

      if (info->ref_count == 1)
        {
          install_builtin_table (context, module);
          install_macro_table (context, module);
        }
    }

  return module;
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
  return !(lt_dlsym (handle, INIT_SYMBOL));
}


/* Return successive loaded modules. */
m4_module *
m4_module_next (m4 *context, m4_module *module)
{
  return module ? module->next : context->modules;
}

/* Return the first loaded module that passes the registered interface test
   and is called NAME.  */
m4_module *
m4__module_find (m4 *context, const char *name)
{
  m4_module **pmodule = (m4_module **) m4_hash_lookup (context->namemap, name);
  return pmodule ? *pmodule : NULL;
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
  assert (iface_id);            /* need to have called m4__module_init */

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
                        _("module %s: opening file %s"),
                        name ? name : MODULE_SELF_NAME,
                        quotearg_style (locale_quoting_style, info->filename));

      /* Provide the m4_module corresponding to the lt_dlhandle, if
         not yet created.  */
      module = (m4_module *) lt_dlcaller_get_data (iface_id, handle);
      if (!module)
        {
          void *old;
          const char *err;

          module = (m4_module *) xzalloc (sizeof *module);
          module->name   = xstrdup (name);
          module->handle = handle;
	  module->next   = context->modules;

	  context->modules = module;
	  m4_hash_insert (context->namemap, xstrdup (name), module);

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

          /* Find and run any initializing function in the opened module,
             the first time the module is opened.  */
          init_func = (m4_module_init_func *) lt_dlsym (handle, INIT_SYMBOL);
          if (init_func)
            {
              init_func (context, module, obs);

              m4_debug_message (context, M4_DEBUG_TRACE_MODULE,
                                _("module %s: init hook called"), name);
            }
          else
            {
              m4_error (context, EXIT_FAILURE, 0, NULL,
                        _("module `%s' has no entry point"), name);
            }
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


/* FIXME - libtool doesn't expose lt_dlerror strings for translation.  */
static const char *
module_dlerror (void)
{
  const char *dlerror = lt_dlerror ();

  if (!dlerror)
    dlerror = _("unknown error");

  return dlerror;
}

/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 90, 91, 92, 93, 98 Free Software Foundation, Inc.
  
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Handling of path search of included files via the builtins "include"
   and "sinclude" (and "loadmodule" if configured).  */

#include "m4.h"

#ifdef WITH_MODULES
#include "pathconf.h"
#endif /* WITH_MODULES */

struct includes
{
  struct includes *next;	/* next directory to search */
  const char *dir;		/* directory */
  int len;
};

typedef struct includes includes;

struct include_list
{
  includes *list;		/* the list of path directories */
  includes *list_end;		/* the end of same */
  int max_length;		/* length of longest directory name */
};

typedef struct include_list include_list;

static include_list dirpath;	/* the list of path directories */

#ifdef WITH_MODULES
static include_list modpath;	/* the list of module directories */
#endif /* WITH_MODULES */


void
include_init (void)
{
  dirpath.list = NULL;
  dirpath.list_end = NULL;
  dirpath.max_length = 0;

#ifdef WITH_MODULES
  modpath.list = NULL;
  modpath.list_end = NULL;
  modpath.max_length = 0;
#endif /* WITH_MODULES */
}

static void
include_directory (struct include_list *list, const char *dir)
{
  includes *incl;

  if (*dir == '\0')
    dir = ".";

  incl = (includes *) xmalloc (sizeof (struct includes));
  incl->next = NULL;
  incl->len = strlen (dir);
  incl->dir = xstrdup (dir);

  if (incl->len > list->max_length) /* remember len of longest directory */
    list->max_length = incl->len;

  if (list->list_end == NULL)
    list->list = incl;
  else
    list->list_end->next = incl;
  list->list_end = incl;
}

static void
env_init (struct include_list *list, char *path, boolean abs)
{
  char *path_end;

  if (path == NULL)
    return;

  do
    {
      path_end = strchr (path, ':');
      if (path_end)
	*path_end = '\0';
      if (!abs || *path == '/')
	include_directory (list, path);
      path = path_end + 1;
    }
  while (path_end);
}


/* Functions for normal input path search */

void
include_env_init (void)
{
  if (no_gnu_extensions)
    return;

  env_init (&dirpath, getenv ("M4PATH"), FALSE);
}

void
add_include_directory (const char *dir)
{
  if (no_gnu_extensions)
    return;

  include_directory (&dirpath, dir);

#ifdef DEBUG_INCL
  fprintf (stderr, "add_include_directory (%s);\n", dir);
#endif
}

FILE *
path_search (const char *dir, char **expanded_name)
{
  FILE *fp;
  includes *incl;
  char *name;			/* buffer for constructed name */

  /* Look in current working directory first.  */
  fp = fopen (dir, "r");
  if (fp != NULL)
    {
      if (expanded_name != NULL)
	*expanded_name = xstrdup (dir);
      return fp;
    }

  /* If file not found, and filename absolute, fail.  */
  if (*dir == '/' || no_gnu_extensions)
    return NULL;

  name = (char *) xmalloc (dirpath.max_length + 1 + strlen (dir) + 1);

  for (incl = dirpath.list; incl != NULL; incl = incl->next)
    {
      strncpy (name, incl->dir, incl->len);
      name[incl->len] = '/';
      strcpy (name + incl->len + 1, dir);

#ifdef DEBUG_INCL
      fprintf (stderr, "path_search (%s) -- trying %s\n", dir, name);
#endif

      fp = fopen (name, "r");
      if (fp != NULL)
	{
	  if (debug_level & DEBUG_TRACE_PATH)
	    DEBUG_MESSAGE2 (_("Path search for `%s' found `%s'"), dir, name);

	  if (expanded_name != NULL)
	    *expanded_name = xstrdup (name);
	  break;
	}
    }

  xfree (name);

  return fp;
}

#ifdef DEBUG_INCL

static int
include_dump (void)
{
  includes *incl;

  fprintf (stderr, "include_dump:\n");
  for (incl = dirpath.list; incl != NULL; incl = incl->next)
    fprintf (stderr, "\t%s\n", incl->dir);
}

#endif /* DEBUG_INCL */


/* Functions for module search path */

#ifdef WITH_MODULES

void
module_env_init (void)
{
  if (no_gnu_extensions)
    return;

  include_directory (&modpath, MODULE_PATH);
  env_init (&modpath, getenv ("M4MODPATH"), TRUE);
}

void
add_module_directory (const char *dir)
{
  if (no_gnu_extensions)
    return;

  if (*dir == '/')
    include_directory (&modpath, dir);

#ifdef DEBUG_INCL
  fprintf (stderr, "add_module_directory (%s);\n", dir);
#endif
}

voidstar
module_search (const char *modname, module_func *try)
{
  voidstar value = NULL;
  includes *incl;
  char *name;			/* buffer for constructed name */

  /* If absolute, modname is a filename.  */
  if (*modname == '/')
    return (*try) (modname);

  name = (char *) xmalloc (modpath.max_length + 1 + strlen (modname) + 1);

  for (incl = modpath.list; incl != NULL; incl = incl->next)
    {
      strncpy (name, incl->dir, incl->len);
      name[incl->len] = '/';
      strcpy (name + incl->len + 1, modname);

#ifdef DEBUG_MODULE
      fprintf (stderr, "module_search (%s) -- trying %s\n", modname, name);
#endif

      value = (*try) (name);
      if (value != NULL)
	{
	  if (debug_level & DEBUG_TRACE_PATH)
	    DEBUG_MESSAGE2 (_("Module search for `%s' found `%s'"),
			    modname, name);
	  break;
	}
    }
  xfree (name);
  return value;
}

#endif /* WITH_MODULES */

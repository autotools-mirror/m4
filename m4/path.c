/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 90, 91, 92, 93, 98 Free Software Foundation, Inc.

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

/* Handling of path search of included files via the builtins "include"
   and "sinclude".  */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#if HAVE_STRING_H
#  include <string.h>
#else
#  if HAVE_STRINGS_H
#    include <strings.h>
#  endif
#endif

#include "m4module.h"
#include "m4private.h"

static void search_path_add (m4__search_path_info *, const char *);
static void search_path_env_init (m4__search_path_info *, char *, bool);


/*
 * General functions for search paths
 */

static void
search_path_add (m4__search_path_info *info, const char *dir)
{
  m4__search_path *path = xmalloc (sizeof m4__search_path);

  if (*dir == '\0')
    dir = ".";

  path->next = NULL;
  path->len = strlen (dir);
  path->dir = xstrdup (dir);

  if (path->len > info->max_length) /* remember len of longest directory */
    info->max_length = path->len;

  if (info->list_end == NULL)
    info->list = path;
  else
    info->list_end->next = path;
  info->list_end = path;
}

static void
search_path_env_init (m4__search_path_info *info, char *path, bool isabs)
{
  char *path_end;

  if (info == NULL || path == NULL)
    return;

  do
    {
      path_end = strchr (path, ':');
      if (path_end)
	*path_end = '\0';
      if (!isabs || *path == '/')
	search_path_add (info, path);
      path = path_end + 1;
    }
  while (path_end);
}


/* Functions for normal input path search */

void
m4_include_env_init (m4 *context)
{
  if (m4_get_no_gnu_extensions_opt (context))
    return;

  search_path_env_init (m4__get_search_path (context),
			getenv ("M4PATH"), false);
}

void
m4_add_include_directory (m4 *context, const char *dir)
{
  if (m4_get_no_gnu_extensions_opt (context))
    return;

  search_path_add (m4__get_search_path (context), dir);

#ifdef DEBUG_INCL
  fprintf (stderr, "add_include_directory (%s);\n", dir);
#endif
}

FILE *
m4_path_search (m4 *context, const char *dir, char **expanded_name)
{
  FILE *fp;
  m4__search_path *incl;
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
  if (*dir == '/' || m4_get_no_gnu_extensions_opt (context))
    return NULL;

  name = (char *) xmalloc (m4__get_search_path (context)->max_length + 1 + strlen (dir) + 1);

  for (incl = m4__get_search_path (context)->list; incl != NULL; incl = incl->next)
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
	  if (BIT_TEST (m4_get_debug_level_opt (context), M4_DEBUG_TRACE_PATH))
	    M4_DEBUG_MESSAGE2 (context, _("Path search for `%s' found `%s'"),
			       dir, name);

	  if (expanded_name != NULL)
	    *expanded_name = xstrdup (name);
	  break;
	}
    }

  free (name);

  return fp;
}

#ifdef DEBUG_INCL

static void
include_dump (m4 *context)
{
  m4__search_path *incl;

  fprintf (stderr, "include_dump:\n");
  for (incl = m4__get_search_path (context)->list; incl != NULL; incl = incl->next)
    fprintf (stderr, "\t%s\n", incl->dir);
}

#endif /* DEBUG_INCL */

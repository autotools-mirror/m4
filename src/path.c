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
   and "sinclude".  */

#include "m4.h"

static struct search_path_info dirpath; /* the list of path directories */


/*
 * Generel functions for search paths
 */

struct search_path_info *
search_path_info_new (void)
{
  struct search_path_info *info;

  info = (struct search_path_info *)
    xmalloc (sizeof (struct search_path_info));
  info->list = NULL;
  info->list_end = NULL;
  info->max_length = 0;

  return info;
}

void
search_path_add (struct search_path_info *info, const char *dir)
{
  search_path *path;

  if (*dir == '\0')
    dir = ".";

  path = (struct search_path *) xmalloc (sizeof (struct search_path));
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

void
search_path_env_init (struct search_path_info *info, char *path, boolean abs)
{
  char *path_end;

  if (info == NULL || path == NULL)
    return;

  do
    {
      path_end = strchr (path, ':');
      if (path_end)
	*path_end = '\0';
      if (!abs || *path == '/')
	search_path_add (info, path);
      path = path_end + 1;
    }
  while (path_end);
}


void
include_init (void)
{
  dirpath.list = NULL;
  dirpath.list_end = NULL;
  dirpath.max_length = 0;
}


/* Functions for normal input path search */

void
include_env_init (void)
{
  if (no_gnu_extensions)
    return;

  search_path_env_init (&dirpath, getenv ("M4PATH"), FALSE);
}

void
add_include_directory (const char *dir)
{
  if (no_gnu_extensions)
    return;

  search_path_add (&dirpath, dir);

#ifdef DEBUG_INCL
  fprintf (stderr, "add_include_directory (%s);\n", dir);
#endif
}

FILE *
path_search (const char *dir, char **expanded_name)
{
  FILE *fp;
  struct search_path *incl;
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

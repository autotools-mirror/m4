/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1998, 2004, 2006, 2007,
   2008, 2009, 2010 Free Software Foundation, Inc.

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

/* Handling of path search of included files via the builtins "include"
   and "sinclude".  */

#include <config.h>

#include <string.h>

#include "m4private.h"

#include "dirname.h"
#include "filenamecat.h"

/* Define this to see runtime debug info.  Implied by DEBUG.  */
/*#define DEBUG_INCL */

static void search_path_add (m4__search_path_info *, const char *, bool);
static void search_path_env_init (m4__search_path_info *, char *, bool);


/*
 * General functions for search paths
 */

static void
search_path_add (m4__search_path_info *info, const char *dir, bool prepend)
{
  m4__search_path *path = (m4__search_path *) xmalloc (sizeof *path);

  path->len = strlen (dir);
  path->dir = xstrdup (dir);

  if (path->len > info->max_length) /* remember len of longest directory */
    info->max_length = path->len;

  if (prepend)
    {
      path->next = info->list;
      info->list = path;
      if (info->list_end == NULL)
        info->list_end = path;
    }
  else
    {
      path->next = NULL;

      if (info->list_end == NULL)
        info->list = path;
      else
        info->list_end->next = path;
      info->list_end = path;
    }
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
        search_path_add (info, path, false);
      path = path_end + 1;
    }
  while (path_end);
}


/* Functions for normal input path search */

void
m4_include_env_init (m4 *context)
{
  char *m4path;

  if (m4_get_posixly_correct_opt (context))
    return;

  m4path = getenv ("M4PATH");
  if (m4path)
    m4path = xstrdup (m4path);
  search_path_env_init (m4__get_search_path (context), m4path, false);
  free (m4path);
}

void
m4_add_include_directory (m4 *context, const char *dir, bool prepend)
{
  if (m4_get_posixly_correct_opt (context))
    return;

  search_path_add (m4__get_search_path (context), dir, prepend);

#ifdef DEBUG_INCL
  xfprintf (stderr, "add_include_directory (%s) %s;\n", dir,
            prepend ? "prepend" : "append");
#endif
}

/* Attempt to open FILE; if it opens, verify that it is not a
   directory, and ensure it does not leak across execs.  */
static FILE *
m4_fopen (m4 *context, const char *file, const char *mode)
{
  FILE *fp = fopen (file, "r");
  if (fp)
    {
      struct stat st;
      int fd = fileno (fp);
      if (fstat (fd, &st) == 0 && S_ISDIR (st.st_mode))
        {
          fclose (fp);
          errno = EISDIR;
          return NULL;
        }
      if (set_cloexec_flag (fileno (fp), true) != 0)
        m4_error (context, 0, errno, NULL,
                  _("cannot protect input file across forks"));
    }
  return fp;
}

/* Search for FILE according to -B options, `.', -I options, then
   M4PATH environment.  If successful, return the open file, and if
   RESULT is not NULL, set *RESULT to a malloc'd string that
   represents the file found with respect to the current working
   directory.  Otherwise, return NULL, and errno reflects the failure
   from searching `.' (regardless of what else was searched).  */

FILE *
m4_path_search (m4 *context, const char *file, char **expanded_name)
{
  FILE *fp;
  m4__search_path *incl;
  char *name; /* buffer for constructed name */
  int e = 0;

  if (expanded_name != NULL)
    *expanded_name = NULL;

  /* Reject empty file.  */
  if (*file == '\0')
    {
      errno = ENOENT;
      return NULL;
    }

  /* If file is absolute, or if we are not searching a path, a single
     lookup will do the trick.  */
  if (IS_ABSOLUTE_FILE_NAME (file) || m4_get_posixly_correct_opt (context))
    {
      fp = m4_fopen (context, file, "r");
      if (fp != NULL)
        {
          if (expanded_name != NULL)
            *expanded_name = xstrdup (file);
          return fp;
        }
      return NULL;
    }

  for (incl = m4__get_search_path (context)->list;
       incl != NULL; incl = incl->next)
    {
      name = file_name_concat (incl->dir, file, NULL);

#ifdef DEBUG_INCL
      xfprintf (stderr, "path_search (%s) -- trying %s\n", file, name);
#endif

      fp = m4_fopen (context, name, "r");
      if (fp != NULL)
        {
          m4_debug_message (context, M4_DEBUG_TRACE_PATH,
                            _("path search for %s found %s"),
                            quotearg_style (locale_quoting_style, file),
                            quotearg_n_style (1, locale_quoting_style, name));
          if (expanded_name != NULL)
            *expanded_name = name;
          else
            free (name);
          return fp;
        }
      else if (!incl->len)
        /* Capture errno only when searching `.'.  */
        e = errno;
      free (name);
    }

  errno = e;
  return NULL;
}

void
m4__include_init (m4 *context)
{
  m4__search_path_info *info = m4__get_search_path (context);

  assert (info);
  search_path_add (info, "", false);
}


#ifdef DEBUG_INCL

static void M4_GNUC_UNUSED
include_dump (m4 *context)
{
  m4__search_path *incl;

  fputs ("include_dump:\n", stderr);
  for (incl = m4__get_search_path (context)->list;
       incl != NULL; incl = incl->next)
    xfprintf (stderr, "\t%s\n", incl->dir);
}

#endif /* DEBUG_INCL */

/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1993, 1998, 2004, 2006-2010, 2013 Free Software
   Foundation, Inc.

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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "m4private.h"

#include "configmake.h"
#include "dirname.h"
#include "filenamecat.h"

/* Define this to see runtime debug info.  Implied by DEBUG.  */
/*#define DEBUG_INCL */

static const char *FILE_SUFFIXES[] = {
  "",
  ".m4f",
  ".m4",
  ".so",
  NULL
};

static const char *NO_SUFFIXES[] = { "", NULL };

static void search_path_add (m4__search_path_info *, const char *, bool);
static void search_path_env_init (m4__search_path_info *, char *, bool);
static void include_env_init (m4 *context);

#ifdef DEBUG_INCL
static void include_dump (m4 *context);
#endif


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

static void
include_env_init (m4 *context)
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



/* Functions for normal input path search */

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


/* Search for FILENAME according to -B options, `.', -I options, then
   M4PATH environment.  If successful, return the open file, and if
   RESULT is not NULL, set *RESULT to a malloc'd string that
   represents the file found with respect to the current working
   directory.  Otherwise, return NULL, and errno reflects the failure
   from searching `.' (regardless of what else was searched).  */
char *
m4_path_search (m4 *context, const char *filename, const char **suffixes)
{
  m4__search_path *incl;
  char *filepath;		/* buffer for constructed name */
  size_t max_suffix_len = 0;
  int i, e = 0;

  /* Reject empty file.  */
  if (*filename == '\0')
    {
      errno = ENOENT;
      return NULL;
    }

  /* Use no suffixes by default.  */
  if (suffixes == NULL)
    suffixes = NO_SUFFIXES;

  /* Find the longest suffix, so that we will always allocate enough
     memory for a filename with suffix.  */
  for (i = 0; suffixes && suffixes[i]; ++i)
    {
      size_t len = strlen (suffixes[i]);
      if (len > max_suffix_len)
        max_suffix_len = len;
    }

  /* If file is absolute, or if we are not searching a path, a single
     lookup will do the trick.  */
  if (IS_ABSOLUTE_FILE_NAME (filename))
    {
      size_t mem = strlen (filename);

      /* Try appending each of the suffixes we were given.  */
      filepath = strncpy (xmalloc (mem + max_suffix_len +1), filename, mem);
      for (i = 0; suffixes && suffixes[i]; ++i)
        {
          strcpy (filepath + mem, suffixes[i]);
          if (access (filepath, R_OK) == 0)
	    return filepath;

          /* If search fails, we'll use the error we got from the first
	     access (usually with no suffix).  */
	  if (i == 0)
	    e = errno;
        }
      free (filepath);

      /* No such file.  */
      errno = e;
      return NULL;
    }

  for (incl = m4__get_search_path (context)->list;
       incl != NULL; incl = incl->next)
    {
      char *pathname = file_name_concat (incl->dir, filename, NULL);
      size_t mem = strlen (pathname);

#ifdef DEBUG_INCL
      xfprintf (stderr, "path_search (%s) -- trying %s\n", filename, pathname);
#endif

      if (access (pathname, R_OK) == 0)
        {
          m4_debug_message (context, M4_DEBUG_TRACE_PATH,
                            _("path search for %s found %s"),
                            quotearg_style (locale_quoting_style, filename),
                            quotearg_n_style (1, locale_quoting_style, pathname));
          return pathname;
        }
      else if (!incl->len)
	/* Capture errno only when searching `.'.  */
	e = errno;

      filepath = strncpy (xmalloc (mem + max_suffix_len +1), pathname, mem);
      free (pathname);

      for (i = 0; suffixes && suffixes[i]; ++i)
        {
          strcpy (filepath + mem, suffixes[i]);
          if (access (filepath, R_OK) == 0)
            return filepath;
        }
      free (filepath);
    }

  errno = e;
  return NULL;
}


/* Attempt to open FILE; if it opens, verify that it is not a
   directory, and ensure it does not leak across execs.  */
FILE *
m4_fopen (m4 *context, const char *file, const char *mode)
{
  FILE *fp = NULL;

  if (file)
    {
      struct stat st;
      int fd;

      fp = fopen (file, mode);
      fd = fileno (fp);

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


/* Generic load function.  Push the input file or load the module named
   FILENAME, if it can be found in the search path.  Complain
   about inaccesible files iff SILENT is false.  */
bool
m4_load_filename (m4 *context, const m4_call_info *caller,
	          const char *filename, m4_obstack *obs, bool silent)
{
  char *filepath = NULL;
  char *suffix   = NULL;
  bool new_input = false;

  if (m4_get_posixly_correct_opt (context))
    {
      if (access (filename, R_OK) == 0)
        filepath = xstrdup (filename);
    }
  else
    filepath = m4_path_search (context, filename, FILE_SUFFIXES);

  if (filepath)
    suffix = strrchr (filepath, '.');

  if (!m4_get_posixly_correct_opt (context)
      && suffix
      && STREQ (suffix, ".so"))
    {
      m4_module_load (context, filename, obs);
    }
  else
    {
      FILE *fp = NULL;

      if (filepath)
        fp = m4_fopen (context, filepath, "r");

      if (fp == NULL)
        {
          if (!silent)
	    m4_error (context, 0, errno, caller, _("cannot open file '%s'"),
		      filename);
          free (filepath);
          return false;
        }

      m4_push_file (context, fp, filepath, true);
      new_input = true;
    }
  free (filepath);

  return new_input;
}


void
m4__include_init (m4 *context)
{
  include_env_init (context);

  {
    m4__search_path_info *info = m4__get_search_path (context);

    /* If M4PATH was not set, then search just the current directory by
       default. */
    assert (info);
    if (info->list_end == NULL)
      search_path_add (info, "", false);

    /* Non-core modules installation directory. */
    search_path_add (info, PKGLIBDIR, false);
  }

#ifdef DEBUG_INCL
  fputs ("initial include search path...\n", stderr);
  include_dump (context);
#endif
}



#ifdef DEBUG_INCL

static void
include_dump (m4 *context)
{
  m4__search_path *incl;

  fputs ("include_dump:\n", stderr);
  for (incl = m4__get_search_path (context)->list;
       incl != NULL; incl = incl->next)
    xfprintf (stderr, "\t'%s'\n", incl->dir);
}

#endif /* DEBUG_INCL */

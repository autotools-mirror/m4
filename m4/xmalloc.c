/* xmalloc.c -- malloc with out of memory checking
   Copyright 1990, 91, 92, 93, 94, 95, 96, 2000
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#if STDC_HEADERS
# include <stdlib.h>
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define textdomain(Domain)
# define _(Text) Text
#endif

#include "system.h"

/* If this file fails to compile because your machine has no memset()
   function, you should ensure that either HAVE_CALLOC or HAVE_BZERO
   are defined in config.h */
#if !HAVE_BZERO
# define bzero(p, s)	memset (s, 0, n)
#endif

#ifndef STDC_HEADERS
void *calloc  ();
void *malloc  ();
void *realloc ();
void free     ();
#endif

/* Prototypes for functions defined here.  */
static void *fixup_null_alloc (size_t n);


/* Exit value when the requested amount of memory is not available.
   The caller may set it to some other value.  */
M4_GLOBAL_DATA int xmalloc_exit_failure = EXIT_FAILURE;


/* Your program must provide these functions in order for
   xmalloc() to work. */
#if __STDC__ && (HAVE_VPRINTF || HAVE_DOPRNT)
extern void error (int, int, const char *, ...);
#else
extern void error ();
#endif

static void *
fixup_null_alloc (n)
     size_t n;
{
  void *p;

  p = 0;
  if (n == 0)
    p = malloc ((size_t) 1);
  if (p == 0)
    error (xmalloc_exit_failure, 0, _("Memory exhausted"));
  return p;
}

/* Allocate N bytes of memory dynamically, with error checking.  */

void *
xmalloc (n)
     size_t n;
{
  void *p;

  p = malloc (n);
  if (p == 0)
    p = fixup_null_alloc (n);
  return p;
}

/* Allocate memory for N elements of S bytes, with error checking.  */

void *
xcalloc (n, s)
     size_t n, s;
{
  void *p;
#if HAVE_CALLOC
  p = calloc (n, s);
  if (p == 0)
    p = fixup_null_alloc (n);
#else
  p = xmalloc (n * s);
  bzero (p, n * s);
#endif
  return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.
   If P is NULL, run xmalloc.  */

void *
xrealloc (p, n)
     void *p;
     size_t n;
{
  if (p == 0)
    return xmalloc (n);
  p = realloc (p, n);
  if (p == 0)
    p = fixup_null_alloc (n);
  return p;
}

/* Don't free NULL pointers. */
void
xfree (stale)
     void *stale;
{
  if (stale)
    free (stale);
}

/* xstrzdup.c -- copy a string segment with out of memory checking
   Copyright 2003 Free Software Foundation, Inc.

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

#if defined (__STDC__) && __STDC__
char *xmalloc (size_t);
char *xstrzdup (char *string, size_t len);
#else
char *xmalloc ();
#endif

/* Return a newly allocated copy of STRING.  */

char *
xstrzdup (string, len)
     char *string;
     size_t len;
{
  char *result = (char *) xmalloc (1+ len);
  size_t index;

  for (index = 0; index < len; ++index)
    result[index] = string[index];
  result[len] = '\0';

  return result;
}

/* list.c -- maintain lists of types with forward pointer fields
   Copyright 2000 Free Software Foundation, Inc.
  
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

/* Written by Gary V. Vaughan <gary@gnu.org> */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "list.h"
#include "error.h"
#include "m4private.h"

#ifndef NULL
#  define NULL	0
#endif

List *
list_new (userdata)
     VOID *userdata;
{
  List *new = (List *) xmalloc (sizeof (List));

  LIST_NEXT(new) = NULL;
  LIST_DATA(new) = userdata;

  return new;
}

List *
list_delete (head, delete)
     List *head;
     void (*delete) M4_PARAMS((VOID *data));
{
#if DEBUG
  if (!delete)
    error (EXIT_FAILURE, 0,
	   _("%s (): NULL %s argument"), __PRETTY_FUNCTION__, "delete");
#endif

  while (head)
    {
      List *next = LIST_NEXT (head);
      (*delete) (head);
      head = next;
    }
 
  return NULL;
}

/* Call Find repeatedly with MATCH and each element of *PHEAD, until
   FIND returns non-NULL, or the list is exhausted.  If a match is found
   the matching element is removed from *PHEAD, and the value returned
   by the matching call to FIND is returned.

  To avoid memory leaks, unless you already have the address of the
  stale element, you should probably return that from FIND if it makes
  a successful match.  */
VOID *
list_remove (phead, match, find)
     List **phead;
     VOID *match;
     ListCompare *find;
{
  List *stale = NULL;
  VOID *result = NULL;
  
#if DEBUG
  if (!find)
    error (EXIT_FAILURE, 0,
	   _("%s (): NULL %s argument"), __PRETTY_FUNCTION__, "find");
#endif

  if (!phead || !*phead)
    return NULL;

  /* Does the head of the passed list match? */
  result = (*find) (*phead, match);
  if (result)
    {
      stale = *phead;
      *phead = LIST_NEXT (stale);
    }
  /* what about the rest of the elements? */
  else
    {
      List *head;
      for (head = *phead; LIST_NEXT (head); head = LIST_NEXT (head))
	{
	  result = (*find) (LIST_NEXT (head), match);
	  if (result)
	    {
	      stale		= LIST_NEXT (head);
	      LIST_NEXT (head)	= LIST_NEXT (stale);
	    }
	}
    }

  return result;
}

List *
list_cons (head, tail)
     List *head;
     List *tail;
{
  if (head)
    {
      LIST_NEXT (head) = tail;
      return head;
    }

  return tail;
}

List *
list_tail (head)
     List *head;
{
  return head ? LIST_NEXT (head) : NULL;
}

List *
list_nth (head, n)
     List *head;
     size_t n;
{
  for (;n > 1 && head; n--)
    head = LIST_NEXT (head);

  return head;
}

/* Call FIND repeatedly with SEARCH and each element of HEAD, until
   FIND returns non-NULL, or the list is exhausted.  If a match is found
   the value returned by the matching call to FIND is returned. */
VOID *
list_find (head, match, find)
     List *head;
     VOID *match;
     ListCompare *find;
{
  VOID *result = NULL;
  
#if DEBUG
  if (!find)
    error (EXIT_FAILURE, 0,
	   _("%s (): NULL %s argument"), __PRETTY_FUNCTION__, "find");
#endif

  for (; head; head = LIST_NEXT (head))
    {
      result = (*find) (head, match);
      if (result)
	break;
    }

  return result;
}  

size_t
list_length (head)
     List *head;
{
  size_t n;
  
  for (n = 0; head; ++n)
    head = LIST_NEXT (head);

  return n;
}

List *
list_reverse (head)
     List *head;
{
  List *result = NULL;
  List *next;

  while (head)
    {
      next		= LIST_NEXT (head);
      LIST_NEXT (head)	= result;
      result		= head;
      head 		= next;
    }

  return result;
}

int
list_foreach (head, foreach, userdata)
     List *head;
     ListCallback *foreach;
     VOID *userdata;
{
#if DEBUG
  if (!foreach)
    error (EXIT_FAILURE, 0,
	   _("%s (): NULL %s argument"), __PRETTY_FUNCTION__, "foreach");
#endif

  for (; head; head = LIST_NEXT (head))
    if ((*foreach) (head, userdata) < 0)
      return -1;

  return 0;
}

/* A generalised list data type.
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

#ifndef M4_LIST_H
#define M4_LIST_H 1

#include <sys/types.h>

#include <m4/system.h>

BEGIN_C_DECLS

/* A generalised list.  This is deliberately transparent so that you
   can make the NEXT field of all your chained data structures first,
   and then cast them to `(List *)' so that they can be manipulated
   by this API.

   Alternatively, you can generate raw List elements using list_new(),
   and put the element data in the USERDATA field.  Either way you
   get to manage the memory involved by yourself.
*/
typedef struct list {
  struct list *next;		/* chain forward pointer*/
  VOID *userdata;		/* incase you want to use raw `List's */
} List;

#define LIST_NEXT(node)	  ((node)->next)
#define LIST_DATA(node)	  ((node)->userdata)

typedef VOID *ListCompare M4_PARAMS((List *node, VOID *userdata));
typedef int ListCallback	  M4_PARAMS((List *node, VOID *userdata));

extern List *list_new	  M4_PARAMS((VOID *userdata));
extern List *list_delete  M4_PARAMS((List *head,
				     void (*delete) (VOID *data)));
extern VOID *list_remove  M4_PARAMS((List **phead, VOID *match,
				     ListCompare *find));
extern List *list_cons	  M4_PARAMS((List *head, List *tail));
extern List *list_tail	  M4_PARAMS((List *head));
extern List *list_nth	  M4_PARAMS((List *head, size_t n));
extern VOID *list_find	  M4_PARAMS((List *head, VOID *match,
				     ListCompare *find));
extern size_t list_length M4_PARAMS((List *head));
extern List *list_reverse M4_PARAMS((List *head));
extern int list_foreach   M4_PARAMS((List *head, ListCallback *foreach,
				     VOID *userdata));

END_C_DECLS

#endif /* !M4_LIST_H */

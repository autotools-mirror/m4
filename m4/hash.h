/* GNU m4 -- A simple macro processor
   Copyright 2001 Free Software Foundation, Inc.
   Written by Gary V. Vaughan <gary@gnu.org>

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

#ifndef M4_HASH_H
#define M4_HASH_H 1

#include "system.h"

/* Must be 1 less than a power of 2 for the resize algorithm to
   be efficient.  */
#define M4_HASH_DEFAULT_SIZE	511

/* When the average number of values per bucket breaks this value
   the table will be grown to reduce the density accordingly.  */
#define M4_HASH_MAXIMUM_DENSITY	3.0

BEGIN_C_DECLS

typedef struct m4_hash m4_hash;
typedef size_t m4_hash_hash_func (const void *key);
typedef int    m4_hash_cmp_func  (const void *key, const void *try);

m4_hash *	m4_hash_new	(size_t size, m4_hash_hash_func *hash_func,
				 m4_hash_cmp_func *cmp_func);
void		m4_hash_delete	(m4_hash *hash);
void		m4_hash_insert	(m4_hash *hash, const void *key, void *value);
void *		m4_hash_remove	(m4_hash *hash, const void *key);
void **		m4_hash_lookup	(m4_hash *hash, const void *key);
size_t		m4_hash_length	(m4_hash *hash);
void		m4_hash_resize	(m4_hash *hash, size_t size);
void		m4_hash_exit	(void);



size_t		m4_hash_string_hash (const void *key);
int		m4_hash_string_cmp  (const void *key, const void *try);



typedef struct m4_hash_iterator m4_hash_iterator;
typedef int m4_hash_apply_func  (m4_hash *hash, const void *key, void *value,
				 void *userdata);

m4_hash_iterator *	m4_hash_iterator_next	(const m4_hash *hash,
						 m4_hash_iterator *place);
const void *		m4_hash_iterator_key	(m4_hash_iterator *place);
void *			m4_hash_iterator_value	(m4_hash_iterator *place);

int			m4_hash_apply		(m4_hash *hash,
						 m4_hash_apply_func *func,
						 void *userdata);

END_C_DECLS

#endif /* !M4_HASH_H */

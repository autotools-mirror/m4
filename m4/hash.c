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

/* TODO:
   - Use an obstack to manage the node memory.
   - Implement the macroized magic values with the API.
 */

#include "hash.h"
#include "m4private.h"

typedef struct m4_hash_node m4_hash_node;

struct m4_hash
{
  size_t size;			/* number of buckets allocated */
  size_t length;		/* number of elements inserted */
  m4_hash_hash_func *hash_func;
  m4_hash_cmp_func *cmp_func;
  m4_hash_node **buckets;
};

struct m4_hash_node
{
  m4_hash_node *next;
  const void *key;
  void *value;
};



#define M4_HASH_SIZE(hash)	((hash)->size)
#define M4_HASH_LENGTH(hash)	((hash)->length)
#define M4_HASH_BUCKETS(hash)	((hash)->buckets)
#define M4_HASH_HASH_FUNC(hash)	((hash)->hash_func)
#define M4_HASH_CMP_FUNC(hash)	((hash)->cmp_func)

#define M4_HASH_NODE_NEXT(node)	((node)->next)
#define M4_HASH_NODE_KEY(node)	((node)->key)
#define M4_HASH_NODE_VAL(node)	((node)->value)

/* Helper macros. */
#define M4_HASH_BUCKET_NTH(hash, n)	(M4_HASH_BUCKETS (hash)[n])
#define M4_HASH_BUCKET_NUM(hash, key)	\
	((*M4_HASH_HASH_FUNC(hash))(key) % M4_HASH_SIZE(hash))
#define M4_HASH_BUCKET_KEY(hash, key)	\
	(M4_HASH_BUCKET_NTH ((hash), M4_HASH_BUCKET_NUM ((hash), (key))))



static void		m4_hash_bucket_delete	(m4_hash *hash, size_t i);
static void		m4_hash_node_delete	(m4_hash *hash,
						 m4_hash_node *node);
static m4_hash_node *	m4_hash_node_new	(const void *key, void *value);
static m4_hash_node *	m4_hash_lookup_node	(m4_hash *hash,
						 const void *key);
static void		m4_hash_maybe_grow	(m4_hash *hash);
static void		m4_hash_bucket_insert	(m4_hash *hash,
						 m4_hash_node *bucket);
static void		m4_hash_node_insert	(m4_hash *hash,
						 m4_hash_node *node);



static m4_hash_node *m4_hash_node_free_list = 0;



/* Allocate and return a new, unpopulated but initialised m4_hash,
   where HASH_FUNC will be used to generate bucket numbers and
   CMP_FUNC will be called to compare keys.  */
m4_hash *
m4_hash_new (m4_hash_hash_func *hash_func, m4_hash_cmp_func *cmp_func)
{
  m4_hash *hash;

  assert (hash_func);
  assert (cmp_func);

  hash			    = XMALLOC (m4_hash, 1);
  M4_HASH_SIZE (hash)	    = M4_HASH_DEFAULT_SIZE;
  M4_HASH_LENGTH (hash)	    = 0;
  M4_HASH_BUCKETS (hash)    = XCALLOC (m4_hash_node *, M4_HASH_DEFAULT_SIZE);
  M4_HASH_HASH_FUNC (hash)  = hash_func;
  M4_HASH_CMP_FUNC (hash)   = cmp_func;

  return hash;
}

/* Recycle each of the nodes in HASH onto the free list, and release
   the rest of the memory used by the table.  Memory addressed by
   the recycled nodes is _NOT_ freed:  this needs to be done manually
   to prevent memory leaks.  */
void
m4_hash_delete (m4_hash *hash)
{
  size_t i;

  assert (hash);

  for (i = 0; i < M4_HASH_SIZE (hash); ++i)
    if (M4_HASH_BUCKET_NTH (hash, i))
      m4_hash_bucket_delete (hash, i);
  XFREE (M4_HASH_BUCKETS (hash));
  XFREE (hash);
}

/* Check that the nodes in bucket I have been cleared, and recycle
   each of the nodes in the bucket to the free list.  Bucket I must
   not be empty when this function is called.  */
void
m4_hash_bucket_delete (m4_hash *hash, size_t i)
{
  m4_hash_node *node;

  assert (hash);
  assert (M4_HASH_BUCKET_NTH (hash, i));
  assert (i < M4_HASH_SIZE (hash));

  for (node = M4_HASH_BUCKET_NTH (hash, i);
       node->next;
       node = M4_HASH_NODE_NEXT (node))
    {
      assert (M4_HASH_NODE_KEY(node) == 0);
      --M4_HASH_LENGTH (hash);
    }

  assert (M4_HASH_NODE_KEY(node) == 0);
  --M4_HASH_LENGTH (hash);

  M4_HASH_NODE_NEXT (node)	= m4_hash_node_free_list;
  m4_hash_node_free_list 	= M4_HASH_BUCKET_NTH (hash, i);
  M4_HASH_BUCKET_NTH (hash, i)	= 0;
}

/* Create and initialise a new node with KEY and VALUE, by reusing a
   node from the free list if possible.  */
m4_hash_node *
m4_hash_node_new (const void *key, void *value)
{
  m4_hash_node *node = 0;

  if (m4_hash_node_free_list)
    {
      node = m4_hash_node_free_list;
      m4_hash_node_free_list = M4_HASH_NODE_NEXT (m4_hash_node_free_list);
    }
  else
    {
      node = XMALLOC (m4_hash_node, 1);
    }

  assert (node);

  M4_HASH_NODE_NEXT (node)= 0;
  M4_HASH_NODE_KEY (node) = key;
  M4_HASH_NODE_VAL (node) = value;

  return node;
}

/* Check that NODE has been cleared, and recycle it to the free list.  */
void
m4_hash_node_delete (m4_hash *hash, m4_hash_node *node)
{
  assert (node);
  assert (M4_HASH_NODE_KEY(node) == 0);

  M4_HASH_NODE_NEXT (node)	= m4_hash_node_free_list;
  m4_hash_node_free_list	= node;

  --M4_HASH_LENGTH (hash);
}

/* Create a new entry in HASH with KEY and VALUE, making use of
   nodes in the free list if possible, and potentially growing
   the size of the table if node density is too high.  */
void
m4_hash_insert (m4_hash *hash, const void *key, void *value)
{
  m4_hash_node *node;

  assert (hash);

  node = m4_hash_node_new (key, value);
  m4_hash_node_insert (hash, node);
  m4_hash_maybe_grow (hash);
}

/* Push the unconnected NODE on to the front of the appropriate
   bucket, effectively preventing retrieval of other nodes with
   the same key (where "sameness" is determined by HASH's
   cmp_func).  */
void
m4_hash_node_insert (m4_hash *hash, m4_hash_node *node)
{
  size_t n;

  assert (hash);
  assert (node);
  assert (M4_HASH_NODE_NEXT (node) == 0);

  n = M4_HASH_BUCKET_NUM (hash, M4_HASH_NODE_KEY (node));
  M4_HASH_NODE_NEXT (node)	= M4_HASH_BUCKET_NTH (hash, n);
  M4_HASH_BUCKET_NTH (hash, n)	= node;

  ++M4_HASH_LENGTH (hash);
}

/* Remove from HASH, the first node with key KEY; comparing keys
   with HASH's cmp_func.  Any nodes with the same KEY previously
   hidden by the removed node will become visible again.  The key
   field of the removed node is returned.  */
void *
m4_hash_remove (m4_hash *hash, const void *key)
{
  size_t n;

  assert (hash);

  n = M4_HASH_BUCKET_NUM (hash, key);

  {
    m4_hash_node *node = 0;

    do
      {
	m4_hash_node *next = node
	  ? M4_HASH_NODE_NEXT (node)
	  : M4_HASH_BUCKET_NTH (hash, n);

	if (next
	    && ((*M4_HASH_CMP_FUNC (hash))(M4_HASH_NODE_KEY (next), key) == 0))
	  {
	    if (node)
	      M4_HASH_NODE_NEXT (node) = M4_HASH_NODE_NEXT (next);
	    else
	      M4_HASH_BUCKET_NTH (hash, n)= M4_HASH_NODE_NEXT (next);

	    key = M4_HASH_NODE_KEY (next);
#ifndef NDEBUG
	    M4_HASH_NODE_KEY (next) = 0;
#endif
	    m4_hash_node_delete (hash, next);
	    break;
	  }
	node = next;
      }
    while (node);
  }

  return (void *) key;
}

/* Return the address of the value field of the first node in
   HASH that has a matching KEY.  The address is returned so that
   an explicit 0 value can be distinguished from a failed lookup
   (also 0).  Fortuitously for M4, this also means that the value
   field can be changed `in situ' to implement a value stack.  */
void **
m4_hash_lookup (m4_hash *hash, const void *key)
{
  m4_hash_node *node;

  assert (hash);

  node = m4_hash_lookup_node (hash, key);

  return node ? &M4_HASH_NODE_VAL (node) : 0;
}

/* Return the first node in HASH that has a matching KEY.  */
m4_hash_node *
m4_hash_lookup_node (m4_hash *hash, const void *key)
{
  m4_hash_node *node;

  assert (hash);

  node = M4_HASH_BUCKET_KEY (hash, key);

  while (node && (*M4_HASH_CMP_FUNC (hash)) (M4_HASH_NODE_KEY (node), key))
    node = M4_HASH_NODE_NEXT (node);

  return node;
}

/* How many entries are currently contained by HASH.  */
size_t
m4_hash_length (m4_hash *hash)
{
  assert (hash);

  return M4_HASH_LENGTH (hash);
}

/* If the node density breaks the threshold, increase the size of
   HASH and repopulate with the original nodes.  */
void
m4_hash_maybe_grow (m4_hash *hash)
{
  float nodes_per_bucket;

  assert (hash);

  nodes_per_bucket = (float) M4_HASH_LENGTH (hash)
    			/ (float) M4_HASH_SIZE (hash);

  if (nodes_per_bucket > M4_HASH_MAXIMUM_DENSITY)
    {
      size_t original_size = M4_HASH_SIZE (hash);
      m4_hash_node **original_buckets = M4_HASH_BUCKETS (hash);

      /* HASH sizes are always 1 less than a power of 2.  */
      M4_HASH_SIZE (hash)    = (2* (1+ original_size)) -1;
      M4_HASH_BUCKETS (hash) = XCALLOC (m4_hash_node *, hash->size);

      {
	size_t i;
	for (i = 0; i < original_size; ++i)
	  if (original_buckets[i])
	    m4_hash_bucket_insert (hash, original_buckets[i]);
      }

      XFREE (original_buckets);
    }
}

/* Insert each node in BUCKET into HASH.  Relative ordering of nodes
   is not preserved.  */
void
m4_hash_bucket_insert (m4_hash *hash, m4_hash_node *bucket)
{
  assert (hash);
  assert (bucket);

  do
    {
      m4_hash_node *next = M4_HASH_NODE_NEXT (bucket);

      /* Break link to rest of the bucket before reinserting.  */
      M4_HASH_NODE_NEXT (bucket) = 0;
      m4_hash_node_insert (hash, bucket);

      bucket = next;
    }
  while (bucket);
}

void
m4_hash_exit (void)
{
  while (m4_hash_node_free_list)
    {
      m4_hash_node *stale = m4_hash_node_free_list;
      m4_hash_node_free_list = M4_HASH_NODE_NEXT (stale);
      xfree (stale);
    }
}
 


struct m4_hash_iterator
{
  const m4_hash *hash;		/* contains the buckets */
  m4_hash_node *place;		/* the node we are about to return */
  m4_hash_node *next;		/* the next node, incase PLACE is removed */
  size_t	next_bucket;	/* the next bucket index following NEXT */
};

#define M4_ITERATOR_HASH(i)	    ((i)->hash)
#define M4_ITERATOR_PLACE(i)	    ((i)->place)
#define M4_ITERATOR_NEXT(i)	    ((i)->next)
#define M4_ITERATOR_NEXT_BUCKET(i)  ((i)->next_bucket)

#define M4_ITERATOR_NEXT_NEXT(i)   M4_HASH_NODE_NEXT (M4_ITERATOR_PLACE (i))

m4_hash_iterator *
m4_hash_iterator_next (const m4_hash *hash, m4_hash_iterator *place)
{
  assert (hash);
  assert (!place || (M4_ITERATOR_HASH (place) == hash));

  /* On the first iteration, allocate an iterator.  */
  if (!place)
    {
      place = XCALLOC (m4_hash_iterator, 1);
      M4_ITERATOR_HASH (place) = hash;
    }

 next:
  M4_ITERATOR_PLACE (place) = M4_ITERATOR_NEXT (place);

  /* If there is another node in the current bucket, select it.  */
  if (M4_ITERATOR_NEXT (place) && M4_HASH_NODE_NEXT (M4_ITERATOR_NEXT (place)))
    {
      M4_ITERATOR_NEXT (place) = M4_HASH_NODE_NEXT (M4_ITERATOR_NEXT (place));
    }
  else
    {
      /* Find the next non-empty bucket.  */
      while ((M4_ITERATOR_NEXT_BUCKET (place) < M4_HASH_SIZE (hash))
	 && (M4_HASH_BUCKET_NTH (hash, M4_ITERATOR_NEXT_BUCKET (place)) == 0))
	{
	  ++M4_ITERATOR_NEXT_BUCKET (place);
	}

      /* Select the first node in the new bucket.  */
      if (M4_ITERATOR_NEXT_BUCKET (place) < M4_HASH_SIZE (hash))
	{
	  M4_ITERATOR_NEXT (place)
	    = M4_HASH_BUCKET_NTH (hash, M4_ITERATOR_NEXT_BUCKET (place));
	}
      else
	M4_ITERATOR_NEXT (place) = 0;

      /* Advance the `next' reference.  */
      ++M4_ITERATOR_NEXT_BUCKET (place);
    }

  /* If there are no more nodes to return, recycle the iterator memory.  */
  if (! (M4_ITERATOR_PLACE (place) || M4_ITERATOR_NEXT (place)))
    {
      XFREE (place);
      return 0;
    }

  /* On the first call we need to put the 1st node in PLACE and
     the 2nd node in NEXT.  */
  if (M4_ITERATOR_NEXT (place) && !M4_ITERATOR_PLACE (place))
    goto next;

  assert (place && M4_ITERATOR_PLACE (place));

  return place;
}

const void *
m4_hash_iterator_key (m4_hash_iterator *place)
{
  assert (place);

  return M4_HASH_NODE_KEY (M4_ITERATOR_PLACE (place));
}

void *
m4_hash_iterator_value (m4_hash_iterator *place)
{
  assert (place);

  return M4_HASH_NODE_VAL (M4_ITERATOR_PLACE (place));
}

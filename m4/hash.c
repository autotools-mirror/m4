/* GNU m4 -- A simple macro processor
   Copyright (C) 2001, 2006-2010, 2013 Free Software Foundation, Inc.
   Written by Gary V. Vaughan <gary@gnu.org>

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

/* TODO:
   - Use an obstack to manage the node memory.
   - Implement the macroized magic values with the API.
 */

#include <config.h>

#include "hash.h"
#include "m4private.h"

#include "bitrotate.h"
#include <limits.h>

typedef struct hash_node hash_node;

struct m4_hash
{
  size_t size;                  /* number of buckets allocated */
  size_t length;                /* number of elements inserted */
  m4_hash_hash_func *hash_func;
  m4_hash_cmp_func *cmp_func;
  hash_node **buckets;
#ifndef NDEBUG
  m4_hash_iterator *iter;       /* current iterator */
#endif
};

struct hash_node
{
  hash_node *next;
  const void *key;
  void *value;
};


struct m4_hash_iterator
{
  const m4_hash *hash;          /* contains the buckets */
  hash_node *   place;          /* the node we are about to return */
  hash_node *   next;           /* the next node, incase PLACE is removed */
  size_t        next_bucket;    /* the next bucket index following NEXT */
#ifndef NDEBUG
  m4_hash_iterator *chain;      /* multiple iterators visiting one hash */
#endif
};


#define HASH_SIZE(hash)         ((hash)->size)
#define HASH_LENGTH(hash)       ((hash)->length)
#define HASH_BUCKETS(hash)      ((hash)->buckets)
#define HASH_HASH_FUNC(hash)    ((hash)->hash_func)
#define HASH_CMP_FUNC(hash)     ((hash)->cmp_func)

#define NODE_NEXT(node)         ((node)->next)
#define NODE_KEY(node)          ((node)->key)
#define NODE_VALUE(node)        ((node)->value)

#define ITERATOR_HASH(i)        ((i)->hash)
#define ITERATOR_PLACE(i)       ((i)->place)
#define ITERATOR_NEXT(i)        ((i)->next)
#define ITERATOR_NEXT_BUCKET(i) ((i)->next_bucket)

#define ITERATOR_NEXT_NEXT(i)   NODE_NEXT (ITERATOR_PLACE (i))

/* Helper macros. */
#define BUCKET_NTH(hash, n)     (HASH_BUCKETS (hash)[n])
#define BUCKET_COUNT(hash, key)                                 \
        ((*HASH_HASH_FUNC (hash))(key) % HASH_SIZE (hash))
#define BUCKET_KEY(hash, key)                                   \
        (BUCKET_NTH ((hash), BUCKET_COUNT ((hash), (key))))

/* Debugging macros.  */
#ifdef NDEBUG
# define HASH_ITER(hash)        0
# define ITER_CHAIN(iter)       0
#else
# define HASH_ITER(hash)        (((m4_hash *) hash)->iter)
# define ITER_CHAIN(iter)       ((iter)->chain)
#endif


static void             bucket_insert   (m4_hash *hash, hash_node *bucket);
static void             bucket_delete   (m4_hash *hash, size_t i);
static hash_node *      node_new        (const void *key, void *value);
static void             node_insert     (m4_hash *hash, hash_node *node);
static hash_node *      node_lookup     (m4_hash *hash, const void *key);
static void             node_delete     (m4_hash *hash, hash_node *node);
static void             maybe_grow      (m4_hash *hash);



static hash_node *free_list = NULL;



/* Allocate and return a new, unpopulated but initialised m4_hash with
   SIZE buckets, where HASH_FUNC will be used to generate bucket numbers
   and CMP_FUNC will be called to compare keys.  */
m4_hash *
m4_hash_new (size_t size, m4_hash_hash_func *hash_func,
             m4_hash_cmp_func *cmp_func)
{
  m4_hash *hash;

  assert (hash_func);
  assert (cmp_func);

  if (size == 0)
    size = M4_HASH_DEFAULT_SIZE;

  hash                  = (m4_hash *) xmalloc (sizeof *hash);
  HASH_SIZE (hash)      = size;
  HASH_LENGTH (hash)    = 0;
  HASH_BUCKETS (hash)   = (hash_node **) xcalloc (size,
                                                  sizeof *HASH_BUCKETS (hash));
  HASH_HASH_FUNC (hash) = hash_func;
  HASH_CMP_FUNC (hash)  = cmp_func;
#ifndef NDEBUG
  HASH_ITER (hash)      = NULL;
#endif

  return hash;
}

m4_hash *
m4_hash_dup (m4_hash *src, m4_hash_copy_func *copy)
{
  m4_hash *dest;

  assert (src);
  assert (copy);

  dest = m4_hash_new (HASH_SIZE (src), HASH_HASH_FUNC (src),
                      HASH_CMP_FUNC (src));

  m4_hash_apply (src, (m4_hash_apply_func *) copy, dest);

  return dest;
}

/* Recycle each of the nodes in HASH onto the free list, and release
   the rest of the memory used by the table.  Memory addressed by the
   recycled nodes is _NOT_ freed: this needs to be done manually to
   prevent memory leaks.  This is not safe to call while HASH is being
   iterated.  */
void
m4_hash_delete (m4_hash *hash)
{
  size_t i;

  assert (hash);
  assert (!HASH_ITER (hash));

  for (i = 0; i < HASH_SIZE (hash); ++i)
    if (BUCKET_NTH (hash, i))
      bucket_delete (hash, i);
  free (HASH_BUCKETS (hash));
  free (hash);
}

/* Check that the nodes in bucket I have been cleared, and recycle
   each of the nodes in the bucket to the free list.  Bucket I must
   not be empty when this function is called.  */
static void
bucket_delete (m4_hash *hash, size_t i)
{
  hash_node *node;

  assert (hash);
  assert (BUCKET_NTH (hash, i));
  assert (i < HASH_SIZE (hash));

  for (node = BUCKET_NTH (hash, i); node->next; node = NODE_NEXT (node))
    {
      assert (NODE_KEY (node) == NULL);
      --HASH_LENGTH (hash);
    }

  assert (NODE_KEY (node) == NULL);
  --HASH_LENGTH (hash);

  NODE_NEXT (node)      = free_list;
  free_list             = BUCKET_NTH (hash, i);
  BUCKET_NTH (hash, i)  = NULL;
}

/* Create and initialise a new node with KEY and VALUE, by reusing a
   node from the free list if possible.  */
static hash_node *
node_new (const void *key, void *value)
{
  hash_node *node = NULL;

  if (free_list)
    {
      node = free_list;
      free_list = NODE_NEXT (free_list);
    }
  else
    node = (hash_node *) xmalloc (sizeof *node);

  assert (node);

  NODE_NEXT  (node)     = NULL;
  NODE_KEY   (node)     = key;
  NODE_VALUE (node)     = value;

  return node;
}

/* Check that NODE has been cleared, and recycle it to the free list.  */
static void
node_delete (m4_hash *hash, hash_node *node)
{
  assert (node);
  assert (NODE_KEY (node) == NULL);

  NODE_NEXT (node)      = free_list;
  free_list             = node;

  --HASH_LENGTH (hash);
}

/* Create a new entry in HASH with KEY and VALUE, making use of nodes
   in the free list if possible, and potentially growing the size of
   the table if node density is too high.  This is not safe to call
   while HASH is being iterated.  Currently, it is not safe to call
   this if another entry already matches KEY.  */
const void *
m4_hash_insert (m4_hash *hash, const void *key, void *value)
{
  hash_node *node;

  assert (hash);
  assert (!HASH_ITER (hash));

  node = node_new (key, value);
  node_insert (hash, node);
  maybe_grow (hash);

  return key;
}

/* Push the unconnected NODE on to the front of the appropriate
   bucket, effectively preventing retrieval of other nodes with
   the same key (where "sameness" is determined by HASH's
   cmp_func).  */
static void
node_insert (m4_hash *hash, hash_node *node)
{
  size_t n;

  assert (hash);
  assert (node);
  assert (NODE_NEXT (node) == NULL);

  n = BUCKET_COUNT (hash, NODE_KEY (node));
  NODE_NEXT (node)      = BUCKET_NTH (hash, n);
  BUCKET_NTH (hash, n)  = node;

  ++HASH_LENGTH (hash);
}

/* Remove from HASH, the first node with key KEY; comparing keys with
   HASH's cmp_func.  Any nodes with the same KEY previously hidden by
   the removed node will become visible again.  The key field of the
   removed node is returned, or NULL if there was no match.  This is
   unsafe if multiple iterators are visiting HASH, or when a lone
   iterator is visiting on a different key.  */
void *
m4_hash_remove (m4_hash *hash, const void *key)
{
  size_t n;
  hash_node *node = NULL;

#ifndef NDEBUG
  m4_hash_iterator *iter = HASH_ITER (hash);

  assert (hash);
  if (HASH_ITER (hash))
    {
      assert (!ITER_CHAIN (iter));
      assert (ITERATOR_PLACE (iter));
    }
#endif

  n = BUCKET_COUNT (hash, key);
  do
    {
      hash_node *next = node ? NODE_NEXT (node) : BUCKET_NTH (hash, n);

      if (next && ((*HASH_CMP_FUNC (hash)) (NODE_KEY (next), key) == 0))
        {
          if (node)
            NODE_NEXT (node) = NODE_NEXT (next);
          else
            BUCKET_NTH (hash, n) = NODE_NEXT (next);

          key = NODE_KEY (next);
#ifndef NDEBUG
          if (iter)
            assert (ITERATOR_PLACE (iter) == next);
          NODE_KEY (next) = NULL;
#endif
          node_delete (hash, next);
          return (void *) key; /* Cast away const.  */
        }
      node = next;
    }
  while (node);

  return NULL;
}

/* Return the address of the value field of the first node in HASH
   that has a matching KEY.  The address is returned so that an
   explicit NULL value can be distinguished from a failed lookup (also
   NULL).  Fortuitously for M4, this also means that the value field
   can be changed `in situ' to implement a value stack.  Safe to call
   even when an iterator is in force.  */
void **
m4_hash_lookup (m4_hash *hash, const void *key)
{
  hash_node *node;

  assert (hash);

  node = node_lookup (hash, key);

  return node ? &NODE_VALUE (node) : NULL;
}

/* Return the first node in HASH that has a matching KEY.  */
static hash_node *
node_lookup (m4_hash *hash, const void *key)
{
  hash_node *node;

  assert (hash);

  node = BUCKET_KEY (hash, key);

  while (node && (*HASH_CMP_FUNC (hash)) (NODE_KEY (node), key))
    node = NODE_NEXT (node);

  return node;
}

/* How many entries are currently contained by HASH.  Safe to call
   even during an interation.  */
size_t
m4_get_hash_length (m4_hash *hash)
{
  assert (hash);

  return HASH_LENGTH (hash);
}

#if 0
/* Force the number of buckets to be the given value.  You probably ought
   not to be using this function once the table has been in use, since
   the maximum density algorithm will grow the number of buckets back to
   what was there before if you try to shrink the table.  It is useful
   to set a smaller or larger initial size if you know in advance what
   order of magnitude of entries will be in the table.  Be aware that
   the efficiency of the lookup and grow features require that the size
   always be 1 less than a power of 2.  Unsafe if HASH is being visited
   by an iterator.  */
void
m4_hash_resize (m4_hash *hash, size_t size)
{
  hash_node **original_buckets;
  size_t original_size;

  assert (hash);
  assert (!HASH_ITER (hash));

  original_size         = HASH_SIZE (hash);
  original_buckets      = HASH_BUCKETS (hash);

  HASH_SIZE (hash)      = size;
  HASH_BUCKETS (hash)   = (hash_node **) xcalloc (size,
                                                  sizeof *HASH_BUCKETS (hash));

  {
    size_t i;
    for (i = 0; i < original_size; ++i)
      if (original_buckets[i])
        bucket_insert (hash, original_buckets[i]);
  }

  free (original_buckets);
}
#endif

/* If the node density breaks the threshold, increase the size of
   HASH and repopulate with the original nodes.  */
static void
maybe_grow (m4_hash *hash)
{
  float nodes_per_bucket;

  assert (hash);

  nodes_per_bucket = (float) HASH_LENGTH (hash) / (float) HASH_SIZE (hash);

  if (nodes_per_bucket > M4_HASH_MAXIMUM_DENSITY)
    {
      size_t original_size = HASH_SIZE (hash);
      hash_node **original_buckets = HASH_BUCKETS (hash);

      /* HASH sizes are always 1 less than a power of 2.  */
      HASH_SIZE (hash)    = (2 * (1 + original_size)) -1;
      HASH_BUCKETS (hash) =
        (hash_node **) xcalloc (HASH_SIZE (hash), sizeof *HASH_BUCKETS (hash));

      {
        size_t i;
        for (i = 0; i < original_size; ++i)
          if (original_buckets[i])
            bucket_insert (hash, original_buckets[i]);
      }

      free (original_buckets);
    }
}

/* Insert each node in BUCKET into HASH.  Relative ordering of nodes
   is not preserved.  This would need to change if we were to
   guarantee relative ordering within a bucket for identical keys.  */
static void
bucket_insert (m4_hash *hash, hash_node *bucket)
{
  assert (hash);
  assert (bucket);

  do
    {
      hash_node *next = NODE_NEXT (bucket);

      /* Break link to rest of the bucket before reinserting.  */
      NODE_NEXT (bucket) = NULL;
      node_insert (hash, bucket);

      bucket = next;
    }
  while (bucket);
}

/* Reclaim all memory used by free nodes.  Safe to call at any time,
   although only worth calling at program shutdown to verify no
   leaks.  */
void
m4_hash_exit (void)
{
  while (free_list)
    {
      hash_node *stale = free_list;
      free_list = NODE_NEXT (stale);
      free (stale);
    }
}



/* Iterate over a given HASH.  Start with PLACE being NULL, then
   repeat with PLACE being the previous return value.  The return
   value is the current location of the iterator, or NULL when the
   walk is complete.  Call m4_free_hash_iterator to abort iteration.
   During the iteration, it is safe to search the list, and if no
   other iterator is active, it is safe to remove the key pointed to
   by this iterator.  All other actions that modify HASH are
   unsafe.  */
m4_hash_iterator *
m4_get_hash_iterator_next (const m4_hash *hash, m4_hash_iterator *place)
{
  assert (hash);
  assert (!place || (ITERATOR_HASH (place) == hash));

  /* On the first iteration, allocate an iterator.  */
  if (!place)
    {
      place = (m4_hash_iterator *) xzalloc (sizeof *place);
      ITERATOR_HASH (place) = hash;
#ifndef NDEBUG
      ITER_CHAIN (place) = HASH_ITER (hash);
      HASH_ITER (hash) = place;
#endif
    }

 next:
  ITERATOR_PLACE (place) = ITERATOR_NEXT (place);

  /* If there is another node in the current bucket, select it.  */
  if (ITERATOR_NEXT (place) && NODE_NEXT (ITERATOR_NEXT (place)))
    {
      ITERATOR_NEXT (place) = NODE_NEXT (ITERATOR_NEXT (place));
    }
  else
    {
      /* Find the next non-empty bucket.  */
      while ((ITERATOR_NEXT_BUCKET (place) < HASH_SIZE (hash))
         && (BUCKET_NTH (hash, ITERATOR_NEXT_BUCKET (place)) == NULL))
        {
          ++ITERATOR_NEXT_BUCKET (place);
        }

      /* Select the first node in the new bucket.  */
      if (ITERATOR_NEXT_BUCKET (place) < HASH_SIZE (hash))
        {
          ITERATOR_NEXT (place)
            = BUCKET_NTH (hash, ITERATOR_NEXT_BUCKET (place));
        }
      else
        ITERATOR_NEXT (place) = NULL;

      /* Advance the `next' reference.  */
      ++ITERATOR_NEXT_BUCKET (place);
    }

  /* If there are no more nodes to return, recycle the iterator memory.  */
  if (! (ITERATOR_PLACE (place) || ITERATOR_NEXT (place)))
    {
      m4_free_hash_iterator (hash, place);
      return NULL;
    }

  /* On the first call we need to put the 1st node in PLACE and
     the 2nd node in NEXT.  */
  if (ITERATOR_NEXT (place) && !ITERATOR_PLACE (place))
    goto next;

  assert (place && ITERATOR_PLACE (place));

  return place;
}

/* Clean up the iterator PLACE within HASH when aborting an iteration
   early.  */
void
m4_free_hash_iterator (const m4_hash *hash, m4_hash_iterator *place)
{
#ifndef NDEBUG
  m4_hash_iterator *iter = NULL;
  m4_hash_iterator *next;

  assert (hash);
  assert (place && (ITERATOR_HASH (place) == hash));

  do
    {
      next = iter ? ITER_CHAIN (iter) : HASH_ITER (hash);
      if (place == next)
        {
          if (iter)
            ITER_CHAIN (iter) = ITER_CHAIN (next);
          else
            HASH_ITER (hash) = ITER_CHAIN (next);
          break;
        }
      iter = next;
    }
  while (iter);
  assert (next);
#endif
  free (place);
}

/* Return the key being visited by the iterator PLACE.  */
const void *
m4_get_hash_iterator_key (m4_hash_iterator *place)
{
  assert (place);

  return NODE_KEY (ITERATOR_PLACE (place));
}

/* Return the value being visited by the iterator PLACE.  */
void *
m4_get_hash_iterator_value (m4_hash_iterator *place)
{
  assert (place);

  return NODE_VALUE (ITERATOR_PLACE (place));
}

/* The following function is used for the cases where we want to do
   something to each and every entry in HASH.  This function traverses
   the hash table, and calls a specified function FUNC for each entry
   in the table.  FUNC is called with a pointer to the entry key,
   value, and the passed DATA argument.  If FUNC returns non-NULL,
   abort the iteration and return that value; a return of NULL implies
   success on all entries.  */
void *
m4_hash_apply (m4_hash *hash, m4_hash_apply_func *func, void *userdata)
{
  m4_hash_iterator *place  = NULL;
  void *            result = NULL;

  assert (hash);
  assert (func);

  while ((place = m4_get_hash_iterator_next (hash, place)))
    {
      result = (*func) (hash, m4_get_hash_iterator_key (place),
                        m4_get_hash_iterator_value (place), userdata);

      if (result != NULL)
        {
          m4_free_hash_iterator (hash, place);
          break;
        }
    }

  return result;
}


/* Using a string (char * and size_t pair) as the hash key is common
   enough that we provide implementations here for use in client hash
   table routines.  */

/* Return a hash value for a string, similar to gnulib's hash module,
   but with length factored in.  */
size_t
m4_hash_string_hash (const void *ptr)
{
  const m4_string *key = (const m4_string *) ptr;
  const char *s = key->str;
  size_t len = key->len;
  size_t val = len;

  while (len--)
    val = rotl_sz (val, 7) + to_uchar (*s++);
  return val;
}

/* Comparison function for hash keys -- used by the underlying
   hash table ADT when searching for a key match during name lookup.  */
int
m4_hash_string_cmp (const void *key, const void *try)
{
  const m4_string *a = (const m4_string *) key;
  const m4_string *b = (const m4_string *) try;
  if (a->len < b->len)
    return -1;
  if (b->len < a->len)
    return 1;
  return memcmp (a->str, b->str, a->len);
}

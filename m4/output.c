/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1998, 2002, 2004, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301  USA
*/

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "m4private.h"

#include "binary-io.h"
#include "clean-temp.h"
#include "gl_avltree_list.h"
#include "exitfail.h"
#include "xvasprintf.h"

/* Define this to see runtime debug output.  Implied by DEBUG.  */
/*#define DEBUG_OUTPUT */

/* Size of initial in-memory buffer size for diversions.  Small diversions
   would usually fit in.  */
#define INITIAL_BUFFER_SIZE 512

/* Maximum value for the total of all in-memory buffer sizes for
   diversions.  */
#define MAXIMUM_TOTAL_SIZE (512 * 1024)

/* Size of buffer size to use while copying files.  */
#define COPY_BUFFER_SIZE (32 * 512)

/* Output functions.  Most of the complexity is for handling cpp like
   sync lines.

   This code is fairly entangled with the code in input.c, and maybe it
   belongs there?  */

typedef struct temp_dir m4_temp_dir;

/* When part of diversion_table, each struct diversion either
   represents an open file (zero size, non-NULL u.file), an in-memory
   buffer (non-zero size, non-NULL u.buffer), or an unused placeholder
   diversion (zero size, u is NULL).  When not part of
   diversion_table, u.next is a pointer to the free_list chain.  */

typedef struct m4_diversion m4_diversion;

struct m4_diversion
  {
    union
      {
	FILE *file;		/* diversion file on disk */
	char *buffer;		/* in-memory diversion buffer */
	m4_diversion *next;	/* free-list pointer */
      } u;
    int divnum;			/* which diversion this represents */
    size_t size;		/* usable size before reallocation */
    size_t used;		/* used length in characters */
  };

/* Sorted list of current diversions.  */
static gl_list_t diversion_table;

/* Number of entries in diversion table.  */
static int diversions;

/* Linked list of reclaimed diversion storage.  */
static m4_diversion *free_list;

/* Total size of all in-memory buffer sizes.  */
static size_t total_buffer_size;

/* Current output diversion, NULL if output is being currently discarded.  */
static m4_diversion *output_diversion;

/* Values of some output_diversion fields, cached out for speed.  */
static FILE *output_file;	/* current value of (file) */
static char *output_cursor;	/* current value of (buffer + used) */
static size_t output_unused;	/* current value of (size - used) */

/* Temporary directory holding all spilled diversion files.  */
static m4_temp_dir *output_temp_dir;



/* --- OUTPUT INITIALIZATION --- */

/* Callback for comparing list elements ELT1 and ELT2 for equality in
   diversion_table.  */
static bool
equal_diversion_CB (const void *elt1, const void *elt2)
{
  const m4_diversion *d1 = (const m4_diversion *) elt1;
  const m4_diversion *d2 = (const m4_diversion *) elt2;
  return d1->divnum == d2->divnum;
}

/* Callback for comparing list elements ELT1 and ELT2 for order in
   diversion_table.  */
static int
cmp_diversion_CB (const void *elt1, const void *elt2)
{
  const m4_diversion *d1 = (const m4_diversion *) elt1;
  const m4_diversion *d2 = (const m4_diversion *) elt2;
  /* No need to worry about overflow, since we don't create diversions
     with negative divnum.  */
  return d1->divnum - d2->divnum;
}

void
m4_output_init (m4 *context)
{
  m4_diversion *diversion = xmalloc (sizeof *diversion);
  const void *tmp = diversion;
  diversion->u.file = stdout;
  diversion->divnum = 0;
  diversion->size = 0;
  diversion->used = 0;
  diversion_table = gl_list_create (GL_AVLTREE_LIST, equal_diversion_CB, NULL,
				    false, 1, &tmp);

  diversions = 1;
  m4_set_current_diversion (context, 0);
  output_diversion = diversion;
  output_file = stdout;
}

void
m4_output_exit (void)
{
  m4_diversion *diversion = free_list;
  assert (gl_list_size (diversion_table) == 1);
  free ((void *) gl_list_get_at (diversion_table, 0));
  gl_list_free (diversion_table);
  while (diversion)
    {
      m4_diversion *stale = diversion;
      diversion = diversion->u.next;
      free (stale);
    }
}

/* Clean up any temporary directory.  Designed for use as an atexit
   handler, where it is not safe to call exit() recursively; so this
   calls _exit if a problem is encountered.  */
static void
cleanup_tmpfile (void)
{
  if (cleanup_temp_dir (output_temp_dir) != 0)
    _exit (exit_failure);
}

/* Create a temporary file open for reading and writing in a secure
   temp directory.  The file will be automatically closed and deleted
   on a fatal signal.  When done with the file, close it with
   close_stream_temp.  Exits on failure, so the return value is always
   an open file.  */
static FILE *
m4_tmpfile (m4 *context)
{
  static unsigned int count;
  char *name;
  FILE *file;

  if (output_temp_dir == NULL)
    {
      errno = 0;
      output_temp_dir = create_temp_dir ("m4-", NULL, true);
      if (output_temp_dir == NULL)
	m4_error (context, EXIT_FAILURE, errno,
		  _("cannot create temporary file for diversion"));
      atexit (cleanup_tmpfile);
    }
  name = xasprintf ("%s/m4-%d", output_temp_dir->dir_name, count++);
  register_temp_file (output_temp_dir, name);
  errno = 0;
  file = fopen_temp (name, O_BINARY ? "wb+" : "w+");
  if (file == NULL)
    m4_error (context, EXIT_FAILURE, errno,
	      _("cannot create temporary file for diversion"));
  free (name);
  return file;
}

/* Reorganize in-memory diversion buffers so the current diversion can
   accomodate LENGTH more characters without further reorganization.  The
   current diversion buffer is made bigger if possible.  But to make room
   for a bigger buffer, one of the in-memory diversion buffers might have
   to be flushed to a newly created temporary file.  This flushed buffer
   might well be the current one.  */
static void
make_room_for (m4 *context, size_t length)
{
  size_t wanted_size;
  m4_diversion *selected_diversion = NULL;

  assert (!output_file);
  assert (output_diversion);
  assert (output_diversion->size || !output_diversion->u.file);

  /* Compute needed size for in-memory buffer.  Diversions in-memory
     buffers start at 0 bytes, then 512, then keep doubling until it is
     decided to flush them to disk.  */

  output_diversion->used = output_diversion->size - output_unused;

  for (wanted_size = output_diversion->size;
       wanted_size <= MAXIMUM_TOTAL_SIZE
	 && wanted_size - output_diversion->used < length;
       wanted_size = wanted_size == 0 ? INITIAL_BUFFER_SIZE : wanted_size * 2)
    ;

  /* Check if we are exceeding the maximum amount of buffer memory.  */

  if (total_buffer_size - output_diversion->size + wanted_size
      > MAXIMUM_TOTAL_SIZE)
    {
      size_t selected_used;
      char *selected_buffer;
      m4_diversion *diversion;
      size_t count;
      gl_list_iterator_t iter;
      const void *elt;

      /* Find out the buffer having most data, in view of flushing it to
	 disk.  Fake the current buffer as having already received the
	 projected data, while making the selection.  So, if it is
	 selected indeed, we will flush it smaller, before it grows.  */

      selected_diversion = output_diversion;
      selected_used = output_diversion->used + length;

      iter = gl_list_iterator_from_to (diversion_table, 1,
				       gl_list_size (diversion_table));
      while (gl_list_iterator_next (&iter, &elt, NULL))
	{
	  diversion = (m4_diversion *) elt;
	  if (diversion->used > selected_used)
	    {
	      selected_diversion = diversion;
	      selected_used = diversion->used;
	    }
	}
      gl_list_iterator_free (&iter);

      /* Create a temporary file, write the in-memory buffer of the
	 diversion to this file, then release the buffer.  */

      selected_buffer = selected_diversion->u.buffer;
      selected_diversion->u.file = m4_tmpfile (context);
      if (set_cloexec_flag (fileno (selected_diversion->u.file), true) != 0)
	m4_error (context, 0, errno,
		  _("cannot protect diversion across forks"));

      if (selected_diversion->used > 0)
	{
	  count = fwrite (selected_buffer, selected_diversion->used, 1,
			  selected_diversion->u.file);
	  if (count != 1)
	    m4_error (context, EXIT_FAILURE, errno,
		      _("cannot flush diversion to temporary file"));
	}

      /* Reclaim the buffer space for other diversions.  */

      free (selected_buffer);
      total_buffer_size -= selected_diversion->size;

      selected_diversion->size = 0;
      selected_diversion->used = 0;
    }

  /* Reload output_file, just in case the flushed diversion was current.  */

  if (output_diversion == selected_diversion)
    {

      /* The flushed diversion was current indeed.  */

      output_file = output_diversion->u.file;
      output_cursor = NULL;
      output_unused = 0;
    }
  else
    {
      /* The buffer may be safely reallocated.  */

      assert (wanted_size > length);
      output_diversion->u.buffer = xrealloc (output_diversion->u.buffer,
					     wanted_size);

      total_buffer_size += wanted_size - output_diversion->size;
      output_diversion->size = wanted_size;

      output_cursor = output_diversion->u.buffer + output_diversion->used;
      output_unused = wanted_size - output_diversion->used;
    }
}

/* Output one character CHAR, when it is known that it goes to a
   diversion file or an in-memory diversion buffer.  A variable m4
   *context must be in scope.  */
#define OUTPUT_CHARACTER(Char)			 \
  if (output_file)				 \
    putc ((Char), output_file);			 \
  else if (output_unused == 0)			 \
    output_character_helper (context, (Char));	 \
  else						 \
    (output_unused--, *output_cursor++ = (Char))

static void
output_character_helper (m4 *context, int character)
{
  make_room_for (context, 1);

  if (output_file)
    putc (character, output_file);
  else
    {
      *output_cursor++ = character;
      output_unused--;
    }
}

/* Output one TEXT having LENGTH characters, when it is known that it goes
   to a diversion file or an in-memory diversion buffer.  */
static void
output_text (m4 *context, const char *text, size_t length)
{
  size_t count;

  if (!output_file && length > output_unused)
    make_room_for (context, length);

  if (output_file)
    {
      count = fwrite (text, length, 1, output_file);
      if (count != 1)
	m4_error (context, EXIT_FAILURE, errno, _("copying inserted file"));
    }
  else
    {
      memcpy (output_cursor, text, length);
      output_cursor += length;
      output_unused -= length;
    }
}

/* Add some text into an obstack OBS, taken from TEXT, having LENGTH
   characters.  If OBS is NULL, rather output the text to an external file
   or an in-memory diversion buffer.  If OBS is NULL, and there is no
   output file, the text is discarded.

   If we are generating sync lines, the output have to be examined, because
   we need to know how much output each input line generates.  In general,
   sync lines are output whenever a single input lines generates several
   output lines, or when several input lines does not generate any output.  */
void
m4_shipout_text (m4 *context, m4_obstack *obs,
		 const char *text, size_t length)
{
  static bool start_of_output_line = true;
  char line[20];
  const char *cursor;

  /* If output goes to an obstack, merely add TEXT to it.  */

  if (obs != NULL)
    {
      obstack_grow (obs, text, length);
      return;
    }

  /* Do nothing if TEXT should be discarded.  */

  if (output_diversion == NULL)
    return;

  /* Output TEXT to a file, or in-memory diversion buffer.  */

  if (!m4_get_sync_output_opt (context))
    switch (length)
      {

	/* In-line short texts.  */

      case 8: OUTPUT_CHARACTER (*text); text++;
      case 7: OUTPUT_CHARACTER (*text); text++;
      case 6: OUTPUT_CHARACTER (*text); text++;
      case 5: OUTPUT_CHARACTER (*text); text++;
      case 4: OUTPUT_CHARACTER (*text); text++;
      case 3: OUTPUT_CHARACTER (*text); text++;
      case 2: OUTPUT_CHARACTER (*text); text++;
      case 1: OUTPUT_CHARACTER (*text);
      case 0:
	return;

	/* Optimize longer texts.  */

      default:
	output_text (context, text, length);
      }
  else
    for (; length-- > 0; text++)
      {
	if (start_of_output_line)
	  {
	    start_of_output_line = false;
	    m4_set_output_line (context, m4_get_output_line (context) + 1);

#ifdef DEBUG_OUTPUT
	    fprintf (stderr, "DEBUG: cur %lu, cur out %lu\n",
		     (unsigned long int) m4_get_current_line (context),
		     (unsigned long int) m4_get_output_line (context));
#endif

	    /* Output a `#line NUM' synchronization directive if needed.
	       If output_line was previously given a negative
	       value (invalidated), then output `#line NUM "FILE"'.  */

	    if (m4_get_output_line (context) != m4_get_current_line (context))
	      {
		sprintf (line, "#line %lu",
			 (unsigned long int) m4_get_current_line (context));
		for (cursor = line; *cursor; cursor++)
		  OUTPUT_CHARACTER (*cursor);
		if (m4_get_output_line (context) < 1
		    && m4_get_current_file (context)[0] != '\0')
		  {
		    OUTPUT_CHARACTER (' ');
		    OUTPUT_CHARACTER ('"');
		    for (cursor = m4_get_current_file (context);
			 *cursor; cursor++)
		      {
			OUTPUT_CHARACTER (*cursor);
		      }
		    OUTPUT_CHARACTER ('"');
		  }
		OUTPUT_CHARACTER ('\n');
		m4_set_output_line (context, m4_get_current_line (context));
	      }
	  }
	OUTPUT_CHARACTER (*text);
	if (*text == '\n')
	  start_of_output_line = true;
      }
}

/* Format an int VAL, and stuff it into an obstack OBS.  Used for
   macros expanding to numbers.  FIXME - support wider types, and
   unsigned types.  */
void
m4_shipout_int (m4_obstack *obs, int val)
{
  char buf[128];

  sprintf(buf, "%d", val);
  obstack_grow (obs, buf, strlen (buf));
}

void
m4_shipout_string (m4 *context, m4_obstack *obs, const char *s, size_t len,
		   bool quoted)
{
  if (s == NULL)
    s = "";

  if (len == 0)
    len = strlen(s);

  if (quoted)
    obstack_grow (obs, context->syntax->lquote.string,
		  context->syntax->lquote.length);
  obstack_grow (obs, s, len);
  if (quoted)
    obstack_grow (obs, context->syntax->rquote.string,
		  context->syntax->rquote.length);
}



/* --- FUNCTIONS FOR USE BY DIVERSIONS --- */

/* Make a file for diversion DIVNUM, and install it in the diversion table.
   Grow the size of the diversion table as needed.  */

/* The number of possible diversions is limited only by memory and
   available file descriptors (each overflowing diversion uses one).  */

void
m4_make_diversion (m4 *context, int divnum)
{
  m4_diversion *diversion;
  gl_list_node_t node = NULL;

  if (m4_get_current_diversion (context) == divnum)
    return;

  if (output_diversion)
    {
      assert (!output_file || output_diversion->u.file == output_file);
      assert (output_diversion->divnum != divnum);
      if (!output_diversion->size && !output_diversion->u.file)
	{
	  if (!gl_list_remove (diversion_table, output_diversion))
	    assert (false);
	  output_diversion->u.next = free_list;
	  free_list = output_diversion;
	}
      else
	output_diversion->used = output_diversion->size - output_unused;
      output_diversion = NULL;
      output_file = NULL;
      output_cursor = NULL;
      output_unused = 0;
    }

  m4_set_current_diversion (context, divnum);

  if (divnum < 0)
    return;

  if (divnum >= diversions)
    diversions = divnum + 1;
  else
    {
      m4_diversion temp;
      temp.divnum = divnum;
      node = gl_sortedlist_search (diversion_table, cmp_diversion_CB, &temp);
    }
  if (node != NULL)
    diversion = (m4_diversion *) gl_list_node_value (diversion_table, node);
  else
    {
      /* First time visiting this diversion.  */
      if (free_list)
	{
	  diversion = free_list;
	  free_list = diversion->u.next;
	  assert (!diversion->size && !diversion->used);
	}
      else
	{
	  diversion = (m4_diversion *) xmalloc (sizeof *diversion);
	  diversion->size = 0;
	  diversion->used = 0;
	}
      diversion->u.file = NULL;
      diversion->divnum = divnum;
      gl_sortedlist_add (diversion_table, cmp_diversion_CB, diversion);
    }

  output_diversion = diversion;
  if (output_diversion->size)
    {
      output_cursor = output_diversion->u.buffer + output_diversion->used;
      output_unused = output_diversion->size - output_diversion->used;
    }
  else
    output_file = output_diversion->u.file;

  m4_set_output_line (context, -1);
}

/* Insert a FILE into the current output file, in the same manner
   diversions are handled.  This allows files to be included, without
   having them rescanned by m4.  */
void
m4_insert_file (m4 *context, FILE *file)
{
  char buffer[COPY_BUFFER_SIZE];
  size_t length;

  /* Optimize out inserting into a sink.  */

  if (!output_diversion)
    return;

  /* Insert output by big chunks.  */
  for (;;)
    {
      length = fread (buffer, 1, COPY_BUFFER_SIZE, file);
      if (ferror (file))
	m4_error (context, EXIT_FAILURE, errno, _("reading inserted file"));
      if (length == 0)
	break;
      output_text (context, buffer, length);
    }
}

/* Insert DIVERSION living at NODE into the current output file.  The
   diversion is NOT placed on the expansion obstack, because it must
   not be rescanned.  When the file is closed, it is deleted by the
   system.  */
static void
m4_insert_diversion_helper (m4 *context, m4_diversion *diversion,
			    gl_list_node_t node)
{
  assert (diversion->divnum > 0
	  && diversion->divnum != m4_get_current_diversion (context));
  /* Effectively undivert only if an output stream is active.  */
  if (output_diversion)
    {
      if (diversion->size)
	output_text (context, diversion->u.buffer, diversion->used);
      else if (diversion->u.file)
	{
	  rewind (diversion->u.file);
	  m4_insert_file (context, diversion->u.file);
	}

      m4_set_output_line (context, -1);
    }

  /* Return all space used by the diversion.  */
  if (diversion->size)
    {
      free (diversion->u.buffer);
      diversion->size = 0;
      diversion->used = 0;
    }
  else if (diversion->u.file)
    close_stream_temp (diversion->u.file);
  gl_list_remove_node (diversion_table, node);
  diversion->u.next = free_list;
  free_list = diversion;
}

/* Insert diversion number DIVNUM into the current output file.  The
   diversion is NOT placed on the expansion obstack, because it must not
   be rescanned.  When the file is closed, it is deleted by the system.  */
void
m4_insert_diversion (m4 *context, int divnum)
{
  m4_diversion *diversion;
  m4_diversion temp;
  gl_list_node_t node;

  /* Do not care about nonexistent diversions, and undiverting stdout
     or self is a no-op.  */
  if (divnum <= 0 || divnum >= diversions
      || m4_get_current_diversion (context) == divnum)
    return;
  temp.divnum = divnum;
  node = gl_sortedlist_search (diversion_table, cmp_diversion_CB, &temp);
  if (node == NULL)
    return;
  diversion = (m4_diversion *) gl_list_node_value (diversion_table, node);
  m4_insert_diversion_helper (context, diversion, node);
}

/* Get back all diversions.  This is done just before exiting from main (),
   and from m4_undivert (), if called without arguments.  */
void
m4_undivert_all (m4 *context)
{
  m4_diversion *diversion;
  gl_list_iterator_t iter;
  gl_list_node_t node;
  int divnum = m4_get_current_diversion (context);
  const void *elt;

  iter = gl_list_iterator_from_to (diversion_table, 1,
				   gl_list_size (diversion_table));
  while (gl_list_iterator_next (&iter, &elt, &node))
    {
      diversion = (m4_diversion *) elt;
      if (diversion->divnum != divnum)
	m4_insert_diversion_helper (context, diversion, node);
    }
  gl_list_iterator_free (&iter);
}

/* Produce all diversion information in frozen format on FILE.  */
void
m4_freeze_diversions (m4 *context, FILE *file)
{
  int saved_number;
  int last_inserted;
  m4_diversion *diversion;
  struct stat file_stat;
  gl_list_iterator_t iter;
  gl_list_node_t node;
  const void *elt;

  saved_number = m4_get_current_diversion (context);
  last_inserted = 0;
  m4_make_diversion (context, 0);
  output_file = file;		/* kludge in the frozen file */

  iter = gl_list_iterator_from_to (diversion_table, 1,
				   gl_list_size (diversion_table));
  while (gl_list_iterator_next (&iter, &elt, &node))
    {
      diversion = (m4_diversion *) elt;
      if (diversion->size || diversion->u.file)
	{
	  if (diversion->size)
	    {
	      assert (diversion->used == (int) diversion->used);
	      fprintf (file, "D%d,%d\n", diversion->divnum,
		       (int) diversion->used);
	    }
	  else
	    {
	      fflush (diversion->u.file);
	      if (fstat (fileno (diversion->u.file), &file_stat) < 0)
		m4_error (context, EXIT_FAILURE, errno,
			  _("cannot stat diversion"));
	      /* FIXME - support 64-bit off_t with 32-bit long, and
		 fix frozen file format to support 64-bit
		 integers.  */
	      if (file_stat.st_size < 0
		  || file_stat.st_size != (unsigned long) file_stat.st_size)
		m4_error (context, EXIT_FAILURE, errno,
			  _("diversion too large"));
	      fprintf (file, "D%d,%lu", diversion->divnum,
		       (unsigned long) file_stat.st_size);
	    }

	  m4_insert_diversion_helper (context, diversion, node);
	  putc ('\n', file);

	  last_inserted = diversion->divnum;
	}
    }
  gl_list_iterator_free (&iter);

  /* Save the active diversion number, if not already.  */

  if (saved_number != last_inserted)
    fprintf (file, "D%d,0\n\n", saved_number);
}

/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 1998, 2002, 2004, 2006,
   2007 Free Software Foundation, Inc.

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

#include "m4private.h"

#include "binary-io.h"
#include "clean-temp.h"
#include "gl_avltree_oset.h"
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

/* When part of diversion_table, each struct m4_diversion either
   represents an open file (zero size, non-NULL u.file), an in-memory
   buffer (non-zero size, non-NULL u.buffer), or an unused placeholder
   diversion (zero size, u is NULL, non-zero used indicates that a
   temporary file exists).  When not part of diversion_table, u.next
   is a pointer to the free_list chain.  */

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

/* Sorted set of diversions 1 through INT_MAX.  */
static gl_oset_t diversion_table;

/* Diversion 0 (not part of diversion_table).  */
static m4_diversion div0;

/* Linked list of reclaimed diversion storage.  */
static m4_diversion *free_list;

/* Obstack from which diversion storage is allocated.  */
static struct obstack diversion_storage;

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

/* Callback for comparing list element ELT against THRESHOLD.  */
static bool
threshold_diversion_CB (const void *elt, const void *threshold)
{
  const m4_diversion *div = (const m4_diversion *) elt;
  /* No need to worry about overflow, since we don't create diversions
     with negative divnum.  */
  return div->divnum >= *(const int *) threshold;
}

/* Initialize the output engine.  */
void
m4_output_init (m4 *context)
{
  diversion_table = gl_oset_create_empty (GL_AVLTREE_OSET, cmp_diversion_CB,
					  NULL);
  div0.u.file = stdout;
  m4_set_current_diversion (context, 0);
  output_diversion = &div0;
  output_file = stdout;
  obstack_init (&diversion_storage);
}

/* Clean up memory allocated during use.  */
void
m4_output_exit (void)
{
  /* Order is important, since we may have registered cleanup_tmpfile
     as an atexit handler, and it must not traverse stale memory.  */
  gl_oset_t table = diversion_table;
  assert (gl_oset_size (diversion_table) == 0);
  diversion_table = NULL;
  gl_oset_free (table);
  obstack_free (&diversion_storage, NULL);
}

/* Clean up any temporary directory.  Designed for use as an atexit
   handler, where it is not safe to call exit() recursively; so this
   calls _exit if a problem is encountered.  */
static void
cleanup_tmpfile (void)
{
  /* Close any open diversions.  */
  bool fail = false;

  if (diversion_table)
    {
      const void *elt;
      gl_oset_iterator_t iter = gl_oset_iterator (diversion_table);
      while (gl_oset_iterator_next (&iter, &elt))
	{
	  m4_diversion *diversion = (m4_diversion *) elt;
	  if (!diversion->size && diversion->u.file
	      && close_stream_temp (diversion->u.file) != 0)
	    {
	      error (0, errno,
		     _("cannot clean temporary file for diversion"));
	      fail = true;
	    }
	}
      gl_oset_iterator_free (&iter);
    }

  /* Clean up the temporary directory.  */
  if (cleanup_temp_dir (output_temp_dir) != 0)
    fail = true;
  if (fail)
    _exit (exit_failure);
}

/* Convert DIVNUM into a temporary file name for use in m4_tmp*.  */
static const char *
m4_tmpname (int divnum)
{
  static char *buffer;
  static char *tail;
  if (buffer == NULL)
    {
      tail = xasprintf ("%s/m4-%d", output_temp_dir->dir_name, INT_MAX);
      buffer = obstack_copy0 (&diversion_storage, tail, strlen (tail));
      free (tail);
      tail = strrchr (buffer, '-') + 1;
    }
  sprintf (tail, "%d", divnum);
  return buffer;
}

/* Create a temporary file for diversion DIVNUM open for reading and
   writing in a secure temp directory.  The file will be automatically
   closed and deleted on a fatal signal.  The file can be closed and
   reopened with m4_tmpclose and m4_tmpopen; when finally done with
   the file, close it and use m4_tmpremove.  Exits on failure, so the
   return value is always an open file.  */
static FILE *
m4_tmpfile (m4 *context, int divnum)
{
  const char *name;
  FILE *file;

  if (output_temp_dir == NULL)
    {
      output_temp_dir = create_temp_dir ("m4-", NULL, true);
      if (output_temp_dir == NULL)
	m4_error (context, EXIT_FAILURE, errno,
		  _("cannot create temporary file for diversion"));
      atexit (cleanup_tmpfile);
    }
  name = m4_tmpname (divnum);
  register_temp_file (output_temp_dir, name);
  file = fopen_temp (name, O_BINARY ? "wb+" : "w+");
  if (file == NULL)
    {
      unregister_temp_file (output_temp_dir, name);
      m4_error (context, EXIT_FAILURE, errno,
		_("cannot create temporary file for diversion"));
    }
  else if (set_cloexec_flag (fileno (file), true) != 0)
    m4_warn (context, errno, _("cannot protect diversion across forks"));
  return file;
}

/* Reopen a temporary file for diversion DIVNUM for reading and
   writing in a secure temp directory.  Exits on failure, so the
   return value is always an open file.  */
static FILE *
m4_tmpopen (m4 *context, int divnum)
{
  const char *name = m4_tmpname (divnum);
  FILE *file;

  file = fopen_temp (name, O_BINARY ? "ab+" : "a+");
  if (file == NULL)
    m4_error (context, EXIT_FAILURE, errno,
	      _("cannot create temporary file for diversion"));
  else if (set_cloexec_flag (fileno (file), true) != 0)
    m4_warn (context, errno, _("cannot protect diversion across forks"));
  return file;
}

/* Close, but don't delete, a temporary FILE.  */
static int
m4_tmpclose (FILE *file)
{
  return close_stream_temp (file);
}

/* Delete a closed temporary FILE for diversion DIVNUM.  */
static int
m4_tmpremove (int divnum)
{
  return cleanup_temp_file (output_temp_dir, m4_tmpname (divnum));
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
      gl_oset_iterator_t iter;
      const void *elt;

      /* Find out the buffer having most data, in view of flushing it to
	 disk.  Fake the current buffer as having already received the
	 projected data, while making the selection.  So, if it is
	 selected indeed, we will flush it smaller, before it grows.  */

      selected_diversion = output_diversion;
      selected_used = output_diversion->used + length;

      iter = gl_oset_iterator (diversion_table);
      while (gl_oset_iterator_next (&iter, &elt))
	{
	  diversion = (m4_diversion *) elt;
	  if (diversion->used > selected_used)
	    {
	      selected_diversion = diversion;
	      selected_used = diversion->used;
	    }
	}
      gl_oset_iterator_free (&iter);

      /* Create a temporary file, write the in-memory buffer of the
	 diversion to this file, then release the buffer.  Zero the
	 diversion before doing anything that can exit () (including
	 m4_tmpfile), so that the atexit handler doesn't try to close
	 a garbage pointer as a file.  */

      selected_buffer = selected_diversion->u.buffer;
      total_buffer_size -= selected_diversion->size;
      selected_diversion->size = 0;
      selected_diversion->u.file = NULL;
      selected_diversion->u.file = m4_tmpfile (context,
					       selected_diversion->divnum);

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
      selected_diversion->used = 1;
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
      /* Close any selected file since it is not the current diversion.  */
      if (selected_diversion)
	{
	  FILE *file = selected_diversion->u.file;
	  selected_diversion->u.file = NULL;
	  if (m4_tmpclose (file) != 0)
	    m4_error (context, 0, errno,
		      _("cannot close temporary file for diversion"));
	}

      /* The buffer may be safely reallocated.  */

      assert (wanted_size >= length);
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
void
m4_output_text (m4 *context, const char *text, size_t length)
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
   characters.  If OBS is NULL, output the text to an external file or
   an in-memory diversion buffer instead.  If OBS is NULL, and there
   is no output file, the text is discarded.  LINE is the line where
   the token starts (not necessarily m4_get_output_line, in the case
   of multiline tokens).

   If we are generating sync lines, the output has to be examined,
   because we need to know how much output each input line generates.
   In general, sync lines are output whenever a single input line
   generates several output lines, or when several input lines do not
   generate any output.  */
void
m4_shipout_text (m4 *context, m4_obstack *obs,
		 const char *text, size_t length, int line)
{
  static bool start_of_output_line = true;
  char linebuf[20];
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

  if (!m4_get_syncoutput_opt (context))
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
	m4_output_text (context, text, length);
      }
  else
    {
      /* Check for syncline only at the start of a token.  Multiline
	 tokens, and tokens that are out of sync but in the middle of
	 the line, must wait until the next raw newline triggers a
	 syncline.  */
      if (start_of_output_line)
	{
	  start_of_output_line = false;
	  m4_set_output_line (context, m4_get_output_line (context) + 1);

#ifdef DEBUG_OUTPUT
	  fprintf (stderr, "DEBUG: line %d, cur %lu, cur out %lu\n", line,
		   (unsigned long int) m4_get_current_line (context),
		   (unsigned long int) m4_get_output_line (context));
#endif

	  /* Output a `#line NUM' synchronization directive if needed.
	     If output_line was previously given a negative
	     value (invalidated), then output `#line NUM "FILE"'.  */

	  if (m4_get_output_line (context) != line)
	    {
	      sprintf (linebuf, "#line %lu",
		       (unsigned long int) m4_get_current_line (context));
	      for (cursor = linebuf; *cursor; cursor++)
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
	      m4_set_output_line (context, line);
	    }
	}

      /* Output the token, and track embedded newlines.  */
      for (; length-- > 0; text++)
	{
	  if (start_of_output_line)
	    {
	      start_of_output_line = false;
	      m4_set_output_line (context, m4_get_output_line (context) + 1);

#ifdef DEBUG_OUTPUT
	      fprintf (stderr, "DEBUG: line %d, cur %lu, cur out %lu\n", line,
		       (unsigned long int) m4_get_current_line (context),
		       (unsigned long int) m4_get_output_line (context));
#endif
	    }
	  OUTPUT_CHARACTER (*text);
	  if (*text == '\n')
	    start_of_output_line = true;
	}
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
  m4_diversion *diversion = NULL;

  if (m4_get_current_diversion (context) == divnum)
    return;

  if (output_diversion)
    {
      assert (!output_file || output_diversion->u.file == output_file);
      assert (output_diversion->divnum != divnum);
      if (!output_diversion->size && !output_diversion->u.file)
	{
	  if (!gl_oset_remove (diversion_table, output_diversion))
	    assert (false);
	  output_diversion->u.next = free_list;
	  output_diversion->used = 0;
	  free_list = output_diversion;
	}
      else if (output_diversion->size)
	output_diversion->used = output_diversion->size - output_unused;
      else if (output_diversion->used)
	{
	  assert (output_diversion->divnum != 0);
	  FILE *file = output_diversion->u.file;
	  output_diversion->u.file = NULL;
	  if (m4_tmpclose (file) != 0)
	    m4_error (context, 0, errno,
		      _("cannot close temporary file for diversion"));
	}
      output_diversion = NULL;
      output_file = NULL;
      output_cursor = NULL;
      output_unused = 0;
    }

  m4_set_current_diversion (context, divnum);

  if (divnum < 0)
    return;

  if (divnum == 0)
    diversion = &div0;
  else
    {
      const void *elt;
      if (gl_oset_search_atleast (diversion_table, threshold_diversion_CB,
				  &divnum, &elt))
	{
	  m4_diversion *temp = (m4_diversion *) elt;
	  if (temp->divnum == divnum)
	    diversion = temp;
	}
    }
  if (diversion == NULL)
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
	  diversion = (m4_diversion *) obstack_alloc (&diversion_storage,
						      sizeof *diversion);
	  diversion->size = 0;
	  diversion->used = 0;
	}
      diversion->u.file = NULL;
      diversion->divnum = divnum;
      if (!gl_oset_add (diversion_table, diversion))
	assert (false);
    }

  output_diversion = diversion;
  if (output_diversion->size)
    {
      output_cursor = output_diversion->u.buffer + output_diversion->used;
      output_unused = output_diversion->size - output_diversion->used;
    }
  else
    {
      if (!output_diversion->u.file && output_diversion->used)
	output_diversion->u.file = m4_tmpopen (context,
					       output_diversion->divnum);
      output_file = output_diversion->u.file;
    }

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
      m4_output_text (context, buffer, length);
    }
}

/* Insert DIVERSION living at NODE into the current output file.  The
   diversion is NOT placed on the expansion obstack, because it must
   not be rescanned.  When the file is closed, it is deleted by the
   system.  */
static void
insert_diversion_helper (m4 *context, m4_diversion *diversion)
{
  assert (diversion->divnum > 0
	  && diversion->divnum != m4_get_current_diversion (context));
  /* Effectively undivert only if an output stream is active.  */
  if (output_diversion)
    {
      if (diversion->size)
	m4_output_text (context, diversion->u.buffer, diversion->used);
      else
	{
	  assert (diversion->used);
	  if (!diversion->u.file)
	    diversion->u.file = m4_tmpopen (context, diversion->divnum);
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
  else
    {
      if (diversion->u.file)
	{
	  FILE *file = diversion->u.file;
	  diversion->u.file = NULL;
	  diversion->used = 0;
	  if (m4_tmpclose (file) != 0)
	    m4_error (context, 0, errno,
		      _("cannot clean temporary file for diversion"));
	}
      if (m4_tmpremove (diversion->divnum) != 0)
	m4_error (context, 0, errno,
		  _("cannot clean temporary file for diversion"));
    }
  if (!gl_oset_remove (diversion_table, diversion))
    assert (false);
  diversion->u.next = free_list;
  free_list = diversion;
}

/* Insert diversion number DIVNUM into the current output file.  The
   diversion is NOT placed on the expansion obstack, because it must not
   be rescanned.  When the file is closed, it is deleted by the system.  */
void
m4_insert_diversion (m4 *context, int divnum)
{
  const void *elt;

  /* Do not care about nonexistent diversions, and undiverting stdout
     or self is a no-op.  */
  if (divnum <= 0 || m4_get_current_diversion (context) == divnum)
    return;
  if (gl_oset_search_atleast (diversion_table, threshold_diversion_CB,
			      &divnum, &elt))
    {
      m4_diversion *diversion = (m4_diversion *) elt;
      if (diversion->divnum == divnum)
	insert_diversion_helper (context, diversion);
    }
}

/* Get back all diversions.  This is done just before exiting from main (),
   and from m4_undivert (), if called without arguments.  */
void
m4_undivert_all (m4 *context)
{
  int divnum = m4_get_current_diversion (context);
  const void *elt;
  gl_oset_iterator_t iter = gl_oset_iterator (diversion_table);
  while (gl_oset_iterator_next (&iter, &elt))
    {
      m4_diversion *diversion = (m4_diversion *) elt;
      if (diversion->divnum != divnum)
	insert_diversion_helper (context, diversion);
    }
  gl_oset_iterator_free (&iter);
}

/* Produce all diversion information in frozen format on FILE.  */
void
m4_freeze_diversions (m4 *context, FILE *file)
{
  int saved_number;
  int last_inserted;
  gl_oset_iterator_t iter;
  const void *elt;

  saved_number = m4_get_current_diversion (context);
  last_inserted = 0;
  m4_make_diversion (context, 0);
  output_file = file;		/* kludge in the frozen file */

  iter = gl_oset_iterator (diversion_table);
  while (gl_oset_iterator_next (&iter, &elt))
    {
      m4_diversion *diversion = (m4_diversion *) elt;
      if (diversion->size || diversion->used)
	{
	  if (diversion->size)
	    {
	      assert (diversion->used == (int) diversion->used);
	      fprintf (file, "D%d,%d\n", diversion->divnum,
		       (int) diversion->used);
	    }
	  else
	    {
	      struct stat file_stat;
	      assert (!diversion->u.file);
	      diversion->u.file = m4_tmpopen (context, diversion->divnum);
	      if (fstat (fileno (diversion->u.file), &file_stat) < 0)
		m4_error (context, EXIT_FAILURE, errno,
			  _("cannot stat diversion"));
	      /* FIXME - support 64-bit off_t with 32-bit long, and
		 fix frozen file format to support 64-bit integers.
		 This implies fixing shipout_text to take off_t.  */
	      if (file_stat.st_size < 0
		  || file_stat.st_size != (unsigned long int) file_stat.st_size)
		m4_error (context, EXIT_FAILURE, errno,
			  _("diversion too large"));
	      fprintf (file, "D%d,%lu\n", diversion->divnum,
		       (unsigned long int) file_stat.st_size);
	    }

	  insert_diversion_helper (context, diversion);
	  putc ('\n', file);

	  last_inserted = diversion->divnum;
	}
    }
  gl_oset_iterator_free (&iter);

  /* Save the active diversion number, if not already.  */

  if (saved_number != last_inserted)
    fprintf (file, "D%d,0\n\n", saved_number);
}

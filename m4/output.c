/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1998, 2002, 2004, 2006-2010, 2013 Free
   Software Foundation, Inc.

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

#include <config.h>

#include <sys/stat.h>

#include "m4private.h"

#include "binary-io.h"
#include "clean-temp.h"
#include "exitfail.h"
#include "gl_avltree_oset.h"
#include "gl_xoset.h"
#include "intprops.h"
#include "quotearg.h"
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
        FILE *file;             /* Diversion file on disk.  */
        char *buffer;           /* Malloc'd diversion buffer.  */
        m4_diversion *next;     /* Free-list pointer */
      } u;
    int divnum;                 /* Which diversion this represents.  */
    size_t size;                /* Usable size before reallocation.  */
    size_t used;                /* Used buffer length, or tmp file exists.  */
  };

/* Sorted set of diversions 1 through INT_MAX.  */
static gl_oset_t diversion_table;

/* Diversion 0 (not part of diversion_table).  */
static m4_diversion div0;

/* Linked list of reclaimed diversion storage.  */
static m4_diversion *free_list;

/* Obstack from which diversion storage is allocated.  */
static m4_obstack diversion_storage;

/* Total size of all in-memory buffer sizes.  */
static size_t total_buffer_size;

/* Current output diversion, NULL if output is being currently
   discarded.  output_diversion->u is guaranteed non-NULL except when
   the diversion has never been used; use size to determine if it is a
   malloc'd buffer or a FILE.  output_diversion->used is 0 if u.file
   is stdout, and non-zero if this is a malloc'd buffer or a temporary
   diversion file.  */
static m4_diversion *output_diversion;

/* Cache of output_diversion->u.file, only valid when
   output_diversion->size is 0.  */
static FILE *output_file;

/* Cache of output_diversion->u.buffer + output_diversion->used, only
   valid when output_diversion->size is non-zero.  */
static char *output_cursor;

/* Cache of output_diversion->size - output_diversion->used, only
   valid when output_diversion->size is non-zero.  */
static size_t output_unused;

/* Temporary directory holding all spilled diversion files.  */
static m4_temp_dir *output_temp_dir;

/* Cache of most recently used spilled diversion files.  */
static FILE *tmp_file1;
static FILE *tmp_file2;

/* Diversions that own tmp_file, or 0.  */
static int tmp_file1_owner;
static int tmp_file2_owner;

/* True if tmp_file2 is more recently used.  */
static bool tmp_file2_recent;


/* Internal routines.  */

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
  const m4_diversion *diversion = (const m4_diversion *) elt;
  /* No need to worry about overflow, since we don't create diversions
     with negative divnum.  */
  return diversion->divnum >= *(const int *) threshold;
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
  static size_t offset;
  if (buffer == NULL)
    {
      obstack_printf (&diversion_storage, "%s/m4-", output_temp_dir->dir_name);
      offset = obstack_object_size (&diversion_storage);
      buffer = (char *) obstack_alloc (&diversion_storage,
                                       INT_BUFSIZE_BOUND (divnum));
    }
  assert (0 < divnum);
  if (snprintf (&buffer[offset], INT_BUFSIZE_BOUND (divnum), "%d", divnum) < 0)
    abort ();
  return buffer;
}

/* Create a temporary file for diversion DIVNUM open for reading and
   writing in a secure temp directory.  The file will be automatically
   closed and deleted on a fatal signal.  The file can be closed and
   reopened with m4_tmpclose and m4_tmpopen, or moved with
   m4_tmprename; when finally done with the file, close it with
   m4_tmpremove.  Exits on failure, so the return value is always an
   open file.  */
static FILE *
m4_tmpfile (m4 *context, int divnum)
{
  const char *name;
  FILE *file;

  if (output_temp_dir == NULL)
    {
      output_temp_dir = create_temp_dir ("m4-", NULL, true);
      if (output_temp_dir == NULL)
        m4_error (context, EXIT_FAILURE, errno, NULL,
                  _("cannot create temporary file for diversion"));
      atexit (cleanup_tmpfile);
    }
  name = m4_tmpname (divnum);
  register_temp_file (output_temp_dir, name);
  file = fopen_temp (name, O_BINARY ? "wb+" : "w+");
  if (file == NULL)
    {
      unregister_temp_file (output_temp_dir, name);
      m4_error (context, EXIT_FAILURE, errno, NULL,
                _("cannot create temporary file for diversion"));
    }
  else if (set_cloexec_flag (fileno (file), true) != 0)
    m4_warn (context, errno, NULL, _("cannot protect diversion across forks"));
  return file;
}

/* Reopen a temporary file for diversion DIVNUM for reading and
   writing in a secure temp directory.  If REREAD, the file is
   positioned at offset 0, otherwise the file is positioned at the
   end.  Exits on failure, so the return value is always an open
   file.  */
static FILE *
m4_tmpopen (m4 *context, int divnum, bool reread)
{
  const char *name;
  FILE *file;

  if (tmp_file1_owner == divnum)
    {
      if (reread && fseeko (tmp_file1, 0, SEEK_SET) != 0)
        m4_error (context, EXIT_FAILURE, errno, NULL,
                  _("cannot seek within diversion"));
      tmp_file2_recent = false;
      return tmp_file1;
    }
  else if (tmp_file2_owner == divnum)
    {
      if (reread && fseeko (tmp_file2, 0, SEEK_SET) != 0)
        m4_error (context, EXIT_FAILURE, errno, NULL,
                  _("cannot seek to beginning of diversion"));
      tmp_file2_recent = true;
      return tmp_file2;
    }
  name = m4_tmpname (divnum);
  /* We need update mode, to avoid truncation.  */
  file = fopen_temp (name, O_BINARY ? "rb+" : "r+");
  if (file == NULL)
    m4_error (context, EXIT_FAILURE, errno, NULL,
              _("cannot create temporary file for diversion"));
  else if (set_cloexec_flag (fileno (file), true) != 0)
    m4_warn (context, errno, NULL, _("cannot protect diversion across forks"));
  /* Update mode starts at the beginning of the stream, but sometimes
     we want the end.  */
  else if (!reread && fseeko (file, 0, SEEK_END) != 0)
    m4_error (context, EXIT_FAILURE, errno, NULL,
              _("cannot seek within diversion"));
  return file;
}

/* Close, but don't delete, a temporary FILE for diversion DIVNUM.  To
   reduce the I/O overhead of repeatedly opening and closing the same
   file, this implementation caches the most recent spilled diversion.
   On the other hand, keeping every spilled diversion open would run
   into EMFILE limits.  */
static int
m4_tmpclose (FILE *file, int divnum)
{
  int result = 0;
  if (divnum != tmp_file1_owner && divnum != tmp_file2_owner)
    {
      if (tmp_file2_recent)
        {
          if (tmp_file1_owner)
            result = close_stream_temp (tmp_file1);
          tmp_file1 = file;
          tmp_file1_owner = divnum;
        }
      else
        {
          if (tmp_file2_owner)
            result = close_stream_temp (tmp_file2);
          tmp_file2 = file;
          tmp_file2_owner = divnum;
        }
    }
  return result;
}

/* Delete a closed temporary FILE for diversion DIVNUM.  */
static int
m4_tmpremove (int divnum)
{
  if (divnum == tmp_file1_owner)
    {
      int result = close_stream_temp (tmp_file1);
      if (result)
        return result;
      tmp_file1_owner = 0;
    }
  else if (divnum == tmp_file2_owner)
    {
      int result = close_stream_temp (tmp_file2);
      if (result)
        return result;
      tmp_file2_owner = 0;
    }
  return cleanup_temp_file (output_temp_dir, m4_tmpname (divnum));
}

/* Transfer the temporary file for diversion OLDNUM to the previously
   unused diversion NEWNUM.  Return an open stream visiting the new
   temporary file, positioned at the end, or exit on failure.  */
static FILE*
m4_tmprename (m4 *context, int oldnum, int newnum)
{
  /* m4_tmpname reuses its return buffer.  */
  char *oldname = xstrdup (m4_tmpname (oldnum));
  const char *newname = m4_tmpname (newnum);
  register_temp_file (output_temp_dir, newname);
  if (oldnum == tmp_file1_owner)
    {
      /* Be careful of mingw, which can't rename an open file.  */
      if (RENAME_OPEN_FILE_WORKS)
        tmp_file1_owner = newnum;
      else
        {
          if (close_stream_temp (tmp_file1))
            m4_error (context, EXIT_FAILURE, errno, NULL,
                      _("cannot close temporary file for diversion"));
          tmp_file1_owner = 0;
        }
    }
  else if (oldnum == tmp_file2_owner)
    {
      /* Be careful of mingw, which can't rename an open file.  */
      if (RENAME_OPEN_FILE_WORKS)
        tmp_file2_owner = newnum;
      else
        {
          if (close_stream_temp (tmp_file2))
            m4_error (context, EXIT_FAILURE, errno, NULL,
                      _("cannot close temporary file for diversion"));
          tmp_file2_owner = 0;
        }
    }
  /* Either it is safe to rename an open file, or no one should have
     oldname open at this point.  */
  if (rename (oldname, newname))
    m4_error (context, EXIT_FAILURE, errno, NULL,
              _("cannot create temporary file for diversion"));
  unregister_temp_file (output_temp_dir, oldname);
  free (oldname);
  return m4_tmpopen (context, newnum, false);
}


/* --- OUTPUT INITIALIZATION --- */

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
  if (tmp_file1_owner)
    m4_tmpremove (tmp_file1_owner);
  if (tmp_file2_owner)
    m4_tmpremove (tmp_file2_owner);
  diversion_table = NULL;
  gl_oset_free (table);
  obstack_free (&diversion_storage, NULL);
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
            m4_error (context, EXIT_FAILURE, errno, NULL,
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
          if (m4_tmpclose (file, selected_diversion->divnum) != 0)
            m4_error (context, 0, errno, NULL,
                      _("cannot close temporary file for diversion"));
        }

      /* The current buffer may be safely reallocated.  */
      assert (wanted_size >= length);
      {
        char *buffer = output_diversion->u.buffer;
        output_diversion->u.buffer = xcharalloc ((size_t) wanted_size);
        memcpy (output_diversion->u.buffer, buffer, output_diversion->used);
        free (buffer);
      }

      total_buffer_size += wanted_size - output_diversion->size;
      output_diversion->size = wanted_size;

      output_cursor = output_diversion->u.buffer + output_diversion->used;
      output_unused = wanted_size - output_diversion->used;
    }
}

/* Output one character CHAR, when it is known that it goes to a
   diversion file or an in-memory diversion buffer.  A variable m4
   *context must be in scope.  */
#define OUTPUT_CHARACTER(Char)                   \
  if (output_file)                               \
    putc ((Char), output_file);                  \
  else if (output_unused == 0)                   \
    output_character_helper (context, (Char));   \
  else                                           \
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

  if (!output_diversion || !length)
    return;

  if (!output_file && length > output_unused)
    make_room_for (context, length);

  if (output_file)
    {
      count = fwrite (text, length, 1, output_file);
      if (count != 1)
        m4_error (context, EXIT_FAILURE, errno, NULL,
                  _("copying inserted file"));
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
m4_divert_text (m4 *context, m4_obstack *obs, const char *text, size_t length,
                int line)
{
  static bool start_of_output_line = true;

  /* If output goes to an obstack, merely add TEXT to it.  */

  if (obs != NULL)
    {
      obstack_grow (obs, text, length);
      return;
    }

  /* Do nothing if TEXT should be discarded.  */

  if (!output_diversion || !length)
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
          xfprintf (stderr, "DEBUG: line %d, cur %lu, cur out %lu\n", line,
                    (unsigned long int) m4_get_current_line (context),
                    (unsigned long int) m4_get_output_line (context));
#endif

          /* Output a `#line NUM' synchronization directive if needed.
             If output_line was previously given a negative
             value (invalidated), then output `#line NUM "FILE"'.  */

          if (m4_get_output_line (context) != line)
            {
              char linebuf[sizeof "#line " + INT_BUFSIZE_BOUND (line)];
              sprintf (linebuf, "#line %lu",
                       (unsigned long int) m4_get_current_line (context));
              m4_output_text (context, linebuf, strlen (linebuf));
              if (m4_get_output_line (context) < 1
                  && m4_get_current_file (context)[0] != '\0')
                {
                  const char *file = m4_get_current_file (context);
                  OUTPUT_CHARACTER (' ');
                  OUTPUT_CHARACTER ('"');
                  m4_output_text (context, file, strlen (file));
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
              xfprintf (stderr, "DEBUG: line %d, cur %lu, cur out %lu\n", line,
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
  /* Using obstack_printf (obs, "%d", val) has too much overhead.  */
  unsigned int uval;
  char buf[INT_BUFSIZE_BOUND (unsigned int)];
  char *p = buf + INT_STRLEN_BOUND (unsigned int);

  if (val < 0)
    {
      obstack_1grow (obs, '-');
      uval = -(unsigned int) val;
    }
  else
    uval = val;
  *p = '\0';
  do
    *--p = '0' + uval % 10;
  while (uval /= 10);
  obstack_grow (obs, p, strlen (p));
}

/* Output the text S, of length LEN, to OBS.  If QUOTED, also output
   current quote characters around S.  If LEN is SIZE_MAX, use the
   string length of S instead.  */
void
m4_shipout_string (m4 *context, m4_obstack *obs, const char *s, size_t len,
                   bool quoted)
{
  m4_shipout_string_trunc (obs, s, len,
                           quoted ? m4_get_syntax_quotes (M4SYNTAX) : NULL,
                           NULL);
}

/* Output the text S, of length LEN, to OBS.  If QUOTES, also output
   quote characters around S.  If LEN is SIZE_MAX, use the string
   length of S instead.  If MAX_LEN, reduce *MAX_LEN by LEN.  If LEN
   is larger than *MAX_LEN, then truncate output and return true;
   otherwise return false.  Quotes do not count against MAX_LEN.  */
bool
m4_shipout_string_trunc (m4_obstack *obs, const char *s, size_t len,
                         const m4_string_pair *quotes, size_t *max_len)
{
  size_t max = max_len ? *max_len : SIZE_MAX;

  assert (obs && s);
  if (len == SIZE_MAX)
    len = strlen (s);
  if (quotes)
    obstack_grow (obs, quotes->str1, quotes->len1);
  if (len < max)
    {
      obstack_grow (obs, s, len);
      max -= len;
    }
  else
    {
      obstack_grow (obs, s, max);
      obstack_grow (obs, "...", 3);
      max = 0;
    }
  if (quotes)
    obstack_grow (obs, quotes->str2, quotes->len2);
  if (max_len)
    *max_len = max;
  return max == 0;
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
          assert (!output_diversion->used);
          if (!gl_oset_remove (diversion_table, output_diversion))
            assert (false);
          output_diversion->u.next = free_list;
          free_list = output_diversion;
        }
      else if (output_diversion->size)
        output_diversion->used = output_diversion->size - output_unused;
      else if (output_diversion->used)
        {
          assert (output_diversion->divnum != 0);
          FILE *file = output_diversion->u.file;
          output_diversion->u.file = NULL;
          if (m4_tmpclose (file, output_diversion->divnum) != 0)
            m4_error (context, 0, errno, NULL,
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
                                               output_diversion->divnum,
                                               false);
      output_file = output_diversion->u.file;
    }

  m4_set_output_line (context, -1);
}

/* Insert a FILE into the current output file, in the same manner
   diversions are handled.  If ESCAPED, ensure the output is all
   ASCII.  */
static void
insert_file (m4 *context, FILE *file, bool escaped)
{
  static char buffer[COPY_BUFFER_SIZE];
  size_t length;
  char *str = buffer;
  bool first = true;

  assert (output_diversion);
  /* Insert output by big chunks.  */
  while (1)
    {
      length = fread (buffer, 1, sizeof buffer, file);
      if (ferror (file))
        m4_error (context, EXIT_FAILURE, errno, NULL,
                  _("reading inserted file"));
      if (length == 0)
        break;
      if (escaped)
        {
          if (first)
            first = false;
          else
            m4_output_text (context, "\\\n", 2);
          str = quotearg_style_mem (escape_quoting_style, buffer, length);
        }
      m4_output_text (context, str, escaped ? strlen (str) : length);
    }
}

/* Insert a FILE into the current output file, in the same manner
   diversions are handled.  This allows files to be included, without
   having them rescanned by m4.  */
void
m4_insert_file (m4 *context, FILE *file)
{
  /* Optimize out inserting into a sink.  */
  if (output_diversion)
    insert_file (context, file, false);
}

/* Insert DIVERSION living at NODE into the current output file.  The
   diversion is NOT placed on the expansion obstack, because it must
   not be rescanned.  If ESCAPED, ensure the output is ASCII.  When
   the file is closed, it is deleted by the system.  */
static void
insert_diversion_helper (m4 *context, m4_diversion *diversion, bool escaped)
{
  assert (diversion->divnum > 0
          && diversion->divnum != m4_get_current_diversion (context));
  /* Effectively undivert only if an output stream is active.  */
  if (output_diversion)
    {
      if (diversion->size)
        {
          if (!output_diversion->u.file)
            {
              /* Transferring diversion metadata is faster than
                 copying contents.  */
              assert (!output_diversion->used && output_diversion != &div0
                      && !output_file);
              output_diversion->u.buffer = diversion->u.buffer;
              output_diversion->size = diversion->size;
              output_cursor = diversion->u.buffer + diversion->used;
              output_unused = diversion->size - diversion->used;
              diversion->u.buffer = NULL;
            }
          else
            {
              char *str = diversion->u.buffer;
              size_t len = diversion->used;
              /* Avoid double-charging the total in-memory size when
                 transferring from one in-memory diversion to
                 another.  */
              total_buffer_size -= diversion->size;
              if (escaped)
                str = quotearg_style_mem (escape_quoting_style, str, len);
              m4_output_text (context, str, escaped ? strlen (str) : len);
            }
        }
      else if (!output_diversion->u.file)
        {
          /* Transferring diversion metadata is faster than copying
             contents.  */
          assert (!output_diversion->used && output_diversion != &div0
                  && !output_file);
          output_diversion->u.file = m4_tmprename (context, diversion->divnum,
                                                   output_diversion->divnum);
          output_diversion->used = 1;
          output_file = output_diversion->u.file;
          diversion->u.file = NULL;
          diversion->size = 1;
        }
      else
        {
          assert (diversion->used);
          if (!diversion->u.file)
            diversion->u.file = m4_tmpopen (context, diversion->divnum, true);
          insert_file (context, diversion->u.file, escaped);
        }

      m4_set_output_line (context, -1);
    }

  /* Return all space used by the diversion.  */
  if (diversion->size)
    {
      if (!output_diversion)
        total_buffer_size -= diversion->size;
      free (diversion->u.buffer);
      diversion->size = 0;
    }
  else
    {
      if (diversion->u.file)
        {
          FILE *file = diversion->u.file;
          diversion->u.file = NULL;
          if (m4_tmpclose (file, diversion->divnum) != 0)
            m4_error (context, 0, errno, NULL,
                      _("cannot clean temporary file for diversion"));
        }
      if (m4_tmpremove (diversion->divnum) != 0)
        m4_error (context, 0, errno, NULL,
                  _("cannot clean temporary file for diversion"));
    }
  diversion->used = 0;
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
        insert_diversion_helper (context, diversion, false);
    }
}

/* Get back all diversions.  This is done just before exiting from main,
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
        insert_diversion_helper (context, diversion, false);
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
  output_file = file; /* kludge in the frozen file */

  iter = gl_oset_iterator (diversion_table);
  while (gl_oset_iterator_next (&iter, &elt))
    {
      m4_diversion *diversion = (m4_diversion *) elt;
      if (diversion->size || diversion->used)
        {
          if (diversion->size)
            {
              assert (diversion->used == (int) diversion->used);
              xfprintf (file, "D%d,%d\n", diversion->divnum,
                        (int) diversion->used);
            }
          else
            {
              struct stat file_stat;
              assert (!diversion->u.file);
              diversion->u.file = m4_tmpopen (context, diversion->divnum,
                                              true);
              if (fstat (fileno (diversion->u.file), &file_stat) < 0)
                m4_error (context, EXIT_FAILURE, errno, NULL,
                          _("cannot stat diversion"));
              /* FIXME - support 64-bit off_t with 32-bit long, and
                 fix frozen file format to support 64-bit integers.
                 This implies fixing m4_divert_text to take off_t.  */
              if (file_stat.st_size < 0
                  || file_stat.st_size != (unsigned long int) file_stat.st_size)
                m4_error (context, EXIT_FAILURE, errno, NULL,
                          _("diversion too large"));
              xfprintf (file, "%c%d,%lu\n", 'D', diversion->divnum,
                        (unsigned long int) file_stat.st_size);
            }

          insert_diversion_helper (context, diversion, true);
          putc ('\n', file);

          last_inserted = diversion->divnum;
        }
    }
  gl_oset_iterator_free (&iter);

  /* Save the active diversion number, if not already.  */

  if (saved_number != last_inserted)
    xfprintf (file, "D%d,0\n\n", saved_number);
}

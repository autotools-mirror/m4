/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2001, 2006, 2007
   Free Software Foundation, Inc.

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

/* printf like formatting for m4.  */

#include "xvasprintf.h"

/* Simple varargs substitute.  We assume int and unsigned int are the
   same size; likewise for long and unsigned long.

   TODO - warn if we use these because too many % specifiers were used in
   relation to number of arguments passed.
   TODO - use xstrtoimax, not atoi, to catch overflow, non-numeric
   arguments, etc.  */

#define ARG_INT(i, argc, argv)			\
  ((argc <= i++) ? 0 : atoi (M4ARG (i - 1)))

#define ARG_LONG(i, argc, argv)			\
  ((argc <= i++) ? 0L : atol (M4ARG (i - 1)))

#define ARG_STR(i, argc, argv)			\
  ((argc <= i++) ? "" : M4ARG (i - 1))

#define ARG_DOUBLE(i, argc, argv)		\
  ((argc <= i++) ? 0.0 : atof (M4ARG (i - 1)))


/* The main formatting function.  Output is placed on the obstack OBS,
   the first argument in ARGV is the formatting string, and the rest
   is arguments for the string.  Warn rather than invoke unspecified
   behavior in the underlying printf when we do not recognize a
   format.  */

static void
format (m4 *context, m4_obstack *obs, int argc, m4_macro_args *argv)
{
  const char *name = M4ARG (0);		/* Macro name.  */
  const char *f;			/* Format control string.  */
  const char *fmt;			/* Position within f.  */
  char fstart[] = "%'+- 0#*.*hhd";	/* Current format spec.  */
  char *p;				/* Position within fstart.  */
  unsigned char c;			/* A simple character.  */
  int index = 1;			/* Index within argc used so far.  */

  /* Flags.  */
  char flags;				/* flags to use in fstart */
  enum {
    THOUSANDS	= 0x01, /* ' */
    PLUS	= 0x02, /* + */
    MINUS	= 0x04, /* - */
    SPACE	= 0x08, /*   */
    ZERO	= 0x10, /* 0 */
    ALT		= 0x20, /* # */
    DONE	= 0x40  /* no more flags */
  };

  /* Precision specifiers.  */
  int width;			/* minimum field width */
  int prec;			/* precision */
  char lflag;			/* long flag */

  /* Specifiers we are willing to accept.  ok['x'] implies %x is ok.
     Various modifiers reduce the set, in order to avoid undefined
     behavior in printf.  */
  char ok[128];

  /* Buffer and stuff.  */
  char *str;			/* malloc'd buffer of formatted text */
  enum {CHAR, INT, LONG, DOUBLE, STR} datatype;

  f = fmt = ARG_STR (index, argc, argv);
  memset (ok, 0, sizeof ok);
  for (;;)
    {
      while ((c = *fmt++) != '%')
	{
	  if (c == '\0')
	    return;
	  obstack_1grow (obs, c);
	}

      if (*fmt == '%')
	{
	  obstack_1grow (obs, '%');
	  fmt++;
	  continue;
	}

      p = fstart + 1; /* % */
      lflag = 0;
      ok['a'] = ok['A'] = ok['c'] = ok['d'] = ok['e'] = ok['E']
	= ok['f'] = ok['F'] = ok['g'] = ok['G'] = ok['i'] = ok['o']
	= ok['s'] = ok['u'] = ok['x'] = ok['X'] = 1;

      /* Parse flags.  */
      flags = 0;
      do
	{
	  switch (*fmt)
	    {
	    case '\'':		/* thousands separator */
	      ok['a'] = ok['A'] = ok['c'] = ok['e'] = ok['E']
		= ok['o'] = ok['s'] = ok['x'] = ok['X'] = 0;
	      flags |= THOUSANDS;
	      break;

	    case '+':		/* mandatory sign */
	      ok['c'] = ok['o'] = ok['s'] = ok['u'] = ok['x'] = ok['X'] = 0;
	      flags |= PLUS;
	      break;

	    case ' ':		/* space instead of positive sign */
	      ok['c'] = ok['o'] = ok['s'] = ok['u'] = ok['x'] = ok['X'] = 0;
	      flags |= SPACE;
	      break;

	    case '0':		/* zero padding */
	      ok['c'] = ok['s'] = 0;
	      flags |= ZERO;
	      break;

	    case '#':		/* alternate output */
	      ok['c'] = ok['d'] = ok['i'] = ok['s'] = ok['u'] = 0;
	      flags |= ALT;
	      break;

	    case '-':		/* left justification */
	      flags |= MINUS;
	      break;

	    default:
	      flags |= DONE;
	      break;
	    }
	}
      while (!(flags & DONE) && fmt++);
      if (flags & THOUSANDS)
	*p++ = '\'';
      if (flags & PLUS)
	*p++ = '+';
      if (flags & MINUS)
	*p++ = '-';
      if (flags & SPACE)
	*p++ = ' ';
      if (flags & ZERO)
	*p++ = '0';
      if (flags & ALT)
	*p++ = '#';

      /* Minimum field width; an explicit 0 is the same as not giving
	 the width.  */
      width = 0;
      *p++ = '*';
      if (*fmt == '*')
	{
	  width = ARG_INT (index, argc, argv);
	  fmt++;
	}
      else
	while (isdigit ((unsigned char) *fmt))
	  {
	    width = 10 * width + *fmt - '0';
	    fmt++;
	  }

      /* Maximum precision; an explicit negative precision is the same
	 as not giving the precision.  A lone '.' is a precision of 0.  */
      prec = -1;
      *p++ = '.';
      *p++ = '*';
      if (*fmt == '.')
	{
	  ok['c'] = 0;
	  if (*(++fmt) == '*')
	    {
	      prec = ARG_INT (index, argc, argv);
	      ++fmt;
	    }
	  else
	    {
	      prec = 0;
	      while (isdigit ((unsigned char) *fmt))
		{
		  prec = 10 * prec + *fmt - '0';
		  fmt++;
		}
	    }
	}

      /* Length modifiers.  We don't yet recognize ll, j, t, or z.  */
      if (*fmt == 'l')
	{
	  *p++ = 'l';
	  lflag = 1;
	  fmt++;
	  ok['c'] = ok['s'] = 0;
	}
      else if (*fmt == 'h')
	{
	  *p++ = 'h';
	  fmt++;
	  if (*fmt == 'h')
	    {
	      *p++ = 'h';
	      fmt++;
	    }
	  ok['a'] = ok['A'] = ok['c'] = ok['e'] = ok['E'] = ok['f'] = ok['F']
	    = ok['g'] = ok['G'] = ok['s'] = 0;
	}

      c = *fmt++;
      if (c > sizeof ok || !ok[c])
	{
	  m4_warn (context, 0, name, _("unrecognized specifier in `%s'"), f);
	  if (c == '\0')
	    fmt--;
	  continue;
	}

      /* Specifiers.  We don't yet recognize C, S, n, or p.  */
      switch (c)
	{
	case 'c':
	  datatype = CHAR;
	  p -= 2; /* %.*c is undefined, so undo the '.*'.  */
	  break;

	case 's':
	  datatype = STR;
	  break;

	case 'd':
	case 'i':
	case 'o':
	case 'x':
	case 'X':
	case 'u':
	  datatype = lflag ? LONG : INT;
	  break;

	case 'a':
	case 'A':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	  datatype = DOUBLE;
	  break;

	default:
	  abort ();
	}
      *p++ = c;
      *p = '\0';

      switch (datatype)
	{
	case CHAR:
	  str = xasprintf (fstart, width, ARG_INT (index, argc, argv));
	  break;

	case INT:
	  str = xasprintf (fstart, width, prec, ARG_INT (index, argc, argv));
	  break;

	case LONG:
	  str = xasprintf (fstart, width, prec, ARG_LONG (index, argc, argv));
	  break;

	case DOUBLE:
	  str = xasprintf (fstart, width, prec,
			   ARG_DOUBLE (index, argc, argv));
	  break;

	case STR:
	  str = xasprintf (fstart, width, prec, ARG_STR (index, argc, argv));
	  break;

	default:
	  abort ();
	}

      /* NULL was returned on failure, such as invalid format string.  For
	 now, just silently ignore that bad specifier.  */
      if (str == NULL)
	continue;

      obstack_grow (obs, str, strlen (str));
      free (str);
    }
}

/* GNU m4 -- A simple macro processor
   Copyright 1989, 1990, 1991, 1992, 1993, 1994, 2001
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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307  USA
*/

/* printf like formatting for m4.  */

/* Simple varargs substitute.  */

#define ARG_INT(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, atoi (M4_TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_UINT(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, (unsigned int) atoi (M4_TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_LONG(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, atol (M4_TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_ULONG(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, (unsigned long) atol (M4_TOKEN_DATA_TEXT (argv[-1]))))

#define ARG_STR(argc, argv) \
	((argc == 0) ? "" : \
	 (--argc, argv++, M4_TOKEN_DATA_TEXT (argv[-1])))

#define ARG_DOUBLE(argc, argv) \
	((argc == 0) ? 0 : \
	 (--argc, argv++, atof (M4_TOKEN_DATA_TEXT (argv[-1]))))


/* The main formatting function.  Output is placed on the obstack OBS, the
   first argument in ARGV is the formatting string, and the rest is
   arguments for the string.  */
void format (struct obstack *obs, int argc, m4_token_data **argv);

void
format (struct obstack *obs, int argc, m4_token_data **argv)
{
  char *fmt;			/* format control string */
  const char *fstart;		/* beginning of current format spec */
  int c;			/* a simple character */

  /* Flags.  */
  char flags;			/* 1 iff treating flags */

  /* Precision specifiers.  */
  int width;			/* minimum field width */
  int prec;			/* precision */
  char lflag;			/* long flag */
  char hflag;			/* short flag */

  /* Buffer and stuff.  */
  char str[4096];		/* buffer for formatted text */
  enum {INT, UINT, LONG, ULONG, DOUBLE, STR} datatype;

  fmt = ARG_STR (argc, argv);
  for (;;)
    {
      while ((c = *fmt++) != '%')
	{
	  if (c == 0)
	    return;
	  obstack_1grow (obs, c);
	}

      fstart = fmt - 1;

      if (*fmt == '%')
	{
	  obstack_1grow (obs, '%');
	  fmt++;
	  continue;
	}

      /* Parse flags.  */
      flags = 1;
      do
	{
	  switch (*fmt)
	    {
	    case '-':		/* left justification */
	    case '+':		/* mandatory sign */
	    case ' ':		/* space instead of positive sign */
	    case '0':		/* zero padding */
	    case '#':		/* alternate output */
	      break;

	    default:
	      flags = 0;
	      break;
	    }
	}
      while (flags && fmt++);

      /* Minimum field width.  */
      width = -1;
      if (*fmt == '*')
	{
	  width = ARG_INT (argc, argv);
	  fmt++;
	}
      else if (isdigit ((int) *fmt))
	{
	  do
	    {
	      fmt++;
	    }
	  while (isdigit ((int) *fmt));
	}

      /* Maximum precision.  */
      prec = -1;
      if (*fmt == '.')
	{
	  if (*(++fmt) == '*')
	    {
	      prec = ARG_INT (argc, argv);
	      ++fmt;
	    }
	  else if (isdigit ((int) *fmt))
	    {
	      do
		{
		  fmt++;
		}
	      while (isdigit ((int) *fmt));
	    }
	}

      /* Length modifiers.  */
      lflag = (*fmt == 'l');
      hflag = (*fmt == 'h');
      if (lflag || hflag)
	fmt++;

      switch (*fmt++)
	{

	case '\0':
	  return;

	case 'c':
	  datatype = INT;
	  break;

	case 's':
	  datatype = STR;
	  break;

	case 'd':
	case 'i':
	  if (lflag)
	    {
	      datatype = LONG;
	    }
	  else
	    {
	      datatype = INT;
	    }
	  break;

	case 'o':
	case 'x':
	case 'X':
	case 'u':
	  if (lflag)
	    {
	      datatype = ULONG;
	    }
	  else
	    {
	      datatype = UINT;
	    }
	  break;

	case 'e':
	case 'E':
	case 'f':
	  datatype = DOUBLE;
	  break;

	default:
	  continue;
	}

      c = *fmt;
      *fmt = '\0';

      switch(datatype)
	{
	case INT:
	  if (width != -1 && prec != -1)
	    sprintf (str, fstart, width, prec, ARG_INT(argc, argv));
	  else if (width != -1)
	    sprintf (str, fstart, width, ARG_INT(argc, argv));
	  else if (prec != -1)
	    sprintf (str, fstart, prec, ARG_INT(argc, argv));
	  else
	    sprintf (str, fstart, ARG_INT(argc, argv));
	  break;

	case UINT:
	  if (width != -1 && prec != -1)
	    sprintf (str, fstart, width, prec, ARG_UINT(argc, argv));
	  else if (width != -1)
	    sprintf (str, fstart, width, ARG_UINT(argc, argv));
	  else if (prec != -1)
	    sprintf (str, fstart, prec, ARG_UINT(argc, argv));
	  else
	    sprintf (str, fstart, ARG_UINT(argc, argv));
	  break;

	case LONG:
	  if (width != -1 && prec != -1)
	    sprintf (str, fstart, width, prec, ARG_LONG(argc, argv));
	  else if (width != -1)
	    sprintf (str, fstart, width, ARG_LONG(argc, argv));
	  else if (prec != -1)
	    sprintf (str, fstart, prec, ARG_LONG(argc, argv));
	  else
	    sprintf (str, fstart, ARG_LONG(argc, argv));
	  break;

	case ULONG:
	  if (width != -1 && prec != -1)
	    sprintf (str, fstart, width, prec, ARG_ULONG(argc, argv));
	  else if (width != -1)
	    sprintf (str, fstart, width, ARG_ULONG(argc, argv));
	  else if (prec != -1)
	    sprintf (str, fstart, prec, ARG_ULONG(argc, argv));
	  else
	    sprintf (str, fstart, ARG_ULONG(argc, argv));
	  break;

	case DOUBLE:
	  if (width != -1 && prec != -1)
	    sprintf (str, fstart, width, prec, ARG_DOUBLE(argc, argv));
	  else if (width != -1)
	    sprintf (str, fstart, width, ARG_DOUBLE(argc, argv));
	  else if (prec != -1)
	    sprintf (str, fstart, prec, ARG_DOUBLE(argc, argv));
	  else
	    sprintf (str, fstart, ARG_DOUBLE(argc, argv));
	  break;

	case STR:
	  if (width != -1 && prec != -1)
	    sprintf (str, fstart, width, prec, ARG_STR(argc, argv));
	  else if (width != -1)
	    sprintf (str, fstart, width, ARG_STR(argc, argv));
	  else if (prec != -1)
	    sprintf (str, fstart, prec, ARG_STR(argc, argv));
	  else
	    sprintf (str, fstart, ARG_STR(argc, argv));
	  break;
	}

      *fmt = c;

      obstack_grow (obs, str, strlen (str));
    }
}

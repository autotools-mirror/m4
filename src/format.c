/* GNU m4 -- A simple macro processor

   Copyright (C) 1989-1994, 2006-2014, 2016-2017, 2020-2026 Free
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/* printf like formatting for m4.  */

#include "m4.h"

#include "minmax.h"

#include <stdckdint.h>

/* Simple varargs substitute.  We assume int and unsigned int are the
   same size; likewise for long and unsigned long.  */

/* Parse STR as an integer, reporting warnings.  */
static int
arg_int (const char *str)
{
  char *endp;
  long value;

  if (!*str)
    {
      M4ERROR ((warning_status, 0, _("empty string treated as 0")));
      return 0;
    }
  errno = 0;
  value = strtol (str, &endp, 10);
  bool overflow = errno == ERANGE;
  if (*endp)
    M4ERROR ((warning_status, 0, _("non-numeric argument %s"), squote (str)));
  else if (c_isspace (*str))
    M4ERROR ((warning_status, 0, _("leading whitespace ignored")));
  int result;
  overflow |= ckd_add (&result, value, 0);
  if (overflow)
    {
      M4ERROR ((warning_status, 0, _("numeric overflow detected")));
      /* Return the closest extremum: this is the most useful when debugging
         width and precision overflow, and is good enough for %c.  */
      return value < 0 ? INT_MIN : INT_MAX;
    }
  return result;
}

/* Parse STR as an ival, reporting warnings.  Return the ival and an
   overflow indicator that is null if no overflow occurred, the end of
   the parsed number otherwise.  */
static struct arg_long { ival val; char const *overflow; }
arg_long (const char *str)
{
  char *endp;

  if (!*str)
    {
      M4ERROR ((warning_status, 0, _("empty string treated as 0")));
      return (struct arg_long) {0};
    }
  errno = 0;
  iival value = strtoiival (str, &endp, 10);
  bool overflow = errno == ERANGE;
  if (*endp)
    M4ERROR ((warning_status, 0, _("non-numeric argument %s"), squote (str)));
  else if (c_isspace (*str))
    M4ERROR ((warning_status, 0, _("leading whitespace ignored")));
  ival result;
  overflow |= ckd_add (&result, value, 0);
  if (overflow)
    M4ERROR ((warning_status, 0, _("numeric overflow detected")));
  return (struct arg_long) {result, overflow ? endp : NULL};
}

/* Parse STR as a double, reporting warnings.  */
static double
arg_double (const char *str)
{
  char *endp;
  double value;

  if (!*str)
    {
      M4ERROR ((warning_status, 0, _("empty string treated as 0")));
      return 0.0;
    }
  errno = 0;
  value = strtod (str, &endp);
  bool overflow = errno == ERANGE;
  if (*endp)
    M4ERROR ((warning_status, 0, _("non-numeric argument %s"), squote (str)));
  else if (c_isspace (*str))
    M4ERROR ((warning_status, 0, _("leading whitespace ignored")));
  if (overflow)
    M4ERROR ((warning_status, 0, _("numeric overflow detected")));
  return value;
}

#define ARG_INT(argc, argv) \
  (((argc) == 0) ? 0 : \
   ((argc)--, arg_int (TOKEN_DATA_TEXT (*(argv)++))))

#define ARG_STR(argc, argv) \
  (((argc) == 0) ? "" : \
   ((argc)--, TOKEN_DATA_TEXT (*(argv)++)))

#define ARG_DOUBLE(argc, argv) \
  (((argc) == 0) ? 0 : \
   ((argc)--, arg_double (TOKEN_DATA_TEXT (*(argv)++))))

/* Parse a width or precision from FMT's prefix.
   If FMT[-1] == '.' this is a precision instead of a width.
   If FMT[0] == '*', get it from the next arg specified by *ARGC and *ARGV;
   otherwise, parse an optional unsigned decimal prefix of FMT.
   If the integer is too large, warn about overflow.
   Set *N to the value, or to zero if the prefix is empty.
   Return the address of the first byte after the prefix.  */
static char const *
parse_width (char const *fmt, idx_t *argc, token_data ***argv, int *n)
{
  if (*fmt == '*')
    {
      *n = ARG_INT (*argc, *argv);
      return fmt + 1;
    }
  else
    {
      char const *f = fmt;
      *n = 0;
      bool v = false;
      for (; c_isdigit (*f); f++)
        {
          v |= ckd_mul (n, *n, 10);
          v |= ckd_add (n, *n, *f - '0');
        }
      if (v)
        {
          int w = ckd_add (&w, f - fmt, 0) ? INT_MAX : w;
          M4ERROR ((warning_status, 0,
                    _(fmt[-1] == '.'
                      ? "integer overflow in format precision %.*s"
                      : "integer overflow in format width %.*s"),
                    w, fmt));
          *n = INT_MAX;
        }
      return f;
    }
}


/*------------------------------------------------------------------.
| The main formatting function.  Output is placed on the obstack    |
| OBS, the first argument in ARGV is the formatting string, and the |
| rest is arguments for the string.  Warn rather than invoke        |
| unspecified behavior in the underlying printf when we do not      |
| recognize a format.                                               |
`------------------------------------------------------------------*/

void
expand_format (struct obstack *obs, idx_t argc, token_data **argv)
{
  const char *f;                /* format control string */
  const char *fmt;              /* position within f */
  char fstart[sizeof "%'+- 0#*.*"PRIdIVAL"d"]; /* current format spec */
  char *p;                      /* position within fstart */
  unsigned char c;              /* a simple character */

  /* Flags.  */
  char flags;                   /* flags to use in fstart */
  enum
  {
    THOUSANDS = 0x01,           /* ' */
    PLUS = 0x02,                /* + */
    MINUS = 0x04,               /* - */
    SPACE = 0x08,               /*   */
    ZERO = 0x10,                /* 0 */
    ALT = 0x20,                 /* # */
    DONE = 0x40                 /* no more flags */
  };

  /* Precision specifiers.  */
  int width;                    /* minimum field width */
  int prec;                     /* precision */

  /* Specifiers we are willing to accept.  ok['x' - OKMIN] implies %x is ok.
     Various modifiers reduce the set, in order to avoid undefined
     behavior in printf.  */
  enum { OKMIN = MIN ('a', 'A'), OKMAX = MAX ('x', 'X') };
  char ok[OKMAX - OKMIN + 1] = {0};

  /* Buffer and stuff.  */
  char *str;                    /* malloc'd buffer of formatted text */

  fstart[0] = '%';
  f = fmt = ARG_STR (argc, argv);
  while (1)
    {
      const char *percent = strchr (fmt, '%');
      if (!percent)
        {
          obstack_grow (obs, fmt, strlen (fmt));
          return;
        }
      obstack_grow (obs, fmt, percent - fmt);
      fmt = percent + 1;

      if (*fmt == '%')
        {
          obstack_1grow (obs, '%');
          fmt++;
          continue;
        }

      p = fstart + 1;           /* % */
      ok['a' - OKMIN] = ok['A' - OKMIN] = ok['c' - OKMIN] = ok['d' - OKMIN]
        = ok['e' - OKMIN] = ok['E' - OKMIN] = ok['f' - OKMIN] = ok['F' - OKMIN]
        = ok['g' - OKMIN] = ok['G' - OKMIN] = ok['i' - OKMIN] = ok['o' - OKMIN]
        = ok['s' - OKMIN] = ok['u' - OKMIN] = ok['x' - OKMIN] = ok['X' - OKMIN]
        = 1;

      /* Parse flags.  */
      flags = 0;
      do
        {
          switch (*fmt)
            {
            case '\'':         /* thousands separator */
              ok['a' - OKMIN] = ok['A' - OKMIN] = ok['c' - OKMIN]
                = ok['e' - OKMIN] = ok['E' - OKMIN] = ok['o' - OKMIN]
                = ok['s' - OKMIN] = ok['x' - OKMIN] = ok['X' - OKMIN]
                = 0;
              flags |= THOUSANDS;
              break;

            case '+':          /* mandatory sign */
              ok['c' - OKMIN] = ok['o' - OKMIN] = ok['s' - OKMIN]
                = ok['u' - OKMIN] = ok['x' - OKMIN] = ok['X' - OKMIN]
                = 0;
              flags |= PLUS;
              break;

            case ' ':          /* space instead of positive sign */
              ok['c' - OKMIN] = ok['o' - OKMIN] = ok['s' - OKMIN]
                = ok['u' - OKMIN] = ok['x' - OKMIN] = ok['X' - OKMIN]
                = 0;
              flags |= SPACE;
              break;

            case '0':          /* zero padding */
              ok['c' - OKMIN] = ok['s' - OKMIN] = 0;
              flags |= ZERO;
              break;

            case '#':          /* alternate output */
              ok['c' - OKMIN] = ok['d' - OKMIN] = ok['i' - OKMIN]
                = ok['s' - OKMIN] = ok['u' - OKMIN]
                = 0;
              flags |= ALT;
              break;

            case '-':          /* left justification */
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
      *p++ = '*';
      fmt = parse_width (fmt, &argc, &argv, &width);

      /* Maximum precision; an explicit negative precision is the same
         as not giving the precision.  A lone '.' is a precision of 0.  */
      *p++ = '.';
      *p++ = '*';
      if (*fmt == '.')
        {
          ok['c' - OKMIN] = 0;
          fmt = parse_width (fmt + 1, &argc, &argv, &prec);
        }
      else
        prec = -1;

      /* For partial backward compatibility, quietly ignore the rarely used
         length modifiers "h", "hh" and "l", which were accepted by m4
         1.4.21 and earlier but had undefined behavior on picky platforms.
         This compatibility hack is undocumented.  */
      fmt += fmt[0] == 'h' ? 1 + (fmt[1] == 'h') : fmt[0] == 'l';

      c = *fmt++;
      if (! (OKMIN <= c && c <= OKMAX && ok[c - OKMIN]))
        {
          M4ERROR ((warning_status, 0,
                    _("Warning: unrecognized specifier in %s"), squote (f)));
          if (c == '\0')
            fmt--;
          continue;
        }

      /* Our constructed format string in fstart is safe.  */
#if _GL_GNUC_PREREQ (4, 3) || defined __clang__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

      /* Specifiers.  We don't recognize C, S, n, or p.  */
      switch (c)
        {
        case 'c':
          p -= 2;               /* %.*c is undefined, so undo the '.*'.  */
          *p++ = 'c';
          *p = '\0';
          str = xasprintf (fstart, width, ARG_INT (argc, argv));
          break;

        case 's':
        case_s:
          *p++ = 's';
          *p = '\0';
          str = xasprintf (fstart, width, prec, ARG_STR (argc, argv));
          break;

        case 'd':
        case 'i':
        case 'o':
        case 'x':
        case 'X':
        case 'u':
          ;
          struct arg_long arg = {0};
          if (argc)
            {
              arg = arg_long (TOKEN_DATA_TEXT (*argv));
              if (arg.overflow)
                {
                  /* Numeric overflow was detected.  Format as a string;
                     this loses less information when debugging.  */
                  prec = MIN (INT_MAX, arg.overflow - TOKEN_DATA_TEXT (*argv));
                  goto case_s;
                }
              argc--, argv++;
            }
          p = mempcpy (p, PRIdIVAL, sizeof PRIdIVAL - 2);
          *p++ = c;
          *p = '\0';
          str = xasprintf (fstart, width, prec, arg.val);
          break;

        case 'a':
        case 'A':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
          *p++ = c;
          *p = '\0';
          str = xasprintf (fstart, width, prec, ARG_DOUBLE (argc, argv));
          break;

        default:
          abort ();
        }
#if _GL_GNUC_PREREQ (4, 3) || defined __clang__
# pragma GCC diagnostic pop
#endif

      /* NULL was returned on failure, such as invalid format string.  For
         now, just silently ignore that bad specifier.  */
      if (str == NULL)
        continue;

      obstack_grow (obs, str, strlen (str));
      free (str);
    }
}

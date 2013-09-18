/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 2001, 2006-2010, 2013 Free Software
   Foundation, Inc.

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

#include "vasnprintf.h"

/* Simple varargs substitute.  We assume int and unsigned int are the
   same size; likewise for long and unsigned long.  We do not yet
   handle long double or long long.  */

/* Parse STR of length LEN as an integer, reporting warnings on behalf
   of ME.  */
static int
arg_int (struct m4 *context, const m4_call_info *me, const char *str,
         size_t len)
{
  char *endp;
  long value;

  /* TODO - also allow parsing `'a' or `"a' which results in the
     numeric value of 'a', as in printf(1).  */
  if (!len)
    {
      m4_warn (context, 0, me, _("empty string treated as 0"));
      return 0;
    }
  errno = 0;
  value = strtol (str, &endp, 10);
  if (endp - str != len)
    m4_warn (context, 0, me, _("non-numeric argument %s"),
             quotearg_style_mem (locale_quoting_style, str, len));
  else if (isspace (to_uchar (*str)))
    m4_warn (context, 0, me, _("leading whitespace ignored"));
  else if (errno == ERANGE || (int) value != value)
    m4_warn (context, 0, me, _("numeric overflow detected"));
  return value;
}

/* Parse STR of length LEN as a long, reporting warnings on behalf of
   ME.  */
static long
arg_long (struct m4 *context, const m4_call_info *me, const char *str,
          size_t len)
{
  char *endp;
  long value;

  /* TODO - also allow parsing `'a' or `"a' which results in the
     numeric value of 'a', as in printf(1).  */
  if (!len)
    {
      m4_warn (context, 0, me, _("empty string treated as 0"));
      return 0L;
    }
  errno = 0;
  value = strtol (str, &endp, 10);
  if (endp - str != len)
    m4_warn (context, 0, me, _("non-numeric argument %s"),
             quotearg_style_mem (locale_quoting_style, str, len));
  else if (isspace (to_uchar (*str)))
    m4_warn (context, 0, me, _("leading whitespace ignored"));
  else if (errno == ERANGE)
    m4_warn (context, 0, me, _("numeric overflow detected"));
  return value;
}

/* Check STR of length LEN for embedded NUL, reporting warnings on
   behalf of ME.  */
static const char *
arg_string (struct m4 *context, const m4_call_info *me, const char *str,
            size_t len)
{
  if (strlen (str) < len)
    m4_warn (context, 0, me, _("argument %s truncated"),
             quotearg_style_mem (locale_quoting_style, str, len));
  return str;
}

/* Parse STR of length LEN as a double, reporting warnings on behalf
   of ME.  */
static double
arg_double (struct m4 *context, const m4_call_info *me, const char *str,
            size_t len)
{
  char *endp;
  double value;

  if (!len)
    {
      m4_warn (context, 0, me, _("empty string treated as 0"));
      return 0.0;
    }
  errno = 0;
  value = strtod (str, &endp);
  if (endp - str != len)
    m4_warn (context, 0, me, _("non-numeric argument %s"),
             quotearg_style_mem (locale_quoting_style, str, len));
  else if (isspace (to_uchar (*str)))
    m4_warn (context, 0, me, _("leading whitespace ignored"));
  else if (errno == ERANGE)
    m4_warn (context, 0, me, _("numeric overflow detected"));
  return value;
}

#define ARG_INT(i, argc, argv)                                  \
  ((argc <= ++i) ? 0 : arg_int (context, me, M4ARG (i), M4ARGLEN (i)))

#define ARG_LONG(i, argc, argv)                                 \
  ((argc <= ++i) ? 0L : arg_long (context, me, M4ARG (i), M4ARGLEN (i)))

#define ARG_STR(i, argc, argv)                                  \
  ((argc <= ++i) ? "" : arg_string (context, me, M4ARG (i), M4ARGLEN (i)))

#define ARG_DOUBLE(i, argc, argv)                               \
  ((argc <= ++i) ? 0.0 : arg_double (context, me, M4ARG (i), M4ARGLEN (i)))


/* The main formatting function.  Output is placed on the obstack OBS,
   the first argument in ARGV is the formatting string, and the rest
   is arguments for the string.  Warn rather than invoke unspecified
   behavior in the underlying printf when we do not recognize a
   format.  */

static void
format (m4 *context, m4_obstack *obs, int argc, m4_macro_args *argv)
{
  const m4_call_info *me = m4_arg_info (argv);
  const char *f;                        /* Format control string.  */
  size_t f_len;                         /* Length of f.  */
  const char *fmt;                      /* Position within f.  */
  char fstart[] = "%'+- 0#*.*hhd";      /* Current format spec.  */
  char *p;                              /* Position within fstart.  */
  unsigned char c;                      /* A simple character.  */
  int i = 1;                            /* Index within argc used so far.  */
  bool valid_format = true;             /* True if entire format string ok.  */

  /* Flags.  */
  char flags;                           /* Flags to use in fstart.  */
  enum {
    THOUSANDS   = 0x01, /* '\''.  */
    PLUS        = 0x02, /* '+'.  */
    MINUS       = 0x04, /* '-'.  */
    SPACE       = 0x08, /* ' '.  */
    ZERO        = 0x10, /* '0'.  */
    ALT         = 0x20, /* '#'.  */
    DONE        = 0x40  /* No more flags.  */
  };

  /* Precision specifiers.  */
  int width;                    /* Minimum field width.  */
  int prec;                     /* Precision.  */
  char lflag;                   /* Long flag.  */

  /* Specifiers we are willing to accept.  ok['x'] implies %x is ok.
     Various modifiers reduce the set, in order to avoid undefined
     behavior in printf.  */
  char ok[128];

  /* Check that formatted text succeeded with correct type.  */
  int result = 0;
  enum {CHAR, INT, LONG, DOUBLE, STR} datatype;

  f = fmt = M4ARG (1);
  f_len = M4ARGLEN (1);
  assert (!f[f_len]); /* Requiring a terminating NUL makes parsing simpler.  */
  memset (ok, 0, sizeof ok);
  while (1)
    {
      const char *percent = (char *) memchr (fmt, '%', f_len);
      if (!percent)
        {
          obstack_grow (obs, fmt, f_len);
          break;
        }
      obstack_grow (obs, fmt, percent - fmt);
      f_len -= percent - fmt + 1;
      fmt = percent + 1;

      if (*fmt == '%')
        {
          obstack_1grow (obs, '%');
          fmt++;
          f_len--;
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
            case '\'': /* thousands separator */
              ok['a'] = ok['A'] = ok['c'] = ok['e'] = ok['E']
                = ok['o'] = ok['s'] = ok['x'] = ok['X'] = 0;
              flags |= THOUSANDS;
              break;

            case '+': /* mandatory sign */
              ok['c'] = ok['o'] = ok['s'] = ok['u'] = ok['x'] = ok['X'] = 0;
              flags |= PLUS;
              break;

            case ' ': /* space instead of positive sign */
              ok['c'] = ok['o'] = ok['s'] = ok['u'] = ok['x'] = ok['X'] = 0;
              flags |= SPACE;
              break;

            case '0': /* zero padding */
              ok['c'] = ok['s'] = 0;
              flags |= ZERO;
              break;

            case '#': /* alternate output */
              ok['c'] = ok['d'] = ok['i'] = ok['s'] = ok['u'] = 0;
              flags |= ALT;
              break;

            case '-': /* left justification */
              flags |= MINUS;
              break;

            default:
              flags |= DONE;
              break;
            }
        }
      while (!(flags & DONE) && (f_len--, fmt++));
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
          width = ARG_INT (i, argc, argv);
          fmt++;
          f_len--;
        }
      else
        while (isdigit ((unsigned char) *fmt))
          {
            width = 10 * width + *fmt - '0';
            fmt++;
            f_len--;
          }

      /* Maximum precision; an explicit negative precision is the same
         as not giving the precision.  A lone '.' is a precision of 0.  */
      prec = -1;
      *p++ = '.';
      *p++ = '*';
      if (*fmt == '.')
        {
          ok['c'] = 0;
          f_len--;
          if (*(++fmt) == '*')
            {
              prec = ARG_INT (i, argc, argv);
              ++fmt;
              f_len--;
            }
          else
            {
              prec = 0;
              while (isdigit ((unsigned char) *fmt))
                {
                  prec = 10 * prec + *fmt - '0';
                  fmt++;
                  f_len--;
                }
            }
        }

      /* Length modifiers.  We don't yet recognize ll, j, t, or z.  */
      if (*fmt == 'l')
        {
          *p++ = 'l';
          lflag = 1;
          fmt++;
          f_len--;
          ok['c'] = ok['s'] = 0;
        }
      else if (*fmt == 'h')
        {
          *p++ = 'h';
          fmt++;
          f_len--;
          if (*fmt == 'h')
            {
              *p++ = 'h';
              fmt++;
              f_len--;
            }
          ok['a'] = ok['A'] = ok['c'] = ok['e'] = ok['E'] = ok['f'] = ok['F']
            = ok['g'] = ok['G'] = ok['s'] = 0;
        }

      c = *fmt;
      if (sizeof ok <= c || !ok[c] || !f_len)
        {
          m4_warn (context, 0, me, _("unrecognized specifier in %s"),
                   quotearg_style_mem (locale_quoting_style, f, M4ARGLEN (1)));
          valid_format = false;
          if (f_len > 0)
            {
              fmt++;
              f_len--;
            }
          continue;
        }
      fmt++;
      f_len--;

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
          result = obstack_printf (obs, fstart, width,
                                   ARG_INT (i, argc, argv));
          break;

        case INT:
          result = obstack_printf (obs, fstart, width, prec,
                                   ARG_INT (i, argc, argv));
          break;

        case LONG:
          result = obstack_printf (obs, fstart, width, prec,
                                   ARG_LONG (i, argc, argv));
          break;

        case DOUBLE:
          result = obstack_printf (obs, fstart, width, prec,
                                   ARG_DOUBLE (i, argc, argv));
          break;

        case STR:
          result = obstack_printf (obs, fstart, width, prec,
                                   ARG_STR (i, argc, argv));
          break;

        default:
          abort ();
        }
      /* Since obstack_printf can only fail with EILSEQ or EINVAL, but
         we constructed fstart, the result should not be negative.  */
      assert (0 <= result);
    }
  if (valid_format)
    m4_bad_argc (context, argc, me, i, i, true);
}

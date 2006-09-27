/* GNU m4 -- A simple macro processor
   Copyright (C) 1999, 2000, 2001, 2006 Free Software Foundation, Inc.

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

#include <stdio.h>

#if TM_IN_SYS_TIME
#  include <sys/time.h>
#else
#  include <time.h>
#endif /* TM_IN_SYS_TIME */

#include <m4module.h>

#define m4_builtin_table	time_LTX_m4_builtin_table

/*	   function	macros	blind	side	minargs	maxargs */
#define builtin_functions					\
  BUILTIN (currenttime,	false,	false,	false,	0,	0  )	\
  BUILTIN (ctime,	false,	false,	false,	0,	1  )	\
  BUILTIN (gmtime,	false,	true,	false,	1,	1  )	\
  BUILTIN (localtime,	false,	true,	false,	1,	1  )	\

#define mktime_functions					\
  BUILTIN (mktime,	false,	true,	false,	6,	7  )	\

#define strftime_functions					\
  BUILTIN (strftime,	false,	true,	false,	2,	2  )	\


#define BUILTIN(handler, macros, blind, side, min, max)  M4BUILTIN(handler)
  builtin_functions
# if HAVE_MKTIME
  mktime_functions
# endif
# if HAVE_STRFTIME
  strftime_functions
# endif
#undef BUILTIN

m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)	\
  { CONC(builtin_, handler), STR(handler),		\
    ((macros ? M4_BUILTIN_GROKS_MACRO : 0)		\
     | (blind ? M4_BUILTIN_BLIND : 0)			\
     | (side ? M4_BUILTIN_SIDE_EFFECT : 0)),		\
    min, max },

  builtin_functions
# if HAVE_MKTIME
  mktime_functions
# endif
# if HAVE_STRFTIME
  strftime_functions
# endif
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};

/**
 * currenttime()
 **/
M4BUILTIN_HANDLER (currenttime)
{
  char buf[64];
  time_t now;
  int l;

  now = time (0L);
  l = sprintf (buf, "%ld", now);

  obstack_grow (obs, buf, l);
}

/**
 * ctime([SECONDS])
 **/
M4BUILTIN_HANDLER (ctime)
{
  time_t t;
  int i;
  const char *s;

  if (argc == 2)
    {
      m4_numeric_arg (context, argc, argv, 1, &i);
      t = i;
    }
  else
    t = time (0L);

  s = ctime (&t);
  obstack_grow (obs, s, 24);
}

static void
format_tm (m4_obstack *obs, struct tm *tm)
{
  m4_shipout_int (obs, tm->tm_sec);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_min);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_hour);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_mday);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_mon);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_year);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_wday);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_yday);
  obstack_1grow (obs, ',');

  m4_shipout_int (obs, tm->tm_isdst);
}

/**
 * gmtime(SECONDS)
 **/
M4BUILTIN_HANDLER (gmtime)
{
  time_t t;
  int i;

  if (!m4_numeric_arg (context, argc, argv, 1, &i))
    return;

  t = i;
  format_tm (obs, gmtime (&t));
}

/**
 * localtime(SECONDS)
 **/
M4BUILTIN_HANDLER (localtime)
{
  time_t t;
  int i;

  if (!m4_numeric_arg (context, argc, argv, 1, &i))
    return;

  t = i;
  format_tm (obs, localtime (&t));
}

#if HAVE_MKTIME
/**
 * mktime(SEC, MIN, HOUR, MDAY, MONTH, YEAR, [ISDST])
 **/
M4BUILTIN_HANDLER (mktime)
{
  struct tm tm;
  time_t t;

  if (!m4_numeric_arg (context, argc, argv, 1, &tm.tm_sec))
    return;
  if (!m4_numeric_arg (context, argc, argv, 2, &tm.tm_min))
    return;
  if (!m4_numeric_arg (context, argc, argv, 3, &tm.tm_hour))
    return;
  if (!m4_numeric_arg (context, argc, argv, 4, &tm.tm_mday))
    return;
  if (!m4_numeric_arg (context, argc, argv, 5, &tm.tm_mon))
    return;
  if (!m4_numeric_arg (context, argc, argv, 6, &tm.tm_year))
    return;
  if (M4ARG (7) && !m4_numeric_arg (context, argc, argv, 7, &tm.tm_isdst))
    return;

  t = mktime (&tm);

  m4_shipout_int (obs, t);
}
#endif /* HAVE_MKTIME */

#if HAVE_STRFTIME
/**
 * strftime(FORMAT, SECONDS)
 **/
M4BUILTIN_HANDLER (strftime)
{
  struct tm *tm;
  time_t t;
  char *buf;
  int l;

  if (!m4_numeric_arg (context, argc, argv, 2, &l))
    return;

  t = l;
  tm = localtime (&t);

  buf = (char *) obstack_alloc (obs, 1024);
  l = strftime (buf, 1024, M4ARG (1), tm);
  obstack_grow (obs, buf, l);
}
#endif /* HAVE_STRFTIME */

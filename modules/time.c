/* GNU m4 -- A simple macro processor
   Copyright (C) 1998 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <m4.h>
#include <builtin.h>

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif /* TM_IN_SYS_TIME */

#define m4_macro_table time_LTX_m4_macro_table

DECLARE(m4_currenttime);
DECLARE(m4_ctime);
DECLARE(m4_gmtime);
DECLARE(m4_localtime);

#ifdef HAVE_MKTIME
DECLARE(m4_mktime);
#endif /* HAVE_MKTIME */

#ifdef HAVE_STRFTIME
DECLARE(m4_strftime);
#endif /* HAVE_STRFTIME */

#undef DECLARE

builtin m4_macro_table[] =
{
  /* name		GNUext	macros	blind	function */
  { "currenttime",	TRUE,	FALSE,	FALSE,	m4_currenttime },
  { "ctime",		TRUE,	FALSE,	FALSE,	m4_ctime },
  { "gmtime",		TRUE,	FALSE,	TRUE,	m4_gmtime },
  { "localtime",	TRUE,	FALSE,	TRUE,	m4_localtime },
#ifdef HAVE_MKTIME
  { "mktime",		TRUE,	FALSE,	TRUE,	m4_mktime },
#endif /* HAVE_MKTIME */
#ifdef HAVE_STRFTIME
  { "strftime",		TRUE,	FALSE,	TRUE,	m4_strftime },
#endif /* HAVE_STRFTIME */
  { 0,			FALSE,	FALSE,	FALSE,	0 },
};



static void
m4_currenttime (struct obstack *obs, int argc, token_data **argv)
{
  char buf[64];
  time_t now;
  int l;

  if (bad_argc (argv[0], argc, 1, 1))
    return;

  now = time(0L);
  l = sprintf(buf, "%ld", now);

  obstack_grow (obs, buf, l);
}

static void
m4_ctime (struct obstack *obs, int argc, token_data **argv)
{
  char buf[64];
  time_t t;
  int l;

  if (bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 2)
    numeric_arg(argv[0], ARG(1), (int *)&t);
  else
    t = time(0L);

  obstack_grow (obs, ctime(&t), 24);
}

static void
format_tm(struct obstack *obs, struct tm *tm)
{
  shipout_int(obs, tm->tm_sec);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_min);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_hour);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_mday);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_mon);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_year);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_wday);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_yday);
  obstack_1grow(obs, ',');

  shipout_int(obs, tm->tm_isdst);
}

static void
m4_gmtime (struct obstack *obs, int argc, token_data **argv)
{
  time_t t;
  struct tm *tm;

  if (bad_argc (argv[0], argc, 2, 2))
    return;

  if (!numeric_arg (argv[0], ARG (1), (int *)&t))
    return;

  format_tm(obs, gmtime(&t));
}

static void
m4_localtime (struct obstack *obs, int argc, token_data **argv)
{
  time_t t;
  struct tm *tm;

  if (bad_argc (argv[0], argc, 2, 2))
    return;

  if (!numeric_arg (argv[0], ARG (1), (int *)&t))
    return;

  format_tm(obs, localtime(&t));
}

#ifdef HAVE_MKTIME
/*-------------------------------------------.
| mktime(sec,min,hour,mday,month,year,isdst) |
`-------------------------------------------*/

static void
m4_mktime (struct obstack *obs, int argc, token_data **argv)
{
  struct tm tm;
  time_t t;

  if (bad_argc (argv[0], argc, 7, 8))
    return;

  if (!numeric_arg (argv[0], ARG (1), &tm.tm_sec))
    return;
  if (!numeric_arg (argv[0], ARG (2), &tm.tm_min))
    return;
  if (!numeric_arg (argv[0], ARG (3), &tm.tm_hour))
    return;
  if (!numeric_arg (argv[0], ARG (4), &tm.tm_mday))
    return;
  if (!numeric_arg (argv[0], ARG (5), &tm.tm_mon))
    return;
  if (!numeric_arg (argv[0], ARG (6), &tm.tm_year))
    return;
  if (ARG(7) && !numeric_arg (argv[0], ARG (7), &tm.tm_isdst))
    return;

  t = mktime(&tm);

  shipout_int(obs, t);
}
#endif /* HAVE_MKTIME */

#ifdef HAVE_STRFTIME
static void
m4_strftime (struct obstack *obs, int argc, token_data **argv)
{
  struct tm *tm;
  time_t t;
  char *buf;
  int l;

  if (bad_argc (argv[0], argc, 3, 3))
    return;

  if (!numeric_arg (argv[0], ARG (2), (int *)&t))
    return;

  tm = localtime(&t);

  buf = (char *) obstack_alloc(obs, 1024);
  l = strftime(buf, 1024, ARG(1), tm);
  obstack_grow(obs, buf, l);
}
#endif /* HAVE_STRFTIME */

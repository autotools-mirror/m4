/* GNU m4 -- A simple macro processor
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.
  
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

#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <sys/utsname.h>
#include <sys/types.h>

#include <m4module.h>

M4BUILTIN(m4_getcwd);
M4BUILTIN(m4_getlogin);
M4BUILTIN(m4_getpid);
M4BUILTIN(m4_getppid);
M4BUILTIN(m4_getuid);
M4BUILTIN(m4_getpwnam);
M4BUILTIN(m4_getpwuid);
M4BUILTIN(m4_hostname);
M4BUILTIN(m4_rand);
M4BUILTIN(m4_srand);
M4BUILTIN(m4_getenv);
M4BUILTIN(m4_setenv);
M4BUILTIN(m4_unsetenv);
M4BUILTIN(m4_uname);

#undef M4BUILTIN

#define m4_macro_table	stdlib_LTX_m4_macro_table

builtin m4_macro_table[] =
{
  /* name		GNUext	macros	blind	function */
  { "getcwd",		TRUE,	FALSE,	FALSE,	m4_getcwd },
  { "getenv",		TRUE,	FALSE,	TRUE,	m4_getenv },
  { "setenv",		TRUE,	FALSE,	TRUE,	m4_setenv },
  { "unsetenv",		TRUE,	FALSE,	TRUE,	m4_unsetenv },
  { "getlogin",		TRUE,	FALSE,	FALSE,	m4_getlogin },
  { "getpid",		TRUE,	FALSE,	FALSE,	m4_getpid },
  { "getppid",		TRUE,	FALSE,	FALSE,	m4_getppid },
  { "getpwnam",		TRUE,	FALSE,	TRUE,	m4_getpwnam },
  { "getpwuid",		TRUE,	FALSE,	TRUE,	m4_getpwuid },
  { "getuid",		TRUE,	FALSE,	FALSE,	m4_getuid },
  { "hostname",		TRUE,	FALSE,	FALSE,	m4_hostname },
  { "rand",		TRUE,	FALSE,	FALSE,	m4_rand },
  { "srand",		TRUE,	FALSE,	FALSE,	m4_srand },
  { "uname",		TRUE,	FALSE,	FALSE,	m4_uname },
  { 0,			FALSE,	FALSE,	FALSE,	0 },
};



static void
m4_getcwd (struct obstack *obs, int argc, token_data **argv)
{
  char buf[1024];
  char *bp;
  int l;

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  bp = getcwd(buf, sizeof buf);

  if (bp != NULL)		/* in case of error return null string */
    m4_shipout_string (obs, buf, 0 , FALSE);
}

static void
m4_getenv (struct obstack *obs, int argc, token_data **argv)
{
  char *env;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  env = getenv((char*)M4ARG(1));

  if (env != NULL)
    m4_shipout_string (obs, env, 0, FALSE);
}

static void
m4_setenv (struct obstack *obs, int argc, token_data **argv)
{
  char *env;
  int overwrite = 1;

  if (m4_bad_argc (argv[0], argc, 3, 4))
    return;

  if (argc == 4)
    if (!m4_numeric_arg(argv[0], (char*)M4ARG(3), &overwrite))
      return;

#ifdef HAVE_SETENV
  setenv((char*)M4ARG(1), (char*)M4ARG(2), overwrite);
#else
#ifdef HAVE_PUTENV
  if (!overwrite && getenv ((char*)M4ARG(1)) != NULL)
    return;

  obstack_grow (obs, (char*)M4ARG(1), strlen ((char*)M4ARG(1)));
  obstack_1grow (obs, '=');
  obstack_grow (obs, (char*)M4ARG(2), strlen ((char*)M4ARG(2)));
  obstack_1grow (obs, '\0');

  env = obstack_finish (obs);
  putenv (env);
#endif /* HAVE_PUTENV */
#endif /* HAVE_SETENV */
}

static void
m4_unsetenv (struct obstack *obs, int argc, token_data **argv)
{
  char *env;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

#ifdef HAVE_UNSETENV
  unsetenv ((char*)M4ARG(1));
#endif /* HAVE_UNSETENV */
}

static void
m4_getlogin (struct obstack *obs, int argc, token_data **argv)
{
  char *login;

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  login = getlogin ();

  if (login != NULL)
    m4_shipout_string (obs, login, 0, FALSE);
}

static void
m4_getpid (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, getpid());
}

static void
m4_getppid (struct obstack *obs, int argc, token_data **argv)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, getppid());
}

static void
m4_getpwnam (struct obstack *obs, int argc, token_data **argv)
{
  struct passwd *pw;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  pw = getpwnam((char*)M4ARG(1));

  if (pw != NULL)
    {
      m4_shipout_string (obs, pw->pw_name, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_passwd, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_int(obs, pw->pw_uid);
      obstack_1grow (obs, ',');
      m4_shipout_int(obs, pw->pw_gid);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_gecos, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_dir, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_shell, 0, TRUE);
    }
}

static void
m4_getpwuid (struct obstack *obs, int argc, token_data **argv)
{
  struct passwd *pw;
  int uid;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  if (!m4_numeric_arg(argv[0], (char*)M4ARG(1), &uid))
    return;

  pw = getpwuid(uid);

  if (pw != NULL)
    {
      m4_shipout_string (obs, pw->pw_name, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_passwd, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_int(obs, pw->pw_uid);
      obstack_1grow (obs, ',');
      m4_shipout_int(obs, pw->pw_gid);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_gecos, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_dir, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, pw->pw_shell, 0, TRUE);
    }
}

static void
m4_hostname (struct obstack *obs, int argc, token_data **argv)
{
  char buf[1024];

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  if (gethostname(buf, sizeof buf) < 0)
    return;

  m4_shipout_string (obs, buf, 0, FALSE);
}

static void
m4_rand (struct obstack *obs, int argc, token_data **argv)
{
  int i;

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, rand());
}

static void
m4_srand (struct obstack *obs, int argc, token_data **argv)
{
  char buf[64];
  int seed;

  if (m4_bad_argc (argv[0], argc, 1, 2))
    return;

  if (argc == 1)
    seed = time(0L) * getpid();
  else
    {
      if (!m4_numeric_arg(argv[0], (char*)M4ARG(1), &seed))
	return;
    }

  srand(seed);
}

static void
m4_uname (struct obstack *obs, int argc, token_data **argv)
{
  struct utsname ut;

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  if (uname(&ut) == 0)
    {
      m4_shipout_string (obs, ut.sysname, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, ut.nodename, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, ut.release, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, ut.version, 0, TRUE);
      obstack_1grow (obs, ',');
      m4_shipout_string (obs, ut.machine, 0, TRUE);
    }
}

static void
m4_getuid (struct obstack *obs, int argc, token_data **argv)
{
  int i;

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, getuid());
}

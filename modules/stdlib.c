/* GNU m4 -- A simple macro processor
   Copyright 1999, 2000 Free Software Foundation, Inc.

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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef TM_IN_SYS_TIME
#  include <sys/time.h>
#else
#  include <time.h>
#endif
#include <sys/utsname.h>
#include <sys/types.h>

#include <m4module.h>

#define m4_builtin_table	stdlib_LTX_m4_builtin_table

/*		function	macros	blind */
#define builtin_functions			\
	BUILTIN (getcwd,	FALSE,	FALSE)	\
	BUILTIN (getenv,	FALSE,	TRUE)	\
	BUILTIN (getlogin,	FALSE,	FALSE)	\
	BUILTIN (getpid,	FALSE,	FALSE)	\
	BUILTIN (getppid,	FALSE,	FALSE)	\
	BUILTIN (getuid,	FALSE,	FALSE)	\
	BUILTIN (getpwnam,	FALSE,	TRUE)	\
	BUILTIN (getpwuid,	FALSE,	TRUE)	\
	BUILTIN (hostname,	FALSE,	FALSE)	\
	BUILTIN (rand,		FALSE,	FALSE)	\
	BUILTIN (srand,		FALSE,	FALSE)	\
	BUILTIN (setenv,	FALSE,	TRUE)	\
	BUILTIN (unsetenv,	FALSE,	TRUE)	\
	BUILTIN (uname,		FALSE,	FALSE)

#define BUILTIN(handler, macros,  blind)	M4BUILTIN(handler);
  builtin_functions
#undef BUILTIN

m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind)		\
	{ STR(handler), CONC(builtin_, handler), macros, blind },

  builtin_functions
#undef BUILTIN

  { 0, 0, FALSE, FALSE },
};


/**
 * getcwd()
 **/
M4BUILTIN_HANDLER (getcwd)
{
  char buf[1024];
  char *bp;

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  bp = getcwd(buf, sizeof buf);

  if (bp != NULL)		/* in case of error return null string */
    m4_shipout_string (obs, buf, 0 , FALSE);
}

/**
 * getenv(NAME)
 **/
M4BUILTIN_HANDLER (getenv)
{
  char *env;

  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

  env = getenv((char*)M4ARG(1));

  if (env != NULL)
    m4_shipout_string (obs, env, 0, FALSE);
}

/**
 * setenv(NAME, VALUE, [OVERWRITE])
 **/
M4BUILTIN_HANDLER (setenv)
{
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

  {
    char *env = obstack_finish (obs);
    putenv (env);
  }
#endif /* HAVE_PUTENV */
#endif /* HAVE_SETENV */
}

/**
 * unsetenv(NAME)
 **/
M4BUILTIN_HANDLER (unsetenv)
{
  if (m4_bad_argc (argv[0], argc, 2, 2))
    return;

#ifdef HAVE_UNSETENV
  unsetenv ((char*)M4ARG(1));
#endif /* HAVE_UNSETENV */
}

/**
 * getlogin()
 **/
M4BUILTIN_HANDLER (getlogin)
{
  char *login;

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  login = getlogin ();

  if (login != NULL)
    m4_shipout_string (obs, login, 0, FALSE);
}

/**
 * getpid()
 **/
M4BUILTIN_HANDLER (getpid)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, getpid());
}

/**
 * getppid()
 **/
M4BUILTIN_HANDLER (getppid)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, getppid());
}

/**
 * getpwnam(NAME)
 **/
M4BUILTIN_HANDLER (getpwnam)
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

/**
 * getpwuid(UID)
 **/
M4BUILTIN_HANDLER (getpwuid)
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

/**
 * hostname()
 **/
M4BUILTIN_HANDLER (hostname)
{
  char buf[1024];

  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  if (gethostname(buf, sizeof buf) < 0)
    return;

  m4_shipout_string (obs, buf, 0, FALSE);
}

/**
 * rand()
 **/
M4BUILTIN_HANDLER (rand)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, rand());
}

/**
 * srand()
 **/
M4BUILTIN_HANDLER (srand)
{
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

/**
 * uname()
 **/
M4BUILTIN_HANDLER (uname)
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

/**
 * getuid()
 **/
M4BUILTIN_HANDLER (getuid)
{
  if (m4_bad_argc (argv[0], argc, 1, 1))
    return;

  m4_shipout_int(obs, getuid());
}

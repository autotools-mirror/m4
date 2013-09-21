/* GNU m4 -- A simple macro processor
   Copyright (C) 1999-2001, 2006-2010, 2013 Free Software Foundation,
   Inc.

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

#include <pwd.h>
#if TM_IN_SYS_TIME
#  include <sys/time.h>
#else
#  include <time.h>
#endif
#include <sys/utsname.h>

/* Build using only the exported interfaces, unless NDEBUG is set, in
   which case use private symbols to speed things up as much as possible.  */
#ifndef NDEBUG
#  include <m4/m4module.h>
#else
#  include "m4private.h"
#endif

/*         function     macros  blind   side    minargs maxargs */
#define builtin_functions                                       \
    BUILTIN (getcwd,    false,  false,  false,  0,      0  )    \
    BUILTIN (getenv,    false,  true,   false,  1,      1  )    \
    BUILTIN (getlogin,  false,  false,  false,  0,      0  )    \
    BUILTIN (getpid,    false,  false,  false,  0,      0  )    \
    BUILTIN (getppid,   false,  false,  false,  0,      0  )    \
    BUILTIN (getuid,    false,  false,  false,  0,      0  )    \
    BUILTIN (getpwnam,  false,  true,   false,  1,      1  )    \
    BUILTIN (getpwuid,  false,  true,   false,  1,      1  )    \
    BUILTIN (hostname,  false,  false,  false,  0,      0  )    \
    BUILTIN (rand,      false,  false,  false,  0,      0  )    \
    BUILTIN (srand,     false,  false,  false,  0,      1  )    \
    BUILTIN (setenv,    false,  true,   false,  2,      3  )    \
    BUILTIN (unsetenv,  false,  true,   false,  1,      1  )    \
    BUILTIN (uname,     false,  false,  false,  0,      0  )    \


#define BUILTIN(handler, macros, blind, side, min, max) M4BUILTIN (handler);
  builtin_functions
#undef BUILTIN

static const m4_builtin m4_builtin_table[] =
{
#define BUILTIN(handler, macros, blind, side, min, max)                 \
  M4BUILTIN_ENTRY (handler, #handler, macros, blind, side, min, max)

  builtin_functions
#undef BUILTIN

  { NULL, NULL, 0, 0, 0 },
};


void
include_stdlib (m4 *context, m4_module *module, m4_obstack *obs)
{
  m4_install_builtins (context, module, m4_builtin_table);
}


/**
 * getcwd()
 **/
M4BUILTIN_HANDLER (getcwd)
{
  /* FIXME - Use gnulib module for arbitrary-length cwd.  */
  char buf[1024];
  char *bp;

  bp = getcwd (buf, sizeof buf);

  if (bp != NULL)               /* in case of error return null string */
    m4_shipout_string (context, obs, buf, SIZE_MAX, false);
}

/**
 * getenv(NAME)
 **/
M4BUILTIN_HANDLER (getenv)
{
  char *env;

  env = getenv (M4ARG (1));

  if (env != NULL)
    m4_shipout_string (context, obs, env, SIZE_MAX, false);
}

/**
 * setenv(NAME, VALUE, [OVERWRITE])
 **/
M4BUILTIN_HANDLER (setenv)
{
  int overwrite = 1;

  if (argc >= 4)
    if (!m4_numeric_arg (context, m4_arg_info (argv), M4ARG (3), M4ARGLEN (3),
                         &overwrite))
      return;

  /* TODO - error checking.  */
  setenv (M4ARG (1), M4ARG (2), overwrite);
}

/**
 * unsetenv(NAME)
 **/
M4BUILTIN_HANDLER (unsetenv)
{
  /* TODO - error checking.  */
  unsetenv (M4ARG (1));
}

/**
 * getlogin()
 **/
M4BUILTIN_HANDLER (getlogin)
{
  char *login;

  login = getlogin ();

  if (login != NULL)
    m4_shipout_string (context, obs, login, SIZE_MAX, false);
}

/**
 * getpid()
 **/
M4BUILTIN_HANDLER (getpid)
{
  m4_shipout_int (obs, getpid ());
}

/**
 * getppid()
 **/
M4BUILTIN_HANDLER (getppid)
{
  m4_shipout_int (obs, getppid ());
}

/**
 * getpwnam(NAME)
 **/
M4BUILTIN_HANDLER (getpwnam)
{
  struct passwd *pw;

  pw = getpwnam (M4ARG (1));

  if (pw != NULL)
    {
      m4_shipout_string (context, obs, pw->pw_name, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_passwd, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_int (obs, pw->pw_uid);
      obstack_1grow (obs, ',');
      m4_shipout_int (obs, pw->pw_gid);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_gecos, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_dir, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_shell, SIZE_MAX, true);
    }
}

/**
 * getpwuid(UID)
 **/
M4BUILTIN_HANDLER (getpwuid)
{
  struct passwd *pw;
  int uid;

  if (!m4_numeric_arg (context, m4_arg_info (argv), M4ARG (1), M4ARGLEN (1),
                       &uid))
    return;

  pw = getpwuid (uid);

  if (pw != NULL)
    {
      m4_shipout_string (context, obs, pw->pw_name, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_passwd, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_int (obs, pw->pw_uid);
      obstack_1grow (obs, ',');
      m4_shipout_int (obs, pw->pw_gid);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_gecos, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_dir, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, pw->pw_shell, SIZE_MAX, true);
    }
}

/**
 * hostname()
 **/
M4BUILTIN_HANDLER (hostname)
{
  char buf[1024];

  if (gethostname (buf, sizeof buf) < 0)
    return;

  m4_shipout_string (context, obs, buf, SIZE_MAX, false);
}

/**
 * rand()
 **/
M4BUILTIN_HANDLER (rand)
{
  m4_shipout_int (obs, rand ());
}

/**
 * srand()
 **/
M4BUILTIN_HANDLER (srand)
{
  int seed;

  if (argc == 1)
    seed = time (0L) * getpid ();
  else
    {
      if (!m4_numeric_arg (context, m4_arg_info (argv), M4ARG (1),
                           M4ARGLEN (1), &seed))
        return;
    }

  srand (seed);
}

/**
 * uname()
 **/
M4BUILTIN_HANDLER (uname)
{
  struct utsname ut;

  if (uname (&ut) == 0)
    {
      m4_shipout_string (context, obs, ut.sysname, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, ut.nodename, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, ut.release, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, ut.version, SIZE_MAX, true);
      obstack_1grow (obs, ',');
      m4_shipout_string (context, obs, ut.machine, SIZE_MAX, true);
    }
}

/**
 * getuid()
 **/
M4BUILTIN_HANDLER (getuid)
{
  m4_shipout_int (obs, getuid ());
}

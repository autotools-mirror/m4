/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 1998-1999 Free Software Foundation, Inc.
  
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>

#include "m4private.h"

/* The name this program was run with. */
M4_GLOBAL_DATA const char *program_name;

/* Operate interactively (-e).  */
M4_GLOBAL_DATA int interactive = 0;

/* Enable sync output for /lib/cpp (-s).  */
M4_GLOBAL_DATA int sync_output = 0;

/* Debug (-d[flags]).  */
M4_GLOBAL_DATA int debug_level = 0;

/* Hash table size (should be a prime) (-Hsize).  */
M4_GLOBAL_DATA int hash_table_size = HASHMAX;

/* Disable GNU extensions (-G).  */
M4_GLOBAL_DATA int no_gnu_extensions = 0;

/* Prefix all builtin functions by `m4_'.  */
M4_GLOBAL_DATA int prefix_all_builtins = 0;

/* Max length of arguments in trace output (-lsize).  */
M4_GLOBAL_DATA int max_debug_argument_length = 0;

/* Suppress warnings about missing arguments.  */
M4_GLOBAL_DATA int suppress_warnings = 0;

/* If not zero, then value of exit status for warning diagnostics.  */
M4_GLOBAL_DATA int warning_status = 0;

/* Artificial limit for expansion_level in macro.c.  */
M4_GLOBAL_DATA int nesting_limit = 250;

/* User provided regexp for describing m4 words.  */
M4_GLOBAL_DATA const char *user_word_regexp = NULL;

/* If nonzero, comments are discarded in the token parser.  */
M4_GLOBAL_DATA int discard_comments = 0;

/* input syntax table. */
M4_GLOBAL_DATA unsigned short m4_syntax_table[256];

/* Quote chars.  */
M4_GLOBAL_DATA m4_string m4_rquote;
M4_GLOBAL_DATA m4_string m4_lquote;

/* Comment chars.  */
M4_GLOBAL_DATA m4_string m4_bcomm;
M4_GLOBAL_DATA m4_string m4_ecomm;
 

/*------------------------------------------------------------------------.
| Addressable function versions of the macros defined in m4private.h.     |
| Since they are functions the caller does not need access to the         |
| internal data structure, so they are safe to export for use in          |
| external modules.                                                       |
`------------------------------------------------------------------------*/
m4_token_data_t
m4_token_data_type (m4_token_data *name)
{
    return M4_TOKEN_DATA_TYPE(name);
}

char *
m4_token_data_text (m4_token_data *name)
{
    return M4_TOKEN_DATA_TEXT(name);
}

char *
m4_token_data_orig_text (m4_token_data *name)
{
#ifdef ENABLE_CHANGEWORD
    return M4_TOKEN_DATA_ORIG_TEXT(name);
#else
    return NULL;
#endif    
}

m4_builtin_func *
m4_token_data_func (m4_token_data *name)
{
    return M4_TOKEN_DATA_FUNC(name);
}

boolean
m4_token_data_func_traced (m4_token_data *name)
{
    return M4_TOKEN_DATA_FUNC_TRACED(name);
}


/*------------------------------------------------------------------------.
| Give friendly warnings if a builtin macro is passed an inappropriate	  |
| number of arguments.  NAME is macro name for messages, ARGC is actual	  |
| number of arguments, MIN is the minimum number of acceptable arguments, |
| negative if not applicable, MAX is the maximum number, negative if not  |
| applicable.								  |
`------------------------------------------------------------------------*/

boolean
m4_bad_argc (m4_token_data *name, int argc, int min, int max)
{
  boolean isbad = FALSE;

  if (min > 0 && argc < min)
    {
      if (!suppress_warnings)
	M4ERROR ((warning_status, 0,
		  _("Warning: Too few arguments to built-in `%s'"),
		  M4_TOKEN_DATA_TEXT (name)));
      isbad = TRUE;
    }
  else if (max > 0 && argc > max && !suppress_warnings)
    M4ERROR ((warning_status, 0,
	      _("Warning: Excess arguments to built-in `%s' ignored"),
	      M4_TOKEN_DATA_TEXT (name)));

  return isbad;
}

const char *
m4_skip_space (const char *arg)
{
  while (M4_IS_SPACE(*arg))
    arg++;
  return arg;
}

/*--------------------------------------------------------------------------.
| The function m4_numeric_arg () converts ARG to an int pointed to by       |
| VALUEP. If the conversion fails, print error message for macro MACRO.     |
| Return TRUE iff conversion succeeds.					    |
`--------------------------------------------------------------------------*/
boolean
m4_numeric_arg (m4_token_data *macro, const char *arg, int *valuep)
{
  char *endp;

  if (*arg == 0 || (*valuep = strtol (m4_skip_space(arg), &endp, 10), 
		    *m4_skip_space(endp) != 0))
    {
      M4ERROR ((warning_status, 0,
		_("Non-numeric argument to built-in `%s'"),
		M4_TOKEN_DATA_TEXT (macro)));
      return FALSE;
    }
  return TRUE;
}

/*----------------------------------------------------------------------.
| Format an int VAL, and stuff it into an obstack OBS.  Used for macros |
| expanding to numbers.						        |
`----------------------------------------------------------------------*/

void
m4_shipout_int (struct obstack *obs, int val)
{
  char buf[128];

  sprintf(buf, "%d", val);
  obstack_grow (obs, buf, strlen (buf));
}

void
m4_shipout_string (struct obstack *obs, const char *s, int len, boolean quoted)
{
  if (s == NULL)
    s = "";

  if (len == 0)
    len = strlen(s);

  if (quoted)
    obstack_grow (obs, m4_lquote.string, m4_lquote.length);
  obstack_grow (obs, s, len);
  if (quoted)
    obstack_grow (obs, m4_rquote.string, m4_rquote.length);
}


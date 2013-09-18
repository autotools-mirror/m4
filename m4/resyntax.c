/* GNU m4 -- A simple macro processor
   Copyright (C) 2006-2008, 2010, 2013 Free Software Foundation, Inc.

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

#include <regex.h>
#include <string.h>

#include "m4private.h"

typedef struct {
  const char    *spec;
  const int     code;
} m4_resyntax;

/* The syntaxes named in this table are saved into frozen files.  Changing
   the mappings will break programs that load a frozen file made before
   such a change...  */

static m4_resyntax const m4_resyntax_map[] =
{
  /* First, the canonical definitions for reverse lookups:  */

  { "AWK",                      RE_SYNTAX_AWK },
  { "ED",                       RE_SYNTAX_ED },
  { "EGREP",                    RE_SYNTAX_EGREP },
  { "EMACS",                    RE_SYNTAX_EMACS },
  { "GNU_AWK",                  RE_SYNTAX_GNU_AWK },
  { "GREP",                     RE_SYNTAX_GREP },
  { "POSIX_AWK",                RE_SYNTAX_POSIX_AWK },
  { "POSIX_BASIC",              RE_SYNTAX_POSIX_BASIC },
  { "POSIX_EGREP",              RE_SYNTAX_POSIX_EGREP },
  { "POSIX_EXTENDED",           RE_SYNTAX_POSIX_EXTENDED },
  { "POSIX_MINIMAL_BASIC",      RE_SYNTAX_POSIX_MINIMAL_BASIC },
  { "SED",                      RE_SYNTAX_SED },

  /* The rest are aliases, for forward lookups only:  */

  { "",                         RE_SYNTAX_EMACS },
  { "BASIC",                    RE_SYNTAX_POSIX_BASIC },
  { "BSD_M4",                   RE_SYNTAX_POSIX_EXTENDED },
  { "EXTENDED",                 RE_SYNTAX_POSIX_EXTENDED },
  { "GAWK",                     RE_SYNTAX_GNU_AWK },
  { "GNU_EGREP",                RE_SYNTAX_EGREP },
  { "GNU_EMACS",                RE_SYNTAX_EMACS },
  { "GNU_M4",                   RE_SYNTAX_EMACS },
  { "MINIMAL",                  RE_SYNTAX_POSIX_MINIMAL_BASIC },
  { "MINIMAL_BASIC",            RE_SYNTAX_POSIX_MINIMAL_BASIC },
  { "POSIX_MINIMAL",            RE_SYNTAX_POSIX_MINIMAL_BASIC },

  /* End marker:  */

  { NULL,                       -1 }
};


/* Return the internal code representing the syntax SPEC, or -1 if
   SPEC is invalid.  The `m4_syntax_map' table is searched case
   insensitively, after replacing any spaces or dashes in SPEC with
   underscore characters.  Possible matches for the "GNU_M4" element
   then, are "gnu m4", "GNU-m4" or "Gnu_M4".  */
int
m4_regexp_syntax_encode (const char *spec)
{
  const m4_resyntax *resyntax;
  char *canonical;
  char *p;

  /* Unless specified otherwise, return the historical GNU M4 default.  */
  if (!spec)
    return RE_SYNTAX_EMACS;

  canonical = xstrdup (spec);

  /* Canonicalise SPEC.  */
  for (p = canonical; *p != '\0'; ++p)
    {
      if ((*p == ' ') || (*p == '-'))
        *p = '_';
      else if (islower (to_uchar (*p)))
        *p = toupper (to_uchar (*p));
    }

  for (resyntax = m4_resyntax_map; resyntax->spec != NULL; ++resyntax)
    {
      if (STREQ (resyntax->spec, canonical))
        break;
    }

  free (canonical);

  return resyntax->code;
}


/* Return the syntax specifier that matches CODE, or NULL if there is
   no match.  */
const char *
m4_regexp_syntax_decode (int code)
{
  const m4_resyntax *resyntax;

  for (resyntax = m4_resyntax_map; resyntax->spec != NULL; ++resyntax)
    {
      if (resyntax->code == code)
        break;
    }

  return resyntax->spec;
}

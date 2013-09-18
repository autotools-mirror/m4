/* GNU m4 -- A simple macro processor
   Copyright (C) 1989-1994, 2006-2007, 2010, 2013 Free Software
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

#ifndef M4_H
#define M4_H

#include <signal.h>

#include "m4private.h"

#include "gettext.h"

/* Error handling.  */
#ifdef USE_STACKOVF
void setup_stackovf_trap (char *const *, char *const *,
                          void (*handler) (void));
void stackovf_exit (void);
#endif


/* File: freeze.c --- frozen state files.  */

void produce_frozen_state (m4 *context, const char *);
void reload_frozen_state  (m4 *context, const char *);

#endif /* M4_H */

/* GNU m4 -- A simple macro processor
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 1994, 2006 Free Software
   Foundation, Inc.

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

#ifndef M4_H
#define M4_H

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "m4module.h"

#include "gettext.h"
#include "ltdl.h"

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

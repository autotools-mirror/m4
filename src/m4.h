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

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Canonicalise Windows and Cygwin recognition macros.  */
#if defined(__CYGWIN32__) && !defined(__CYGWIN__)
# define __CYGWIN__ __CYGWIN32__
#endif
#if defined(_WIN32) && !defined(WIN32)
# define WIN32 _WIN32
#endif

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>

#include "m4module.h"

/* An ANSI string.h and pre-ANSI memory.h might conflict.  */

#if defined (HAVE_STRING_H) || defined (STDC_HEADERS)
# include <string.h>
# if !defined (STDC_HEADERS) && defined (HAVE_MEMORY_H)
#  include <memory.h>
# endif
#else
# include <strings.h>
# ifndef memcpy
#  define memcpy(D, S, N) bcopy((S), (D), (N))
# endif
# ifndef strchr
#  define strchr(S, C) index ((S), (C))
# endif
# ifndef strrchr
#  define strrchr(S, C) rindex ((S), (C))
# endif
# ifndef bcopy
void bcopy ();
# endif
#endif

#if STDC_HEADERS || HAVE_STDLIB_H
#  include <stdlib.h>
#else /* not STDC_HEADERS */

voidstar malloc ();
voidstar realloc ();
char *getenv ();
double atof ();
long strtol ();

#endif /* STDC_HEADERS */

#include <errno.h>

#ifndef errno
extern int errno;
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_LOCALE_H
# include <locale.h>
#else
# define setlocale(Category, Locale)
#endif

#if HAVE_SIGNAL_H
# include <signal.h>
#elif HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

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

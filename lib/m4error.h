/* Declaration for error-reporting function
   Copyright (C) 1995, 1996, 1997 Free Software Foundation, Inc.

   This file is part of the GNU C Library.  Its master source is NOT part of
   the C library, however.  The master source lives in /gd/gnu/lib.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef M4ERROR_H
#define M4ERROR_H 1

/* DLL building support on win32 hosts;  mostly to workaround their
   ridiculous implementation of data symbol exporting. */
#ifndef M4_SCOPE
#  ifdef _WIN32
  /* Incase we are linking a dll with this library, the
     LIBM4_DLL_IMPORT takes precedence over a generic DLL_EXPORT
     when defining the SCOPE variable for M4.  */
#    ifdef LIBM4_DLL_IMPORT	/* define if linking with this dll */
#      define M4_SCOPE extern __declspec(dllimport)
#    else
#      ifdef DLL_EXPORT		/* defined by libtool (if required) */
#        define M4_SCOPE	__declspec(dllexport)
#      endif /* DLL_EXPORT */
#    endif /* LIBM4_DLL_IMPORT */
#  endif /* M4_SCOPE */
#  ifndef M4_SCOPE		/* static linking or !_WIN32 */
#    define M4_SCOPE	extern
#  endif
#endif


#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(Spec) /* empty */
# endif
/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __format__ format
#  define __printf__ printf
# endif
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if defined (__STDC__) && __STDC__

/* Print a message with `fprintf (stderr, FORMAT, ...)';
   if ERRNUM is nonzero, follow it with ": " and strerror (ERRNUM).
   If STATUS is nonzero, terminate the program with `exit (STATUS)'.  */

extern void error (int status, int errnum, const char *format, ...)
     __attribute__ ((__format__ (__printf__, 3, 4)));

extern void error_at_line (int status, int errnum, const char *fname,
			   unsigned int lineno, const char *format, ...)
     __attribute__ ((__format__ (__printf__, 5, 6)));

/* If NULL, error will flush stdout, then print on stderr the program
   name, a colon and a space.  Otherwise, error will call this
   function without parameters instead.  */
M4_SCOPE void (*error_print_progname) (void);

#else
void error ();
void error_at_line ();
M4_SCOPE void (*error_print_progname) ();
#endif

/* This variable is incremented each time `error' is called.  */
M4_SCOPE unsigned int error_message_count;

/* Sometimes we want to have at most one error per line.  This
   variable controls whether this mode is selected or not.  */
M4_SCOPE int error_one_per_line;

#ifdef	__cplusplus
}
#endif

#endif /* m4error.h */

/* GNU m4 -- A simple macro processor
   Copyright 2000 Free Software Foundation, Inc.

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

/**
 * This file is installed, so cannot rely on the contents of config.h.
 * It works best if included _after_ system headers.
 **/

#ifndef M4_SYSTEM_H
#define M4_SYSTEM_H 1

/* I have yet to see a system that doesn't have these... */
#include <stdio.h>
#include <sys/types.h>

/* This is okay in an installed file, because it will not change the
   behaviour of the including program whether ENABLE_NLS is defined
   or not.  */
#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(Text) gettext ((Text))
#  else
#    define _(Text) (Text)
#  endif
#endif


/* All header files should be inside BEGIN_C_DECLS ... END_C_DECLS, so
   that the library can be linked into a C++ program.  The multi-include
   guard macros must be outside, as should any #includes -- for simplicity
   everything else should go inside.  */

#ifndef BEGIN_C_DECLS
#  ifdef __cplusplus
#    define BEGIN_C_DECLS	extern "C" {
#    define END_C_DECLS		}
#  else /* !__cplusplus */
#    define BEGIN_C_DECLS	/* empty */
#    define END_C_DECLS		/* empty */
#  endif /* __cplusplus */
#endif /* !BEGIN_C_DECLS */

BEGIN_C_DECLS



/* DLL building support on win32 hosts;  mostly to workaround their
   ridiculous implementation of data symbol exporting.  As it stands,
   this code assumes that the project will build a single library,
   defining DLL_EXPORT when compiling objects destined for the library,
   and LIBM4_DLL_IMPORT when linking against the library.  */

/* Canonicalise Windows and Cygwin recognition macros.  */
#if defined __CYGWIN32__ && !defined __CYGWIN__
#  define __CYGWIN__ __CYGWIN32__
#endif
#if defined _WIN32 && !defined WIN32
#  define WIN32 _WIN32
#endif

#if defined WIN32 && !defined __CYGWIN__
/* M4_DIRSEP_CHAR is accepted *in addition* to '/' as a directory
   separator when it is set. */
#  define M4_DIRSEP_CHAR	'\\'
#  define M4_PATHSEP_CHAR	';'
#endif
#ifndef M4_PATHSEP_CHAR
#  define M4_PATHSEP_CHAR	':'
#endif

#ifndef M4_SCOPE
#  ifdef WIN32
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

/* Always define global variables with this macro.  */
#ifdef DLL_EXPORT		/* defined by libtool (if required) */
#  define M4_GLOBAL_DATA	__declspec(dllexport)
#else
#  define M4_GLOBAL_DATA	/* empty */
#endif /* DLL_EXPORT */



/* M4_PARAMS is a macro used to wrap function prototypes, so that compilers
   that don't understand ANSI C prototypes still work, and ANSI C
   compilers can issue warnings about type mismatches. */
#undef M4_PARAMS
#if defined (__STDC__) || defined (_AIX) || (defined (__mips) && defined (_SYSTYPE_SVR4)) || defined(WIN32) || defined(__cplusplus)
# define M4_PARAMS(protos)	protos
#else
# define M4_PARAMS(protos)	()
#endif



/* M4_STMT_START/END are used to create macros which expand to a
   a single compound statement in a portable way, but crucially in
   a way sympathetic to the compiler to maximise optimisation.  */
#undef M4_STMT_START
#undef M4_STMT_END
#if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#  define M4_STMT_START        (void)(
#  define M4_STMT_END          )
#else
#  if (defined (sun) || defined (__sun__))
#    define M4_STMT_START      if (1)
#    define M4_STMT_END        else (void)0
#  else
#    define M4_STMT_START      do
#    define M4_STMT_END        while (0)
#  endif
#endif


#if !defined __PRETTY_FUNCTION__
#  define __PRETTY_FUNCTION__	"<unknown>"
#endif



/* Using ``VOID *'' for untyped pointers prevents pre-ANSI compilers
   from choking.  */
#ifndef VOID
#  if __STDC__
#    define VOID void
#  else
#    define VOID char
#  endif
#endif



/* Preprocessor token manipulation.  */

/* The extra indirection to the _STR macro is required so that if the
   argument to STR() is a macro, it will be expanded before being quoted.   */
#ifndef STR
#  if __STDC__
#    define _STR(arg)	#arg
#  else
#    define _STR(arg)	"arg"
#  endif
#  define STR(arg)	_STR(arg)
#endif

#ifndef CONC
#  if __STDC__
#    define CONC(a, b)	a##b
#  else
#    define CONC(a, b)	a/**/b
#  endif
#endif



/* Make sure these are defined.  */
#ifndef EXIT_FAILURE
#  define EXIT_SUCCESS	0
#  define EXIT_FAILURE	1
#endif



/* If FALSE is defined, we presume TRUE is defined too.  In this case,
   merely typedef boolean as being int.  Or else, define these all.  */
#ifndef FALSE
/* Do not use `enum boolean': this tag is used in SVR4 <sys/types.h>.  */
typedef enum { FALSE = 0, TRUE = 1 } m4_boolean;
#else
typedef int m4_boolean;
#endif
/* `boolean' is already a macro on some systems.  */
#ifndef boolean
#  define boolean m4_boolean
#endif



/* Memory allocation.  */
#define XCALLOC(type, num)	((type *) xcalloc ((num), sizeof(type)))
#define XMALLOC(type, num)	((type *) xmalloc ((num) * sizeof(type)))
#define XREALLOC(type, p, num)	((type *) xrealloc ((p), (num) * sizeof(type)))
#define XFREE(stale)				M4_STMT_START {		\
  	if (stale) { free ((VOID *) stale);  stale = 0; }		\
						} M4_STMT_END

extern VOID *xcalloc  M4_PARAMS((size_t n, size_t s));
extern VOID *xmalloc  M4_PARAMS((size_t n));
extern VOID *xrealloc M4_PARAMS((VOID *p, size_t n));
extern void  xfree    M4_PARAMS((VOID *stale));

extern char *xstrdup  M4_PARAMS((const char *string));

END_C_DECLS

#endif /* !M4_SYSTEM_H */

AC_DEFUN(AM_WITH_MODULES,
  [AC_MSG_CHECKING(if support for dynamic modules is wanted)
  AC_ARG_WITH(modules,
  [  --with-modules          add support for dynamic modules],
  [use_modules=$withval], [use_modules=no])
  AC_MSG_RESULT($use_modules)

  if test "$use_modules" = yes; then
    LIBS="$LIBS -ldl"
    AC_CHECK_HEADER([dlfcn.h],
      [AC_CACHE_CHECK([for dlopen in libdl], ac_cv_func_dlopen_libdl,
	 [AC_TRY_LINK([#include <dlfcn.h>],
	    [(void)dlopen(0, RTLD_NOW)],
	    ac_cv_func_dlopen_libdl=yes,
	    ac_cv_func_dlopen_libdl=no)])],
	  ac_cv_func_dlopen_libdl=no)

    if test "$ac_cv_func_dlopen_libdl$ac_cv_header_dlfcn_h" = yesyes; then
      AC_DEFINE(WITH_MODULES)
      LDFLAGS="$LDFLAGS -rdynamic"
    else
      LIBS=`echo $LIBS | sed -e 's/-ldl//'`
      AC_MSG_WARN([-ldl library not found or does not appear to work])
      use_modules=no
    fi
  fi
  ])

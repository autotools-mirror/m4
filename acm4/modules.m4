# Written by René Seindal (rene@seindal.dk)
#
# This file can be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.

# serial 1


AC_DEFUN(AM_WITH_MODULES,
  [AC_MSG_CHECKING(if support for dynamic modules is wanted)
  AC_ARG_WITH(modules,
  [  --with-modules          add support for dynamic modules],
  [use_modules=$withval], [use_modules=no])
  AC_MSG_RESULT($use_modules)

  if test "$use_modules" = yes; then
    dnl We might no have it anyway, after all.
    with_modules=no

    dnl Test for dlopen in libc
    AC_CHECK_FUNCS(dlopen)
    if test "$ac_cv_func_dlopen" = yes; then
       with_modules=yes
    fi

    dnl Test for dlopen in libdl
    if test "$with_modules" = no; then
      AC_CHECK_LIB(dl, dlopen)
      if test "$ac_cv_lib_dl_dlopen" = yes; then
	with_modules=yes

#	LIBS="$LIBS -ldl"
	AC_DEFINE(HAVE_DLOPEN,1)
      fi
    fi

#    dnl Test for dld_link in libdld
#    if test "$with_modules" = no; then
#      AC_CHECK_LIB(dld, dld_link)
#      if test "$ac_cv_lib_dld_dld_link" = "yes"; then
#	 with_modules=yes
#	 AC_DEFINE(HAVE_DLD,1)
#      fi
#    fi

    dnl Test for shl_load in libdld
    if test "$with_modules" = no; then
       AC_CHECK_LIB(dld, shl_load)
       if test "$ac_cv_lib_dld_shl_load" = yes; then
	  with_modules=yes

#	  LIBS="$LIBS -ldld"
	  AC_DEFINE(HAVE_SHL_LOAD,1)
       fi
    fi

    if test "$with_modules" = yes; then
      dnl This is for libtool
      DLLDFLAGS=-export-dynamic

      MODULES_DIR=modules
      MODULE_PATH="${pkglibexecdir}"

      AC_DEFINE(WITH_MODULES, 1)
    fi

    AC_SUBST(DLLDFLAGS)
    AC_SUBST(MODULES_DIR)
    AC_SUBST(MODULE_PATH)
  fi
  ])




AC_DEFUN(AM_WITH_MODULES,
  [AC_MSG_CHECKING(if support for dynamic modules is wanted)
  AC_ARG_WITH(modules,
  [  --with-modules          add support for dynamic modules],
  [use_modules=$withval], [use_modules=no])
  AC_MSG_RESULT($use_modules)

  if test "$use_modules" = yes; then
    AC_CHECK_FUNCS(dlopen dlsym dlclose)
    if test "$ac_cv_func_dlopen" = yes; then
       MODULE_O=module.o
       MODULES_DIR=modules
    fi

    AC_CHECK_LIB(dl, dlopen, HAVE_LIBDL=true)
    if test "$HAVE_LIBDL" = true; then
      LIBS="$LIBS -ldl"
      MODULE_O=module.o
      MODULES_DIR=modules
      AC_DEFINE(HAVE_DLOPEN,1)
    fi

    dnl Default extension for shared libraries.
    SHARED_EXT=.so

    dnl Maybe this is a system using shl_load and shl_findsym?
    if test "${MODULES_DIR}" = ""; then
       AC_CHECK_LIB(dld, shl_load, HAVE_LIBDLD=true)
       if test "$HAVE_LIBDLD" = true; then
	  LIBS="$LIBS -ldld"
	  MODULE_O=module.o
	  MODULES_DIR=modules
	  SHARED_EXT=.O
	  AC_DEFINE(USE_SHL_LOAD,1)
       fi
    fi

    if test "$MODULES_DIR"; then
      SHARED_LD=ld
      DASH_SHARED=""
      case "$host_os" in
	sunos4*)    DASH_SHARED=-Bdynamic ;;
	 linux*)    DASH_SHARED=-shared; DLLDFLAGS=-rdynamic ;;
	 winnt*)    SHARED_LD=./make-dll; SHARED_EXT=.dll; DLLDFLAGS=-rdynamic ;;
	   osf*)    DASH_SHARED=-shared; SHARED_LD=$CC ;;
	  irix*)    DASH_SHARED=-shared; SHARED_LD=$CC ;;
	  bsdi*)    DASH_SHARED=-shared; DLLDFLAGS=-rdynamic ;;
       freebsd*)    DASH_SHARED=-Bshareable; SHARED_LD='$(LD) 2>/dev/null';;
       solaris*)
	  # If both the GNU compiler and linker are installed, then we need
	  # to add special options in order to compile the modules.
	  if test "$GCC" = "yes"; then
	     DASH_SHARED=-shared; GCC_FPIC=-fpic; 
	     SHARED_LD='$(CC)'; DLLDFLAGS="-Xlinker -E";
	  else
	     DASH_SHARED=-G;
	  fi
	  ;;
           aix*) DLLDFLAGS="-bexpall -brtl" ;;

	  hpux*)    DASH_SHARED="-b -E" GCC_FPIC=-fpic DLLDFLAGS="-Xlinker -E" ;;
      esac
    fi

    AC_SUBST(SHARED_LD)
    AC_SUBST(DASH_SHARED)
    AC_SUBST(MODULE_O)
    AC_SUBST(MODULES_DIR)
    AC_SUBST(DLLDFLAGS)
    AC_SUBST(SHARED_EXT)
    AC_SUBST(OS_NAME)
    AC_SUBST(GCC_FPIC)

    MODULE_PATH="${pkglibexecdir}"
    AC_SUBST(MODULE_PATH)
  fi
  ])

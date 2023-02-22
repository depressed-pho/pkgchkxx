# -*- autoconf -*-
AC_DEFUN([AX_LIBFETCH], [
    AC_ARG_WITH(
        [libfetch-prefix],
        [AS_HELP_STRING([--with-libfetch-prefix], [path to libfetch installation directory])])
    AS_IF([test x"$with_libfetch_prefix" != x"" -a x"$with_libfetch_prefix" != x"no"],
          [LIBFETCH_CPPFLAGS="-I${with_libfetch_prefix}/include"
           LIBFETCH_LIBS="-L${with_libfetch_prefix}/lib"])

    saved_CPPFLAGS="$CPPFLAGS"
    saved_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $LIBFETCH_CPPFLAGS"
    LIBS="$LIBS $LIBFETCH_LIBS"
    AC_CHECK_HEADER([fetch.h], [], [AC_MSG_ERROR([fetch.h is missing])])
    AC_CHECK_LIB(
        [fetch],
        [fetchGetURL],
        [LIBFETCH_LIBS="$LIBFETCH_LIBS -lfetch"],
        [AC_MSG_ERROR([libfetch is missing])])
    LIBS="$saved_LIBS"
    CPPFLAGS="$saved_CPPFLAGS"

    AC_SUBST([LIBFETCH_CPPFLAGS])
    AC_SUBST([LIBFETCH_LIBS])
])

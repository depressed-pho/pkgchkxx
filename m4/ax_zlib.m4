# -*- autoconf -*-
AC_DEFUN([AX_ZLIB], [
    AC_ARG_WITH(
        [zlib-prefix],
        [AS_HELP_STRING([--with-zlib-prefix], [path to zlib installation directory])])
    AS_IF([test x"$with_zlib_prefix" != x"" -a x"$with_zlib_prefix" != x"no"],
          [ZLIB_CPPFLAGS="-I${with_zlib_prefix}/include"
           ZLIB_LIBS="-L${with_zlib_prefix}/lib"])

    saved_CPPFLAGS="$CPPFLAGS"
    saved_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $ZLIB_CPPFLAGS"
    LIBS="$LIBS $ZLIB_LIBS"
    AC_CHECK_HEADER([zlib.h], [], [AC_MSG_ERROR([zlib.h is missing])])
    AC_CHECK_LIB(
        [z],
        [inflate],
        [ZLIB_LIBS="$ZLIB_LIBS -lz"],
        [AC_MSG_ERROR([libz is missing])])
    LIBS="$saved_LIBS"
    CPPFLAGS="$saved_CPPFLAGS"

    AC_SUBST([ZLIB_CPPFLAGS])
    AC_SUBST([ZLIB_LIBS])
])

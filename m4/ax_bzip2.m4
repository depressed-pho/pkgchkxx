# -*- autoconf -*-
AC_DEFUN([AX_BZIP2], [
    AC_ARG_WITH(
        [bzip2-prefix],
        [AS_HELP_STRING([--with-bzip2-prefix], [path to libbz2 installation directory])])
    AS_IF([test x"$with_bzip2_prefix" != x"" -a x"$with_bzip2_prefix" != x"no"],
          [BZIP2_CPPFLAGS="-I${with_bzip2_prefix}/include"
           BZIP2_LIBS="-L${with_bzip2_prefix}/lib"])

    saved_CPPFLAGS="$CPPFLAGS"
    saved_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $BZIP2_CPPFLAGS"
    LIBS="$LIBS $BZIP2_LIBS"
    AC_CHECK_HEADER([bzlib.h], [], [AC_MSG_ERROR([bzlib.h is missing])])
    AC_CHECK_LIB(
        [bz2],
        [BZ2_bzDecompressInit],
        [BZIP2_LIBS="$BZIP2_LIBS -lbz2"],
        [AC_MSG_ERROR([libbz2 is missing])])
    LIBS="$saved_LIBS"
    CPPFLAGS="$saved_CPPFLAGS"

    AC_SUBST([BZIP2_CPPFLAGS])
    AC_SUBST([BZIP2_LIBS])
])

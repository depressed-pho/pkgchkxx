# -*- autoconf -*-
AC_DEFUN([AX_DOXYGEN], [
    AC_ARG_VAR([DOXYGEN], [path to doxygen command])
    AC_ARG_VAR([DIA], [path to dia command])
    AC_ARG_VAR([DOT], [path to dot command])

    AC_ARG_ENABLE(
        [doxygen],
        [AS_HELP_STRING([--disable-doxygen], [don't gnerate API documentation with Doxygen])])
    AS_IF([test x"$enable_doxygen" != x"no"],
        [AS_IF([test x"$DOXYGEN" =  x], [AC_PATH_PROG([DOXYGEN], [doxygen])])
         AS_IF([test x"$DOXYGEN" != x],
             [# Doxygen isn't explicitly disabled, and it was found in the system.
              AS_IF([test x"$DIA" = x], [AC_PATH_PROG([DIA], [dia])])
              AS_IF([test x"$DOT" = x], [AC_PATH_PROG([DOT], [dot])])])])
    AS_IF([test x"$DOT" != x], [HAVE_DOT="YES"], [HAVE_DOT="NO"])

    # Strangely, Doxyfile wants a directory where the dia binary resides,
    # not a path to dia itself.
    AS_IF([test x"$DIA" != x],
          [DIA_PATH=`AS_DIRNAME([["$DIA"]])`],
          [DIA_PATH=])

    AM_CONDITIONAL([ENABLE_DOXYGEN], [test x"$enable_doxygen" != x"no" && test -n "$DOXYGEN"])
    AC_SUBST([DOXYGEN])
    AC_SUBST([DIA_PATH])
    AC_SUBST([DOT])
    AC_SUBST([HAVE_DOT])
])

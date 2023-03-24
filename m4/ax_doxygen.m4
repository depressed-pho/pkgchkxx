# -*- autoconf -*-
AC_DEFUN([AX_DOXYGEN], [
    AC_ARG_VAR([DOXYGEN], [path to doxygen command])
    AC_ARG_VAR([DIA], [path to dia command])
    AC_ARG_VAR([DOT], [path to dot command])

    AC_ARG_ENABLE(
        [doxygen],
        [AS_HELP_STRING([--disable-doxygen], [Don't gnerate API documentation with Doxygen])])
    AS_IF([test x"$enable_doxygen" != x"no"],
        [AS_IF([test -z "$DOXYGEN"], [AC_PATH_PROG([DOXYGEN], [doxygen])])
         AS_IF([test -n "$DOXYGEN"],
             [# Doxygen isn't explicitly disabled, and it was found in the system.
              AS_IF([test -z "$DIA"], [AC_PATH_PROG([DIA], [dia], [dia])])
              AS_IF([test -z "$DOT"], [AC_PATH_PROG([DOT], [dot])])])])
    AS_IF([test -n x"$DOT"], [HAVE_DOT="YES"], [HAVE_DOT="NO"])

    AM_CONDITIONAL([ENABLE_DOXYGEN], [test x"$enable_doxygen" != x"no" && test -n "$DOXYGEN"])
    AC_SUBST([DOXYGEN])
    AC_SUBST([DIA])
    AC_SUBST([DOT])
    AC_SUBST([HAVE_DOT])
])
